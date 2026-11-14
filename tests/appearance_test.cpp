#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

#include "render/appearance.hpp"

using namespace k10s;

namespace {
bool sameColor(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
} // namespace

TEST_CASE("same uid -> same appearance") {
    const Appearance a = appearanceFor("pod-17");
    const Appearance b = appearanceFor("pod-17");
    CHECK(sameColor(a.skin, b.skin));
    CHECK(sameColor(a.shirt, b.shirt));
    CHECK(sameColor(a.pants, b.pants));
    CHECK(sameColor(a.hair, b.hair));
    CHECK(a.hairStyle == b.hairStyle);
    CHECK(a.scale == b.scale);
}

TEST_CASE("appearance values stay in range") {
    for (int i = 0; i < 200; ++i) {
        const Appearance a = appearanceFor("pod-" + std::to_string(i));
        CHECK(a.hairStyle >= 0);
        CHECK(a.hairStyle <= 2);
        CHECK(a.scale >= 0.85f);
        CHECK(a.scale <= 1.15f);
        CHECK(a.shirt.a == 255);
    }
}

TEST_CASE("different uids give varied looks") {
    std::set<int> shirts;
    for (int i = 0; i < 50; ++i) {
        const Color c = appearanceFor("pod-" + std::to_string(i)).shirt;
        shirts.insert(c.r << 16 | c.g << 8 | c.b);
    }
    CHECK(shirts.size() > 40);
}

#include "util/hash.hpp"

TEST_CASE("fnv1a matches known reference values") {
    CHECK(fnv1a("") == 14695981039346656037ULL);
    CHECK(fnv1a("a") == 0xaf63dc4c8601ec8cULL);
}

TEST_CASE("mix64 spreads low bits for near-identical strings") {
    std::set<std::uint64_t> buckets;
    for (int i = 40; i < 48; ++i) {
        buckets.insert((mix64(fnv1a("pod-" + std::to_string(i))) & 0xFFFF) / 8192); // 8 buckets
    }
    CHECK(buckets.size() >= 4);
}
