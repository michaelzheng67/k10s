#pragma once

#include <cstdint>
#include <string_view>

namespace k10s {

// FNV-1a: tiny, fast, and stable across runs/platforms (unlike std::hash).
constexpr std::uint64_t fnv1a(std::string_view s) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (char c : s) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// splitmix64 finalizer: scrambles every input bit into every output bit.
// FNV-1a's low bits barely change for strings that differ only at the end
// ("pod-40" vs "pod-41"), so mix before slicing bits out of the hash.
constexpr std::uint64_t mix64(std::uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

} // namespace k10s
