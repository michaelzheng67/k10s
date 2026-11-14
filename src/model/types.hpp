#pragma once

#include <cstdint>
#include <string>

namespace k10s {

enum class PodPhase { Pending, Running, Succeeded, Failed, CrashLoopBackOff };

struct Node {
    std::string uid;
    std::string name;
    bool ready{false};
};

struct Pod {
    std::string uid;
    std::string name;
    std::string namespace_;
    std::string nodeName;
    PodPhase phase{PodPhase::Pending};
    int restarts{0};
};

inline bool isHealthy(const Pod& p) {
    return p.phase == PodPhase::Running || p.phase == PodPhase::Succeeded;
}

} // namespace k10s