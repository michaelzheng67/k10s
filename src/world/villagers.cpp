#include "world/villagers.hpp"

#include <cmath>
#include <numbers>
#include <utility>

#include "util/hash.hpp"

namespace {

// Each villager gets their own stable spot in the yard so they don't all crowd one point.
Vector2 yardSpot(const std::string& uid, Vector2 yard) {
    const std::uint64_t h = k10s::mix64(k10s::fnv1a(uid));
    const float angle = static_cast<float>(h & 0xFFFF) / 65535.0f * 2.0f * std::numbers::pi_v<float>;
    const float radius = 0.35f + 0.65f * static_cast<float>((h >> 16) & 0xFFFF) / 65535.0f;
    return {yard.x + std::cos(angle) * radius * 80.0f, yard.y + std::sin(angle) * radius * 20.0f};
}

} // namespace

namespace k10s {

void Villagers::add(const std::string& input_uid, Vector2 input_home, Appearance input_look) {
    if (index.contains(input_uid))
        return;

    uid.push_back(input_uid);
    home.push_back(input_home);
    look.push_back(input_look);
    pos.push_back(input_home);
    vel.push_back({0.0f, 0.0f});
    onFire.push_back(0);

    index[input_uid] = uid.size() - 1;
}

// Swap-and-pop: O(1) removal that keeps every array dense.
//   1. Move the LAST villager into the removed villager's slot.
//   2. Pop the back of every array.
//   3. Point the moved villager's index entry at its new slot.
// Order is not preserved, which is fine: nothing depends on it.
void Villagers::remove(const std::string& input_uid) {
    auto it = index.find(input_uid);
    if (it == index.end())
        return;

    const std::size_t i = it->second;
    const std::size_t last = uid.size() - 1;
    index.erase(it); // erase first: input_uid might alias uid[i], which we overwrite below

    if (i != last) {
        pos[i] = pos[last];
        vel[i] = vel[last];
        home[i] = home[last];
        look[i] = look[last];
        onFire[i] = onFire[last];
        uid[i] = std::move(uid[last]); // move, not copy: steals the string's buffer
        index[uid[i]] = i;
    }

    pos.pop_back();
    vel.pop_back();
    home.pop_back();
    look.pop_back();
    onFire.pop_back();
    uid.pop_back();
}

void Villagers::sync(const ClusterState& state,
                     const std::unordered_map<std::string, Vector2>& housePos) {
    for (const auto& [podUid, pod] : state.pods()) {
        if (!index.contains(podUid)) {
            auto house = housePos.find(pod.nodeName);
            if (house == housePos.end())
                continue;
            add(podUid, yardSpot(podUid, house->second), appearanceFor(podUid));
        }
        onFire[index.at(podUid)] = isHealthy(pod) ? 0 : 1;
    }

    std::vector<std::string> dead;
    for (const auto& u : uid) {
        if (!state.pods().contains(u)) {
            dead.push_back(u);
        }
    }

    for (const auto& u : dead)
        remove(u);
}

} // namespace k10s
