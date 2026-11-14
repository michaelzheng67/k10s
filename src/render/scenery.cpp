#include "render/scenery.hpp"

#include <array>
#include <random>

namespace k10s {

std::vector<Decoration> makeScenery(std::uint64_t seed, Rectangle area) {
    std::mt19937_64 rng{seed};
    std::uniform_real_distribution<float> x{area.x, area.x + area.width};
    std::uniform_real_distribution<float> y{area.y, area.y + area.height};
    std::uniform_real_distribution<float> unit{0.0f, 1.0f};

    constexpr std::array<Color, 5> petals{{
        {245, 245, 245, 255}, {250, 220, 80, 255}, {235, 120, 160, 255},
        {170, 140, 230, 255}, {240, 150, 70, 255},
    }};

    std::vector<Decoration> out;
    out.reserve(40 + 300 + 80);
    for (int i = 0; i < 40; ++i) {
        const bool light = unit(rng) < 0.5f;
        out.push_back({Decoration::Kind::Patch, {x(rng), y(rng)}, 40.0f + unit(rng) * 80.0f,
                       light ? Color{140, 195, 100, 70} : Color{80, 140, 60, 60}});
    }
    for (int i = 0; i < 300; ++i) {
        out.push_back({Decoration::Kind::Tuft, {x(rng), y(rng)}, 4.0f + unit(rng) * 4.0f,
                       Color{70, 130, 55, 255}});
    }
    for (int i = 0; i < 80; ++i) {
        const auto idx = std::uniform_int_distribution<std::size_t>{0, petals.size() - 1}(rng);
        out.push_back({Decoration::Kind::Flower, {x(rng), y(rng)}, 2.0f + unit(rng) * 1.5f,
                       petals[idx]});
    }
    return out;
}

void drawScenery(const std::vector<Decoration>& decor) {
    for (const auto& d : decor) {
        switch (d.kind) {
        case Decoration::Kind::Patch:
            DrawEllipse(static_cast<int>(d.pos.x), static_cast<int>(d.pos.y), d.size,
                        d.size * 0.45f, d.color);
            break;
        case Decoration::Kind::Tuft:
            DrawLineEx(d.pos, {d.pos.x - d.size * 0.5f, d.pos.y - d.size}, 1.5f, d.color);
            DrawLineEx(d.pos, {d.pos.x, d.pos.y - d.size * 1.2f}, 1.5f, d.color);
            DrawLineEx(d.pos, {d.pos.x + d.size * 0.5f, d.pos.y - d.size}, 1.5f, d.color);
            break;
        case Decoration::Kind::Flower:
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
