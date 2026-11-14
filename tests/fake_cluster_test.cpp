#include <catch2/catch_test_macros.hpp>

#include "model/cluster_state.hpp"
#include "sim/fake_cluster.hpp"

using namespace k10s;

namespace {

ClusterState applyAll(const std::vector<Event>& events) {
    ClusterState s;
    for (const auto& e : events) s.apply(e);
    return s;
}

} // namespace

TEST_CASE("snapshot has one Added event per node and pod") {
    FakeCluster sim{42, 3, 4};
    const auto events = sim.snapshot();

    CHECK(events.size() == 3 + 3 * 4);
    for (const auto& e : events) {
        std::visit([](const auto& ev) { CHECK(ev.type == EventType::Added); }, e);
    }
}

TEST_CASE("snapshot builds a consistent cluster") {
    FakeCluster sim{42, 3, 4};
    const ClusterState s = applyAll(sim.snapshot());

    CHECK(s.nodeCount() == 3);
    CHECK(s.podCount() == 12);
}

TEST_CASE("snapshot: nodes ready, pods running and placed on real nodes") {
    FakeCluster sim{42, 2, 3};
    const auto events = sim.snapshot();
    const ClusterState s = applyAll(events);

    for (const auto& e : events) {
        if (const auto* ne = std::get_if<NodeEvent>(&e)) {
            CHECK(ne->node.ready);
        } else if (const auto* pe = std::get_if<PodEvent>(&e)) {
            CHECK(pe->pod.phase == PodPhase::Running);
            CHECK(s.findNode(pe->pod.nodeName) != nullptr);
        }
    }
}

TEST_CASE("snapshot pod uids are unique") {
    FakeCluster sim{42, 5, 10};
    const ClusterState s = applyAll(sim.snapshot());
    CHECK(s.podCount() == 50); // duplicates would collapse in the map
}

TEST_CASE("empty cluster") {
    FakeCluster sim{42, 0, 5};
    CHECK(sim.snapshot().empty());
}

namespace {

// Flattens events into comparable strings so two runs can be diffed.
std::vector<std::string> describe(const std::vector<Event>& events) {
    std::vector<std::string> out;
    for (const auto& e : events) {
        std::visit(
            [&](const auto& ev) {
                if constexpr (std::is_same_v<std::decay_t<decltype(ev)>, PodEvent>) {
                    out.push_back(ev.pod.uid + ":" + std::to_string(static_cast<int>(ev.pod.phase)) +
                                  ":" + std::to_string(ev.pod.restarts));
                } else {
                    out.push_back(ev.node.uid);
                }
            },
            e);
    }
    return out;
}

} // namespace

TEST_CASE("same seed -> identical event stream") {
    FakeCluster a{7, 4, 10};
    FakeCluster b{7, 4, 10};
    for (int i = 0; i < 200; ++i) {
        REQUIRE(describe(a.tick()) == describe(b.tick()));
    }
}

TEST_CASE("different seeds diverge") {
    FakeCluster a{1, 4, 10};
    FakeCluster b{2, 4, 10};
    bool differed = false;
    for (int i = 0; i < 200 && !differed; ++i) {
        differed = describe(a.tick()) != describe(b.tick());
    }
    CHECK(differed);
}

TEST_CASE("tick emits only pod Modified events that toggle crash state") {
    FakeCluster sim{99, 3, 10};
    ClusterState s = applyAll(sim.snapshot());

    int crashes = 0;
    int recoveries = 0;
    for (int i = 0; i < 500; ++i) {
        for (const auto& e : sim.tick()) {
            const auto* pe = std::get_if<PodEvent>(&e);
            REQUIRE(pe != nullptr);
            REQUIRE(pe->type == EventType::Modified);

            const Pod* before = s.findPod(pe->pod.uid);
            REQUIRE(before != nullptr);
            if (pe->pod.phase == PodPhase::CrashLoopBackOff) {
                CHECK(before->phase == PodPhase::Running);
                CHECK(pe->pod.restarts == before->restarts + 1);
                ++crashes;
            } else {
                CHECK(pe->pod.phase == PodPhase::Running);
                CHECK(before->phase == PodPhase::CrashLoopBackOff);
                ++recoveries;
            }
            s.apply(e);
        }
    }
    CHECK(crashes > 0);
    CHECK(recoveries > 0);
    CHECK(s.podCount() == 30);
}
