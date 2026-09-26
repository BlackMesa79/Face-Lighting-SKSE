#include "LightCallScan.h"
#include <iostream>
#include <stdexcept>
void Check(bool ok) { if (!ok) throw std::runtime_error("Call-site validation failed"); }
int main() {
    try {
        // mov eax, imm32 containing E8 must not be considered a call.
        std::array<std::uint8_t, 11> bytes{0xB8,0xE8,0,0,0,0xE8,0xF6,0x0F,0,0,0xC3};
        auto sites = LightCallScan(bytes, 0x1000, 0x2000);
        Check(sites && *sites == std::vector<std::size_t>{5});
        sites = LightCallScan(bytes, 0x1000, 0x2001);
        Check(sites && sites->empty());
        bytes[5] = 0x90;
        Check(!LightCallScan(std::span(bytes).first(4), 0x1000, 0x2000));
        const std::array<std::uint8_t, 5> jump{0xE9,0,0,0,0};
        Check(!LightCallScan(jump, 0x1000, 0x2000));
        const std::array<std::uint8_t, 6> indirectJump{0xFF,0x25,0,0,0,0};
        Check(!LightCallScan(indirectJump, 0x1000, 0x2000));
        const std::array<std::uint8_t, 5> backwards{0xE8,0xFB,0xEF,0xFF,0xFF};
        sites = LightCallScan(backwards, 0x2000, 0x1000);
        Check(sites && sites->size() == 1 && sites->front() == 0);
        std::cout << "Instruction boundaries, signed calls, truncated code and entry detours checked.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
