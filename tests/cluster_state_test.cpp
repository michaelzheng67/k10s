#include <catch2/catch_test_macros.hpp>

#include "model/cluster_state.hpp"

using namespace k10s;

namespace {

Pod makePod(std::string uid, PodPhase phase = PodPhase::Running) {
    Pod p;
    p.uid = std::move(uid);
    p.name = "pod-" + p.uid;
    p.namespace_ = "default";
    p.nodeName = "node-1";
    p.phase = phase;
    return p;
}

Node makeNode(std::string uid) {
    Node n;
    n.uid = std::move(uid);
    n.name = "node-" + n.uid;
    n.ready = true;
    return n;
}

} // namespace

TEST_CASE("empty state") {
    ClusterState s;
    CHECK(s.podCount() == 0);
    CHECK(s.nodeCount() == 0);
    CHECK(s.findPod("nope") == nullptr);
    CHECK(s.findNode("nope") == nullptr);
}

TEST_CASE("pod added then found") {
    ClusterState s;
    s.apply(PodEvent{EventType::Added, makePod("a")});

    REQUIRE(s.podCount() == 1);
    const Pod* p = s.findPod("a");
    REQUIRE(p != nullptr);
    CHECK(p->name == "pod-a");
}

TEST_CASE("pod modified overwrites by uid") {
    ClusterState s;
    s.apply(PodEvent{EventType::Added, makePod("a", PodPhase::Running)});
    s.apply(PodEvent{EventType::Modified, makePod("a", PodPhase::CrashLoopBackOff)});

    CHECK(s.podCount() == 1);
    CHECK(s.findPod("a")->phase == PodPhase::CrashLoopBackOff);
}

TEST_CASE("modified for unknown uid inserts") {
    ClusterState s;
    s.apply(PodEvent{EventType::Modified, makePod("a")});
    CHECK(s.podCount() == 1);
}

TEST_CASE("added for existing uid overwrites (relist semantics)") {
    ClusterState s;
    s.apply(PodEvent{EventType::Added, makePod("a", PodPhase::Pending)});
    s.apply(PodEvent{EventType::Added, makePod("a", PodPhase::Running)});

    CHECK(s.podCount() == 1);
    CHECK(s.findPod("a")->phase == PodPhase::Running);
}

TEST_CASE("pod deleted") {
    ClusterState s;
    s.apply(PodEvent{EventType::Added, makePod("a")});
    s.apply(PodEvent{EventType::Added, makePod("b")});
    s.apply(PodEvent{EventType::Deleted, makePod("a")});

    CHECK(s.podCount() == 1);
    CHECK(s.findPod("a") == nullptr);
    CHECK(s.findPod("b") != nullptr);
}

TEST_CASE("deleting unknown uid is a no-op") {
    ClusterState s;
    s.apply(PodEvent{EventType::Added, makePod("a")});
    s.apply(PodEvent{EventType::Deleted, makePod("ghost")});
    CHECK(s.podCount() == 1);
}

TEST_CASE("node lifecycle") {
    ClusterState s;
    s.apply(NodeEvent{EventType::Added, makeNode("n1")});
    REQUIRE(s.findNode("n1") != nullptr);
    CHECK(s.findNode("n1")->ready);

    Node notReady = makeNode("n1");
    notReady.ready = false;
    s.apply(NodeEvent{EventType::Modified, notReady});
    CHECK_FALSE(s.findNode("n1")->ready);

    s.apply(NodeEvent{EventType::Deleted, makeNode("n1")});
    CHECK(s.nodeCount() == 0);
}

TEST_CASE("pod and node maps are independent") {
    ClusterState s;
    s.apply(PodEvent{EventType::Added, makePod("same-uid")});
    s.apply(NodeEvent{EventType::Added, makeNode("same-uid")});
    s.apply(NodeEvent{EventType::Deleted, makeNode("same-uid")});

    CHECK(s.podCount() == 1);
    CHECK(s.nodeCount() == 0);
}

TEST_CASE("isHealthy") {
    CHECK(isHealthy(makePod("a", PodPhase::Running)));
    CHECK(isHealthy(makePod("a", PodPhase::Succeeded)));
    CHECK_FALSE(isHealthy(makePod("a", PodPhase::Failed)));
    CHECK_FALSE(isHealthy(makePod("a", PodPhase::CrashLoopBackOff)));
}
