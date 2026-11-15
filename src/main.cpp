#include "kube/kubeconfig.hpp"
#include "model/cluster_state.hpp"
#include "render/camera.hpp"
#include "render/person.hpp"
#include "source/fake_source.hpp"
#include "source/kube_source.hpp"
#include "source/session.hpp"
#include "theme/theme.hpp"
#include "ui/command_ui.hpp"
#include "util/config.hpp"
#include "world/layout.hpp"
#include "world/movement.hpp"
#include "world/villagers.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <random>
#include <raylib.h>
#include <rlgl.h>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

constexpr int kWidth{1280};
constexpr int kHeight{720};
constexpr const char* const kTitle{"k10s"};
class Window {

  public:
    explicit Window(int w, int h, const char* title) {
        SetConfigFlags(FLAG_MSAA_4X_HINT); // smooth (anti-aliased) shape edges
        InitWindow(w, h, title);
        if (!IsWindowReady())
            throw std::runtime_error("failed to open window");
        SetTargetFPS(60);
    }

    ~Window() { CloseWindow(); }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;
};

namespace {

void drawHud(const std::string& context, std::size_t nodes, std::size_t pods, long burning) {
    DrawRectangle(0, 0, kWidth, 34, Color{20, 25, 30, 210});
    DrawText("k10s", 12, 7, 20, RAYWHITE);
    std::string label = "context: " + context;
    while (label.size() > 12 && MeasureText(label.c_str(), 18) > kWidth / 2) // leave room for stats
        label = label.substr(0, label.size() - 4) + "...";
    DrawText(label.c_str(), 80, 9, 18, Color{170, 180, 190, 255});
    const char* stats = TextFormat("nodes %zu   pods %zu", nodes, pods);
    const char* fire = TextFormat("on fire %ld", burning);
    const int fireW = MeasureText(fire, 18);
    const int statsW = MeasureText(stats, 18);
    DrawText(stats, kWidth - fireW - statsW - 40, 9, 18, RAYWHITE);
    DrawText(fire, kWidth - fireW - 14, 9, 18,
             burning > 0 ? Color{255, 140, 40, 255} : Color{140, 200, 120, 255});
}

bool contains(Rectangle outer, Rectangle inner) {
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.width <= outer.x + outer.width &&
           inner.y + inner.height <= outer.y + outer.height;
}

// Built-in simulated clusters for development and screenshots. Only compiled into builds
// with K10S_DEMO_CONTEXTS (on by default for Debug); release builds show real clusters only.
struct FakeContext {
    const char* name;
    std::uint64_t seed;
    int nodes;
    int podsPerNode;
};
#ifdef K10S_DEMO_CONTEXTS
constexpr std::array<FakeContext, 3> kFakeContexts{{
    {"fake-village", 42, 6, 8},
    {"fake-hamlet", 7, 3, 5},
    {"fake-town", 99, 14, 6},
}};
#else
constexpr std::array<FakeContext, 0> kFakeContexts{};
#endif

// Where to go when no real context is usable: a demo cluster if built in, else nothing.
std::string fallbackContext() { return kFakeContexts.empty() ? std::string{} : kFakeContexts.front().name; }
std::string fallbackNote() {
    return kFakeContexts.empty() ? "run `k10s --contexts` to see what k10s found" : "showing a demo cluster";
}

// Used when there's nothing to connect to: the village stays empty, the app stays usable
// (you can still :ctx to a context once one exists).
class EmptySource final : public k10s::DataSource {
  public:
    void run(std::stop_token st, k10s::EventQueue&) override {
        while (!st.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

// Returns the data source for a context, or nullptr if it's unknown.
std::unique_ptr<k10s::DataSource> makeSource(const std::string& context,
                                             const k10s::Kubeconfig& kubeconfig) {
    for (const auto& f : kFakeContexts)
        if (context == f.name)
            return std::make_unique<k10s::FakeSource>(f.seed, f.nodes, f.podsPerNode);
    if (const auto* ctx = kubeconfig.find(context))
        return std::make_unique<k10s::KubeSource>(*ctx);
    return nullptr;
}

// k10s [--context NAME]   (like k9s; defaults to the kubeconfig's current-context)
struct StartChoice {
    std::string context;
    std::string note; // why we didn't use current-context, shown to the user
};

// --context flag > K10S_CONTEXT > kubeconfig current-context > first real context > demo.
StartChoice pickStartContext(int argc, char** argv, const k10s::Kubeconfig& kubeconfig) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string_view{argv[i]} == "--context")
            return {argv[i + 1], {}};
    if (const char* env = std::getenv("K10S_CONTEXT"))
        return {env, {}};
    const std::string& cur = kubeconfig.currentContext;
    if (kubeconfig.find(cur))
        return {cur, {}};
    if (!kubeconfig.contexts.empty()) {
        const std::string& first = kubeconfig.contexts.front().name;
        return {first, cur.empty() ? "kubeconfig has no current-context; using " + first
                                   : "current-context '" + cur + "' not found; using " + first};
    }
    if (!kubeconfig.warnings.empty())
        return {fallbackContext(),
                "couldn't load kubeconfig (" + kubeconfig.warnings.front() + "); " + fallbackNote()};
    return {fallbackContext(), "no contexts in kubeconfig; " + fallbackNote()};
}

// `k10s --contexts`: what k10s sees in your kubeconfig, for troubleshooting.
int printContexts() {
    const k10s::Kubeconfig cfg = k10s::loadKubeconfig();
    std::printf("kubeconfig: %s\n", std::getenv("KUBECONFIG") ? std::getenv("KUBECONFIG") : "~/.kube/config");
    std::printf("current-context: %s\n", cfg.currentContext.empty() ? "(none)" : cfg.currentContext.c_str());
    if (cfg.contexts.empty())
        std::printf("no contexts found\n");
    for (const auto& c : cfg.contexts)
        std::printf("  %s %-40s %s\n", c.name == cfg.currentContext ? "*" : " ", c.name.c_str(),
                    c.server.c_str());
    for (const auto& w : cfg.warnings)
        std::printf("warning: %s\n", w.c_str());
    return 0;
}

} // namespace

// k10s [--theme NAME]; else K10S_THEME; else saved preference; else "village".
std::string pickStartTheme(int argc, char** argv, const k10s::UserConfig& cfg) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string_view{argv[i]} == "--theme")
            return argv[i + 1];
    if (const char* env = std::getenv("K10S_THEME"))
        return env;
    if (!cfg.theme.empty())
        return cfg.theme;
    return "village";
}

int main(int argc, char** argv) {
    // raylib logs every init step at INFO; only show warnings/errors unless --verbose.
    SetTraceLogLevel(LOG_WARNING);
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--verbose")
            SetTraceLogLevel(LOG_INFO);
        if (arg == "--version" || arg == "-v") {
            std::printf("k10s %s\n", K10S_VERSION);
            return 0;
        }
        if (arg == "--contexts")
            return printContexts();
        if (arg == "--help" || arg == "-h") {
            std::printf("usage: k10s [--context NAME] [--theme village|city|beach] [--verbose]\n"
                        "       k10s --contexts   list kubeconfig contexts\n"
                        "in-app: drag/scroll to pan/zoom, f to fit, :ctx, :theme, :q\n");
            return 0;
        }
    }

    Window w{kWidth, kHeight, kTitle};
    SetExitKey(KEY_NULL); // Esc closes menus (like k9s) instead of quitting; use :q

    // Context list: built-in fakes first, then everything in your kubeconfig.
    const k10s::Kubeconfig kubeconfig = k10s::loadKubeconfig();
    std::vector<std::string> contexts;
    for (const auto& f : kFakeContexts)
        contexts.emplace_back(f.name);
    for (const auto& c : kubeconfig.contexts)
        contexts.push_back(c.name);

    for (const auto& warning : kubeconfig.warnings)
        std::fprintf(stderr, "k10s: kubeconfig: %s\n", warning.c_str());

    auto [currentContext, startNote] = pickStartContext(argc, argv, kubeconfig);
    auto startSource = makeSource(currentContext, kubeconfig);
    if (!startSource && !currentContext.empty()) {
        startNote = "unknown context '" + currentContext + "'; " + fallbackNote();
        currentContext = fallbackContext();
        startSource = makeSource(currentContext, kubeconfig);
    }
    if (!startSource) { // nothing to connect to: empty village, message explains why
        currentContext = "(none)";
        startSource = std::make_unique<EmptySource>();
    }
    auto session = std::make_unique<k10s::Session>(std::move(startSource));
    k10s::CommandUi ui;
    if (!startNote.empty()) {
        std::fprintf(stderr, "k10s: %s\n", startNote.c_str());
        ui.flash(startNote, 10.0f);
    }
    k10s::ClusterState state;

    k10s::UserConfig userConfig = k10s::loadUserConfig();
    std::string currentTheme = pickStartTheme(argc, argv, userConfig);
    auto theme = k10s::makeTheme(currentTheme);
    if (!theme) {
        std::fprintf(stderr, "k10s: unknown theme '%s', using village\n", currentTheme.c_str());
        currentTheme = "village";
        theme = k10s::makeTheme(currentTheme);
    }

    k10s::Villagers villagers;
    std::mt19937 moveRng{1234};
    bool stateChanged = true;

    k10s::VillageLayout layout;
    k10s::CameraController camera{Rectangle{0.0f, 34.0f, static_cast<float>(kWidth), kHeight - 64.0f}};

    // Meadow decorations cover whatever the camera can see; regenerated as the village grows.
    Rectangle sceneryArea{};
    std::vector<k10s::Decoration> scenery;

    // Dev hook: K10S_SCREENSHOT=out.png saves a frame after ~3s and exits.
    const char* screenshotPath = std::getenv("K10S_SCREENSHOT");
    const char* startCommand = std::getenv("K10S_COMMAND"); // dev hook, e.g. "ctx"
    int frame = 0;

    while (!WindowShouldClose()) {
        // --- input ----------------------------------------------------------
        const k10s::CommandUi::Choices choices{contexts, currentContext, k10s::themeNames(), currentTheme};
        auto action = ui.update(choices, GetFrameTime());
        if (startCommand && frame == 0) {
            action = ui.run(startCommand, choices);
            startCommand = nullptr;
        }
        if (action.kind == k10s::CommandUi::ActionKind::Quit)
            break;
        if (action.kind == k10s::CommandUi::ActionKind::SwitchContext) {
            if (auto source = makeSource(action.arg, kubeconfig)) {
                session.reset(); // stop + join the old thread before starting a new one
                state = k10s::ClusterState{};
                villagers = k10s::Villagers{};
                layout.clear();
                camera.fit(/*snap=*/true);
                session = std::make_unique<k10s::Session>(std::move(source));
                currentContext = action.arg;
                stateChanged = true;
            } else {
                ui.flash("unknown context: " + action.arg);
            }
        }
        if (action.kind == k10s::CommandUi::ActionKind::SwitchTheme) {
            if (auto next = k10s::makeTheme(action.arg)) {
                theme = std::move(next);
                currentTheme = action.arg;
                // Building sizes differ per theme: re-lay out and re-home the villagers.
                layout.clear();
                villagers = k10s::Villagers{};
                sceneryArea = {};
                camera.fit(/*snap=*/true);
                stateChanged = true;
                userConfig.theme = currentTheme;
                if (!k10s::saveUserConfig(userConfig))
                    ui.flash("theme set, but couldn't save " + k10s::userConfigPath().string());
            }
        }

        // --- update ---------------------------------------------------------
        while (auto e = session->events().try_pop()) {
            state.apply(*e);
            stateChanged = true;
        }

        if (stateChanged) {
            layout.update(state, *theme);
            std::unordered_map<std::string, Vector2> yardPos; // where each node's villagers live
            for (const auto& h : layout.houses())
                yardPos[h.name] = h.yard();
            villagers.sync(state, yardPos);
            stateChanged = false;
        }

        const Rectangle world = layout.bounds();           // where buildings and villagers are
        const Rectangle framed = theme->frame(world);      // what the camera keeps in view
        const float dt = std::min(GetFrameTime(), 0.05f); // avoid huge jumps after a stall
        camera.update(framed, dt, /*acceptInput=*/!ui.capturingInput() && !screenshotPath);

        const Rectangle visible = camera.visibleWorld();
        if (!contains(sceneryArea, visible)) {
            sceneryArea = {visible.x - visible.width * 0.5f, visible.y - visible.height * 0.5f,
                           visible.width * 2.0f, visible.height * 2.0f};
            scenery = theme->makeScenery(7, sceneryArea);
        }

        k10s::updateVillagers(villagers, dt, moveRng, world);

        // --- draw -----------------------------------------------------------
        const float now = static_cast<float>(GetTime());
        BeginDrawing();
        ClearBackground(theme->background());

        BeginMode2D(camera.camera());
        theme->drawGround(visible, world, now);
        theme->drawScenery(scenery, world, now);
        for (const auto& h : layout.houses())
            theme->drawYard(h.yard());

        // Painter's algorithm: draw buildings, props and villagers together, back to front by
        // their ground y, so villagers can walk behind buildings and trees.
        enum class Kind { Building, Prop, Villager };
        struct Drawable {
            float y;
            Kind kind;
            std::size_t idx;
            std::size_t sub; // which prop of the building
        };
        const auto& houses = layout.houses();
        std::vector<Drawable> scene;
        scene.reserve(houses.size() * 3 + villagers.size());
        for (std::size_t h = 0; h < houses.size(); ++h) {
            scene.push_back({houses[h].base.y, Kind::Building, h, 0});
            const auto& props = theme->building(houses[h].uid).props;
            for (std::size_t p = 0; p < props.size(); ++p)
                scene.push_back({houses[h].base.y + props[p].dy, Kind::Prop, h, p});
        }
        for (std::size_t v = 0; v < villagers.size(); ++v)
            scene.push_back({villagers.pos[v].y, Kind::Villager, v, 0});
        std::sort(scene.begin(), scene.end(),
                  [](const Drawable& a, const Drawable& b) { return a.y < b.y; });

        for (const auto& d : scene) {
            switch (d.kind) {
            case Kind::Building: {
                const auto& h = houses[d.idx];
                const auto& info = theme->building(h.uid);
                theme->drawBuilding(h.base, h.uid, h.ready, now);
                k10s::drawSign({h.base.x, h.base.y - info.labelHeight}, h.name, theme->signStyle(),
                               info.halfWidth * 1.5f, camera.camera().zoom);
                break;
            }
            case Kind::Prop: {
                const auto& h = houses[d.idx];
                const auto& p = theme->building(h.uid).props[d.sub];
                theme->drawProp({h.base.x + p.dx, h.base.y + p.dy}, p, now);
                break;
            }
            case Kind::Villager:
                k10s::drawPerson(villagers.pos[d.idx], villagers.vel[d.idx], villagers.look[d.idx],
                                 villagers.onFire[d.idx] != 0, now);
                break;
            }
        }
        EndMode2D();

        drawHud(currentContext, state.nodeCount(), villagers.size(),
                std::count(villagers.onFire.begin(), villagers.onFire.end(), 1));

        ui.draw(choices, kWidth, kHeight);

        if (screenshotPath && ++frame == 180) {
            rlDrawRenderBatchActive(); // flush pending draws before reading pixels
            Image img = LoadImageFromScreen();
            ExportImage(img, screenshotPath);
            UnloadImage(img);
            break;
        }

        EndDrawing();
    }

    return 0;
}
