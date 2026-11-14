#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <stop_token>
#include <string>
#include <unordered_set>

#include "kube/http.hpp"
#include "model/types.hpp"
#include "source/data_source.hpp"

namespace k10s {

// Kubernetes JSON -> our model. Return nullopt (never throw) on malformed objects.
std::optional<Pod> parsePod(const nlohmann::json& j);
std::optional<Node> parseNode(const nlohmann::json& j);

// Streams a real cluster: LIST (snapshot) then WATCH (deltas), re-listing on gaps.
class KubeSource : public DataSource {
  public:
    explicit KubeSource(KubeContext ctx) : http_(std::move(ctx)) {}
    void run(std::stop_token st, EventQueue& out) override;

  private:
    KubeHttp http_;
    std::unordered_set<std::string> knownPods_;  // uids we've sent to the renderer
    std::unordered_set<std::string> knownNodes_;
};

} // namespace k10s
