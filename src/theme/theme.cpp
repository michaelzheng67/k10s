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

namespace {

// raylib's default font is a 10px pixel font: crisp at 10/20/30px, garbled when scaled to
// anything else. Signs are drawn at arbitrary zoom, so use a mipmapped, smoothly filtered
// copy of its glyph atlas (created lazily, once the GL context exists).
const Font& signFont() {
    static const Font font = [] {
        Font f = GetFontDefault();
        Image atlas = LoadImageFromTexture(f.texture);
        f.texture = LoadTextureFromImage(atlas);
        UnloadImage(atlas);
        GenTextureMipmaps(&f.texture);
        SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
        return f;
    }();
    return font;
}

} // namespace

void drawSign(Vector2 centre, const std::string& text, const SignStyle& style, float maxWidth,
              float zoom) {
    constexpr float fontSize = 12.0f, spacing = 1.0f, padX = 4.0f, padY = 2.5f;
    const Font& font = signFont();

    // Middle-ellipsis until it fits: names usually differ at the end (...-prod vs ...-dev).
    std::string label = text;
    auto width = [&](const std::string& s) { return MeasureTextEx(font, s.c_str(), fontSize, spacing).x; };
    for (std::size_t keep = text.size(); width(label) > maxWidth - 2.0f * padX && keep > 4;) {
        --keep;
        label = text.substr(0, (keep + 1) / 2) + ".." + text.substr(text.size() - keep / 2);
    }

    const float w = width(label) + 2.0f * padX, h = fontSize + 2.0f * padY;
    const Rectangle r{centre.x - w / 2.0f, centre.y - h / 2.0f, w, h};
    DrawRectangleRounded(r, 0.3f, 4, style.board);
    DrawRectangleRoundedLinesEx(r, 0.3f, 4, 1.0f, style.border);
    if (fontSize * zoom >= 6.0f) // too small to read when zoomed far out
        DrawTextEx(font, label.c_str(), {r.x + padX, r.y + padY}, fontSize, spacing, style.text);
}

} // namespace k10s
