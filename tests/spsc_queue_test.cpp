#include <catch2/catch_test_macros.hpp>

#include <string>

#include "util/spsc_queue.hpp"

using k10s::SpscQueue;

TEST_CASE("new queue is empty") {
    SpscQueue<int, 4> q;
    CHECK_FALSE(q.try_pop().has_value());
}

TEST_CASE("FIFO order") {
    SpscQueue<int, 8> q;
    for (int i = 0; i < 5; ++i) REQUIRE(q.try_push(i));
    for (int i = 0; i < 5; ++i) {
        auto v = q.try_pop();
        REQUIRE(v.has_value());
        CHECK(*v == i);
    }
    CHECK_FALSE(q.try_pop().has_value());
}

TEST_CASE("holds Capacity - 1 items (one slot is sacrificed)") {
    SpscQueue<int, 4> q;
    CHECK(q.try_push(1));
    CHECK(q.try_push(2));
    CHECK(q.try_push(3));
    CHECK_FALSE(q.try_push(4)); // full
    CHECK(q.try_pop() == 1);
    CHECK(q.try_push(4)); // room again
}

TEST_CASE("wraps around the ring many times") {
    SpscQueue<int, 4> q;
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(q.try_push(i));
        REQUIRE(q.try_push(i + 1));
        CHECK(q.try_pop() == i);
        CHECK(q.try_pop() == i + 1);
    }
    CHECK_FALSE(q.try_pop().has_value());
}

TEST_CASE("works with non-trivial types") {
    SpscQueue<std::string, 4> q;
    REQUIRE(q.try_push(std::string(100, 'x'))); // heap-allocated string
    auto v = q.try_pop();
    REQUIRE(v.has_value());
    CHECK(v->size() == 100);
}
