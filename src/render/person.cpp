#include "render/person.hpp"

#include <algorithm>
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

void drawSmoke(Vector2 top, float s, float t, float phase) {
    for (int k = 0; k < 3; ++k) {
        const float p = std::fmod(t * 0.9f + phase + static_cast<float>(k) / 3.0f, 1.0f);
        const Vector2 c{top.x + std::sin(t * 2.0f + phase + static_cast<float>(k)) * 3.0f * s,
                        top.y - p * 22.0f * s};
        const auto alpha = static_cast<unsigned char>((1.0f - p) * 110.0f);
        DrawCircleV(c, (2.5f + p * 4.0f) * s, Color{70, 70, 70, alpha});
    }
}

} // namespace

void drawPerson(Vector2 feet, Vector2 vel, const Appearance& a, bool onFire, float time) {
    const float s = a.scale;
    const float legH = 10.0f * s, legW = 4.0f * s;
    const float bodyH = 14.0f * s, bodyW = 12.0f * s;
    const float headR = 5.5f * s;
    const float totalH = legH + bodyH + 2.0f * headR;

    // Walk cycle: faster stride when running, no stride when standing still.
    const float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
    const float walk = std::min(speed / 25.0f, 1.0f);
    const float stride = std::sin(time * (onFire ? 22.0f : 11.0f) + a.phase);
    const float liftL = std::max(0.0f, stride) * 3.0f * s * walk;
    const float liftR = std::max(0.0f, -stride) * 3.0f * s * walk;
    const float bob = std::abs(stride) * 1.2f * s * walk;
    const float look = speed > 3.0f ? (vel.x > 0.0f ? 1.0f : -1.0f) * 0.9f * s : 0.0f;

    // Fire glow behind everything.
    if (onFire) {
        DrawCircleGradient(static_cast<int>(feet.x), static_cast<int>(feet.y - totalH * 0.5f),
                           24.0f * s, Color{255, 140, 0, 90}, Color{255, 140, 0, 0});
    }

    // Shadow
    DrawEllipse(static_cast<int>(feet.x), static_cast<int>(feet.y), 8.0f * s, 2.5f * s,
                Color{0, 0, 0, 60});

    // Legs (each lifts on its half of the stride)
    DrawRectangleV({feet.x - bodyW / 2.0f + s, feet.y - legH - liftL}, {legW, legH}, a.pants);
    DrawRectangleV({feet.x + bodyW / 2.0f - s - legW, feet.y - legH - liftR}, {legW, legH},
                   a.pants);

    // Body
    const float bodyTop = feet.y - legH - bodyH - bob;
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

    // Eyes (glance in the direction of travel)
    DrawCircleV({head.x - 2.0f * s + look, head.y + 0.8f * s}, 0.9f * s, BLACK);
    DrawCircleV({head.x + 2.0f * s + look, head.y + 0.8f * s}, 0.9f * s, BLACK);

    if (onFire) {
        drawFlames(feet, totalH, s, time, a.phase);
        drawSmoke({feet.x, feet.y - totalH - 10.0f * s}, s, time, a.phase);
    }
}

} // namespace k10s
