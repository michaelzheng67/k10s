#pragma once

#include <raylib.h>

#include "render/appearance.hpp"

namespace k10s {

// Draws a villager standing with their feet at `feet`, animated by `vel` and `time`.
void drawPerson(Vector2 feet, Vector2 vel, const Appearance& a, bool onFire, float time);

} // namespace k10s
