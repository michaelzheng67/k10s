#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

#include "model/cluster_state.hpp"
#include "source/fake_source.hpp"
#include "source/session.hpp"

using namespace k10s;
using namespace std::chrono_literals;

namespace {

// Drains a session until the expected number of snapshot events arrive (or timeout).
ClusterState drainSnapshot(Session& s, std::size_t expectedEvents) {
    ClusterState state;
    std::size_t got = 0;
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (got < expectedEvents && std::chrono::steady_clock::now() < deadline) {
        if (auto e = s.events().try_pop()) {
            state.apply(*e);
            ++got;
        } else {
            std::this_thread::yield();
        }
    }
    return state;
}

} // namespace

TEST_CASE("session streams the source's snapshot") {
    Session s{std::make_unique<FakeSource>(42, 3, 4)};
    const auto state = drainSnapshot(s, 3 + 12);
    CHECK(state.nodeCount() == 3);
    CHECK(state.podCount() == 12);
}

TEST_CASE("destroying a session stops its thread promptly") {
    auto s = std::make_unique<Session>(std::make_unique<FakeSource>(1, 2, 2));
    std::this_thread::sleep_for(50ms); // let it get into its sleep loop
    const auto start = std::chrono::steady_clock::now();
    s.reset();
    CHECK(std::chrono::steady_clock::now() - start < 500ms);
}

TEST_CASE("switching sessions: new snapshot, old thread gone") {
    auto s = std::make_unique<Session>(std::make_unique<FakeSource>(42, 6, 8));
    CHECK(drainSnapshot(*s, 6 + 48).podCount() == 48);

    s.reset();
    s = std::make_unique<Session>(std::make_unique<FakeSource>(7, 3, 5));
    const auto state = drainSnapshot(*s, 3 + 15);
    CHECK(state.nodeCount() == 3);
    CHECK(state.podCount() == 15);
}

TEST_CASE("destroying a session whose queue is full doesn't hang") {
    // 200 nodes x 30 pods = 6200 snapshot events > 4096 queue slots, and nobody pops:
    // the producer is stuck in its retry loop and must still notice the stop request.
    auto s = std::make_unique<Session>(std::make_unique<FakeSource>(3, 200, 30));
    std::this_thread::sleep_for(100ms);
    const auto start = std::chrono::steady_clock::now();
    s.reset();
    CHECK(std::chrono::steady_clock::now() - start < 500ms);
}
