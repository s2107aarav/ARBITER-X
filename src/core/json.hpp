#pragma once

#include <string>
#include <string_view>

namespace arbiter {

inline std::string json_quote(std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for (unsigned char ch : value) {
        if (ch == '"' || ch == '\\') {
            out += '\\';
            out += static_cast<char>(ch);
        } else if (ch < 0x20) {
            out += "\\u00";
            out += hex[ch >> 4];
            out += hex[ch & 15];
        } else {
            out += static_cast<char>(ch);
        }
    }
    out += '"';
    return out;
}

}  // namespace arbiter
