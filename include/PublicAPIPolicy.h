#pragma once
#include "FaceLightingAPI.h"
#include <algorithm>
#include <cstring>
#include <string_view>

namespace PublicAPIPolicy {
    using FaceLightingAPI::Result;
    inline Result ValidateToken(const FaceLightingAPI::Token& token, std::uint64_t session) {
        if (token.session != session) return Result::StaleSession;
        if (!token.handle || !token.formID) return Result::InvalidTarget;
        return Result::Ok;
    }
    inline void CopyName(char (&destination)[256], std::string_view text) {
        auto count = std::min(text.size(), sizeof(destination) - 1);
        if (count < text.size())
            while (count && (static_cast<unsigned char>(text[count]) & 0xC0) == 0x80) --count;
        if (count) std::memcpy(destination, text.data(), count);
        destination[count] = '\0';
    }
}
