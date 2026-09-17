#pragma once
#include "SKSEMenuFramework.h"

// Scope every style change so other mods keep their own appearance.
namespace MenuStyle {
    struct Scope {
        Scope() {
            using namespace ImGuiMCP;
            PushStyleColor(ImGuiCol_CheckMark, ImVec4{0.90f, 0.73f, 0.42f, 1});
            PushStyleColor(ImGuiCol_SliderGrab, ImVec4{0.75f, 0.57f, 0.30f, 1});
            PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4{0.95f, 0.79f, 0.47f, 1});
            PushStyleColor(ImGuiCol_Header, ImVec4{0.19f, 0.18f, 0.16f, 1});
            PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{0.30f, 0.26f, 0.19f, 1});
            PushStyleColor(ImGuiCol_HeaderActive, ImVec4{0.36f, 0.29f, 0.18f, 1});
            PushStyleColor(ImGuiCol_Button, ImVec4{0.28f, 0.24f, 0.17f, 1});
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.43f, 0.34f, 0.20f, 1});
            PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.52f, 0.40f, 0.22f, 1});
            PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{9, 6});
            PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{10, 9});
            PushItemWidth(GetContentRegionAvail().x * 0.52f);
        }
        ~Scope() {
            ImGuiMCP::PopItemWidth();
            ImGuiMCP::PopStyleVar(3);
            ImGuiMCP::PopStyleColor(9);
        }
    };
    inline void Heading(const char* label) {
        ImGuiMCP::Spacing();
        ImGuiMCP::TextColored({0.93f, 0.78f, 0.49f, 1}, "%s", label);
        ImGuiMCP::Separator();
        ImGuiMCP::Spacing();
    }
    inline bool Section(const char* label, bool open = true) {
        return ImGuiMCP::CollapsingHeader(label, open ? ImGuiMCP::ImGuiTreeNodeFlags_DefaultOpen : 0);
    }
}
