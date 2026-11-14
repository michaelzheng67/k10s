#include "theme/theme.hpp"

#include <cmath>

namespace k10s {

std::unique_ptr<Theme> makeVillageTheme();
std::unique_ptr<Theme> makeCityTheme();
std::unique_ptr<Theme> makeBeachTheme();

void Theme::drawGround(Rectangle, Rectangle, float) const {}

const std::vector<std::string>& themeNames() {
    static const std::vector<std::string> names{"village", "city", "beach"};
    return names;
}

std::unique_ptr<Theme> makeTheme(std::string_view name) {
    if (name == "village") return makeVillageTheme();
    if (name == "city") return makeCityTheme();
    if (name == "beach") return makeBeachTheme();
    return nullptr;
}

Color darken(Color c, float f) {
    return Color{static_cast<unsigned char>(static_cast<float>(c.r) * f),
                 static_cast<unsigned char>(static_cast<float>(c.g) * f),
                 static_cast<unsigned char>(static_cast<float>(c.b) * f), c.a};
}

Color withAlpha(Color c, unsigned char a) {
    c.a = a;
    return c;
}

void drawSmoke(Vector2 top, bool heavy, float t, float phase) {
    const int puffs = heavy ? 7 : 4;
    const Color base = heavy ? Color{60, 60, 60, 255} : Color{230, 230, 230, 255};
    for (int k = 0; k < puffs; ++k) {
        const float p = std::fmod(t * 0.35f + phase + static_cast<float>(k) / static_cast<float>(puffs), 1.0f);
        const Vector2 c{top.x + p * 18.0f + std::sin(t + static_cast<float>(k)) * 3.0f, top.y - p * 50.0f};
        DrawCircleV(c, 4.0f + p * 9.0f,
                    withAlpha(base, static_cast<unsigned char>((1.0f - p) * (heavy ? 200.0f : 120.0f))));
    }
}

} // namespace k10s
