#include <catch2/catch_test_macros.hpp>

#include <string>
#include <thread>

#include "util/spsc_queue.hpp"

using k10s::SpscQueue;

TEST_CASE("one producer, one consumer, 1M ints: nothing lost, duplicated, or reordered") {
    constexpr int kCount = 1'000'000;
    SpscQueue<int, 1024> q;

    // jthread joins automatically in its destructor, so a failing REQUIRE
    // (which throws) can't leave a running thread behind and std::terminate.
    std::jthread producer([&] {
        for (int i = 0; i < kCount; ++i) {
            while (!q.try_push(i)) {
                std::this_thread::yield(); // full: let the consumer catch up
            }
        }
    });

    int expected = 0;
    while (expected < kCount) {
        if (auto v = q.try_pop()) {
            REQUIRE(*v == expected);
            ++expected;
        }
    }
    CHECK_FALSE(q.try_pop().has_value());
}

TEST_CASE("strings survive the handoff intact (catches torn reads of slot contents)") {
    constexpr int kCount = 100'000;
    SpscQueue<std::string, 64> q; // small queue -> producer and consumer collide constantly

    std::jthread producer([&] {
        for (int i = 0; i < kCount; ++i) {
            std::string s = "event-" + std::to_string(i);
            while (!q.try_push(s)) {
                std::this_thread::yield();
            }
        }
    });

    for (int i = 0; i < kCount;) {
        if (auto v = q.try_pop()) {
            REQUIRE(*v == "event-" + std::to_string(i));
            ++i;
        }
    }
}
