#include "kube/kubeconfig.hpp"
#include "model/cluster_state.hpp"
#include "render/house.hpp"
#include "render/person.hpp"
#include "render/scenery.hpp"
#include "source/fake_source.hpp"
#include "source/session.hpp"
#include "ui/command_ui.hpp"
#include "world/movement.hpp"
#include "world/villagers.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <chrono>
#include <cstdlib>
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
constexpr Color grass{106, 168, 79, 255};
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

struct PlacedHouse {
    std::string name;
    Vector2 base; // bottom-centre
    k10s::HouseStyle style;
    bool ready;
};

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
constexpr std::array<FakeContext, 2> kFakeContexts{{
    {"fake-village", 42, 6, 8},
    {"fake-hamlet", 7, 3, 5},
}};

// Returns the data source for a context, or nullptr if we can't connect to it yet.
std::unique_ptr<k10s::DataSource> makeSource(const std::string& context) {
    for (const auto& f : kFakeContexts)
        if (context == f.name)
            return std::make_unique<k10s::FakeSource>(f.seed, f.nodes, f.podsPerNode);
    return nullptr; // real clusters: KubeSource arrives in M5
}

} // namespace

int main() {
    Window w{kWidth, kHeight, kTitle};
    SetExitKey(KEY_NULL); // Esc closes menus (like k9s) instead of quitting; use :q

    // Context list: built-in fakes first, then everything in your kubeconfig.
    const k10s::Kubeconfig kubeconfig = k10s::loadKubeconfig();
    std::vector<std::string> contexts;
    for (const auto& f : kFakeContexts)
        contexts.emplace_back(f.name);
    for (const auto& c : kubeconfig.contexts)
        contexts.push_back(c.name);

    std::string currentContext = kFakeContexts[0].name;
    auto session = std::make_unique<k10s::Session>(makeSource(currentContext));
    k10s::CommandUi ui;
    k10s::ClusterState state;

    k10s::Villagers villagers;
    std::mt19937 moveRng{1234};
    bool stateChanged = true;

    // Layout constants.
    constexpr int cols = 3;
    constexpr float firstX = 240.0f, spacingX = 400.0f;
    constexpr float firstBaseY = 300.0f, spacingY = 310.0f;
    constexpr float yardDepth = 68.0f; // how far in front of the house villagers hang out
    constexpr Rectangle bounds{10.0f, 50.0f, kWidth - 20.0f, kHeight - 60.0f};

    const auto scenery = k10s::makeScenery(7, Rectangle{0, 34, kWidth, kHeight - 34});

    // Dev hook: K10S_SCREENSHOT=out.png saves a frame after ~3s and exits.
    const char* screenshotPath = std::getenv("K10S_SCREENSHOT");
    int frame = 0;

    while (!WindowShouldClose()) {
        // --- input ----------------------------------------------------------
        const auto action = ui.update(contexts, currentContext, GetFrameTime());
        if (action.kind == k10s::CommandUi::ActionKind::Quit)
            break;
        if (action.kind == k10s::CommandUi::ActionKind::SwitchContext) {
            if (auto source = makeSource(action.arg)) {
                session.reset(); // stop + join the old thread before starting a new one
                state = k10s::ClusterState{};
                villagers = k10s::Villagers{};
                session = std::make_unique<k10s::Session>(std::move(source));
                currentContext = action.arg;
                stateChanged = true;
            } else {
                ui.flash("can't connect to real clusters yet (M5): " + action.arg);
            }
        }

        // --- update ---------------------------------------------------------
        while (auto e = session->events().try_pop()) {
            state.apply(*e);
            stateChanged = true;
        }

        // Lay nodes out on a grid: index -> (row, col) -> base position.
        std::vector<PlacedHouse> houses;
        std::unordered_map<std::string, Vector2> yardPos; // where each node's villagers live
        int i = 0;
        for (const auto& [uid, node] : state.nodes()) {
            const Vector2 base{firstX + static_cast<float>(i % cols) * spacingX,
                               firstBaseY + static_cast<float>(i / cols) * spacingY};
            houses.push_back({node.name, base, k10s::houseStyleFor(uid), node.ready});
            yardPos[node.name] = {base.x, base.y + yardDepth};
            ++i;
        }

        if (stateChanged) {
            villagers.sync(state, yardPos);
            stateChanged = false;
        }

        const float dt = std::min(GetFrameTime(), 0.05f); // avoid huge jumps after a stall
        k10s::updateVillagers(villagers, dt, moveRng, bounds);

        // --- draw -----------------------------------------------------------
        const float now = static_cast<float>(GetTime());
        BeginDrawing();
        ClearBackground(grass);

        k10s::drawScenery(scenery);
        for (const auto& [name, yard] : yardPos)
            k10s::drawYard(yard);

        // Painter's algorithm: draw houses and villagers together, back to front by
        // their ground y, so villagers can walk behind houses.
        struct Drawable {
            float y;
            bool isHouse;
            std::size_t idx;
        };
        std::vector<Drawable> scene;
        scene.reserve(houses.size() + villagers.size());
        for (std::size_t h = 0; h < houses.size(); ++h)
            scene.push_back({houses[h].base.y, true, h});
        for (std::size_t v = 0; v < villagers.size(); ++v)
            scene.push_back({villagers.pos[v].y, false, v});
        std::sort(scene.begin(), scene.end(),
                  [](const Drawable& a, const Drawable& b) { return a.y < b.y; });

        for (const auto& d : scene) {
            if (d.isHouse) {
                const auto& h = houses[d.idx];
                k10s::drawHouse(h.base, h.style, h.ready, now);
                // Name sign on the gable, so the crowd in the yard never covers it.
                const float roofH = h.style.width * h.style.roofPitch;
                drawLabel(h.name, {h.base.x, h.base.y - h.style.height - roofH * 0.38f});
            } else {
                k10s::drawPerson(villagers.pos[d.idx], villagers.vel[d.idx], villagers.look[d.idx],
                                 villagers.onFire[d.idx] != 0, now);
            }
        }

        drawHud(currentContext, state.nodeCount(), villagers.size(),
                std::count(villagers.onFire.begin(), villagers.onFire.end(), 1));

        ui.draw(contexts, currentContext, kWidth, kHeight);

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
