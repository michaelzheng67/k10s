#pragma once

#include <raylib.h>

#include <array>
#include <string>

namespace k10s {

struct TreeSpec {
    float dx{0.0f};    // offset from the house's base-centre
    float dy{0.0f};
    float scale{1.0f};
    Color leaves{};
    bool pine{false};
};

struct HouseStyle {
    enum class Roof { Gable, Steep, Hip };
    enum class Walls { Plain, Planks, Brick };

    Color wall{}, roof{}, door{};
    float width{180.0f};
    float storyHeight{110.0f};
    int stories{1};       // 1 or 2
    Roof roofShape{Roof::Gable};
    Walls walls{Walls::Plain};
    float roofPitch{0.45f}; // roof height as a fraction of width
    int windowsPerStory{2}; // 1..3 (ground floor gives one slot to the door)
    bool chimney{true};
    bool shutters{false};
    bool flowerBoxes{false};
    bool archedDoor{false};
    int treeCount{0};
    std::array<TreeSpec, 2> trees{};

    float wallHeight() const { return storyHeight * static_cast<float>(stories); }
    float roofHeight() const { return width * roofPitch; }
    float totalHeight() const { return wallHeight() + roofHeight(); }
};

// Deterministic per-node look (seeded from the node's uid).
HouseStyle houseStyleFor(const std::string& uid);

// `base` is the bottom-centre of the house (where it meets the ground).
void drawHouse(Vector2 base, const HouseStyle& h, bool ready, float time);

void drawTree(Vector2 base, const TreeSpec& t, float time);

} // namespace k10s
