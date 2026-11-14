#include "render/appearance.hpp"

#include <array>
#include <cstdint>
#include <random>
#include <string_view>

namespace {

constexpr std::uint64_t fnv1a(std::string_view s) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (char c : s) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

constexpr std::array<Color, 5> kSkins{{
    {255, 220, 185, 255},
    {235, 190, 150, 255},
    {200, 150, 110, 255},
    {150, 100, 70, 255},
    {100, 65, 45, 255},
}};

constexpr std::array<Color, 6> kHair{{
    {30, 25, 20, 255},    // black
    {90, 55, 30, 255},    // brown
    {230, 200, 120, 255}, // blonde
    {170, 70, 30, 255},   // red
    {160, 160, 160, 255}, // gray
    {60, 40, 25, 255},    // dark brown
}};

template <std::size_t N> Color pick(const std::array<Color, N>& palette, std::mt19937_64& rng) {
    std::uniform_int_distribution<std::size_t> d{0, N - 1};
    return palette[d(rng)];
}

Color randomColor(std::mt19937_64& rng) {
    std::uniform_int_distribution<int> channel{40, 230};
    return Color{static_cast<unsigned char>(channel(rng)), static_cast<unsigned char>(channel(rng)),
                 static_cast<unsigned char>(channel(rng)), 255};
}

} // namespace

namespace k10s {

Appearance appearanceFor(const std::string& uid) {
    std::mt19937_64 rng{fnv1a(uid)};

    Appearance a;
    a.skin = pick(kSkins, rng);
    a.hair = pick(kHair, rng);
    a.shirt = randomColor(rng);
    a.pants = randomColor(rng);
    a.hairStyle = std::uniform_int_distribution<int>{0, 2}(rng);
    a.scale = std::uniform_real_distribution<float>{0.85f, 1.15f}(rng);
    return a;
}

} // namespace k10s
