#include "PlayerDialoguePolicy.h"
#include <iostream>
#include <stdexcept>
void Check(bool value) { if (!value) throw std::runtime_error("Player dialogue policy failed"); }
int main() {
    try {
        for (bool autoOn : {false, true}) for (bool autoOff : {false, true})
            for (bool initiallyOn : {false, true}) {
                PlayerDialoguePolicy state;
                Check(!state.Update(false, autoOn, autoOff, initiallyOn));
                auto request = state.Update(true, autoOn, autoOff, initiallyOn);
                Check(request.has_value() == (autoOn && !initiallyOn));
                bool enabled = request.value_or(initiallyOn);
                Check(!state.Update(true, autoOn, autoOff, enabled));
                request = state.Update(false, autoOn, autoOff, enabled);
                Check(request.has_value() == (autoOff && enabled));
                if (request) Check(!*request);
                enabled = request.value_or(enabled);
                Check(!state.Update(false, autoOn, autoOff, enabled));
            }
        PlayerDialoguePolicy state;
        Check(state.Update(true, true, true, false) == true);
        // A manual off during dialogue must not be undone every frame.
        Check(!state.Update(true, true, true, false));
        // On load, discard the old conversation rather than applying its close action.
        state.Reset();
        Check(!state.Update(false, true, true, true));
        Check(!state.Update(true, true, true, true));
        Check(state.Update(false, true, true, true) == false);
        std::cout << "Dialogue option combinations, manual changes and reset checks passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
