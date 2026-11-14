#pragma once

#include <raylib.h>

#include <string>
#include <vector>

#include "model/cluster_state.hpp"
#include "theme/theme.hpp"

namespace k10s {

struct PlacedHouse {
    std::string uid;
    std::string name;
    Vector2 base;        // bottom-centre of the building
    Rectangle footprint; // building + props + yard; buildings never overlap these
    bool ready{true};

    Vector2 yard() const { return {base.x, base.y + 68.0f}; }
};

// Scatters houses organically around a village centre.
//  - Deterministic: the same set of nodes always produces the same village.
//  - Stable: existing houses never move when nodes are added or removed.
class VillageLayout {
  public:
    // Places new nodes, drops removed ones, refreshes readiness.
    // Building sizes come from `theme`; after switching themes, clear() and update again.
    void update(const ClusterState& state, const Theme& theme);
    void clear() { houses_.clear(); }

    const std::vector<PlacedHouse>& houses() const { return houses_; }
    const PlacedHouse* find(const std::string& name) const;

    // Union of all footprints (plus margin); what the camera should show.
    Rectangle bounds() const;

  private:
    bool fits(const Rectangle& r) const;
    void place(const std::string& uid, const std::string& name, bool ready, const Theme& theme);

    std::vector<PlacedHouse> houses_;
};

} // namespace k10s
