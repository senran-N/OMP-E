#include "core/base64.h"

namespace ompe {
namespace {

constexpr char kEncTable[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int decode_char(unsigned char c) noexcept {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

} // namespace

std::string base64_encode(const uint8_t* data, size_t len) {
    if (!data || len == 0) return {};
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= len) {
        const uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
                           (static_cast<uint32_t>(data[i + 1]) << 8) |
                           static_cast<uint32_t>(data[i + 2]);
        out.push_back(kEncTable[(n >> 18) & 63]);
        out.push_back(kEncTable[(n >> 12) & 63]);
        out.push_back(kEncTable[(n >> 6) & 63]);
        out.push_back(kEncTable[n & 63]);
        i += 3;
    }
    const size_t rem = len - i;
    if (rem == 1) {
        const uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        out.push_back(kEncTable[(n >> 18) & 63]);
        out.push_back(kEncTable[(n >> 12) & 63]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
                           (static_cast<uint32_t>(data[i + 1]) << 8);
        out.push_back(kEncTable[(n >> 18) & 63]);
        out.push_back(kEncTable[(n >> 12) & 63]);
        out.push_back(kEncTable[(n >> 6) & 63]);
        out.push_back('=');
    }
    return out;
}

std::string base64_encode(std::string_view text) {
    return base64_encode(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

bool base64_decode(std::string_view encoded, std::vector<uint8_t>& out) {
    out.clear();
    if (encoded.empty()) return true;
    if ((encoded.size() % 4) != 0) return false;

    out.reserve((encoded.size() / 4) * 3);
    for (size_t i = 0; i < encoded.size(); i += 4) {
        const unsigned char c0 = static_cast<unsigned char>(encoded[i]);
        const unsigned char c1 = static_cast<unsigned char>(encoded[i + 1]);
        const unsigned char c2 = static_cast<unsigned char>(encoded[i + 2]);
        const unsigned char c3 = static_cast<unsigned char>(encoded[i + 3]);

        const int v0 = decode_char(c0);
        const int v1 = decode_char(c1);
        if (v0 < 0 || v1 < 0) return false;

        const bool pad2 = (c2 == '=');
        const bool pad3 = (c3 == '=');
        if (pad2 && !pad3) return false;
        if ((pad2 || pad3) && (i + 4 != encoded.size())) return false;

        const int v2 = pad2 ? 0 : decode_char(c2);
        const int v3 = pad3 ? 0 : decode_char(c3);
        if (v2 < 0 || v3 < 0) return false;

        const uint32_t n = (static_cast<uint32_t>(v0) << 18) |
                           (static_cast<uint32_t>(v1) << 12) |
                           (static_cast<uint32_t>(v2) << 6) |
                           static_cast<uint32_t>(v3);

        out.push_back(static_cast<uint8_t>((n >> 16) & 0xFFu));
        if (!pad2) out.push_back(static_cast<uint8_t>((n >> 8) & 0xFFu));
        if (!pad3) out.push_back(static_cast<uint8_t>(n & 0xFFu));
    }
    return true;
}

bool base64_decode_to_string(std::string_view encoded, std::string& out) {
    std::vector<uint8_t> bytes;
    if (!base64_decode(encoded, bytes)) return false;
    out.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

bool is_valid_utf8(std::string_view s) noexcept {
    size_t i = 0;
    while (i < s.size()) {
        const uint8_t b0 = static_cast<uint8_t>(s[i]);
        if (b0 <= 0x7Fu) {
            ++i;
        } else if (b0 >= 0xC2u && b0 <= 0xDFu) {
            if (i + 1 >= s.size()) return false;
            const uint8_t b1 = static_cast<uint8_t>(s[i + 1]);
            if ((b1 & 0xC0u) != 0x80u) return false;
            i += 2;
        } else if (b0 >= 0xE0u && b0 <= 0xEFu) {
            if (i + 2 >= s.size()) return false;
            const uint8_t b1 = static_cast<uint8_t>(s[i + 1]);
            const uint8_t b2 = static_cast<uint8_t>(s[i + 2]);
            if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u) return false;
            if (b0 == 0xE0u && b1 < 0xA0u) return false;
            if (b0 == 0xEDu && b1 >= 0xA0u) return false; // UTF-16 surrogates invalid in UTF-8
            i += 3;
        } else if (b0 >= 0xF0u && b0 <= 0xF4u) {
            if (i + 3 >= s.size()) return false;
            const uint8_t b1 = static_cast<uint8_t>(s[i + 1]);
            const uint8_t b2 = static_cast<uint8_t>(s[i + 2]);
            const uint8_t b3 = static_cast<uint8_t>(s[i + 3]);
            if ((b1 & 0xC0u) != 0x80u || (b2 & 0xC0u) != 0x80u || (b3 & 0xC0u) != 0x80u) return false;
            if (b0 == 0xF0u && b1 < 0x90u) return false;
            if (b0 == 0xF4u && b1 >= 0x90u) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

} // namespace ompe
