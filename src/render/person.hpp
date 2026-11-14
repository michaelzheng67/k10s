#pragma once

#include <raylib.h>

#include "render/appearance.hpp"

namespace k10s {

// Draws a villager standing with their feet at `feet`.
void drawPerson(Vector2 feet, const Appearance& a, bool onFire, float time);

} // namespace k10s
