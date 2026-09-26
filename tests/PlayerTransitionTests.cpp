#include "LightTransition.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
void Check(bool ok) { if (!ok) throw std::runtime_error("Player fade check failed"); }
int main() {
    try {
        LightTransition fade;
        fade.Update(true, true, 0.2f, 0);
        fade.Update(true, true, 0.2f, 0.1f);
        Check(std::abs(fade.value - 0.5f) < 0.001f);
        fade.Update(true, true, 0.2f, 0.1f);
        Check(fade.value == 1);
        fade = {};
        fade.Update(true, true, 0.5f, 0);
        Check(fade.value == 0);
        fade.Update(true, true, 0.5f, 0.25f);
        Check(std::abs(fade.value - 0.5f) < 0.001f);
        fade.Update(true, true, 0.5f, 0.25f);
        Check(fade.value == 1);
        fade.Update(false, true, 0.5f, 0);
        Check(fade.value == 1); // off must keep light alive for the fade.
        fade.Update(false, true, 0.5f, 0.25f);
        Check(std::abs(fade.value - 0.5f) < 0.001f);
        fade.Update(true, true, 0.5f, 0);
        Check(std::abs(fade.value - 0.5f) < 0.001f); // rapid reversal is continuous.
        fade.Update(true, true, 0.5f, 0.25f);
        Check(fade.value > 0.5f && fade.value < 1);
        auto paused = fade.value;
        fade.Update(true, true, 0.5f, 0);
        Check(fade.value == paused);
        fade.Update(false, false, 0.5f, 0);
        Check(fade.value == 0);
        fade.Update(true, true, 0, 0);
        Check(fade.value == 1);
        fade.Update(false, true, 0.5f, 0);
        fade.Update(false, true, 0.5f, 0.5f);
        Check(fade.value == 0);
        std::cout << "Player fade-in/out, reversal, pause, immediate mode and final removal passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
