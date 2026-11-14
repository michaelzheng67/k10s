#include "render/person.hpp"

#include <cmath>

namespace k10s {

namespace {

void drawFlames(Vector2 feet, float height, float s, float t, float phase) {
    constexpr Color outer{255, 120, 20, 230};
    constexpr Color inner{255, 220, 60, 240};
    for (int i = 0; i < 3; ++i) {
        const float fi = static_cast<float>(i);
        const float x = feet.x + (fi - 1.0f) * 5.0f * s;
        const float flick = 4.0f * s * std::sin(t * 14.0f + phase + fi * 2.1f);
        const float baseY = feet.y - height * 0.35f;
        const float tipY = feet.y - height - 8.0f * s - flick;
        DrawTriangle({x, tipY}, {x - 5.0f * s, baseY}, {x + 5.0f * s, baseY}, outer);
        DrawTriangle({x, tipY + 6.0f * s}, {x - 2.5f * s, baseY}, {x + 2.5f * s, baseY}, inner);
    }
}

} // namespace

void drawPerson(Vector2 feet, const Appearance& a, bool onFire, float time) {
    const float s = a.scale;
    const float legH = 10.0f * s, legW = 4.0f * s;
    const float bodyH = 14.0f * s, bodyW = 12.0f * s;
    const float headR = 5.5f * s;

    // Shadow
    DrawEllipse(static_cast<int>(feet.x), static_cast<int>(feet.y), 8.0f * s, 2.5f * s,
                Color{0, 0, 0, 60});

    // Legs
    DrawRectangleV({feet.x - bodyW / 2.0f + s, feet.y - legH}, {legW, legH}, a.pants);
    DrawRectangleV({feet.x + bodyW / 2.0f - s - legW, feet.y - legH}, {legW, legH}, a.pants);

    // Body
    const float bodyTop = feet.y - legH - bodyH;
    DrawRectangleRounded({feet.x - bodyW / 2.0f, bodyTop, bodyW, bodyH}, 0.4f, 4, a.shirt);

    // Head (+ long hair behind it)
    const Vector2 head{feet.x, bodyTop - headR + s};
    if (a.hairStyle == 1) {
        DrawRectangleRounded(
            {head.x - headR - s, head.y - headR, 2.0f * headR + 2.0f * s, 2.0f * headR + 4.0f * s},
            0.6f, 4, a.hair);
    }
    DrawCircleV(head, headR, a.skin);
    if (a.hairStyle != 2) {
        DrawCircleSector(head, headR + 0.5f, 180.0f, 360.0f, 12, a.hair);
    }

    // Eyes
    DrawCircleV({head.x - 2.0f * s, head.y + 0.8f * s}, 0.9f * s, BLACK);
    DrawCircleV({head.x + 2.0f * s, head.y + 0.8f * s}, 0.9f * s, BLACK);

    if (onFire) {
        drawFlames(feet, legH + bodyH + 2.0f * headR, s, time, feet.x * 0.37f);
    }
}

} // namespace k10s
