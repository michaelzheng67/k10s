#pragma once

#include <raylib.h>

#include <cstdint>
#include <vector>

namespace k10s {

struct Decoration {
    enum class Kind { Patch, Tuft, Flower } kind;
    Vector2 pos;
    float size;
    Color color;
};

// Generated once; the same seed always gives the same meadow.
std::vector<Decoration> makeScenery(std::uint64_t seed, Rectangle area);

void drawScenery(const std::vector<Decoration>& decor);

// A worn patch of dirt in front of a house where its villagers hang out.
void drawYard(Vector2 centre);

} // namespace k10s
