#pragma once

#include <variant>

#include "model/types.hpp"

namespace k10s {
enum class EventType { Added, Modified, Deleted };

struct PodEvent {
    EventType type;
    Pod pod;
};

struct NodeEvent {
    EventType type{EventType::Added};
    Node node;
};

using Event = std::variant<PodEvent, NodeEvent>;
} // namespace k10s