#include "core/i18n.h"
#include "core/json.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace ompe {
namespace {

std::string to_lower_ascii(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

std::string to_upper_ascii(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    return out;
}

std::string primary_language(std::string_view code) {
    const size_t dash = code.find('-');
    return to_lower_ascii(dash == std::string_view::npos ? code : code.substr(0, dash));
}

std::string read_file_text(const fs::path& p) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

} // namespace

std::string normalize_locale_tag(std::string_view raw) {
    // Strip encoding / modifier suffixes like ".UTF-8" or "@euro"
    size_t end = raw.size();
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '.' || raw[i] == '@' || raw[i] == ' ') {
            end = i;
            break;
        }
    }
    std::string cleaned(raw.substr(0, end));
    for (char& c : cleaned) {
        if (c == '_') c = '-';
    }
    if (cleaned.empty()) return "en-US";

    const std::string low = to_lower_ascii(cleaned);
    if (low == "auto") return "auto";
    if (low == "c" || low == "posix") return "en-US";

    // Canonical default region mappings for bare 2-letter language tags
    if (low == "en") return "en-US";
    if (low == "zh" || low == "zh-hans" || low == "zh-hans-cn") return "zh-CN";
    if (low == "zh-hant" || low == "zh-hant-tw") return "zh-TW";
    if (low == "ja") return "ja-JP";
    if (low == "ko") return "ko-KR";
    if (low == "fr") return "fr-FR";
    if (low == "de") return "de-DE";
    if (low == "es") return "es-ES";
    if (low == "pt") return "pt-BR";
    if (low == "ru") return "ru-RU";

    // Canonical BCP-47 casing: lang (lower) - Script (Title, len 4) - REGION (UPPER, len 2)
    std::vector<std::string> parts;
    size_t pos = 0;
    while (pos <= cleaned.size()) {
        size_t next = cleaned.find('-', pos);
        if (next == std::string::npos) next = cleaned.size();
        if (next > pos) parts.push_back(cleaned.substr(pos, next - pos));
        pos = next + 1;
    }
    if (parts.empty()) return "en-US";

    std::string out = to_lower_ascii(parts[0]);
    for (size_t i = 1; i < parts.size(); ++i) {
        out.push_back('-');
        if (parts[i].size() == 2) {
            out += to_upper_ascii(parts[i]);
        } else if (parts[i].size() == 4) {
            std::string sc = to_lower_ascii(parts[i]);
            sc[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(sc[0])));
            out += sc;
        } else {
            out += to_lower_ascii(parts[i]);
        }
    }
    return out;
}

std::string detect_system_locale() {
#ifdef _WIN32
    wchar_t wname[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(wname, LOCALE_NAME_MAX_LENGTH) > 0) {
        std::string ascii;
        for (size_t i = 0; wname[i] != 0; ++i) {
            if (wname[i] < 128) ascii.push_back(static_cast<char>(wname[i]));
        }
        if (!ascii.empty()) return normalize_locale_tag(ascii);
    }
    const LANGID lid = GetUserDefaultUILanguage();
    if ((lid & 0x3FFu) == LANG_CHINESE) return "zh-CN";
    return "en-US";
#else
    const char* env = std::getenv("LC_ALL");
    if (!env || !*env) env = std::getenv("LC_MESSAGES");
    if (!env || !*env) env = std::getenv("LANG");
    if (env && *env) return normalize_locale_tag(env);
    return "en-US";
#endif
}

I18nCatalog::I18nCatalog() {
    system_locale_ = detect_system_locale();
    // Always seed minimal built-in en-US metadata so even without disk files, catalog stays valid.
    Bundle en;
    en.meta = {"en-US", "English", "English (US)", "", false, false, false};
    bundles_["en-US"] = std::move(en);
    rebuild_sorted_locales();
    set_locale("auto");
}

bool I18nCatalog::load_json_text(std::string_view json_text, bool user_supplied) {
    Json root;
    if (!Json::parse(json_text, root) || !root.is_object()) return false;

    const Json* meta_j = root.find("_meta");
    if (!meta_j || !meta_j->is_object()) return false;
    const std::string raw_code = (*meta_j)["code"].as_string_copy();
    if (raw_code.empty()) return false;

    const std::string code = normalize_locale_tag(raw_code);
    auto& bundle = bundles_[code];
    bundle.meta.code = code;

    const std::string name = (*meta_j)["name"].as_string_copy();
    if (!name.empty()) bundle.meta.native_name = name;
    else if (bundle.meta.native_name.empty()) bundle.meta.native_name = code;

    const std::string en_name = (*meta_j)["englishName"].as_string_copy();
    if (!en_name.empty()) bundle.meta.english_name = en_name;
    else if (bundle.meta.english_name.empty()) bundle.meta.english_name = bundle.meta.native_name;

    const std::string fb = (*meta_j)["fallback"].as_string_copy();
    if (!fb.empty()) bundle.meta.fallback = normalize_locale_tag(fb);
    else if (bundle.meta.fallback.empty() && code != "en-US") bundle.meta.fallback = "en-US";

    if (const Json* cjk = meta_j->find("cjkSpacing"); cjk && cjk->is_bool()) {
        bundle.meta.cjk_spacing = cjk->as_bool(false);
    }
    if (const Json* rtl = meta_j->find("rtl"); rtl && rtl->is_bool()) {
        bundle.meta.rtl = rtl->as_bool(false);
    }
    if (user_supplied) {
        bundle.meta.user_supplied = true;
    }

    if (const Json* strings = root.find("strings"); strings && strings->is_object()) {
        for (const auto& [k, v] : strings->as_object()) {
            if (v.is_string()) {
                bundle.strings[k] = v.as_string_copy();
            }
        }
    }

    rebuild_sorted_locales();
    rebuild_active_table();
    return true;
}

void I18nCatalog::load_directories(const std::vector<std::string>& dirs, int user_dir_start_index) {
    std::error_code ec;
    for (size_t di = 0; di < dirs.size(); ++di) {
        const std::string& dir = dirs[di];
        if (dir.empty()) continue;
        fs::path dp(dir);
        if (!fs::is_directory(dp, ec)) continue;

        const bool is_user = (user_dir_start_index >= 0 && static_cast<int>(di) >= user_dir_start_index);
        std::vector<fs::path> files;
        for (const auto& entry : fs::directory_iterator(dp, ec)) {
            if (ec) break;
            if (!entry.is_regular_file(ec)) continue;
            if (entry.path().extension() == ".json") {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());
        for (const auto& fp : files) {
            const std::string txt = read_file_text(fp);
            if (!txt.empty()) {
                load_json_text(txt, is_user);
            }
        }
    }
    set_locale(requested_pref_);
}

void I18nCatalog::rebuild_sorted_locales() {
    sorted_locales_.clear();
    sorted_locales_.reserve(bundles_.size());
    for (const auto& [_, b] : bundles_) {
        sorted_locales_.push_back(b.meta);
    }
    std::sort(sorted_locales_.begin(), sorted_locales_.end(), [](const LocaleMeta& a, const LocaleMeta& b) {
        auto rank = [](const std::string& c) -> int {
            if (c == "en-US") return 0;
            if (c == "zh-CN") return 1;
            return 2;
        };
        const int ra = rank(a.code);
        const int rb = rank(b.code);
        if (ra != rb) return ra < rb;
        return a.code < b.code;
    });
}

std::string I18nCatalog::negotiate_code(std::string_view raw) const {
    std::string target = normalize_locale_tag(raw);
    if (target == "auto" || target.empty()) {
        target = system_locale_.empty() ? "en-US" : system_locale_;
    }

    // 1. Exact BCP-47 match
    if (bundles_.find(target) != bundles_.end()) {
        return target;
    }

    // 2. Primary language prefix match (e.g. "zh-SG" -> "zh-CN", "en-GB" -> "en-US")
    const std::string lang = primary_language(target);
    for (const auto& meta : sorted_locales_) {
        if (primary_language(meta.code) == lang) {
            return meta.code;
        }
    }

    // 3. Default base fallback
    return "en-US";
}

void I18nCatalog::rebuild_active_table() {
    active_strings_.clear();

    // Build fallback chain from active -> fallback -> ... -> en-US (protect against cycles)
    std::vector<std::string> chain;
    std::string cur = active_meta_.code;
    for (int depth = 0; depth < 6 && !cur.empty(); ++depth) {
        if (std::find(chain.begin(), chain.end(), cur) != chain.end()) break;
        chain.push_back(cur);
        auto it = bundles_.find(cur);
        if (it == bundles_.end()) break;
        cur = it->second.meta.fallback;
    }
    if (std::find(chain.begin(), chain.end(), "en-US") == chain.end()) {
        chain.push_back("en-US");
    }

    // Apply from lowest-priority fallback up to highest-priority active locale
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        auto bit = bundles_.find(*it);
        if (bit == bundles_.end()) continue;
        for (const auto& [k, v] : bit->second.strings) {
            active_strings_[k] = v;
        }
    }
}

bool I18nCatalog::set_locale(std::string_view pref_or_code) {
    requested_pref_ = pref_or_code.empty() ? "auto" : normalize_locale_tag(pref_or_code);
    const std::string resolved = negotiate_code(requested_pref_);
    auto it = bundles_.find(resolved);
    if (it != bundles_.end()) {
        active_meta_ = it->second.meta;
    } else {
        active_meta_ = {"en-US", "English", "English (US)", "", false, false, false};
    }
    rebuild_active_table();
    return it != bundles_.end();
}

const char* I18nCatalog::get(std::string_view key, const char* fallback_en) const noexcept {
    if (!key.empty()) {
        auto it = active_strings_.find(key);
        if (it != active_strings_.end()) {
            return it->second.c_str();
        }
    }
    return fallback_en ? fallback_en : "";
}

std::string I18nCatalog::format(std::string_view key, const char* fallback_en,
                                std::initializer_list<std::pair<std::string_view, std::string_view>> args) const {
    const std::string_view tpl(get(key, fallback_en));
    std::string out;
    out.reserve(tpl.size() + 32);

    size_t i = 0;
    while (i < tpl.size()) {
        if (tpl[i] == '{') {
            const size_t close = tpl.find('}', i + 1);
            if (close != std::string_view::npos) {
                const std::string_view name = tpl.substr(i + 1, close - i - 1);
                bool replaced = false;
                for (const auto& [k, v] : args) {
                    if (k == name) {
                        out.append(v.data(), v.size());
                        replaced = true;
                        break;
                    }
                }
                if (replaced) {
                    i = close + 1;
                    continue;
                }
            }
        }
        out.push_back(tpl[i]);
        ++i;
    }
    return out;
}

} // namespace ompe
