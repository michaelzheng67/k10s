#pragma once

#include <raylib.h>
#include <string>

namespace k10s {

struct Appearance {
    Color skin{235, 200, 160, 255};
    Color shirt{70, 110, 200, 255};
    Color pants{60, 60, 80, 255};
    Color hair{60, 40, 25, 255};
    int hairStyle{0}; // 0 = short, 1 = long, 2 = bald
    float scale{1.0f};
};

Appearance appearanceFor(const std::string& uid);

} // namespace k10s
