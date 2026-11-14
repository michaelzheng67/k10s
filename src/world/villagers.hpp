#pragma once

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "model/cluster_state.hpp"
#include "render/appearance.hpp"

namespace k10s {
struct Villagers {
    std::vector<Vector2> pos, vel, home;
    std::vector<Appearance> look;
    std::vector<std::string> uid;
    std::vector<std::uint8_t> onFire;

    std::unordered_map<std::string, std::size_t> index;
    void add(const std::string& input_uid, Vector2 input_home, Appearance input_look);
    void remove(const std::string& input_uid);
    std::size_t size() const { return uid.size(); }

    void sync(const ClusterState& state, const std::unordered_map<std::string, Vector2>& housePos);
};
} // namespace k10s
