#pragma once

#include <cstdint>
#include <stop_token>

#include "sim/fake_cluster.hpp"
#include "source/data_source.hpp"

namespace k10s {
class FakeSource : public DataSource {
  public:
    FakeSource(std::uint64_t seed, int nodes, int podsPerNode) : sim_(seed, nodes, podsPerNode) {}
    void run(std::stop_token st, EventQueue& out) override;

  private:
    FakeCluster sim_;
};
} // namespace k10s