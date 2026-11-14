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
    DrawText(TextFormat("context: %s", context.c_str()), 80, 9, 18, Color{170, 180, 190, 255});
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

// A name sign centred at `centre`.
void drawLabel(const std::string& text, Vector2 centre) {
    constexpr int fontSize = 16;
    const int w = MeasureText(text.c_str(), fontSize);
    const int x = static_cast<int>(centre.x) - w / 2;
    const int y = static_cast<int>(centre.y) - fontSize / 2;
    DrawRectangleRounded({static_cast<float>(x - 6), static_cast<float>(y - 2),
                          static_cast<float>(w + 12), static_cast<float>(fontSize + 4)},
                         0.4f, 4, Color{30, 30, 30, 150});
    DrawText(text.c_str(), x, y, fontSize, RAYWHITE);
}

// Built-in simulated clusters, so k10s works (and demos) with no real cluster.
struct FakeContext {
    const char* name;
    std::uint64_t seed;
    int nodes;
    int podsPerNode;
};
constexpr std::array<FakeContext, 3> kFakeContexts{{
    {"fake-village", 42, 6, 8},
    {"fake-hamlet", 7, 3, 5},
    {"fake-town", 99, 14, 6},
}};

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
std::string pickStartContext(int argc, char** argv, const k10s::Kubeconfig& kubeconfig) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string_view{argv[i]} == "--context")
            return argv[i + 1];
    if (const char* env = std::getenv("K10S_CONTEXT"))
        return env;
    if (kubeconfig.find(kubeconfig.currentContext))
        return kubeconfig.currentContext;
    return kFakeContexts[0].name;
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
        if (arg == "--help" || arg == "-h") {
            std::printf("usage: k10s [--context NAME] [--theme village|city|beach] [--verbose]\n"
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

    std::string currentContext = pickStartContext(argc, argv, kubeconfig);
    auto startSource = makeSource(currentContext, kubeconfig);
    if (!startSource) {
        std::fprintf(stderr, "k10s: unknown context '%s', using %s\n", currentContext.c_str(),
                     kFakeContexts[0].name);
        currentContext = kFakeContexts[0].name;
        startSource = makeSource(currentContext, kubeconfig);
    }
    auto session = std::make_unique<k10s::Session>(std::move(startSource));
    k10s::CommandUi ui;
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
    int frame = 0;

    while (!WindowShouldClose()) {
        // --- input ----------------------------------------------------------
        const k10s::CommandUi::Choices choices{contexts, currentContext, k10s::themeNames(), currentTheme};
        const auto action = ui.update(choices, GetFrameTime());
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
            case Kind::Building:
                theme->drawBuilding(houses[d.idx].base, houses[d.idx].uid, houses[d.idx].ready, now);
                break;
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

        // Name signs in screen space so they stay readable when zoomed out.
        for (const auto& h : houses) {
            const Vector2 sign{h.base.x, h.base.y - theme->building(h.uid).labelHeight};
            drawLabel(h.name, GetWorldToScreen2D(sign, camera.camera()));
        }

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
