#pragma once

#include <cmath>
#include <cstdint>
#include <ostream>
#include <string>

namespace humtrace::json {

// Write a JSON string. Control characters are escaped; invalid UTF-8 bytes are replaced with
// U+FFFD so the output is always valid JSON (file names are not guaranteed to be UTF-8).
inline void write_string(std::ostream& out, const std::string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (std::size_t i = 0; i < value.size();) {
        const auto b = static_cast<unsigned char>(value[i]);
        if (b < 0x80) {
            switch (b) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (b < 0x20) out << "\\u00" << hex[b >> 4] << hex[b & 0x0f];
                else out << static_cast<char>(b);
            }
            ++i;
            continue;
        }
        std::size_t length = 0;
        std::uint32_t code = 0;
        if ((b & 0xE0) == 0xC0) { length = 2; code = b & 0x1F; }
        else if ((b & 0xF0) == 0xE0) { length = 3; code = b & 0x0F; }
        else if ((b & 0xF8) == 0xF0) { length = 4; code = b & 0x07; }
        bool valid = length != 0 && i + length <= value.size();
        for (std::size_t k = 1; valid && k < length; ++k) {
            const auto c = static_cast<unsigned char>(value[i + k]);
            valid = (c & 0xC0) == 0x80;
            code = (code << 6) | (c & 0x3F);
        }
        static constexpr std::uint32_t minimum[] = {0, 0, 0x80, 0x800, 0x10000};
        valid = valid && code >= minimum[length] && code <= 0x10FFFF && !(code >= 0xD800 && code <= 0xDFFF);
        if (valid) {
            out.write(value.data() + i, static_cast<std::streamsize>(length));
            i += length;
        } else {
            out << "\xEF\xBF\xBD";
            ++i;
        }
    }
    out << '"';
}

inline void write_number(std::ostream& out, double value) {
    if (std::isfinite(value)) out << value;
    else out << "null";
}

} // namespace humtrace::json
