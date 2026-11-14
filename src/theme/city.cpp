// City theme: high-rises with lit window grids on a paved plaza.
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <unordered_map>

#include "theme/theme.hpp"
#include "util/hash.hpp"

namespace k10s {

namespace {

constexpr int kLamp = 0, kPlanterTree = 1, kBench = 2;             // props
constexpr int kStain = 0, kManhole = 1, kCrosswalk = 2, kHydrant = 3, kPlanter = 4; // scenery

constexpr std::array<Color, 6> kFacades{{
    {90, 130, 170, 255},  // glass blue
    {170, 170, 165, 255}, // concrete
    {160, 90, 70, 255},   // brick
    {210, 195, 160, 255}, // limestone
    {70, 75, 85, 255},    // dark steel
    {120, 150, 140, 255}, // green glass
}};
constexpr std::array<Color, 5> kAwnings{{
    {200, 60, 60, 255}, {40, 110, 160, 255}, {60, 140, 80, 255}, {230, 170, 50, 255}, {120, 70, 150, 255},
}};

template <std::size_t N> Color pick(const std::array<Color, N>& p, std::mt19937_64& rng) {
    return p[std::uniform_int_distribution<std::size_t>{0, N - 1}(rng)];
}

struct TowerStyle {
    float width{150.0f};
    int floors{6};
    float floorHeight{26.0f};
    Color facade{};
    Color awning{};
    int roofKind{0}; // 0 flat + AC units, 1 water tower, 2 antenna, 3 stepped crown
    std::uint64_t windowSeed{0};

    float bodyHeight() const { return floorHeight * static_cast<float>(floors) + 30.0f; } // +storefront
};

class CityTheme final : public Theme {
  public:
    std::string_view name() const override { return "city"; }
    Color background() const override { return {150, 152, 155, 255}; }
    SignStyle signStyle() const override { return {{35, 38, 45, 255}, {90, 95, 105, 255}, {255, 225, 140, 255}}; }

    void drawGround(Rectangle visible, Rectangle, float) const override {
        // Paving slabs: a grid aligned to world coordinates so it doesn't swim when panning.
        constexpr float tile = 48.0f;
        const Color line{135, 137, 140, 255};
        const float x0 = std::floor(visible.x / tile) * tile;
        const float y0 = std::floor(visible.y / tile) * tile;
        for (float x = x0; x < visible.x + visible.width; x += tile)
            DrawLineEx({x, visible.y}, {x, visible.y + visible.height}, 1.5f, line);
        for (float y = y0; y < visible.y + visible.height; y += tile)
            DrawLineEx({visible.x, y}, {visible.x + visible.width, y}, 1.5f, line);
    }

    std::vector<Decoration> makeScenery(std::uint64_t seed, Rectangle area) const override {
        std::mt19937_64 rng{seed};
        std::uniform_real_distribution<float> x{area.x, area.x + area.width}, y{area.y, area.y + area.height};
        std::uniform_real_distribution<float> unit{0.0f, 1.0f};
        const float density = std::min((area.width * area.height) / (1280.0f * 686.0f), 14.0f);
        auto count = [&](float n) { return static_cast<int>(n * density); };

        std::vector<Decoration> out;
        for (int i = 0; i < count(12.0f); ++i)
            out.push_back({kStain, {x(rng), y(rng)}, 30.0f + unit(rng) * 50.0f, Color{120, 122, 125, 50}});
        for (int i = 0; i < count(4.0f); ++i)
            out.push_back({kManhole, {x(rng), y(rng)}, 9.0f, Color{80, 82, 85, 255}});
        for (int i = 0; i < count(1.5f); ++i)
            out.push_back({kCrosswalk, {x(rng), y(rng)}, 1.0f, Color{235, 235, 230, 230}});
        for (int i = 0; i < count(3.0f); ++i)
            out.push_back({kHydrant, {x(rng), y(rng)}, 1.0f, Color{210, 50, 40, 255}});
        for (int i = 0; i < count(5.0f); ++i)
            out.push_back({kPlanter, {x(rng), y(rng)}, 0.8f + unit(rng) * 0.4f, Color{70, 140, 70, 255}});
        return out;
    }

    void drawScenery(const std::vector<Decoration>& decor, Rectangle, float) const override {
        for (const auto& d : decor) {
            switch (d.kind) {
            case kStain:
                DrawEllipse(static_cast<int>(d.pos.x), static_cast<int>(d.pos.y), d.size, d.size * 0.5f, d.color);
                break;
            case kManhole:
                DrawCircleV(d.pos, d.size, d.color);
                DrawCircleLines(static_cast<int>(d.pos.x), static_cast<int>(d.pos.y), d.size * 0.7f, darken(d.color, 0.7f));
                break;
            case kCrosswalk:
                for (int k = 0; k < 6; ++k)
                    DrawRectangleV({d.pos.x + static_cast<float>(k) * 14.0f, d.pos.y}, {8.0f, 46.0f}, d.color);
                break;
            case kHydrant:
                DrawRectangleV({d.pos.x - 4.0f, d.pos.y - 14.0f}, {8.0f, 14.0f}, d.color);
                DrawCircleV({d.pos.x, d.pos.y - 14.0f}, 4.0f, d.color);
                DrawRectangleV({d.pos.x - 7.0f, d.pos.y - 10.0f}, {14.0f, 3.0f}, darken(d.color, 0.8f));
                break;
            case kPlanter:
                DrawRectangleV({d.pos.x - 12.0f * d.size, d.pos.y - 8.0f * d.size}, {24.0f * d.size, 10.0f * d.size},
                               Color{120, 110, 100, 255});
                DrawCircleV({d.pos.x, d.pos.y - 11.0f * d.size}, 10.0f * d.size, d.color);
                break;
            default:
                break;
            }
        }
    }

    void drawYard(Vector2 c) const override {
        // A lighter plaza square with a kerb.
        const Rectangle r{c.x - 95.0f, c.y - 32.0f, 190.0f, 64.0f};
        DrawRectangleRounded(r, 0.25f, 6, Color{190, 190, 185, 255});
        DrawRectangleRoundedLinesEx(r, 0.25f, 6, 2.0f, Color{120, 120, 118, 255});
    }

    const BuildingInfo& building(const std::string& uid) const override { return entry(uid).info; }

    void drawBuilding(Vector2 base, const std::string& uid, bool ready, float time) const override {
        const TowerStyle& t = entry(uid).style;
        const float w = t.width, bodyH = t.bodyHeight();
        const float x0 = base.x - w / 2.0f, top = base.y - bodyH;

        DrawEllipse(static_cast<int>(base.x), static_cast<int>(base.y), w * 0.6f, 9.0f, Color{0, 0, 0, 50});

        // Body with a darker side strip for depth
        DrawRectangleV({x0, top}, {w, bodyH}, t.facade);
        DrawRectangleV({x0 + w * 0.82f, top}, {w * 0.18f, bodyH}, darken(t.facade, 0.82f));

        // Window grid (lit pattern is deterministic per building; a few flicker)
        const int cols = std::max(3, static_cast<int>(w / 26.0f));
        const float cellW = w / static_cast<float>(cols);
        const float winW = cellW * 0.55f, winH = t.floorHeight * 0.55f;
        for (int f = 0; f < t.floors; ++f) {
            const float wy = top + 10.0f + static_cast<float>(f) * t.floorHeight;
            for (int c = 0; c < cols; ++c) {
                const std::uint64_t h = mix64(t.windowSeed + static_cast<std::uint64_t>(f * 131 + c));
                bool lit = ready && (h % 100) < 65;
                if (lit && (h % 37) == 0) lit = std::sin(time * 3.0f + static_cast<float>(h % 100)) > -0.6f;
                const Color glass = lit ? Color{255, 228, 150, 255} : Color{55, 65, 80, 255};
                const float wx = x0 + static_cast<float>(c) * cellW + (cellW - winW) / 2.0f;
                DrawRectangleV({wx, wy}, {winW, winH}, glass);
            }
        }

        // Storefront: glass, awning, door
        const float shopTop = base.y - 30.0f;
        DrawRectangleV({x0, shopTop}, {w, 30.0f}, darken(t.facade, 0.7f));
        DrawRectangleV({x0 + 8.0f, shopTop + 6.0f}, {w - 16.0f, 22.0f},
                       ready ? Color{200, 225, 235, 255} : Color{60, 70, 80, 255});
        DrawRectangleV({base.x - 9.0f, shopTop + 6.0f}, {18.0f, 24.0f}, Color{50, 50, 55, 255});
        for (int k = 0; k < 6; ++k) { // striped awning
            const float sx = x0 + 4.0f + static_cast<float>(k) * (w - 8.0f) / 6.0f;
            DrawRectangleV({sx, shopTop - 6.0f}, {(w - 8.0f) / 6.0f, 8.0f}, k % 2 ? WHITE : t.awning);
        }

        // Parapet + roof gear
        DrawRectangleV({x0 - 3.0f, top - 6.0f}, {w + 6.0f, 8.0f}, darken(t.facade, 0.6f));
        Vector2 smokeAt{base.x + w * 0.25f, top - 8.0f};
        switch (t.roofKind) {
        case 1: { // water tower
            const float tx = x0 + w * 0.25f;
            DrawLineEx({tx, top - 6.0f}, {tx + 4.0f, top - 26.0f}, 2.0f, Color{80, 60, 45, 255});
            DrawLineEx({tx + 24.0f, top - 6.0f}, {tx + 20.0f, top - 26.0f}, 2.0f, Color{80, 60, 45, 255});
            DrawRectangleRounded({tx - 2.0f, top - 52.0f, 28.0f, 28.0f}, 0.3f, 4, Color{130, 90, 60, 255});
            DrawTriangle({tx + 12.0f, top - 62.0f}, {tx - 4.0f, top - 50.0f}, {tx + 28.0f, top - 50.0f},
                         Color{90, 70, 55, 255});
            smokeAt = {x0 + w * 0.7f, top - 8.0f};
            break;
        }
        case 2: { // antenna with a blinking light
            DrawLineEx({base.x, top - 6.0f}, {base.x, top - 60.0f}, 3.0f, Color{60, 60, 65, 255});
            const bool on = std::fmod(time + static_cast<float>(t.windowSeed % 7), 1.5f) < 0.3f;
            DrawCircleV({base.x, top - 62.0f}, 3.5f, on ? Color{255, 60, 50, 255} : Color{120, 40, 40, 255});
            break;
        }
        case 3: // stepped crown
            DrawRectangleV({x0 + w * 0.15f, top - 22.0f}, {w * 0.7f, 16.0f}, darken(t.facade, 0.9f));
            DrawRectangleV({x0 + w * 0.32f, top - 36.0f}, {w * 0.36f, 14.0f}, darken(t.facade, 0.8f));
            smokeAt = {base.x, top - 36.0f};
            break;
        default: // AC units
            DrawRectangleV({x0 + 10.0f, top - 18.0f}, {24.0f, 12.0f}, Color{180, 180, 180, 255});
            DrawRectangleV({x0 + w - 40.0f, top - 16.0f}, {20.0f, 10.0f}, Color{170, 170, 170, 255});
            break;
        }
        if (!ready) drawSmoke(smokeAt, true, time, base.x * 0.013f);
    }

    void drawProp(Vector2 at, const Prop& p, float time) const override {
        const float s = p.scale;
        switch (p.kind) {
        case kLamp: {
            DrawEllipse(static_cast<int>(at.x), static_cast<int>(at.y), 8.0f * s, 3.0f * s, Color{0, 0, 0, 50});
            DrawRectangleV({at.x - 2.0f * s, at.y - 60.0f * s}, {4.0f * s, 60.0f * s}, Color{50, 55, 60, 255});
            DrawRectangleV({at.x - 2.0f * s, at.y - 62.0f * s}, {16.0f * s, 3.0f * s}, Color{50, 55, 60, 255});
            const float glow = 0.75f + 0.25f * std::sin(time * 2.0f + at.x);
            DrawCircleGradient(static_cast<int>(at.x + 14.0f * s), static_cast<int>(at.y - 56.0f * s), 16.0f * s,
                               Color{255, 230, 150, static_cast<unsigned char>(110.0f * glow)},
                               Color{255, 230, 150, 0});
            DrawCircleV({at.x + 14.0f * s, at.y - 58.0f * s}, 4.0f * s, Color{255, 240, 190, 255});
            break;
        }
        case kPlanterTree:
            DrawEllipse(static_cast<int>(at.x), static_cast<int>(at.y), 18.0f * s, 5.0f * s, Color{0, 0, 0, 45});
            DrawRectangleV({at.x - 14.0f * s, at.y - 14.0f * s}, {28.0f * s, 14.0f * s}, Color{110, 105, 100, 255});
            DrawRectangleV({at.x - 3.0f * s, at.y - 36.0f * s}, {6.0f * s, 24.0f * s}, Color{100, 70, 45, 255});
            DrawCircleV({at.x, at.y - 46.0f * s}, 18.0f * s, p.color);
            DrawCircleV({at.x - 8.0f * s, at.y - 40.0f * s}, 11.0f * s, darken(p.color, 0.85f));
            break;
        case kBench:
            DrawRectangleV({at.x - 18.0f * s, at.y - 10.0f * s}, {36.0f * s, 5.0f * s}, Color{140, 100, 60, 255});
            DrawRectangleV({at.x - 18.0f * s, at.y - 20.0f * s}, {36.0f * s, 4.0f * s}, Color{140, 100, 60, 255});
            DrawRectangleV({at.x - 16.0f * s, at.y - 6.0f * s}, {3.0f * s, 6.0f * s}, Color{50, 50, 55, 255});
            DrawRectangleV({at.x + 13.0f * s, at.y - 6.0f * s}, {3.0f * s, 6.0f * s}, Color{50, 50, 55, 255});
            break;
        default:
            break;
        }
    }

  private:
    struct Entry {
        TowerStyle style;
        BuildingInfo info;
    };

    const Entry& entry(const std::string& uid) const {
        if (auto it = cache_.find(uid); it != cache_.end()) return it->second;
        std::mt19937_64 rng{mix64(fnv1a(uid)) ^ 0xC17ULL};
        std::uniform_real_distribution<float> unit{0.0f, 1.0f};
        Entry e;
        TowerStyle& t = e.style;
        t.width = 110.0f + unit(rng) * 80.0f;
        t.floors = std::uniform_int_distribution<int>{4, 11}(rng);
        t.floorHeight = 24.0f + unit(rng) * 6.0f;
        t.facade = pick(kFacades, rng);
        t.awning = pick(kAwnings, rng);
        t.roofKind = std::uniform_int_distribution<int>{0, 3}(rng);
        t.windowSeed = rng();

        e.info.halfWidth = t.width / 2.0f + 4.0f;
        e.info.height = t.bodyHeight() + (t.roofKind == 0 ? 50.0f : 75.0f);
        e.info.labelHeight = 46.0f; // storefront sign just above the awning

        float side = unit(rng) < 0.5f ? -1.0f : 1.0f;
        const int props = std::uniform_int_distribution<int>{1, 3}(rng);
        for (int i = 0; i < props; ++i) {
            const int kind = std::uniform_int_distribution<int>{0, 2}(rng);
            e.info.props.push_back({kind, side * (t.width / 2.0f + 24.0f + unit(rng) * 30.0f), -5.0f + unit(rng) * 30.0f,
                                    0.9f + unit(rng) * 0.3f, Color{70, 140, 70, 255}});
            side = -side;
        }
        return cache_.emplace(uid, std::move(e)).first->second;
    }

    mutable std::unordered_map<std::string, Entry> cache_;
};

} // namespace

std::unique_ptr<Theme> makeCityTheme() { return std::make_unique<CityTheme>(); }

} // namespace k10s
