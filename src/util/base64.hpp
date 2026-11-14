#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace k10s {

// Standard (RFC 4648) base64 decode. Ignores whitespace; returns nullopt on bad input.
inline std::optional<std::string> base64Decode(std::string_view in) {
    constexpr auto table = [] {
        std::array<std::int8_t, 256> t{};
        t.fill(-1);
        constexpr std::string_view chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (std::size_t i = 0; i < chars.size(); ++i)
            t[static_cast<unsigned char>(chars[i])] = static_cast<std::int8_t>(i);
        return t;
    }();

    std::string out;
    out.reserve(in.size() * 3 / 4);
    std::uint32_t buf = 0;
    int bits = 0;
    for (char c : in) {
        if (c == '=' ) break;
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        const std::int8_t v = table[static_cast<unsigned char>(c)];
        if (v < 0) return std::nullopt;
        buf = (buf << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

} // namespace k10s
