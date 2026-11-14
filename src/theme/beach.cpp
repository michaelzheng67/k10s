// Beach theme: pastel huts and lifeguard towers on sand, with the ocean along the top.
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <unordered_map>

#include "theme/theme.hpp"
#include "util/hash.hpp"

namespace k10s {

namespace {

constexpr int kPalm = 0, kUmbrella = 1, kSurfboard = 2;                         // props
constexpr int kDune = 0, kShell = 1, kStarfish = 2, kTowel = 3, kSandcastle = 4; // scenery

constexpr std::array<Color, 6> kHutColors{{
    {150, 215, 200, 255}, // mint
    {245, 170, 180, 255}, // pink
    {250, 225, 130, 255}, // butter
    {140, 195, 235, 255}, // sky
    {245, 245, 240, 255}, // white
    {250, 180, 120, 255}, // peach
}};
constexpr std::array<Color, 5> kBright{{
    {230, 70, 70, 255}, {40, 140, 210, 255}, {250, 200, 40, 255}, {60, 180, 120, 255}, {240, 120, 40, 255},
}};

template <std::size_t N> Color pick(const std::array<Color, N>& p, std::mt19937_64& rng) {
    return p[std::uniform_int_distribution<std::size_t>{0, N - 1}(rng)];
}

struct HutStyle {
    bool lifeguard{false};
    float width{120.0f};
    float wallHeight{70.0f};
    float stilts{20.0f};
    Color wall{}, accent{};
    bool thatch{true};
    bool awning{false};
};

// The waterline sits just above the village so the ocean frames the top of the scene.
float waterline(Rectangle village) { return village.y + 20.0f; }

class BeachTheme final : public Theme {
  public:
    std::string_view name() const override { return "beach"; }
    Color background() const override { return {238, 214, 160, 255}; }
    SignStyle signStyle() const override { return {{245, 238, 220, 255}, {150, 105, 65, 255}, {40, 110, 160, 255}}; }

    Rectangle frame(Rectangle v) const override {
        constexpr float ocean = 220.0f; // keep a band of sea in view above the huts
        return {v.x, v.y - ocean, v.width, v.height + ocean};
    }

    void drawGround(Rectangle visible, Rectangle village, float time) const override {
        const float shore = waterline(village);
        if (visible.y >= shore + 40.0f) return; // ocean not on screen
        const float left = visible.x, w = visible.width;

        // Wet sand, then the sea (deep -> shallow), then animated foam along the shore.
        DrawRectangleV({left, shore - 10.0f}, {w, 40.0f}, Color{215, 190, 140, 255});
        if (visible.y < shore) {
            DrawRectangleV({left, visible.y}, {w, shore - visible.y}, Color{40, 120, 180, 255});
            DrawRectangleGradientV(static_cast<int>(left), static_cast<int>(shore - 140.0f), static_cast<int>(w) + 2, 140,
                                   Color{40, 120, 180, 255}, Color{80, 185, 205, 255});
        }
        // Wave crests drifting sideways
        for (int row = 0; row < 6; ++row) {
            const float y = shore - 30.0f - static_cast<float>(row) * 45.0f;
            if (y < visible.y) break;
            const float drift = std::fmod(time * (12.0f + static_cast<float>(row) * 4.0f), 120.0f);
            for (float x = std::floor(left / 120.0f) * 120.0f - 120.0f + drift; x < left + w; x += 120.0f)
                DrawLineEx({x, y}, {x + 40.0f, y - 3.0f}, 2.0f, Color{255, 255, 255, 90});
        }
        // Foam edge that laps in and out
        for (float x = std::floor(left / 24.0f) * 24.0f; x < left + w; x += 24.0f) {
            const float lap = 5.0f * std::sin(time * 1.4f + x * 0.02f);
            DrawCircleV({x, shore + lap}, 10.0f, Color{250, 250, 245, 220});
        }
    }

    std::vector<Decoration> makeScenery(std::uint64_t seed, Rectangle area) const override {
        std::mt19937_64 rng{seed};
        std::uniform_real_distribution<float> x{area.x, area.x + area.width}, y{area.y, area.y + area.height};
        std::uniform_real_distribution<float> unit{0.0f, 1.0f};
        const float density = std::min((area.width * area.height) / (1280.0f * 686.0f), 14.0f);
        auto count = [&](float n) { return static_cast<int>(n * density); };

        std::vector<Decoration> out;
        for (int i = 0; i < count(30.0f); ++i)
            out.push_back({kDune, {x(rng), y(rng)}, 50.0f + unit(rng) * 90.0f, Color{248, 228, 180, 120}});
        for (int i = 0; i < count(60.0f); ++i)
            out.push_back({kShell, {x(rng), y(rng)}, 2.0f + unit(rng) * 2.0f,
                           unit(rng) < 0.5f ? Color{250, 235, 225, 255} : Color{240, 180, 170, 255}});
        for (int i = 0; i < count(10.0f); ++i)
            out.push_back({kStarfish, {x(rng), y(rng)}, 6.0f + unit(rng) * 3.0f, Color{240, 130, 60, 255}});
        for (int i = 0; i < count(5.0f); ++i)
            out.push_back({kTowel, {x(rng), y(rng)}, 1.0f, pick(kBright, rng)});
        for (int i = 0; i < count(4.0f); ++i)
            out.push_back({kSandcastle, {x(rng), y(rng)}, 0.8f + unit(rng) * 0.4f, Color{220, 190, 130, 255}});
        return out;
    }

    void drawScenery(const std::vector<Decoration>& decor, Rectangle village, float) const override {
        const float dry = waterline(village) + 30.0f; // nothing sits in the sea
        for (const auto& d : decor) {
            if (d.pos.y < dry) continue;
            switch (d.kind) {
            case kDune:
                DrawEllipse(static_cast<int>(d.pos.x), static_cast<int>(d.pos.y), d.size, d.size * 0.35f, d.color);
                break;
            case kShell:
                DrawCircleSector(d.pos, d.size * 1.6f, 180.0f, 360.0f, 8, d.color);
                break;
            case kStarfish:
                for (int k = 0; k < 5; ++k) {
                    const float a = static_cast<float>(k) * 1.2566f - 1.5708f;
                    const Vector2 tip{d.pos.x + std::cos(a) * d.size, d.pos.y + std::sin(a) * d.size};
                    DrawLineEx(d.pos, tip, d.size * 0.45f, d.color);
                }
                break;
            case kTowel:
                DrawRectangleV({d.pos.x, d.pos.y}, {46.0f, 22.0f}, d.color);
                DrawRectangleV({d.pos.x, d.pos.y + 7.0f}, {46.0f, 4.0f}, WHITE);
                DrawRectangleV({d.pos.x, d.pos.y + 14.0f}, {46.0f, 4.0f}, WHITE);
                break;
            case kSandcastle: {
                const float s = d.size;
                DrawRectangleV({d.pos.x - 14.0f * s, d.pos.y - 12.0f * s}, {28.0f * s, 12.0f * s}, d.color);
                DrawRectangleV({d.pos.x - 6.0f * s, d.pos.y - 22.0f * s}, {12.0f * s, 10.0f * s}, darken(d.color, 0.92f));
                DrawTriangle({d.pos.x, d.pos.y - 30.0f * s}, {d.pos.x - 6.0f * s, d.pos.y - 22.0f * s},
                             {d.pos.x + 6.0f * s, d.pos.y - 22.0f * s}, darken(d.color, 0.85f));
                break;
            }
            default:
                break;
            }
        }
    }

    void drawYard(Vector2 c) const override {
        DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), 95.0f, 32.0f, Color{250, 235, 195, 160});
        // A couple of towels laid out, placed deterministically from the yard's position.
        const auto h = mix64(static_cast<std::uint64_t>(static_cast<std::int64_t>(c.x * 7.0f + c.y * 13.0f)));
        const Color a = kBright[h % kBright.size()], b = kBright[(h >> 8) % kBright.size()];
        DrawRectangleV({c.x - 70.0f, c.y - 6.0f}, {40.0f, 18.0f}, a);
        DrawRectangleV({c.x - 70.0f, c.y + 1.0f}, {40.0f, 4.0f}, WHITE);
        DrawRectangleV({c.x + 34.0f, c.y - 2.0f}, {40.0f, 18.0f}, b);
        DrawRectangleV({c.x + 34.0f, c.y + 5.0f}, {40.0f, 4.0f}, WHITE);
    }

    const BuildingInfo& building(const std::string& uid) const override { return entry(uid).info; }

    void drawBuilding(Vector2 base, const std::string& uid, bool ready, float time) const override {
        const HutStyle& h = entry(uid).style;
        const float w = h.width, x0 = base.x - w / 2.0f;
        const float floorY = base.y - h.stilts;
        const float top = floorY - h.wallHeight;
        constexpr Color wood{150, 105, 65, 255};

        DrawEllipse(static_cast<int>(base.x), static_cast<int>(base.y), w * 0.6f, 8.0f, Color{0, 0, 0, 40});

        // Stilts + deck
        for (int k = 0; k < 4; ++k) {
            const float sx = x0 + 6.0f + static_cast<float>(k) * (w - 18.0f) / 3.0f;
            DrawRectangleV({sx, floorY}, {6.0f, h.stilts}, wood);
        }
        if (h.lifeguard) { // cross-bracing and a ladder
            DrawLineEx({x0 + 9.0f, floorY}, {x0 + w - 9.0f, base.y}, 3.0f, wood);
            DrawLineEx({x0 + w - 9.0f, floorY}, {x0 + 9.0f, base.y}, 3.0f, wood);
            for (float ly = floorY + 8.0f; ly < base.y; ly += 10.0f)
                DrawLineEx({base.x + 10.0f, ly}, {base.x + 26.0f, ly}, 2.0f, darken(wood, 0.8f));
        }
        DrawRectangleV({x0 - 6.0f, floorY - 4.0f}, {w + 12.0f, 7.0f}, darken(wood, 0.85f));

        // Walls: vertical planks
        DrawRectangleV({x0, top}, {w, h.wallHeight}, h.wall);
        for (float px = x0 + 12.0f; px < x0 + w; px += 12.0f)
            DrawLineEx({px, top + 2.0f}, {px, floorY - 4.0f}, 1.0f, withAlpha(darken(h.wall, 0.7f), 90));
        DrawRectangleLinesEx({x0, top, w, h.wallHeight}, 2.0f, darken(h.wall, 0.6f));

        if (h.lifeguard) { // red cross sign
            DrawRectangleV({base.x - 12.0f, top + 10.0f}, {24.0f, 24.0f}, WHITE);
            DrawRectangleV({base.x - 3.0f, top + 13.0f}, {6.0f, 18.0f}, Color{220, 40, 40, 255});
            DrawRectangleV({base.x - 9.0f, top + 19.0f}, {18.0f, 6.0f}, Color{220, 40, 40, 255});
        } else {
            // Door + porthole-ish window
            DrawRectangleV({x0 + w * 0.58f, floorY - h.wallHeight * 0.68f}, {w * 0.22f, h.wallHeight * 0.64f}, h.accent);
            const Vector2 win{x0 + w * 0.28f, top + h.wallHeight * 0.42f};
            DrawCircleV(win, 10.0f, ready ? Color{255, 235, 160, 255} : Color{55, 60, 70, 255});
            DrawCircleLines(static_cast<int>(win.x), static_cast<int>(win.y), 10.0f, darken(h.wall, 0.5f));
        }

        // Roof: thatch (shaggy) or a striped tin slope
        const float roofH = w * 0.38f;
        const Vector2 peak{base.x, top - roofH};
        const Vector2 l{x0 - 12.0f, top + 4.0f}, r{x0 + w + 12.0f, top + 4.0f};
        if (h.thatch) {
            DrawTriangle(peak, l, r, Color{200, 165, 90, 255});
            for (float fx = l.x; fx < r.x; fx += 8.0f) // fringe
                DrawTriangle({fx + 4.0f, top + 12.0f}, {fx, top + 3.0f}, {fx + 8.0f, top + 3.0f}, Color{185, 150, 80, 255});
            DrawLineEx(l, peak, 2.0f, Color{160, 125, 65, 255});
            DrawLineEx(peak, r, 2.0f, Color{160, 125, 65, 255});
        } else {
            DrawTriangle(peak, l, r, h.accent);
            DrawTriangle(peak, {base.x, top + 4.0f}, r, darken(h.accent, 0.85f));
        }
        if (h.lifeguard) { // flag
            DrawLineEx(peak, {peak.x, peak.y - 30.0f}, 2.0f, Color{80, 80, 80, 255});
            const float wave = std::sin(time * 4.0f) * 3.0f;
            DrawTriangle({peak.x, peak.y - 30.0f}, {peak.x, peak.y - 18.0f}, {peak.x + 20.0f, peak.y - 24.0f + wave},
                         Color{230, 50, 40, 255});
        }
        if (h.awning) {
            for (int k = 0; k < 5; ++k)
                DrawRectangleV({x0 + static_cast<float>(k) * w / 5.0f, top + 4.0f}, {w / 5.0f, 10.0f}, k % 2 ? WHITE : h.accent);
        }
        if (!ready) drawSmoke(peak, true, time, base.x * 0.013f);
    }

    void drawProp(Vector2 at, const Prop& p, float time) const override {
        const float s = p.scale;
        switch (p.kind) {
        case kPalm: {
            DrawEllipse(static_cast<int>(at.x + 20.0f * s), static_cast<int>(at.y), 30.0f * s, 7.0f * s, Color{0, 0, 0, 40});
            // Curved trunk from segments
            Vector2 prev = at;
            const float sway = std::sin(time * 1.1f + at.x * 0.03f) * 3.0f * s;
            for (int k = 1; k <= 8; ++k) {
                const float t = static_cast<float>(k) / 8.0f;
                const Vector2 next{at.x + (18.0f * t * t) * s + sway * t, at.y - 95.0f * t * s};
                DrawLineEx(prev, next, (9.0f - 3.0f * t) * s, k % 2 ? Color{150, 110, 70, 255} : Color{130, 95, 60, 255});
                prev = next;
            }
            for (int k = 0; k < 6; ++k) { // fronds
                const float a = -2.8f + static_cast<float>(k) * 0.5f + std::sin(time * 1.5f + static_cast<float>(k)) * 0.05f;
                const Vector2 tip{prev.x + std::cos(a) * 46.0f * s, prev.y + std::sin(a) * 26.0f * s + 14.0f * s};
                DrawTriangle(prev, {tip.x, tip.y - 4.0f * s}, {tip.x, tip.y + 5.0f * s}, p.color);
                DrawTriangle(prev, {tip.x, tip.y + 5.0f * s}, {tip.x, tip.y - 4.0f * s}, p.color);
            }
            DrawCircleV({prev.x - 3.0f * s, prev.y + 4.0f * s}, 4.0f * s, Color{110, 80, 40, 255}); // coconuts
            DrawCircleV({prev.x + 4.0f * s, prev.y + 5.0f * s}, 4.0f * s, Color{100, 70, 35, 255});
            break;
        }
        case kUmbrella: {
            DrawEllipse(static_cast<int>(at.x), static_cast<int>(at.y), 30.0f * s, 8.0f * s, Color{0, 0, 0, 35});
            DrawLineEx(at, {at.x, at.y - 55.0f * s}, 3.0f * s, Color{230, 230, 230, 255});
            for (int k = 0; k < 6; ++k) {
                const float a0 = 180.0f + static_cast<float>(k) * 30.0f;
                DrawCircleSector({at.x, at.y - 50.0f * s}, 34.0f * s, a0, a0 + 30.0f, 6, k % 2 ? WHITE : p.color);
            }
            break;
        }
        case kSurfboard:
            DrawEllipse(static_cast<int>(at.x), static_cast<int>(at.y - 30.0f * s), 8.0f * s, 32.0f * s, p.color);
            DrawLineEx({at.x, at.y - 60.0f * s}, {at.x, at.y}, 2.0f * s, WHITE);
            break;
        default:
            break;
        }
    }

  private:
    struct Entry {
        HutStyle style;
        BuildingInfo info;
    };

    const Entry& entry(const std::string& uid) const {
        if (auto it = cache_.find(uid); it != cache_.end()) return it->second;
        std::mt19937_64 rng{mix64(fnv1a(uid)) ^ 0xBEAC4ULL};
        std::uniform_real_distribution<float> unit{0.0f, 1.0f};
        Entry e;
        HutStyle& h = e.style;
        h.lifeguard = unit(rng) < 0.18f;
        h.width = h.lifeguard ? 80.0f + unit(rng) * 20.0f : 110.0f + unit(rng) * 50.0f;
        h.wallHeight = h.lifeguard ? 50.0f : 60.0f + unit(rng) * 25.0f;
        h.stilts = h.lifeguard ? 60.0f + unit(rng) * 20.0f : (unit(rng) < 0.6f ? 14.0f + unit(rng) * 16.0f : 0.0f);
        h.wall = h.lifeguard ? Color{245, 245, 240, 255} : pick(kHutColors, rng);
        h.accent = h.lifeguard ? Color{220, 50, 40, 255} : pick(kBright, rng);
        h.thatch = !h.lifeguard && unit(rng) < 0.55f;
        h.awning = !h.lifeguard && unit(rng) < 0.4f;

        const float roofH = h.width * 0.38f;
        e.info.halfWidth = h.width / 2.0f + 14.0f;
        e.info.height = h.stilts + h.wallHeight + roofH + (h.lifeguard ? 40.0f : 30.0f);
        e.info.labelHeight = h.stilts + h.wallHeight + roofH * 0.4f;

        float side = unit(rng) < 0.5f ? -1.0f : 1.0f;
        const int props = std::uniform_int_distribution<int>{1, 3}(rng);
        for (int i = 0; i < props; ++i) {
            const float r = unit(rng);
            const int kind = r < 0.5f ? kPalm : (r < 0.8f ? kUmbrella : kSurfboard);
            const Color color = kind == kPalm ? Color{60, 150, 70, 255} : pick(kBright, rng);
            e.info.props.push_back({kind, side * (h.width / 2.0f + 30.0f + unit(rng) * 30.0f), -5.0f + unit(rng) * 25.0f,
                                    0.85f + unit(rng) * 0.3f, color});
            side = -side;
        }
        return cache_.emplace(uid, std::move(e)).first->second;
    }

    mutable std::unordered_map<std::string, Entry> cache_;
};

} // namespace

std::unique_ptr<Theme> makeBeachTheme() { return std::make_unique<BeachTheme>(); }

} // namespace k10s
