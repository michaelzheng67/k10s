#include <catch2/catch_test_macros.hpp>

#include <string>

#include "render/house.hpp"
#include "world/layout.hpp"

using namespace k10s;

namespace {

ClusterState clusterWith(int nodes) {
    ClusterState s;
    for (int i = 0; i < nodes; ++i) {
        const std::string name = "node-" + std::to_string(i);
        s.apply(NodeEvent{EventType::Added, Node{.uid = "uid-" + name, .name = name, .ready = true}});
    }
    return s;
}

bool overlap(const Rectangle& a, const Rectangle& b) {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

const auto kTheme = makeTheme("village");

} // namespace

TEST_CASE("every node gets a house, and footprints never overlap") {
    VillageLayout layout;
    layout.update(clusterWith(30), *kTheme);
    const auto& houses = layout.houses();
    REQUIRE(houses.size() == 30);
    for (std::size_t i = 0; i < houses.size(); ++i)
        for (std::size_t j = i + 1; j < houses.size(); ++j)
            CHECK_FALSE(overlap(houses[i].footprint, houses[j].footprint));
}

TEST_CASE("same cluster -> same village") {
    VillageLayout a, b;
    a.update(clusterWith(10), *kTheme);
    b.update(clusterWith(10), *kTheme);
    for (const auto& h : a.houses()) {
        const auto* other = b.find(h.name);
        REQUIRE(other != nullptr);
        CHECK(other->base.x == h.base.x);
        CHECK(other->base.y == h.base.y);
    }
}

TEST_CASE("adding and removing nodes doesn't move existing houses") {
    VillageLayout layout;
    ClusterState s = clusterWith(5);
    layout.update(s, *kTheme);
    const Vector2 before = layout.find("node-2")->base;

    s.apply(NodeEvent{EventType::Added, Node{.uid = "uid-new", .name = "aaa-new", .ready = true}});
    s.apply(NodeEvent{EventType::Deleted, Node{.uid = "uid-node-0"}});
    layout.update(s, *kTheme);

    REQUIRE(layout.find("node-2") != nullptr);
    CHECK(layout.find("node-2")->base.x == before.x);
    CHECK(layout.find("node-2")->base.y == before.y);
    CHECK(layout.find("aaa-new") != nullptr);
    CHECK(layout.find("node-0") == nullptr);
    CHECK(layout.houses().size() == 5);
}

TEST_CASE("readiness changes propagate to the house") {
    VillageLayout layout;
    ClusterState s = clusterWith(1);
    layout.update(s, *kTheme);
    CHECK(layout.find("node-0")->ready);

    s.apply(NodeEvent{EventType::Modified, Node{.uid = "uid-node-0", .name = "node-0", .ready = false}});
    layout.update(s, *kTheme);
    CHECK_FALSE(layout.find("node-0")->ready);
}

TEST_CASE("bounds contain every footprint") {
    VillageLayout layout;
    layout.update(clusterWith(12), *kTheme);
    const Rectangle b = layout.bounds();
    for (const auto& h : layout.houses()) {
        CHECK(h.footprint.x >= b.x);
        CHECK(h.footprint.y >= b.y);
        CHECK(h.footprint.x + h.footprint.width <= b.x + b.width);
        CHECK(h.footprint.y + h.footprint.height <= b.y + b.height);
    }
}

TEST_CASE("house styles vary across nodes") {
    int twoStory = 0, hip = 0, brick = 0, withTrees = 0;
    for (int i = 0; i < 200; ++i) {
        const auto st = houseStyleFor("uid-" + std::to_string(i));
        twoStory += st.stories == 2;
        hip += st.roofShape == HouseStyle::Roof::Hip;
        brick += st.walls == HouseStyle::Walls::Brick;
        withTrees += st.treeCount > 0;
    }
    CHECK(twoStory > 20);
    CHECK(hip > 10);
    CHECK(brick > 20);
    CHECK(withTrees > 60);
}

TEST_CASE("every theme lays out without overlaps") {
    for (const auto& name : themeNames()) {
        const auto theme = makeTheme(name);
        REQUIRE(theme != nullptr);
        VillageLayout layout;
        layout.update(clusterWith(20), *theme);
        const auto& houses = layout.houses();
        REQUIRE(houses.size() == 20);
        for (std::size_t i = 0; i < houses.size(); ++i)
            for (std::size_t j = i + 1; j < houses.size(); ++j)
                CHECK_FALSE(overlap(houses[i].footprint, houses[j].footprint));
    }
}

TEST_CASE("unknown theme name") { CHECK(makeTheme("moon") == nullptr); }
