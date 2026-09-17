#pragma once
#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace SelectedNPCRecord {
    inline constexpr std::size_t limit = 32;
    struct Record { std::uint32_t id; std::uint32_t enabled; };
    static_assert(sizeof(Record) == 8);
    constexpr bool ValidLength(std::uint32_t length) { return length % sizeof(Record) == 0 && length <= limit * sizeof(Record); }
    template <class Resolver>
    void AppendResolved(std::vector<Record>& output, std::span<const Record> input, Resolver resolve) {
        for (const auto& row : input) {
            std::uint32_t id = 0;
            if (output.size() >= limit) break;
            if (row.enabled > 1 || !row.id || !resolve(row.id, id) || !id || id == 0x14) continue;
            if (std::none_of(output.begin(), output.end(), [&](const auto& entry) { return entry.id == id; })) output.push_back({id, row.enabled});
        }
    }
}
