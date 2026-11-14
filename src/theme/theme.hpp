#pragma once

#include <raylib.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace k10s {

// Something drawn beside a building and depth-sorted with villagers (tree, palm, lamp...).
// `kind` is interpreted by the theme that produced it.
struct Prop {
    int kind{0};
    float dx{0.0f}, dy{0.0f}; // offset from the building's base-centre
    float scale{1.0f};
    Color color{};
};

// Flat ground decoration (grass tuft, manhole, seashell...). Drawn under everything.
struct Decoration {
    int kind{0};
    Vector2 pos{};
    float size{1.0f};
    Color color{};
};

// What the layout needs to know about a node's building. Deterministic per uid.
struct BuildingInfo {
    float halfWidth{100.0f}; // building incl. overhangs (props are added separately)
    float height{150.0f};    // above the base, incl. roof and smoke/antenna headroom
    float labelHeight{80.0f}; // where the name sign goes, above the base
    std::vector<Prop> props;
};

// A visual theme: decides how ground, buildings, props and scenery look. Everything else
// (layout, villagers, fire, camera) is theme-agnostic.
class Theme {
  public:
    virtual ~Theme() = default;

    virtual std::string_view name() const = 0;
    virtual Color background() const = 0;

    // The area the camera should frame for a village with these bounds. Themes can reserve
    // extra space (e.g. the beach keeps a band of ocean in view).
    virtual Rectangle frame(Rectangle village) const { return village; }

    // World-space ground details drawn right after the background (e.g. pavement, ocean).
    // `visible` is the world rect on screen, `village` the layout bounds.
    virtual void drawGround(Rectangle visible, Rectangle village, float time) const;

    virtual std::vector<Decoration> makeScenery(std::uint64_t seed, Rectangle area) const = 0;
    virtual void drawScenery(const std::vector<Decoration>& decor, Rectangle village,
                             float time) const = 0;

    // The patch in front of a building where its villagers hang out.
    virtual void drawYard(Vector2 centre) const = 0;

    virtual const BuildingInfo& building(const std::string& uid) const = 0;
    virtual void drawBuilding(Vector2 base, const std::string& uid, bool ready, float time) const = 0;
    virtual void drawProp(Vector2 at, const Prop& prop, float time) const = 0;
};

// --- registry ---------------------------------------------------------------
const std::vector<std::string>& themeNames(); // "village", "city", "beach"
std::unique_ptr<Theme> makeTheme(std::string_view name); // nullptr if unknown

// Shared drawing helpers for theme implementations.
Color darken(Color c, float f);
Color withAlpha(Color c, unsigned char a);
void drawSmoke(Vector2 top, bool heavy, float time, float phase);

} // namespace k10s
