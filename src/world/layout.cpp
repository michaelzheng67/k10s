#include "world/layout.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <unordered_set>

#include "util/hash.hpp"

namespace k10s {

namespace {

constexpr float kGap = 24.0f;       // minimum space between footprints
constexpr float kYardBelow = 120.0f; // yard extends this far below the base

Rectangle footprintFor(Vector2 base, const BuildingInfo& info) {
    float halfW = info.halfWidth;
    for (const auto& p : info.props) halfW = std::max(halfW, std::abs(p.dx) + 40.0f * p.scale);
    halfW = std::max(halfW, 100.0f); // yard width
    return {base.x - halfW, base.y - info.height, 2.0f * halfW, info.height + kYardBelow};
}

bool overlaps(const Rectangle& a, const Rectangle& b) {
    return a.x < b.x + b.width + kGap && b.x < a.x + a.width + kGap && a.y < b.y + b.height + kGap &&
           b.y < a.y + a.height + kGap;
}

} // namespace

const PlacedHouse* VillageLayout::find(const std::string& name) const {
    for (const auto& h : houses_)
        if (h.name == name) return &h;
    return nullptr;
}

bool VillageLayout::fits(const Rectangle& r) const {
    return std::none_of(houses_.begin(), houses_.end(),
                        [&](const PlacedHouse& h) { return overlaps(r, h.footprint); });
}

void VillageLayout::place(const std::string& uid, const std::string& name, bool ready,
                          const Theme& theme) {
    const BuildingInfo& style = theme.building(uid);
    std::mt19937_64 rng{mix64(fnv1a(uid)) ^ 0x9e3779b97f4a7c15ULL};
    std::uniform_real_distribution<float> unit{0.0f, 1.0f};

    // Sample candidate spots in an ellipse around the centre and keep the closest one
    // that fits: houses cluster into a compact village. Grow the search area if crowded.
    float radius = 250.0f + 140.0f * std::sqrt(static_cast<float>(houses_.size()));
    for (int attempt = 0; attempt < 20; ++attempt, radius *= 1.3f) {
        Vector2 best{};
        float bestScore = std::numeric_limits<float>::max();
        for (int i = 0; i < 160; ++i) {
            const float r = radius * std::sqrt(unit(rng));
            const float theta = unit(rng) * 2.0f * std::numbers::pi_v<float>;
            const Vector2 base{std::cos(theta) * r * 1.6f, std::sin(theta) * r}; // wide, like the screen
            const Rectangle fp = footprintFor(base, style);
            if (!fits(fp)) continue;
            const float score = std::hypot(base.x / 1.6f, base.y) + unit(rng) * 60.0f; // a bit of mess
            if (score < bestScore) {
                bestScore = score;
                best = base;
            }
        }
        if (bestScore < std::numeric_limits<float>::max()) {
            houses_.push_back({uid, name, best, footprintFor(best, style), ready});
            return;
        }
    }
}

void VillageLayout::update(const ClusterState& state, const Theme& theme) {
    // Drop houses whose node is gone (their spot becomes free for newcomers).
    std::erase_if(houses_, [&](const PlacedHouse& h) { return state.findNode(h.uid) == nullptr; });

    // Refresh readiness; collect new nodes.
    std::unordered_set<std::string> placed;
    for (auto& h : houses_) {
        placed.insert(h.uid);
        h.ready = state.findNode(h.uid)->ready;
    }
    std::vector<const Node*> fresh;
    for (const auto& [uid, node] : state.nodes())
        if (!placed.contains(uid)) fresh.push_back(&node);

    // Place newcomers in name order so the same cluster always lays out the same way.
    std::sort(fresh.begin(), fresh.end(), [](const Node* a, const Node* b) { return a->name < b->name; });
    for (const Node* n : fresh) place(n->uid, n->name, n->ready, theme);
}

Rectangle VillageLayout::bounds() const {
    if (houses_.empty()) return {-640.0f, -360.0f, 1280.0f, 720.0f};
    float minX = std::numeric_limits<float>::max(), minY = minX;
    float maxX = std::numeric_limits<float>::lowest(), maxY = maxX;
    for (const auto& h : houses_) {
        minX = std::min(minX, h.footprint.x);
        minY = std::min(minY, h.footprint.y);
        maxX = std::max(maxX, h.footprint.x + h.footprint.width);
        maxY = std::max(maxY, h.footprint.y + h.footprint.height);
    }
    constexpr float margin = 60.0f;
    return {minX - margin, minY - margin, maxX - minX + 2.0f * margin, maxY - minY + 2.0f * margin};
}

} // namespace k10s
