#include "model/cluster_state.hpp"
#include "render/person.hpp"
#include "sim/fake_cluster.hpp"
#include <raylib.h>
#include <stdexcept>
#include <string>
#include <unordered_map>

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

    double lastTick = GetTime();

    while (!WindowShouldClose()) {
        if (GetTime() - lastTick >= 1.0) {
            for (const auto& e : sim.tick())
                state.apply(e);
            lastTick = GetTime();
        }

        BeginDrawing();
        ClearBackground(grass);

        // Lay nodes out on a grid: index -> (row, col) -> screen position.
        constexpr int cols = 3;
        constexpr float marginX = 140.0f, marginY = 180.0f;
        constexpr float spacingX = 400.0f, spacingY = 320.0f;
        constexpr float houseW = 200.0f, houseH = 140.0f;

        std::unordered_map<std::string, Vector2> housePos;

        int i = 0;
        for (const auto& [uid, node] : state.nodes()) {
            const float x = marginX + static_cast<float>(i % cols) * spacingX;
            const float y = marginY + static_cast<float>(i / cols) * spacingY;
            housePos[node.name] = {x, y};
            drawHouse(x, y, houseW, houseH);
            DrawText(node.name.c_str(), static_cast<int>(x), static_cast<int>(y + houseH + 8.0f),
                     20, RAYWHITE);
            ++i;
        }

        // Villagers: one per pod, standing in front of their node's house.
        constexpr int perRow = 8;
        constexpr float villagerGap = 24.0f, rowGap = 40.0f;
        const float now = static_cast<float>(GetTime());

        std::unordered_map<std::string, int> placed;
        for (const auto& [uid, pod] : state.pods()) {
            auto it = housePos.find(pod.nodeName);
            if (it == housePos.end())
                continue;

            const Vector2 house = it->second;
            const int k = placed[pod.nodeName]++;
            const Vector2 feet{
                house.x + 12.0f + static_cast<float>(k % perRow) * villagerGap,
                house.y + houseH + 70.0f + static_cast<float>(k / perRow) * rowGap,
            };
            const k10s::Appearance look = k10s::appearanceFor(uid);
            drawPerson(feet, look, !isHealthy(pod), now);
        }

        EndDrawing();
    }

    return 0;
}
