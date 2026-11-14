#pragma once

#include <raylib.h>

#include <string>

namespace k10s {

struct HouseStyle {
    Color wall{}, roof{}, door{};
    float width{180.0f}, height{120.0f};
    float roofPitch{0.45f}; // roof height as a fraction of width
    int windows{2};         // 1 or 2
    bool chimney{true};
};

// Deterministic per-node look (seeded from the node's uid).
HouseStyle houseStyleFor(const std::string& uid);

// `base` is the bottom-centre of the house (where it meets the ground).
void drawHouse(Vector2 base, const HouseStyle& h, bool ready, float time);

// Total height including roof, used for layout.
inline float houseTotalHeight(const HouseStyle& h) { return h.height + h.width * h.roofPitch; }

} // namespace k10s
