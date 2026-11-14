#include "world/movement.hpp"

#include <algorithm>
#include <cmath>

namespace k10s {

namespace {

struct Tuning {
    float accel;    // strength of random push (px/s^2)
    float spring;   // pull back toward home (1/s^2)
    float damping;  // velocity decay (1/s)
    float maxSpeed; // px/s
};

constexpr Tuning kCalm{260.0f, 1.2f, 2.0f, 30.0f};
constexpr Tuning kPanic{1400.0f, 0.5f, 0.4f, 170.0f};

} // namespace

void updateVillagers(Villagers& v, float dt, std::mt19937& rng, Rectangle bounds) {
    std::uniform_real_distribution<float> jitter{-1.0f, 1.0f};

    // Hot loop: touches only pos / vel / home / onFire, the payoff of SoA.
    for (std::size_t i = 0; i < v.size(); ++i) {
        const Tuning& t = v.onFire[i] ? kPanic : kCalm;
        Vector2& p = v.pos[i];
        Vector2& vel = v.vel[i];
        const Vector2 h = v.home[i];

        vel.x += (jitter(rng) * t.accel + (h.x - p.x) * t.spring) * dt;
        vel.y += (jitter(rng) * t.accel + (h.y - p.y) * t.spring) * dt;

        const float damp = std::max(0.0f, 1.0f - t.damping * dt);
        vel.x *= damp;
        vel.y *= damp;

        const float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        if (speed > t.maxSpeed) {
            vel.x *= t.maxSpeed / speed;
            vel.y *= t.maxSpeed / speed;
        }

        p.x += vel.x * dt;
        p.y += vel.y * dt;

        // Bounce off the edges of the village.
        if (p.x < bounds.x || p.x > bounds.x + bounds.width) {
            vel.x = -vel.x;
            p.x = std::clamp(p.x, bounds.x, bounds.x + bounds.width);
        }
        if (p.y < bounds.y || p.y > bounds.y + bounds.height) {
            vel.y = -vel.y;
            p.y = std::clamp(p.y, bounds.y, bounds.y + bounds.height);
        }
    }
}

} // namespace k10s
