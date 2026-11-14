#include "sim/fake_cluster.hpp"

#include <string>

namespace k10s {
FakeCluster::FakeCluster(std::uint64_t seed, int nodeCount, int podsPerNode) : rng_(seed) {
    for (int i = 0; i < nodeCount; ++i) {
        const std::string node_name = "node-" + std::to_string(i);
        nodes_.push_back(Node{.uid = node_name, .name = node_name, .ready = true});

        for (int j = 0; j < podsPerNode; ++j) {
            const std::string uid = "pod-" + std::to_string(nextPodId_++);
            pods_.push_back(Pod{
                .uid = uid,
                .name = uid,
                .namespace_ = "default",
                .nodeName = node_name,
                .phase = PodPhase::Running,
            });
        }
    }
}

std::vector<Event> FakeCluster::snapshot() const {
    std::vector<Event> events;
    events.reserve(nodes_.size() + pods_.size());
    for (const auto& n : nodes_) {
        events.push_back(NodeEvent{EventType::Added, n});
    }
    for (const auto& p : pods_) {
        events.push_back(PodEvent{EventType::Added, p});
    }

    return events;
}

std::vector<Event> FakeCluster::tick() {
    std::vector<Event> events;
    std::bernoulli_distribution crash{0.05};
    std::bernoulli_distribution recover{0.2};
    for (auto& p : pods_) {
        if (p.phase == PodPhase::Running && crash(rng_)) {
            p.phase = PodPhase::CrashLoopBackOff;
            ++p.restarts;
            events.push_back(PodEvent{EventType::Modified, p});
        } else if (p.phase == PodPhase::CrashLoopBackOff && recover(rng_)) {
            p.phase = PodPhase::Running;
            events.push_back(PodEvent{EventType::Modified, p});
        }
    }

    return events;
}

} // namespace k10s