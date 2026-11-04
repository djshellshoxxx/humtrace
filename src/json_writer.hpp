// Purpose: minimal JSON scalar writers shared by report and comparison output (internal).
#pragma once

#include <cmath>
#include <ostream>
#include <string>

namespace humtrace::json {

inline void write_string(std::ostream& output, const std::string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    output << '"';
    for (const unsigned char byte : value) {
        switch (byte) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (byte < 0x20)
                output << "\\u00" << hex[byte >> 4] << hex[byte & 0x0f];
            else
                output << static_cast<char>(byte);
        }
    }
    output << '"';
}

inline void write_number(std::ostream& output, double value) {
    if (std::isfinite(value))
        output << value;
    else
        output << "null";
}

} // namespace humtrace::json
