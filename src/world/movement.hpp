#pragma once

#include <raylib.h>

#include <random>

#include "world/villagers.hpp"

namespace k10s {

// Healthy villagers amble around their yard; burning ones sprint around in a panic.
// Villagers are kept inside `bounds`.
void updateVillagers(Villagers& v, float dt, std::mt19937& rng, Rectangle bounds);

} // namespace k10s
