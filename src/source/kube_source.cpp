#include "source/kube_source.hpp"

#include <string>

namespace k10s {

namespace {

PodPhase phaseFromString(const std::string& s) {
    if (s == "Running") return PodPhase::Running;
    if (s == "Succeeded") return PodPhase::Succeeded;
    if (s == "Failed") return PodPhase::Failed;
    return PodPhase::Pending; // "Pending", "Unknown", or anything new
}

} // namespace

std::optional<Pod> parsePod(const nlohmann::json& j) {
    try {
        Pod p;
        const auto& meta = j.at("metadata");
        p.uid = meta.at("uid").get<std::string>();
        p.name = meta.at("name").get<std::string>();
        p.namespace_ = meta.value("namespace", "");

        if (j.contains("spec")) p.nodeName = j["spec"].value("nodeName", "");
        if (!j.contains("status")) return p;

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
                if (c.value("type", "") == "Ready") n.ready = c.value("status", "") == "True";
            }
        }
        return n;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

} // namespace k10s
