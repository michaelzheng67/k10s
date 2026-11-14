#include "render/camera.hpp"

#include <algorithm>
#include <cmath>

namespace k10s {

namespace {
constexpr float kMaxZoom = 3.0f;
constexpr float kPanSpeed = 700.0f; // screen px/s for arrow keys
} // namespace

Camera2D CameraController::fitted(Rectangle world) const {
    Camera2D c{};
    c.zoom = std::min({1.0f, view_.width / world.width, view_.height / world.height});
    c.target = {world.x + world.width / 2.0f, world.y + world.height / 2.0f};
    c.offset = {view_.x + view_.width / 2.0f, view_.y + view_.height / 2.0f};
    return c;
}

void CameraController::fit(bool snap) {
    autoFit_ = true;
    snap_ = snap_ || snap;
}

Rectangle CameraController::visibleWorld() const {
    const Vector2 tl = GetScreenToWorld2D({view_.x, view_.y}, cam_);
    const Vector2 br = GetScreenToWorld2D({view_.x + view_.width, view_.y + view_.height}, cam_);
    return {tl.x, tl.y, br.x - tl.x, br.y - tl.y};
}

void CameraController::zoomAt(Vector2 screenPoint, float factor, Rectangle world) {
    // Keep the world point under the cursor fixed while zooming: re-anchor the camera there.
    const Vector2 anchor = GetScreenToWorld2D(screenPoint, cam_);
    const float minZoom = std::min(0.15f, fitted(world).zoom * 0.5f);
    cam_.offset = screenPoint;
    cam_.target = anchor;
    cam_.zoom = std::clamp(cam_.zoom * factor, minZoom, kMaxZoom);
}

void CameraController::clamp(Rectangle world) {
    // Don't let the view wander off into empty meadow: keep the screen centre over the village.
    const Vector2 centre = GetScreenToWorld2D({view_.x + view_.width / 2.0f, view_.y + view_.height / 2.0f}, cam_);
    const float cx = std::clamp(centre.x, world.x, world.x + world.width);
    const float cy = std::clamp(centre.y, world.y, world.y + world.height);
    cam_.target.x += cx - centre.x;
    cam_.target.y += cy - centre.y;
}

void CameraController::update(Rectangle world, float dt, bool acceptInput) {
    if (acceptInput) {
        const Vector2 mouse = GetMousePosition();
        const bool mouseInView = CheckCollisionPointRec(mouse, view_);

        // Scroll wheel / trackpad: zoom toward the cursor.
        if (const float wheel = GetMouseWheelMove(); wheel != 0.0f && mouseInView) {
            autoFit_ = false;
            zoomAt(mouse, std::pow(1.15f, wheel), world);
        }

        // Click-and-drag (any button) pans.
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
            IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            const Vector2 d = GetMouseDelta();
            if (d.x != 0.0f || d.y != 0.0f) {
                autoFit_ = false;
                cam_.target.x -= d.x / cam_.zoom;
                cam_.target.y -= d.y / cam_.zoom;
            }
        }

        // Keyboard: arrows pan, +/- zoom around the screen centre, f fits.
        const float step = kPanSpeed * dt / cam_.zoom;
        const float dx = static_cast<float>(IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT));
        const float dy = static_cast<float>(IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP));
        if (dx != 0.0f || dy != 0.0f) {
            autoFit_ = false;
            cam_.target.x += dx * step;
            cam_.target.y += dy * step;
        }
        const Vector2 centre{view_.x + view_.width / 2.0f, view_.y + view_.height / 2.0f};
        if (IsKeyDown(KEY_EQUAL) || IsKeyDown(KEY_KP_ADD)) {
            autoFit_ = false;
            zoomAt(centre, 1.0f + 1.5f * dt, world);
        }
        if (IsKeyDown(KEY_MINUS) || IsKeyDown(KEY_KP_SUBTRACT)) {
            autoFit_ = false;
            zoomAt(centre, 1.0f / (1.0f + 1.5f * dt), world);
        }
        if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_ZERO)) fit(false);
    }

    if (autoFit_) {
        const Camera2D want = fitted(world);
        if (snap_) {
            cam_ = want;
            snap_ = false;
        } else {
            // Move the anchor back to the view centre without a visual jump, then glide.
            if (cam_.offset.x != want.offset.x || cam_.offset.y != want.offset.y) {
                cam_.target = GetScreenToWorld2D(want.offset, cam_);
                cam_.offset = want.offset;
            }
            const float ease = std::min(1.0f, dt * 4.0f);
            cam_.target.x += (want.target.x - cam_.target.x) * ease;
            cam_.target.y += (want.target.y - cam_.target.y) * ease;
            cam_.zoom += (want.zoom - cam_.zoom) * ease;
        }
    } else {
        clamp(world);
    }
}

} // namespace k10s
