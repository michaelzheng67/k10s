#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "model/events.hpp"
#include "model/types.hpp"

namespace k10s {
class ClusterState {
    using Uid = std::string;
    std::unordered_map<Uid, Pod> _pods;
    std::unordered_map<Uid, Node> _nodes;

  public:
    void apply(const Event& e);
    const Pod* findPod(const std::string& uid) const {
        auto it = _pods.find(uid);
        return it == _pods.end() ? nullptr : &it->second;
    }
    const Node* findNode(const std::string& uid) const {
        auto it = _nodes.find(uid);
        return it == _nodes.end() ? nullptr : &it->second;
    }
    std::size_t podCount() const { return _pods.size(); }
    std::size_t nodeCount() const { return _nodes.size(); }
};
} // namespace k10s