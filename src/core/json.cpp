#include "core/json.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace ompe {
namespace {

void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0x10FFFFu) {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

struct Parser {
    std::string_view src;
    size_t pos = 0;
    int depth = 0;
    std::string* err = nullptr;

    static constexpr int kMaxDepth = 256;

    bool fail(std::string_view msg) {
        if (err && err->empty()) {
            *err = std::string(msg) + " at offset " + std::to_string(pos);
        }
        return false;
    }

    void skip_ws() noexcept {
        while (pos < src.size()) {
            const char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos;
            } else {
                break;
            }
        }
    }

    bool parse_hex4(uint32_t& out) {
        if (pos + 4 > src.size()) return fail("truncated unicode escape");
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = src[pos++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<uint32_t>(c - 'A' + 10);
            else return fail("invalid hex digit in unicode escape");
        }
        out = v;
        return true;
    }

    bool parse_string(std::string& out) {
        if (pos >= src.size() || src[pos] != '"') return fail("expected '\"'");
        ++pos;
        out.clear();
        while (pos < src.size()) {
            const char c = src[pos++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20u) return fail("unescaped control character");
            if (c == '\\') {
                if (pos >= src.size()) return fail("truncated escape");
                const char e = src[pos++];
                switch (e) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        uint32_t u1 = 0;
                        if (!parse_hex4(u1)) return false;
                        if (u1 >= 0xD800u && u1 <= 0xDBFFu) {
                            if (pos + 2 <= src.size() && src[pos] == '\\' && src[pos + 1] == 'u') {
                                pos += 2;
                                uint32_t u2 = 0;
                                if (!parse_hex4(u2)) return false;
                                if (u2 >= 0xDC00u && u2 <= 0xDFFFu) {
                                    const uint32_t cp = 0x10000u + (((u1 - 0xD800u) << 10) | (u2 - 0xDC00u));
                                    append_utf8(out, cp);
                                } else {
                                    append_utf8(out, 0xFFFDu);
                                }
                            } else {
                                append_utf8(out, 0xFFFDu);
                            }
                        } else if (u1 >= 0xDC00u && u1 <= 0xDFFFu) {
                            append_utf8(out, 0xFFFDu);
                        } else {
                            append_utf8(out, u1);
                        }
                        break;
                    }
                    default:
                        return fail("invalid escape sequence");
                }
            } else {
                out.push_back(c);
            }
        }
        return fail("unterminated string");
    }

    bool parse_number(Json& out) {
        const size_t start = pos;
        if (src[pos] == '-') ++pos;
        if (pos >= src.size()) return fail("truncated number");
        if (src[pos] == '0') {
            ++pos;
        } else if (src[pos] >= '1' && src[pos] <= '9') {
            while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') ++pos;
        } else {
            return fail("invalid number");
        }

        bool is_float = false;
        if (pos < src.size() && src[pos] == '.') {
            is_float = true;
            ++pos;
            if (pos >= src.size() || src[pos] < '0' || src[pos] > '9') return fail("invalid fraction");
            while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') ++pos;
        }
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            is_float = true;
            ++pos;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) ++pos;
            if (pos >= src.size() || src[pos] < '0' || src[pos] > '9') return fail("invalid exponent");
            while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9') ++pos;
        }

        const std::string_view token = src.substr(start, pos - start);
        if (!is_float) {
            int64_t iv = 0;
            auto res = std::from_chars(token.data(), token.data() + token.size(), iv);
            if (res.ec == std::errc{} && res.ptr == token.data() + token.size()) {
                out = Json(iv);
                return true;
            }
        }
        std::string tmp(token);
        char* endp = nullptr;
        const double dv = std::strtod(tmp.c_str(), &endp);
        if (!endp || static_cast<size_t>(endp - tmp.c_str()) != tmp.size() || !std::isfinite(dv)) {
            return fail("number out of range");
        }
        out = Json(dv);
        return true;
    }

    bool parse_value(Json& out) {
        skip_ws();
        if (pos >= src.size()) return fail("unexpected end of input");
        if (++depth > kMaxDepth) return fail("max nesting depth exceeded");

        bool ok = false;
        const char c = src[pos];
        if (c == 'n') {
            if (src.substr(pos, 4) == "null") {
                pos += 4;
                out = Json(nullptr);
                ok = true;
            } else {
                ok = fail("invalid literal");
            }
        } else if (c == 't') {
            if (src.substr(pos, 4) == "true") {
                pos += 4;
                out = Json(true);
                ok = true;
            } else {
                ok = fail("invalid literal");
            }
        } else if (c == 'f') {
            if (src.substr(pos, 5) == "false") {
                pos += 5;
                out = Json(false);
                ok = true;
            } else {
                ok = fail("invalid literal");
            }
        } else if (c == '"') {
            std::string s;
            ok = parse_string(s);
            if (ok) out = Json(std::move(s));
        } else if (c == '[') {
            ++pos;
            Json::Array arr;
            skip_ws();
            if (pos < src.size() && src[pos] == ']') {
                ++pos;
                out = Json(std::move(arr));
                ok = true;
            } else {
                ok = true;
                while (ok) {
                    Json elem;
                    if (!parse_value(elem)) {
                        ok = false;
                        break;
                    }
                    arr.push_back(std::move(elem));
                    skip_ws();
                    if (pos >= src.size()) {
                        ok = fail("unterminated array");
                        break;
                    }
                    if (src[pos] == ']') {
                        ++pos;
                        break;
                    }
                    if (src[pos] != ',') {
                        ok = fail("expected ',' or ']' in array");
                        break;
                    }
                    ++pos;
                }
                if (ok) out = Json(std::move(arr));
            }
        } else if (c == '{') {
            ++pos;
            Json::Object obj;
            skip_ws();
            if (pos < src.size() && src[pos] == '}') {
                ++pos;
                out = Json(std::move(obj));
                ok = true;
            } else {
                ok = true;
                while (ok) {
                    skip_ws();
                    std::string key;
                    if (!parse_string(key)) {
                        ok = false;
                        break;
                    }
                    skip_ws();
                    if (pos >= src.size() || src[pos] != ':') {
                        ok = fail("expected ':' after object key");
                        break;
                    }
                    ++pos;
                    Json val;
                    if (!parse_value(val)) {
                        ok = false;
                        break;
                    }
                    obj.insert_or_assign(std::move(key), std::move(val));
                    skip_ws();
                    if (pos >= src.size()) {
                        ok = fail("unterminated object");
                        break;
                    }
                    if (src[pos] == '}') {
                        ++pos;
                        break;
                    }
                    if (src[pos] != ',') {
                        ok = fail("expected ',' or '}' in object");
                        break;
                    }
                    ++pos;
                }
                if (ok) out = Json(std::move(obj));
            }
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            ok = parse_number(out);
        } else {
            ok = fail("unexpected character");
        }

        --depth;
        return ok;
    }
};

void escape_string(std::string& out, std::string_view s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20u) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
}

void dump_impl(const Json& j, std::string& out, int indent, int level) {
    switch (j.type()) {
        case Json::Type::Null:
            out += "null";
            break;
        case Json::Type::Bool:
            out += j.as_bool() ? "true" : "false";
            break;
        case Json::Type::Int:
            out += std::to_string(j.as_int());
            break;
        case Json::Type::Double: {
            const double d = j.as_double();
            if (!std::isfinite(d)) {
                out += "null";
            } else {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.17g", d);
                out += buf;
            }
            break;
        }
        case Json::Type::String:
            escape_string(out, j.as_string());
            break;
        case Json::Type::Array: {
            const auto& arr = j.as_array();
            if (arr.empty()) {
                out += "[]";
                break;
            }
            out.push_back('[');
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) out.push_back(',');
                if (indent > 0) {
                    out.push_back('\n');
                    out.append(static_cast<size_t>((level + 1) * indent), ' ');
                }
                dump_impl(arr[i], out, indent, level + 1);
            }
            if (indent > 0) {
                out.push_back('\n');
                out.append(static_cast<size_t>(level * indent), ' ');
            }
            out.push_back(']');
            break;
        }
        case Json::Type::Object: {
            const auto& obj = j.as_object();
            if (obj.empty()) {
                out += "{}";
                break;
            }
            out.push_back('{');
            bool first = true;
            for (const auto& [k, v] : obj) {
                if (!first) out.push_back(',');
                first = false;
                if (indent > 0) {
                    out.push_back('\n');
                    out.append(static_cast<size_t>((level + 1) * indent), ' ');
                }
                escape_string(out, k);
                out += (indent > 0) ? ": " : ":";
                dump_impl(v, out, indent, level + 1);
            }
            if (indent > 0) {
                out.push_back('\n');
                out.append(static_cast<size_t>(level * indent), ' ');
            }
            out.push_back('}');
            break;
        }
    }
}

} // namespace

bool Json::parse(std::string_view input, Json& out, std::string* err) {
    if (err) err->clear();
    Parser p{input, 0, 0, err};
    Json parsed;
    if (!p.parse_value(parsed)) return false;
    p.skip_ws();
    if (p.pos != input.size()) return p.fail("trailing garbage after JSON value");
    out = std::move(parsed);
    return true;
}

std::string Json::dump(int indent) const {
    std::string out;
    out.reserve(128);
    dump_impl(*this, out, indent, 0);
    return out;
}

} // namespace ompe
