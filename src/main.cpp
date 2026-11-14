#include "model/cluster_state.hpp"
#include "render/person.hpp"
#include "sim/fake_cluster.hpp"
#include "world/movement.hpp"
#include "world/villagers.hpp"
#include <raylib.h>
#include <algorithm>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

constexpr int kWidth{1280};
constexpr int kHeight{720};
constexpr const char* const kTitle{"k10s"};
constexpr Color grass{106, 168, 79, 255};
constexpr Color wall{150, 100, 60, 255};
constexpr Color roof{170, 60, 50, 255};

void drawHouse(float x, float y, float w, float h) {
    const float roofH = w * 0.5f;    // wider house -> taller roof
    const float overhang = w * 0.1f; // roof sticks out past the walls

    DrawRectangleV(Vector2{x, y}, Vector2{w, h}, wall);

    // peak -> bottom-left -> bottom-right (counter-clockwise on screen)
    DrawTriangle(Vector2{x + w / 2.0f, y - roofH}, Vector2{x - overhang, y},
                 Vector2{x + w + overhang, y}, roof);
}

class Window {

  public:
    explicit Window(int w, int h, const char* title) {
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
    constexpr float marginX = 140.0f, marginY = 180.0f;
    constexpr float spacingX = 400.0f, spacingY = 320.0f;
    constexpr float houseW = 200.0f, houseH = 140.0f;
    constexpr float yardDepth = 70.0f; // how far in front of the house villagers hang out
    constexpr Rectangle bounds{10.0f, 40.0f, kWidth - 20.0f, kHeight - 50.0f};

    while (!WindowShouldClose()) {
        // --- update ---------------------------------------------------------
        if (GetTime() - lastTick >= 1.0) {
            for (const auto& e : sim.tick())
                state.apply(e);
            lastTick = GetTime();
            stateChanged = true;
        }

        // Lay nodes out on a grid: index -> (row, col) -> screen position.
        std::unordered_map<std::string, Vector2> housePos; // top-left of each house
        std::unordered_map<std::string, Vector2> yardPos;  // where its villagers live
        int i = 0;
        for (const auto& [uid, node] : state.nodes()) {
            const float x = marginX + static_cast<float>(i % cols) * spacingX;
            const float y = marginY + static_cast<float>(i / cols) * spacingY;
            housePos[node.name] = {x, y};
            yardPos[node.name] = {x + houseW / 2.0f, y + houseH + yardDepth};
            ++i;
        }

        if (stateChanged) {
            villagers.sync(state, yardPos);
            stateChanged = false;
        }

        const float dt = std::min(GetFrameTime(), 0.05f); // avoid huge jumps after a stall
        k10s::updateVillagers(villagers, dt, moveRng, bounds);

        // --- draw -----------------------------------------------------------
        BeginDrawing();
        ClearBackground(grass);

        for (const auto& [name, pos] : housePos) {
            drawHouse(pos.x, pos.y, houseW, houseH);
            DrawText(name.c_str(), static_cast<int>(pos.x), static_cast<int>(pos.y + houseH + 8.0f),
                     20, RAYWHITE);
        }

        // Draw villagers back-to-front (by y) so nearer ones overlap farther ones.
        std::vector<std::size_t> order(villagers.size());
        std::iota(order.begin(), order.end(), std::size_t{0});
        std::sort(order.begin(), order.end(),
                  [&](std::size_t a, std::size_t b) { return villagers.pos[a].y < villagers.pos[b].y; });

        const float now = static_cast<float>(GetTime());
        for (std::size_t idx : order) {
            k10s::drawPerson(villagers.pos[idx], villagers.look[idx], villagers.onFire[idx] != 0, now);
        }

        DrawText(TextFormat("pods: %zu   on fire: %d", villagers.size(),
                            static_cast<int>(std::count(villagers.onFire.begin(),
                                                        villagers.onFire.end(), 1))),
                 10, 10, 20, RAYWHITE);

        EndDrawing();
    }

    return 0;
}
