#include <catch2/catch_test_macros.hpp>

#include "world/villagers.hpp"

using namespace k10s;

namespace {

void requireConsistent(const Villagers& v) {
    const auto n = v.size();
    REQUIRE(v.pos.size() == n);
    REQUIRE(v.vel.size() == n);
    REQUIRE(v.home.size() == n);
    REQUIRE(v.look.size() == n);
    REQUIRE(v.onFire.size() == n);
    REQUIRE(v.index.size() == n);
    for (std::size_t i = 0; i < n; ++i) {
        REQUIRE(v.index.at(v.uid[i]) == i);
    }
}

} // namespace

TEST_CASE("add keeps arrays in sync") {
    Villagers v;
    v.add("a", {10, 20}, {});
    v.add("b", {30, 40}, {});
    CHECK(v.size() == 2);
    requireConsistent(v);
    CHECK(v.pos[v.index.at("b")].x == 30.0f);
}

TEST_CASE("duplicate add is ignored") {
    Villagers v;
    v.add("a", {10, 20}, {});
    v.add("a", {99, 99}, {});
    CHECK(v.size() == 1);
    CHECK(v.home[0].x == 10.0f);
    requireConsistent(v);
}

TEST_CASE("remove middle swaps last into the hole") {
    Villagers v;
    v.add("a", {1, 0}, {});
    v.add("b", {2, 0}, {});
    v.add("c", {3, 0}, {});
    v.remove("a");

    CHECK(v.size() == 2);
    CHECK_FALSE(v.index.contains("a"));
    CHECK(v.home[v.index.at("c")].x == 3.0f);
    CHECK(v.home[v.index.at("b")].x == 2.0f);
    requireConsistent(v);
}

TEST_CASE("remove last and remove only") {
    Villagers v;
    v.add("a", {1, 0}, {});
    v.add("b", {2, 0}, {});
    v.remove("b");
    requireConsistent(v);
    v.remove("a");
    CHECK(v.size() == 0);
    requireConsistent(v);
}

TEST_CASE("remove unknown is a no-op") {
    Villagers v;
    v.add("a", {1, 0}, {});
    v.remove("ghost");
    CHECK(v.size() == 1);
    requireConsistent(v);
}

TEST_CASE("remove with an argument aliasing internal storage") {
    Villagers v;
    v.add("a", {1, 0}, {});
    v.add("b", {2, 0}, {});
    v.add("c", {3, 0}, {});
    v.remove(v.uid[0]); // reference into the vector we're mutating
    CHECK(v.size() == 2);
    CHECK_FALSE(v.index.contains("a"));
    requireConsistent(v);
}

TEST_CASE("many adds and removes stay consistent") {
    Villagers v;
    for (int i = 0; i < 100; ++i) v.add("p" + std::to_string(i), {}, {});
    for (int i = 0; i < 100; i += 3) v.remove("p" + std::to_string(i));
    CHECK(v.size() == 66);
    requireConsistent(v);
}

#include "model/cluster_state.hpp"

namespace {

Pod syncPod(const std::string& uid, const std::string& node, PodPhase phase = PodPhase::Running) {
    Pod p;
    p.uid = uid;
    p.name = uid;
    p.nodeName = node;
    p.phase = phase;
    return p;
}

const std::unordered_map<std::string, Vector2> kYards{{"n1", {100, 100}}, {"n2", {300, 100}}};

} // namespace

TEST_CASE("sync adds villagers for new pods at their yard") {
    ClusterState s;
    s.apply(PodEvent{EventType::Added, syncPod("a", "n1")});
    s.apply(PodEvent{EventType::Added, syncPod("b", "n2")});

    Villagers v;
    v.sync(s, kYards);
    CHECK(v.size() == 2);
    CHECK(v.home[v.index.at("b")].x == 300.0f);
    requireConsistent(v);
}

TEST_CASE("sync skips pods whose node has no yard") {
    ClusterState s;
    s.apply(PodEvent{EventType::Added, syncPod("a", "unknown-node")});
    Villagers v;
    v.sync(s, kYards);
    CHECK(v.size() == 0);
}

TEST_CASE("sync updates fire and removes deleted pods") {
    ClusterState s;
    s.apply(PodEvent{EventType::Added, syncPod("a", "n1")});
    s.apply(PodEvent{EventType::Added, syncPod("b", "n1")});
    Villagers v;
    v.sync(s, kYards);
    CHECK(v.onFire[v.index.at("a")] == 0);

    s.apply(PodEvent{EventType::Modified, syncPod("a", "n1", PodPhase::CrashLoopBackOff)});
    s.apply(PodEvent{EventType::Deleted, syncPod("b", "n1")});
    v.sync(s, kYards);

    CHECK(v.size() == 1);
    CHECK(v.onFire[v.index.at("a")] == 1);
    CHECK_FALSE(v.index.contains("b"));
    requireConsistent(v);
}

TEST_CASE("sync keeps position of existing villagers") {
    ClusterState s;
    s.apply(PodEvent{EventType::Added, syncPod("a", "n1")});
    Villagers v;
    v.sync(s, kYards);
    v.pos[0] = {555, 555};
    v.sync(s, kYards);
    CHECK(v.pos[0].x == 555.0f);
}
