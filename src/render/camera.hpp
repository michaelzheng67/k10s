#pragma once

#include <raylib.h>

namespace k10s {

// Map-style camera: scroll to zoom (toward the cursor), drag or arrow keys to pan,
// `+`/`-` to zoom, `f` to fit. Auto-fits the village until the user takes control.
class CameraController {
  public:
    // `view` is the screen area the world is shown in (between the top and bottom bars).
    explicit CameraController(Rectangle view) : view_(view) {}

    // Call once per frame. `world` is what auto-fit should frame; `acceptInput` is false
    // while a menu has the keyboard.
    void update(Rectangle world, float dt, bool acceptInput);

    // Re-enable auto-fit. `snap` jumps there instantly (used on context switch).
    void fit(bool snap);

    const Camera2D& camera() const { return cam_; }
    bool autoFit() const { return autoFit_; }

    // The world-space rectangle currently on screen.
    Rectangle visibleWorld() const;

  private:
    Camera2D fitted(Rectangle world) const;
    void zoomAt(Vector2 screenPoint, float factor, Rectangle world);
    void clamp(Rectangle world);

    Rectangle view_;
    Camera2D cam_{{0, 0}, {0, 0}, 0.0f, 1.0f};
    bool autoFit_{true};
    bool snap_{true};
};

} // namespace k10s
