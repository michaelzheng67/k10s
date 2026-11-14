#include "render/scenery.hpp"

#include <algorithm>
#include <array>
#include <random>

namespace k10s {

namespace {
constexpr int kPatch = 0, kTuft = 1, kFlower = 2;
} // namespace

std::vector<Decoration> makeScenery(std::uint64_t seed, Rectangle area) {
    std::mt19937_64 rng{seed};
    std::uniform_real_distribution<float> x{area.x, area.x + area.width};
    std::uniform_real_distribution<float> y{area.y, area.y + area.height};
    std::uniform_real_distribution<float> unit{0.0f, 1.0f};

    constexpr std::array<Color, 5> petals{{
        {245, 245, 245, 255}, {250, 220, 80, 255}, {235, 120, 160, 255},
        {170, 140, 230, 255}, {240, 150, 70, 255},
    }};

    // Densities tuned for a 1280x686 meadow; scale counts with the area covered.
    // Capped so a far zoom-out doesn't generate tens of thousands of (tiny, invisible) tufts.
    const float density = std::min((area.width * area.height) / (1280.0f * 686.0f), 14.0f);
    const auto count = [&](float n) { return static_cast<int>(n * density); };

    std::vector<Decoration> out;
    out.reserve(static_cast<std::size_t>(count(420.0f)));
    for (int i = 0; i < count(40.0f); ++i) {
        const bool light = unit(rng) < 0.5f;
        out.push_back({kPatch, {x(rng), y(rng)}, 40.0f + unit(rng) * 80.0f,
                       light ? Color{140, 195, 100, 70} : Color{80, 140, 60, 60}});
    }
    for (int i = 0; i < count(300.0f); ++i) {
        out.push_back({kTuft, {x(rng), y(rng)}, 4.0f + unit(rng) * 4.0f,
                       Color{70, 130, 55, 255}});
    }
    for (int i = 0; i < count(80.0f); ++i) {
        const auto idx = std::uniform_int_distribution<std::size_t>{0, petals.size() - 1}(rng);
        out.push_back({kFlower, {x(rng), y(rng)}, 2.0f + unit(rng) * 1.5f,
                       petals[idx]});
    }
    return out;
}

void drawScenery(const std::vector<Decoration>& decor) {
    for (const auto& d : decor) {
        switch (d.kind) {
        case kPatch:
            DrawEllipse(static_cast<int>(d.pos.x), static_cast<int>(d.pos.y), d.size,
                        d.size * 0.45f, d.color);
            break;
        case kTuft:
            DrawLineEx(d.pos, {d.pos.x - d.size * 0.5f, d.pos.y - d.size}, 1.5f, d.color);
            DrawLineEx(d.pos, {d.pos.x, d.pos.y - d.size * 1.2f}, 1.5f, d.color);
            DrawLineEx(d.pos, {d.pos.x + d.size * 0.5f, d.pos.y - d.size}, 1.5f, d.color);
            break;
        case kFlower:
            for (int k = 0; k < 4; ++k) {
                const float dx = (k % 2 == 0 ? 1.0f : -1.0f) * d.size * (k < 2 ? 1.0f : 0.0f);
                const float dy = (k % 2 == 0 ? 1.0f : -1.0f) * d.size * (k < 2 ? 0.0f : 1.0f);
                DrawCircleV({d.pos.x + dx, d.pos.y + dy}, d.size * 0.8f, d.color);
            }
            DrawCircleV(d.pos, d.size * 0.6f, Color{250, 200, 60, 255});
            break;
        }
    }
}

void drawYard(Vector2 centre) {
    DrawEllipse(static_cast<int>(centre.x), static_cast<int>(centre.y), 95.0f, 34.0f,
                Color{170, 140, 95, 110});
    DrawEllipse(static_cast<int>(centre.x), static_cast<int>(centre.y), 70.0f, 22.0f,
                Color{180, 150, 105, 90});
}

} // namespace k10s
