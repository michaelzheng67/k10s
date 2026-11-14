// The default theme: cottages, trees and a flowery meadow.
#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "render/house.hpp"
#include "render/scenery.hpp"
#include "theme/theme.hpp"

namespace k10s {

namespace {

constexpr int kRoundTree = 0, kPineTree = 1;

class VillageTheme final : public Theme {
  public:
    std::string_view name() const override { return "village"; }
    Color background() const override { return {106, 168, 79, 255}; }

    std::vector<Decoration> makeScenery(std::uint64_t seed, Rectangle area) const override {
        return k10s::makeScenery(seed, area);
    }
    void drawScenery(const std::vector<Decoration>& decor, Rectangle, float) const override {
        k10s::drawScenery(decor);
    }
    void drawYard(Vector2 centre) const override { k10s::drawYard(centre); }

    const BuildingInfo& building(const std::string& uid) const override { return entry(uid).info; }

    void drawBuilding(Vector2 base, const std::string& uid, bool ready, float time) const override {
        drawHouse(base, entry(uid).style, ready, time);
    }

    void drawProp(Vector2 at, const Prop& p, float time) const override {
        drawTree(at, TreeSpec{p.dx, p.dy, p.scale, p.color, p.kind == kPineTree}, time);
    }

  private:
    struct Entry {
        HouseStyle style;
        BuildingInfo info;
    };

    // Styles are deterministic per uid; cache them so we don't re-seed an RNG every frame.
    const Entry& entry(const std::string& uid) const {
        auto it = cache_.find(uid);
        if (it != cache_.end()) return it->second;
        Entry e;
        e.style = houseStyleFor(uid);
        e.info.halfWidth = e.style.width / 2.0f + e.style.width * 0.08f;
        e.info.height = e.style.totalHeight() + (e.style.chimney ? 40.0f : 10.0f);
        e.info.labelHeight = e.style.wallHeight() + e.style.roofHeight() * 0.38f;
        for (int i = 0; i < e.style.treeCount; ++i) {
            const TreeSpec& t = e.style.trees[static_cast<std::size_t>(i)];
            e.info.props.push_back({t.pine ? kPineTree : kRoundTree, t.dx, t.dy, t.scale, t.leaves});
        }
        return cache_.emplace(uid, std::move(e)).first->second;
    }

    mutable std::unordered_map<std::string, Entry> cache_;
};

} // namespace

std::unique_ptr<Theme> makeVillageTheme() { return std::make_unique<VillageTheme>(); }

} // namespace k10s
