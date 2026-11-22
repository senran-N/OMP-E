#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ompe {

struct LocaleMeta {
    std::string code;          // Canonical BCP-47 tag, e.g. "en-US", "zh-CN", "ja-JP"
    std::string native_name;   // Displayed in pickers, e.g. "English", "简体中文", "日本語"
    std::string english_name;  // Searchable English name, e.g. "Simplified Chinese"
    std::string fallback;      // Optional fallback BCP-47 tag, default "en-US"
    bool cjk_spacing = false;  // Tighter CJK tracking instead of wide uppercase Latin letter-spacing
    bool rtl = false;          // Reserved for future right-to-left script layout support
    bool user_supplied = false;// True if loaded from or overridden by user locales directory
};

// Normalize locale identifiers such as "zh_CN.UTF-8", "zh", "en", "ja-jp" into canonical BCP-47 tags.
std::string normalize_locale_tag(std::string_view raw);

// Detect OS UI locale as a normalized BCP-47 tag (e.g. "zh-CN", "en-US", "ja-JP").
std::string detect_system_locale();

// UTF-8 safe truncation to <= max_bytes without splitting multi-byte code points.
inline std::string utf8_truncate_ellipsis(std::string_view s, size_t max_bytes) {
    if (s.size() <= max_bytes) return std::string(s);
    if (max_bytes <= 3) return "...";
    size_t cut = max_bytes - 3;
    while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0u) == 0x80u) {
        --cut;
    }
    std::string out(s.data(), cut);
    out += "...";
    return out;
}

// Transparent hash for zero-allocation std::string_view lookups into string maps.
struct TransparentStringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
    size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

// Extensible, data-driven BCP-47 i18n catalog.
// Loads bundled JSON locale packs (assets/locales/*.json) and user overrides (~/.omp/desktop/locales/*.json).
// Adding any new language (ja-JP, ko-KR, de-DE, fr-FR, etc.) requires ONLY dropping a JSON file — zero C++ changes.
class I18nCatalog {
public:
    I18nCatalog();

    // Load (and merge) locale JSON packs from directories in ascending priority order.
    // Later directories can both add new languages and override keys in earlier packs.
    void load_directories(const std::vector<std::string>& dirs, int user_dir_start_index = -1);

    // Load or merge a single locale pack from JSON text (useful for tests & embedded defaults).
    bool load_json_text(std::string_view json_text, bool user_supplied = false);

    // Activate a locale preference: "auto" (OS negotiation) or any BCP-47 tag (e.g. "zh-CN", "ja-JP").
    // Resolves via exact match -> language-prefix match -> fallback chain -> "en-US".
    bool set_locale(std::string_view pref_or_code);

    // Zero-allocation hot-path lookup returning stable const char* for the active locale.
    const char* get(std::string_view key, const char* fallback_en = "") const noexcept;

    // Parameterized lookup with named {placeholder} replacement (supports arbitrary word-order reordering).
    std::string format(std::string_view key, const char* fallback_en,
                       std::initializer_list<std::pair<std::string_view, std::string_view>> args) const;

    const LocaleMeta& active_meta() const noexcept { return active_meta_; }
    const std::string& active_code() const noexcept { return active_meta_.code; }
    const std::string& requested_pref() const noexcept { return requested_pref_; }
    const std::string& system_locale() const noexcept { return system_locale_; }
    const std::vector<LocaleMeta>& locales() const noexcept { return sorted_locales_; }
    bool is_cjk_spacing() const noexcept { return active_meta_.cjk_spacing; }
    bool is_rtl() const noexcept { return active_meta_.rtl; }

    void set_user_locales_dir(std::string dir) { user_locales_dir_ = std::move(dir); }
    const std::string& user_locales_dir() const noexcept { return user_locales_dir_; }

private:
    struct Bundle {
        LocaleMeta meta;
        std::unordered_map<std::string, std::string> strings;
    };

    void rebuild_sorted_locales();
    void rebuild_active_table();
    std::string negotiate_code(std::string_view raw) const;

    std::unordered_map<std::string, Bundle> bundles_;
    std::vector<LocaleMeta> sorted_locales_;
    std::unordered_map<std::string, std::string, TransparentStringHash, std::equal_to<>> active_strings_;
    LocaleMeta active_meta_{"en-US", "English", "English (US)", "", false, false, false};
    std::string requested_pref_ = "auto";
    std::string system_locale_;
    std::string user_locales_dir_;
};

} // namespace ompe
