#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "render/camera.hpp"

using namespace k10s;
using Catch::Matchers::WithinAbs;

namespace {
constexpr Rectangle kView{0.0f, 34.0f, 1280.0f, 656.0f};

bool containsRect(Rectangle outer, Rectangle inner) {
    constexpr float eps = 0.5f;
    return inner.x >= outer.x - eps && inner.y >= outer.y - eps &&
           inner.x + inner.width <= outer.x + outer.width + eps &&
           inner.y + inner.height <= outer.y + outer.height + eps;
}
} // namespace

TEST_CASE("auto-fit shows the whole world (small village: no zoom-in past 1x)") {
    CameraController cam{kView};
    const Rectangle world{-300.0f, -200.0f, 600.0f, 400.0f};
    cam.update(world, 0.016f, false);
    CHECK(cam.camera().zoom == 1.0f);
    CHECK(containsRect(cam.visibleWorld(), world));
}

TEST_CASE("auto-fit zooms out for a big village") {
    CameraController cam{kView};
    const Rectangle world{-2000.0f, -1000.0f, 4000.0f, 2000.0f};
    cam.update(world, 0.016f, false);
    CHECK(cam.camera().zoom < 0.35f);
    CHECK(containsRect(cam.visibleWorld(), world));
}

TEST_CASE("auto-fit glides toward a grown village instead of jumping") {
    CameraController cam{kView};
    cam.update({-300.0f, -200.0f, 600.0f, 400.0f}, 0.016f, false); // first frame snaps
    const Rectangle bigger{-2000.0f, -1000.0f, 4000.0f, 2000.0f};
    cam.update(bigger, 0.016f, false);
    const float midway = cam.camera().zoom;
    CHECK(midway < 1.0f);
    CHECK(midway > 0.35f); // not there yet
    for (int i = 0; i < 300; ++i) cam.update(bigger, 0.016f, false);
    CHECK_THAT(cam.camera().zoom, WithinAbs(0.32f, 0.01f));
}

TEST_CASE("fit(snap) jumps immediately") {
    CameraController cam{kView};
    cam.update({-300.0f, -200.0f, 600.0f, 400.0f}, 0.016f, false);
    cam.fit(true);
    const Rectangle bigger{-2000.0f, -1000.0f, 4000.0f, 2000.0f};
    cam.update(bigger, 0.016f, false);
    CHECK(containsRect(cam.visibleWorld(), bigger));
}
