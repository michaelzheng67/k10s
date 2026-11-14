#include "model/cluster_state.hpp"
#include "util/overloaded.hpp"

namespace k10s {

void ClusterState::apply(const Event& e) {
    std::visit(overloaded{
                   [this](const PodEvent& ev) {
                       switch (ev.type) {
                       case EventType::Added:
                       case EventType::Modified:
                           _pods[ev.pod.uid] = ev.pod;
                           break;
                       case EventType::Deleted:
                           _pods.erase(ev.pod.uid);
                           break;
                       }
                   },
                   [this](const NodeEvent& ev) {
                       switch (ev.type) {
                       case EventType::Added:
                       case EventType::Modified:
                           _nodes[ev.node.uid] = ev.node;
                           break;
                       case EventType::Deleted:
                           _nodes.erase(ev.node.uid);
                           break;
                       }
                   },
               },
               e);
}

} // namespace k10s