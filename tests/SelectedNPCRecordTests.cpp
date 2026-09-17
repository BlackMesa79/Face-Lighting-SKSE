#include "SelectedNPCRecord.h"
#include <iostream>
#include <stdexcept>
void Check(bool value) { if (!value) throw std::runtime_error("Selected NPC record failed"); }
int main() {
    try {
        using namespace SelectedNPCRecord;
        Check(ValidLength(0) && ValidLength(256) && !ValidLength(257) && !ValidLength(7));
        std::vector<Record> output;
        const std::vector<Record> input{{0,1},{1,1},{1,0},{2,0},{3,2},{4,1},{5,1},{6,1}};
        AppendResolved(output, input, [](auto oldID, auto& newID) {
            if (oldID == 4) return false; // removed plugin
            newID = oldID == 5 ? 0x14 : oldID == 6 ? 0 : oldID + 100;
            return true;
        });
        Check(output.size() == 2 && output[0].id == 101 && output[0].enabled == 1 && output[1].id == 102 && output[1].enabled == 0);
        // Multiple records cannot evade capacity or remapped duplicate checks.
        std::vector<Record> large;
        for (unsigned i = 1000; i < 1100; ++i) large.push_back({i, 1});
        AppendResolved(output, large, [](auto id, auto& resolved) { resolved = id; return true; });
        Check(output.size() == limit);
        output.clear();
        AppendResolved(output, input, [](auto, auto& resolved) { resolved = 200; return true; });
        Check(output.size() == 1);
        std::cout << "Selected NPC record bounds, remapping, missing forms, invalid flags and deduplication passed.\n";
    } catch (const std::exception& error) { std::cerr << error.what(); return 1; }
}
