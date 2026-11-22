#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ompe {

std::string base64_encode(const uint8_t* data, size_t len);
std::string base64_encode(std::string_view text);
bool base64_decode(std::string_view encoded, std::vector<uint8_t>& out);
bool base64_decode_to_string(std::string_view encoded, std::string& out);
bool is_valid_utf8(std::string_view s) noexcept;

} // namespace ompe
