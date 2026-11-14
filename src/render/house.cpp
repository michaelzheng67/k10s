#include "render/house.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include "util/hash.hpp"

namespace k10s {

namespace {

constexpr std::array<Color, 8> kWalls{{
    {238, 226, 200, 255}, // cream
    {196, 150, 105, 255}, // light wood
    {225, 225, 218, 255}, // whitewash
    {170, 195, 215, 255}, // pale blue
    {180, 200, 160, 255}, // sage
    {190, 110, 90, 255},  // brick red
    {230, 200, 140, 255}, // sandstone
    {205, 175, 200, 255}, // lavender
}};

constexpr std::array<Color, 6> kRoofs{{
    {170, 60, 50, 255},  // red tile
    {75, 80, 95, 255},   // slate
    {110, 70, 45, 255},  // brown shingle
    {70, 110, 70, 255},  // green
    {60, 90, 140, 255},  // blue
    {150, 120, 70, 255}, // thatch
}};

constexpr std::array<Color, 5> kDoors{{
    {100, 60, 35, 255},
    {140, 40, 40, 255},
    {40, 70, 110, 255},
    {50, 90, 60, 255},
    {200, 160, 60, 255},
}};

constexpr std::array<Color, 4> kLeaves{{
    {60, 130, 60, 255},
    {80, 150, 70, 255},
    {45, 110, 70, 255},
    {110, 150, 60, 255},
}};

template <std::size_t N> Color pick(const std::array<Color, N>& p, std::mt19937_64& rng) {
    return p[std::uniform_int_distribution<std::size_t>{0, N - 1}(rng)];
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

void drawChimneySmoke(Vector2 top, bool ready, float t, float phase) {
    const int puffs = ready ? 4 : 7;
    const Color base = ready ? Color{230, 230, 230, 255} : Color{60, 60, 60, 255};
    for (int k = 0; k < puffs; ++k) {
        const float p = std::fmod(t * 0.35f + phase + static_cast<float>(k) / static_cast<float>(puffs), 1.0f);
        const Vector2 c{top.x + p * 18.0f + std::sin(t + static_cast<float>(k)) * 3.0f,
                        top.y - p * 50.0f};
        DrawCircleV(c, 4.0f + p * 9.0f,
                    withAlpha(base, static_cast<unsigned char>((1.0f - p) * (ready ? 120.0f : 200.0f))));
    }
}

void drawWindow(Vector2 tl, float size, bool lit, const HouseStyle& h) {
    const Color glass = lit ? Color{250, 230, 150, 255} : Color{55, 60, 70, 255};
    constexpr Color frame{70, 50, 35, 255};
    if (h.shutters) {
        const Color sh = darken(h.door, 0.9f);
        DrawRectangleV({tl.x - size * 0.38f, tl.y}, {size * 0.32f, size}, sh);
        DrawRectangleV({tl.x + size * 1.06f, tl.y}, {size * 0.32f, size}, sh);
    }
    DrawRectangleV(tl, {size, size}, glass);
    DrawRectangleLinesEx({tl.x, tl.y, size, size}, 2.0f, frame);
    DrawLineEx({tl.x + size / 2.0f, tl.y}, {tl.x + size / 2.0f, tl.y + size}, 2.0f, frame);
    DrawLineEx({tl.x, tl.y + size / 2.0f}, {tl.x + size, tl.y + size / 2.0f}, 2.0f, frame);
    if (h.flowerBoxes) {
        DrawRectangleV({tl.x - 2.0f, tl.y + size}, {size + 4.0f, 6.0f}, Color{120, 80, 45, 255});
        constexpr std::array<Color, 3> petals{{{235, 90, 110, 255}, {250, 220, 80, 255}, {240, 240, 240, 255}}};
        for (int i = 0; i < 4; ++i) {
            DrawCircleV({tl.x + size * (0.12f + 0.25f * static_cast<float>(i)), tl.y + size - 1.0f}, 2.6f,
                        petals[static_cast<std::size_t>(i) % petals.size()]);
        }
    }
}

void drawWallTexture(const HouseStyle& h, float x0, float y0, float w, float ht) {
    const Color line = withAlpha(darken(h.wall, 0.7f), 70);
    if (h.walls == HouseStyle::Walls::Planks) {
        for (float y = y0 + 10.0f; y < y0 + ht - 8.0f; y += 10.0f)
            DrawLineEx({x0 + 2.0f, y}, {x0 + w - 2.0f, y}, 1.0f, line);
    } else if (h.walls == HouseStyle::Walls::Brick) {
        int row = 0;
        for (float y = y0 + 8.0f; y < y0 + ht - 8.0f; y += 8.0f, ++row) {
            DrawLineEx({x0 + 2.0f, y}, {x0 + w - 2.0f, y}, 1.0f, line);
            for (float x = x0 + (row % 2 ? 8.0f : 16.0f); x < x0 + w - 2.0f; x += 16.0f)
                DrawLineEx({x, y}, {x, std::min(y + 8.0f, y0 + ht - 8.0f)}, 1.0f, line);
        }
    }
}

void drawRoof(const HouseStyle& h, Vector2 base, float x0, float y0, float w) {
    const float roofH = h.roofHeight();
    const float overhang = w * 0.08f;
    const Color dark = darken(h.roof, 0.8f);
    const Color edge = darken(h.roof, 0.6f);
    const Vector2 left{x0 - overhang, y0 + 4.0f};
    const Vector2 right{x0 + w + overhang, y0 + 4.0f};
    const Vector2 mid{base.x, y0 + 4.0f};

    if (h.roofShape == HouseStyle::Roof::Hip) {
        // Trapezoid: sloped ends with a flat ridge.
        const Vector2 ridgeL{x0 + w * 0.28f, y0 - roofH};
        const Vector2 ridgeR{x0 + w * 0.72f, y0 - roofH};
        DrawTriangle(ridgeL, left, mid, h.roof);
        DrawTriangle(ridgeL, mid, ridgeR, h.roof);
        DrawTriangle(ridgeR, mid, right, dark);
        DrawLineEx(left, ridgeL, 2.0f, edge);
        DrawLineEx(ridgeL, ridgeR, 2.0f, edge);
        DrawLineEx(ridgeR, right, 2.0f, edge);
    } else {
        const Vector2 peak{base.x, y0 - roofH};
        DrawTriangle(peak, left, mid, h.roof);
        DrawTriangle(peak, mid, right, dark);
        DrawLineEx(left, peak, 2.0f, edge);
        DrawLineEx(peak, right, 2.0f, edge);
    }
}

} // namespace

HouseStyle houseStyleFor(const std::string& uid) {
    std::mt19937_64 rng{mix64(fnv1a(uid))};
    std::uniform_real_distribution<float> unit{0.0f, 1.0f};
    auto chance = [&](float p) { return unit(rng) < p; };

    HouseStyle h;
    h.wall = pick(kWalls, rng);
    h.roof = pick(kRoofs, rng);
    h.door = pick(kDoors, rng);
    h.stories = chance(0.3f) ? 2 : 1;
    h.width = 150.0f + unit(rng) * (h.stories == 2 ? 40.0f : 60.0f);
    h.storyHeight = (h.stories == 2 ? 80.0f : 100.0f) + unit(rng) * 25.0f;

    const float roll = unit(rng);
    h.roofShape = roll < 0.55f ? HouseStyle::Roof::Gable
                  : roll < 0.8f ? HouseStyle::Roof::Steep
                                : HouseStyle::Roof::Hip;
    h.roofPitch = h.roofShape == HouseStyle::Roof::Steep ? 0.6f + unit(rng) * 0.12f
                                                         : 0.36f + unit(rng) * 0.12f;

    const float wallRoll = unit(rng);
    h.walls = wallRoll < 0.45f ? HouseStyle::Walls::Plain
              : wallRoll < 0.75f ? HouseStyle::Walls::Planks
                                 : HouseStyle::Walls::Brick;

    h.windowsPerStory = std::uniform_int_distribution<int>{1, 3}(rng);
    h.chimney = chance(0.7f);
    h.shutters = chance(0.4f);
    h.flowerBoxes = chance(0.35f);
    h.archedDoor = chance(0.4f);

    h.treeCount = std::uniform_int_distribution<int>{0, 2}(rng);
    float side = chance(0.5f) ? -1.0f : 1.0f;
    for (int i = 0; i < h.treeCount; ++i) {
        TreeSpec& t = h.trees[static_cast<std::size_t>(i)];
        t.dx = side * (h.width / 2.0f + 35.0f + unit(rng) * 35.0f);
        t.dy = -10.0f + unit(rng) * 25.0f;
        t.scale = 0.8f + unit(rng) * 0.5f;
        t.leaves = pick(kLeaves, rng);
        t.pine = chance(0.4f);
        side = -side; // second tree on the other side
    }
    return h;
}

void drawTree(Vector2 base, const TreeSpec& t, float time) {
    const float s = t.scale;
    const float sway = std::sin(time * 1.3f + base.x * 0.05f) * 1.5f * s;
    DrawEllipse(static_cast<int>(base.x), static_cast<int>(base.y), 26.0f * s, 7.0f * s,
                Color{0, 0, 0, 45});
    DrawRectangleV({base.x - 5.0f * s, base.y - 34.0f * s}, {10.0f * s, 34.0f * s},
                   Color{110, 75, 45, 255});
    if (t.pine) {
        for (int i = 0; i < 3; ++i) {
            const float fi = static_cast<float>(i);
            const float y = base.y - 28.0f * s - fi * 22.0f * s;
            const float hw = (34.0f - fi * 8.0f) * s;
            DrawTriangle({base.x + sway, y - 36.0f * s}, {base.x - hw, y}, {base.x + hw, y},
                         darken(t.leaves, 0.85f + fi * 0.07f));
        }
    } else {
        const Vector2 c{base.x + sway, base.y - 62.0f * s};
        DrawCircleV({c.x - 16.0f * s, c.y + 8.0f * s}, 22.0f * s, darken(t.leaves, 0.85f));
        DrawCircleV({c.x + 16.0f * s, c.y + 8.0f * s}, 22.0f * s, darken(t.leaves, 0.9f));
        DrawCircleV(c, 28.0f * s, t.leaves);
        DrawCircleV({c.x - 8.0f * s, c.y - 8.0f * s}, 10.0f * s, withAlpha(WHITE, 30));
    }
}

void drawHouse(Vector2 base, const HouseStyle& h, bool ready, float time) {
    const float w = h.width, ht = h.wallHeight();
    const float x0 = base.x - w / 2.0f, y0 = base.y - ht;
    const float roofH = h.roofHeight();
    const float phase = base.x * 0.013f;

    // Ground shadow
    DrawEllipse(static_cast<int>(base.x), static_cast<int>(base.y), w * 0.62f, 10.0f,
                Color{0, 0, 0, 45});

    // Chimney (drawn before the roof so the roof hides its base)
    const Vector2 chimneyTop{x0 + w * 0.72f, y0 - roofH * 0.9f};
    if (h.chimney)
        DrawRectangleV(chimneyTop, {w * 0.09f, roofH * 0.6f}, darken(h.wall, 0.6f));

    // Walls
    DrawRectangleV({x0, y0}, {w, ht}, h.wall);
    drawWallTexture(h, x0, y0, w, ht);
    DrawRectangleV({x0, base.y - 8.0f}, {w, 8.0f}, darken(h.wall, 0.8f));
    if (h.stories == 2) // floor divider
        DrawRectangleV({x0, y0 + h.storyHeight - 3.0f}, {w, 5.0f}, darken(h.wall, 0.75f));
    DrawRectangleLinesEx({x0, y0, w, ht}, 2.0f, darken(h.wall, 0.55f));

    drawRoof(h, base, x0, y0, w);

    // Door on the ground floor (arched or square, with a knob)
    const float doorW = w * 0.17f;
    const float doorH = std::min(h.storyHeight * 0.62f, 60.0f);
    const bool doorCentred = h.windowsPerStory != 2;
    const float doorX = doorCentred ? base.x - doorW / 2.0f : base.x + w * 0.1f;
    DrawRectangleV({doorX, base.y - doorH}, {doorW, doorH}, h.door);
    if (h.archedDoor)
        DrawCircleSector({doorX + doorW / 2.0f, base.y - doorH}, doorW / 2.0f, 180.0f, 360.0f, 12,
                         h.door);
    DrawCircleV({doorX + doorW * 0.78f, base.y - doorH * 0.5f}, 2.0f, Color{230, 200, 90, 255});

    // Windows: every story gets a row; the ground floor leaves room for the door.
    const float win = std::min(w * 0.16f, h.storyHeight * 0.3f);
    for (int story = 0; story < h.stories; ++story) {
        const float storyTop = base.y - h.storyHeight * static_cast<float>(story + 1);
        const float winY = storyTop + h.storyHeight * 0.2f;
        const bool ground = story == 0;
        int count = h.windowsPerStory;
        if (ground && count == 3) count = 2; // door takes the middle slot
        if (!ground) count = std::max(2, h.windowsPerStory);

        if (count == 1) {
            drawWindow({ground ? x0 + w * 0.14f : base.x - win / 2.0f, winY}, win, ready, h);
        } else if (ground && doorCentred) {
            drawWindow({x0 + w * 0.12f, winY}, win, ready, h);
            drawWindow({x0 + w * 0.88f - win, winY}, win, ready, h);
        } else {
            const float gap = (w - static_cast<float>(count) * win) / static_cast<float>(count + 1);
            const float limit = ground ? doorX - 6.0f : x0 + w;
            for (int i = 0; i < count; ++i) {
                const float wx = x0 + gap + static_cast<float>(i) * (win + gap);
                if (ground && wx + win > limit) break;
                drawWindow({wx, winY}, win, ready, h);
            }
        }
    }

    // Smoke: gentle from the chimney; thick and dark if the node is unhealthy
    if (h.chimney) {
        drawChimneySmoke({chimneyTop.x + w * 0.045f, chimneyTop.y}, ready, time, phase);
    } else if (!ready) {
        drawChimneySmoke({base.x, y0 - roofH}, ready, time, phase);
    }
}

} // namespace k10s
