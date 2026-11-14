#include "source/kube_source.hpp"

#include <chrono>
#include <string>
#include <thread>

namespace k10s {

namespace {

PodPhase phaseFromString(const std::string& s) {
    if (s == "Running")
        return PodPhase::Running;
    if (s == "Succeeded")
        return PodPhase::Succeeded;
    if (s == "Failed")
        return PodPhase::Failed;
    return PodPhase::Pending; // "Pending", "Unknown", or anything new
}

void push(EventQueue& out, const Event& e, const std::stop_token& st) {
    while (!out.try_push(e) && !st.stop_requested())
        std::this_thread::yield();
}

// Sleeps in short steps so a stop request is noticed quickly.
void sleepFor(std::chrono::milliseconds total, const std::stop_token& st) {
    using namespace std::chrono_literals;
    for (auto slept = 0ms; slept < total && !st.stop_requested(); slept += 100ms)
        std::this_thread::sleep_for(100ms);
}

} // namespace

std::optional<Pod> parsePod(const nlohmann::json& j) {
    try {
        Pod p;
        const auto& meta = j.at("metadata");
        p.uid = meta.at("uid").get<std::string>();
        p.name = meta.at("name").get<std::string>();
        p.namespace_ = meta.value("namespace", "");

        if (j.contains("spec"))
            p.nodeName = j["spec"].value("nodeName", "");
        if (!j.contains("status"))
            return p;

        const auto& status = j["status"];
        p.phase = phaseFromString(status.value("phase", ""));

        // `phase` stays "Running" while a container crash-loops, so check each container.
        if (status.contains("containerStatuses")) {
            for (const auto& c : status["containerStatuses"]) {
                p.restarts += c.value("restartCount", 0);
                if (c.contains("state") && c["state"].contains("waiting") &&
                    c["state"]["waiting"].value("reason", "") == "CrashLoopBackOff") {
                    p.phase = PodPhase::CrashLoopBackOff;
                }
            }
        }
        return p;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

std::optional<Node> parseNode(const nlohmann::json& j) {
    try {
        Node n;
        const auto& meta = j.at("metadata");
        n.uid = meta.at("uid").get<std::string>();
        n.name = meta.at("name").get<std::string>();

        if (j.contains("status") && j["status"].contains("conditions")) {
            for (const auto& c : j["status"]["conditions"]) {
                if (c.value("type", "") == "Ready")
                    n.ready = c.value("status", "") == "True";
            }
        }
        return n;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

void KubeSource::run(std::stop_token st, EventQueue& out) {
    using namespace std::chrono_literals;
    while (!st.stop_requested()) {
        std::string rv = relist(st, out); // step A: snapshot + diff
        if (rv.empty()) {
            sleepFor(2s, st); // cluster unreachable: back off instead of hammering it
            continue;
        }
        while (!st.stop_requested() && watch(rv, st, out)) {
        } // step B: deltas; false -> gap or error -> relist
    }
}

std::string KubeSource::relist(const std::stop_token& st, EventQueue& out) {
    // --- nodes ---
    {
        auto res = http_.get("/api/v1/nodes", st);
        if (!res.ok())
            return "";
        const auto j = nlohmann::json::parse(res.body, nullptr, false);
        if (j.is_discarded() || !j.contains("items"))
            return "";

        std::unordered_set<std::string> seen;
        for (const auto& item : j["items"]) {
            if (auto node = parseNode(item)) {
                seen.insert(node->uid);
                push(out, NodeEvent{EventType::Added, std::move(*node)}, st);
            }
        }
        for (const auto& uid : knownNodes_)
            if (!seen.contains(uid))
                push(out, NodeEvent{EventType::Deleted, Node{.uid = uid}}, st);
        knownNodes_ = std::move(seen);
    }

    // --- pods ---
    std::string rv;
    {
        auto res = http_.get("/api/v1/pods", st);
        if (!res.ok())
            return "";
        const auto j = nlohmann::json::parse(res.body, nullptr, false);
        if (j.is_discarded() || !j.contains("items"))
            return "";

        std::unordered_set<std::string> seen;
        for (const auto& item : j["items"]) {
            if (auto pod = parsePod(item)) {
                seen.insert(pod->uid);
                push(out, PodEvent{EventType::Added, std::move(*pod)}, st);
            }
        }
        for (const auto& uid : knownPods_)
            if (!seen.contains(uid))
                push(out, PodEvent{EventType::Deleted, Pod{.uid = uid}}, st);
        knownPods_ = std::move(seen);

        if (j.contains("metadata"))
            rv = j["metadata"].value("resourceVersion", "");
    }
    return rv;
}

bool KubeSource::watch(std::string& rv, const std::stop_token& st, EventQueue& out) {
    bool gone = false;
    auto res = http_.stream("/api/v1/pods?watch=1&allowWatchBookmarks=true&resourceVersion=" + rv,
                            st, [&](std::string_view line) {
                                const auto ev = nlohmann::json::parse(line, nullptr, false);
                                if (ev.is_discarded() || !ev.contains("object"))
                                    return;
                                const std::string type = ev.value("type", "");
                                const auto& obj = ev["object"];

                                if (type == "ERROR") {
                                    if (obj.value("code", 0) == 410)
                                        gone = true;
                                    return;
                                }
                                if (obj.contains("metadata")) {
                                    rv = obj["metadata"].value("resourceVersion", rv);
                                }
                                if (type == "BOOKMARK")
                                    return;

                                auto pod = parsePod(obj);
                                if (!pod.has_value()) {
                                    return;
                                }

                                EventType t;
                                if (type == "ADDED") {
                                    t = EventType::Added;
                                    knownPods_.insert(pod->uid);
                                } else if (type == "MODIFIED") {
                                    t = EventType::Modified;
                                    knownPods_.insert(pod->uid);
                                } else if (type == "DELETED") {
                                    t = EventType::Deleted;
                                    knownPods_.erase(pod->uid);
                                } else {
                                    return;
                                }

                                push(out, PodEvent{t, std::move(*pod)}, st);
                            });
    // Relist on a 410 gap, a transport error, or an HTTP error status (403, 410, 5xx...).
    // A normal close (server-side watch timeout) is 200 with no error: re-watch from rv.
    if (gone || !res.ok())
        return false;
    return true;
}

} // namespace k10s
