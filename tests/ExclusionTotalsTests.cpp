#include "ExclusionTotals.h"
#include "Exclusion1170Layout.h"
#include "LightCallScan.h"
#include <iostream>
#include <limits>
#include <stdexcept>
void Check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
int main() {
    try {
        ExclusionTotals t;
        t.Add(0, 0.8f); t.Add(1, 0.6f); t.Add(2, 0.03f);
        auto f = t.Filter(t.all, 163.0f, 163.0f, 100);
        Check(f && std::abs(*f - 100) < 0.001f, "external and environment terms preserved");
        ExclusionTotals moved;
        moved.Add(0, 0.8f); moved.Add(1, 0.4f); moved.Add(2, 0.01f);
        f = moved.Filter(moved.all, 141, 141, 100);
        Check(f && std::abs(*f - 100) < 0.001f, "changing player/NPC contributions do not change environment estimate");
        ExclusionTotals off; off.Add(0, 0.8f);
        f = off.Filter(off.all, 100, 100, 100);
        Check(f && *f == 100, "all face lights off is identity");
        Check(!t.Filter(t.all + 0.1f, 163, 163, 100), "missed/foreign query rejected");
        Check(!t.Filter(t.all, 163, 120, 100), "cache mismatch rejected");
        Check(!t.Filter(t.all, 163, 163, 1), "unverified scale rejected");
        Check(!t.Filter(t.all, 10, 10, 100), "inconsistent negative environment rejected");
        t.Add(1, std::numeric_limits<float>::quiet_NaN());
        Check(!t.Filter(t.all, 163, 163, 100), "invalid light contribution rejected");
        ExclusionTotals empty;
        Check(empty.Filter(0, 22, 22, 100).value_or(-1) == 22, "no local lights still preserves ambient");
        auto q = Exclusion1170::GetLuminanceAtPoint;
        auto c = Exclusion1170::CalculateLightValue;
        Check(Exclusion1170::Matches(q, c), "captured layout accepted");
        auto sites = LightCallScan(q, 0x14A3A60, 0x1509E30);
        Check(sites && *sites == std::vector<std::size_t>{0x83, 0xF4}, "captured query has both expected light calls");
        sites = LightCallScan(c, 0x713710, 0x14A3A60);
        Check(sites && *sites == std::vector<std::size_t>{0x1D3}, "captured calculation has one paired query");
        sites = LightCallScan(c, 0x713710, 0x6EC8D0);
        Check(sites && *sites == std::vector<std::size_t>{0x2FC}, "captured calculation has one final store");
        c[0x1F4] ^= 0x20;
        Check(Exclusion1170::Matches(q, c), "existing external hook relocation may vary");
        c[0x2EE] ^= 1;
        Check(!Exclusion1170::Matches(q, c), "changed arithmetic rejected");
        c = Exclusion1170::CalculateLightValue;
        q[0x88] ^= 1;
        Check(!Exclusion1170::Matches(q, c), "changed aggregation rejected");
        std::cout << "Paired exclusion, changing lights, invalid sums/cache/scale and captured runtime layout passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
