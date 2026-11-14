#include "render/house.hpp"

#include <array>
#include <cmath>
#include <random>

#include "util/hash.hpp"

namespace k10s {

namespace {

constexpr std::array<Color, 6> kWalls{{
    {238, 226, 200, 255}, // cream
    {196, 150, 105, 255}, // light wood
    {225, 225, 218, 255}, // whitewash
    {170, 195, 215, 255}, // pale blue
    {180, 200, 160, 255}, // sage
    {190, 110, 90, 255},  // brick
}};

constexpr std::array<Color, 5> kRoofs{{
    {170, 60, 50, 255},  // red tile
    {75, 80, 95, 255},   // slate
    {110, 70, 45, 255},  // brown shingle
    {70, 110, 70, 255},  // green
    {60, 90, 140, 255},  // blue
}};

constexpr std::array<Color, 4> kDoors{{
    {100, 60, 35, 255},
    {140, 40, 40, 255},
    {40, 70, 110, 255},
    {50, 90, 60, 255},
}};

template <std::size_t N> Color pick(const std::array<Color, N>& p, std::mt19937_64& rng) {
    return p[std::uniform_int_distribution<std::size_t>{0, N - 1}(rng)];
}

Color darken(Color c, float f) {
    return Color{static_cast<unsigned char>(static_cast<float>(c.r) * f),
                 static_cast<unsigned char>(static_cast<float>(c.g) * f),
                 static_cast<unsigned char>(static_cast<float>(c.b) * f), c.a};
}

void drawChimneySmoke(Vector2 top, bool ready, float t, float phase) {
    const int puffs = ready ? 4 : 7;
    const Color base = ready ? Color{230, 230, 230, 255} : Color{60, 60, 60, 255};
    for (int k = 0; k < puffs; ++k) {
        const float p = std::fmod(t * 0.35f + phase + static_cast<float>(k) / static_cast<float>(puffs), 1.0f);
        const Vector2 c{top.x + p * 18.0f + std::sin(t + static_cast<float>(k)) * 3.0f,
                        top.y - p * 50.0f};
        Color col = base;
        col.a = static_cast<unsigned char>((1.0f - p) * (ready ? 120.0f : 200.0f));
        DrawCircleV(c, 4.0f + p * 9.0f, col);
    }
}

void drawWindow(Vector2 tl, float size, bool lit) {
    const Color glass = lit ? Color{250, 230, 150, 255} : Color{55, 60, 70, 255};
    constexpr Color frame{70, 50, 35, 255};
    DrawRectangleV(tl, {size, size}, glass);
    DrawRectangleLinesEx({tl.x, tl.y, size, size}, 2.0f, frame);
    DrawLineEx({tl.x + size / 2.0f, tl.y}, {tl.x + size / 2.0f, tl.y + size}, 2.0f, frame);
    DrawLineEx({tl.x, tl.y + size / 2.0f}, {tl.x + size, tl.y + size / 2.0f}, 2.0f, frame);
}

} // namespace

HouseStyle houseStyleFor(const std::string& uid) {
    std::mt19937_64 rng{fnv1a(uid)};
    HouseStyle h;
    h.wall = pick(kWalls, rng);
    h.roof = pick(kRoofs, rng);
    h.door = pick(kDoors, rng);
    h.width = std::uniform_real_distribution<float>{160.0f, 200.0f}(rng);
    h.height = std::uniform_real_distribution<float>{100.0f, 130.0f}(rng);
    h.roofPitch = std::uniform_real_distribution<float>{0.38f, 0.55f}(rng);
    h.windows = std::uniform_int_distribution<int>{1, 2}(rng);
    h.chimney = std::bernoulli_distribution{0.7}(rng);
    return h;
}

void drawHouse(Vector2 base, const HouseStyle& h, bool ready, float time) {
    const float w = h.width, ht = h.height;
    const float x0 = base.x - w / 2.0f, y0 = base.y - ht;
    const float roofH = w * h.roofPitch;
    const float overhang = w * 0.08f;
    const float phase = base.x * 0.013f;

    // Ground shadow
    DrawEllipse(static_cast<int>(base.x), static_cast<int>(base.y), w * 0.62f, 10.0f,
                Color{0, 0, 0, 45});

    // Chimney (drawn before the roof so the roof hides its base)
    const Vector2 chimneyTop{x0 + w * 0.72f, y0 - roofH * 0.9f};
    if (h.chimney) {
        DrawRectangleV(chimneyTop, {w * 0.09f, roofH * 0.6f}, darken(h.wall, 0.6f));
    }

    // Walls with a darker base strip and outline
    DrawRectangleV({x0, y0}, {w, ht}, h.wall);
    DrawRectangleV({x0, base.y - 8.0f}, {w, 8.0f}, darken(h.wall, 0.8f));
    DrawRectangleLinesEx({x0, y0, w, ht}, 2.0f, darken(h.wall, 0.55f));

    // Roof: two halves shaded differently for a bit of depth
    const Vector2 peak{base.x, y0 - roofH};
    const Vector2 left{x0 - overhang, y0 + 4.0f};
    const Vector2 right{x0 + w + overhang, y0 + 4.0f};
    const Vector2 mid{base.x, y0 + 4.0f};
    DrawTriangle(peak, left, mid, h.roof);
    DrawTriangle(peak, mid, right, darken(h.roof, 0.8f));
    DrawLineEx(left, peak, 2.0f, darken(h.roof, 0.6f));
    DrawLineEx(peak, right, 2.0f, darken(h.roof, 0.6f));

    // Door (with a knob)
    const float doorW = w * 0.17f, doorH = ht * 0.48f;
    const float doorX = h.windows == 1 ? base.x + w * 0.12f : base.x - doorW / 2.0f;
    DrawRectangleRounded({doorX, base.y - doorH, doorW, doorH}, 0.25f, 4, h.door);
    DrawCircleV({doorX + doorW * 0.78f, base.y - doorH * 0.5f}, 2.0f, Color{230, 200, 90, 255});

    // Windows (dark when the node isn't Ready)
    const float win = w * 0.17f;
    const float winY = y0 + ht * 0.2f;
    if (h.windows == 1) {
        drawWindow({base.x - w * 0.32f, winY}, win, ready);
    } else {
        drawWindow({x0 + w * 0.1f, winY}, win, ready);
        drawWindow({x0 + w * 0.9f - win, winY}, win, ready);
    }

    // Smoke: gentle from the chimney; thick and dark if the node is unhealthy
    if (h.chimney) {
        drawChimneySmoke({chimneyTop.x + w * 0.045f, chimneyTop.y}, ready, time, phase);
    } else if (!ready) {
        drawChimneySmoke(peak, ready, time, phase);
    }
}

} // namespace k10s
