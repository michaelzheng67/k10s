#include <raylib.h>
#include <stdexcept>

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
    while (!WindowShouldClose()) {

        BeginDrawing();
        ClearBackground(grass);
        drawHouse(540.0f, 300.0f, 200.0f, 160.0f);
        drawHouse(150.0f, 400.0f, 120.0f, 90.0f);
        drawHouse(950.0f, 350.0f, 160.0f, 130.0f);

        EndDrawing();
    }

    return 0;
}
