#include <catch2/catch_test_macros.hpp>

#include "source/kube_source.hpp"

using namespace k10s;
using nlohmann::json;

TEST_CASE("parsePod: crash-looping pod reports phase Running but is CrashLoopBackOff") {
    // Trimmed from a real `kubectl get pod -o json` on the kind cluster.
    const auto j = json::parse(R"({
      "metadata": {"name": "flaky-778c8dbcb6-9gq8x", "namespace": "village",
                   "uid": "e72822cf-1487-4fc8-b3d0-f59e0a23c1c3", "resourceVersion": "4626"},
      "spec": {"nodeName": "k10s-worker3"},
      "status": {"phase": "Running", "containerStatuses": [
        {"name": "flaky", "ready": false, "restartCount": 9,
         "state": {"waiting": {"reason": "CrashLoopBackOff", "message": "back-off 5m0s"}}}]}
    })");
    const auto p = parsePod(j);
    REQUIRE(p.has_value());
    CHECK(p->uid == "e72822cf-1487-4fc8-b3d0-f59e0a23c1c3");
    CHECK(p->name == "flaky-778c8dbcb6-9gq8x");
    CHECK(p->namespace_ == "village");
    CHECK(p->nodeName == "k10s-worker3");
    CHECK(p->restarts == 9);
    CHECK(p->phase == PodPhase::CrashLoopBackOff);
    CHECK_FALSE(isHealthy(*p));
}

TEST_CASE("parsePod: healthy running pod, restarts summed across containers") {
    const auto j = json::parse(R"({
      "metadata": {"name": "web-1", "namespace": "village", "uid": "u1"},
      "spec": {"nodeName": "k10s-worker"},
      "status": {"phase": "Running", "containerStatuses": [
        {"restartCount": 1, "state": {"running": {}}},
        {"restartCount": 2, "state": {"running": {}}}]}
    })");
    const auto p = parsePod(j);
    REQUIRE(p.has_value());
    CHECK(p->phase == PodPhase::Running);
    CHECK(p->restarts == 3);
    CHECK(isHealthy(*p));
}

TEST_CASE("parsePod: pending pod has no node and no container statuses") {
    const auto p = parsePod(json::parse(R"({
      "metadata": {"name": "new", "namespace": "default", "uid": "u2"},
      "spec": {}, "status": {"phase": "Pending"}
    })"));
    REQUIRE(p.has_value());
    CHECK(p->nodeName.empty());
    CHECK(p->phase == PodPhase::Pending);
}

TEST_CASE("parsePod: other phases") {
    auto withPhase = [](const char* phase) {
        return parsePod(json{{"metadata", {{"uid", "u"}, {"name", "n"}}},
                             {"status", {{"phase", phase}}}});
    };
    CHECK(withPhase("Succeeded")->phase == PodPhase::Succeeded);
    CHECK(withPhase("Failed")->phase == PodPhase::Failed);
    CHECK(withPhase("Unknown")->phase == PodPhase::Pending);
}

TEST_CASE("parsePod: malformed input returns nullopt instead of throwing") {
    CHECK_FALSE(parsePod(json::parse(R"({})")).has_value());
    CHECK_FALSE(parsePod(json::parse(R"({"metadata": {"name": "x"}})")).has_value()); // no uid
    CHECK_FALSE(parsePod(json::parse(R"({"metadata": {"uid": 5, "name": "x"}})")).has_value());
    CHECK_FALSE(parsePod(json::parse(R"([1, 2, 3])")).has_value());
}

TEST_CASE("parseNode: Ready condition") {
    const auto ready = parseNode(json::parse(R"({
      "metadata": {"name": "k10s-worker", "uid": "n1"},
      "status": {"conditions": [
        {"type": "MemoryPressure", "status": "False"},
        {"type": "Ready", "status": "True"}]}
    })"));
    REQUIRE(ready.has_value());
    CHECK(ready->name == "k10s-worker");
    CHECK(ready->ready);

    const auto notReady = parseNode(json::parse(R"({
      "metadata": {"name": "w2", "uid": "n2"},
      "status": {"conditions": [{"type": "Ready", "status": "Unknown"}]}
    })"));
    REQUIRE(notReady.has_value());
    CHECK_FALSE(notReady->ready);
}

TEST_CASE("parseNode: malformed input returns nullopt") {
    CHECK_FALSE(parseNode(json::parse(R"({"metadata": {}})")).has_value());
}
