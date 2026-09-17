#pragma once
#include <array>
namespace Hotkeys {
    inline constexpr const char* padNames[]{"Off", "D-pad Up", "D-pad Down", "D-pad Left", "D-pad Right",
        "Start", "Back", "LS", "RS", "LB", "RB", "A", "B", "X", "Y", "LT", "RT"};
    constexpr bool ValidPadCode(int code) { return code == 0 || (code >= 266 && code <= 281); }
    constexpr int PadIndex(int code) { return code >= 266 && code <= 281 ? code - 265 : 0; }
    constexpr int PadCode(int index) { return index >= 1 && index <= 16 ? index + 265 : 0; }
    constexpr bool PadMatch(int main, int modifier, unsigned pressed, bool modifierHeld) {
        return main != 0 && main != modifier && pressed == static_cast<unsigned>(main) &&
            (modifier == 0 || modifierHeld);
    }
    constexpr unsigned ModifierMask(int code) {
        return code == 42 || code == 54 ? 1u : code == 29 || code == 157 ? 2u :
            code == 56 || code == 184 ? 4u : 0u;
    }
    constexpr bool MatchesModifier(int modifier, unsigned held) {
        const unsigned expected = modifier >= 1 && modifier <= 3 ? 1u << (modifier - 1) : 0u;
        return held == expected;
    }
    inline constexpr std::array codes{0,30,48,46,32,18,33,34,35,23,36,37,38,50,49,24,25,16,19,31,20,22,47,17,45,21,44,59,60,61,62,63,64,65,66,67,68,87,88,2,3,4,5,6,7,8,9,10,11,210,211,199,207,201,209,200,208,203,205,82,79,80,81,75,76,77,71,72,73,83,78,74,55,181,156,57,28,14,15,58,69,70,12,13,26,27,39,40,43,51,52,53};
    inline constexpr const char* names[]{"Off", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","1","2","3","4","5","6","7","8","9","0","Insert","Delete","Home","End","Page Up","Page Down","Up","Down","Left","Right","Num 0","Num 1","Num 2","Num 3","Num 4","Num 5","Num 6","Num 7","Num 8","Num 9","Num .","Num +","Num -","Num *","Num /","Num Enter","Space","Enter","Backspace","Tab","Caps Lock","Num Lock","Scroll Lock","-","=","[","]","Semicolon","Apostrophe","Backslash",",",".","/"};
    static_assert(codes.size() == std::size(names));
    constexpr int Index(int code) {
        for (int i = 0; i < static_cast<int>(codes.size()); ++i)
            if (codes[i] == code) return i;
        return static_cast<int>(codes.size()); // Additional custom item, never Off.
    }
    constexpr int FromIndex(int index, int customCode) {
        return index >= 0 && index < static_cast<int>(codes.size()) ? codes[index] : customCode;
    }
    void Install();
}

