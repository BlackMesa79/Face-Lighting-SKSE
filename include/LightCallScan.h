#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>
#include "hde64.h"

// Decode boundaries before considering E8 bytes. Never match an opcode inside an operand.
inline std::optional<std::vector<std::size_t>> LightCallScan(
    std::span<const std::uint8_t> code, std::uintptr_t base, std::uintptr_t target) {
    if (code.empty() || code[0] == 0xE9 || code[0] == 0xEB ||
        (code.size() >= 2 && code[0] == 0xFF && code[1] == 0x25)) return std::nullopt;
    std::vector<std::size_t> sites;
    for (std::size_t pos = 0; pos < code.size();) {
        std::array<std::uint8_t, 32> padded{};
        const auto size = (std::min)(padded.size(), code.size() - pos);
        std::memcpy(padded.data(), code.data() + pos, size);
        hde64s instruction{};
        const auto length = hde64_disasm(padded.data(), &instruction);
        if (!length || instruction.flags & F_ERROR || length > size) return std::nullopt;
        if (length == 5 && instruction.opcode == 0xE8 && code[pos] == 0xE8) {
            std::int32_t displacement;
            std::memcpy(&displacement, code.data() + pos + 1, sizeof(displacement));
            if (base + pos + 5 + displacement == target) sites.push_back(pos);
        }
        pos += length;
    }
    return sites;
}
