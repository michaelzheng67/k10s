#pragma once
#include "model/events.hpp"
#include <cstdint>
#include <random>
#include <vector>

namespace k10s {
class FakeCluster {
    std::mt19937_64 rng_;
    std::vector<Node> nodes_;
    std::vector<Pod> pods_;
    int nextPodId_{0};

  public:
    FakeCluster(std::uint64_t seed, int nodeCount, int podsPerNode);
    std::vector<Event> snapshot() const;
    std::vector<Event> tick();
};
} // namespace k10s