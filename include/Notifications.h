#pragma once
#include "Localization.h"
#include <string_view>
#include <string>
namespace Notifications {
    void Show(const Localization::Text& text, std::string_view detail = {});
    std::string Snapshot();
}
