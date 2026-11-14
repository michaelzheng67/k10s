#include "model/cluster_state.hpp"
#include "render/house.hpp"
#include "render/person.hpp"
#include "render/scenery.hpp"
#include "sim/fake_cluster.hpp"
#include "world/movement.hpp"
#include "world/villagers.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <algorithm>
#include <cstdlib>
#include <random>
#include <stdexcept>
#include <string>
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

void drawHud(std::size_t nodes, std::size_t pods, long burning) {
    DrawRectangle(0, 0, kWidth, 34, Color{20, 25, 30, 210});
    DrawText("k10s", 12, 7, 20, RAYWHITE);
    DrawText("context: fake-cluster", 80, 9, 18, Color{170, 180, 190, 255});
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

} // namespace

int main() {
    Window w{kWidth, kHeight, kTitle};
    k10s::FakeCluster sim{42, 6, 8};
    k10s::ClusterState state;
    for (const auto& e : sim.snapshot())
        state.apply(e);

    k10s::Villagers villagers;
    std::mt19937 moveRng{1234};
    bool stateChanged = true;
    double lastTick = GetTime();

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
        // --- update ---------------------------------------------------------
        if (GetTime() - lastTick >= 1.0) {
            for (const auto& e : sim.tick())
                state.apply(e);
            lastTick = GetTime();
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

        drawHud(state.nodeCount(), villagers.size(),
                std::count(villagers.onFire.begin(), villagers.onFire.end(), 1));

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
