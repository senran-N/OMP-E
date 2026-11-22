#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ompe {

class Json {
public:
    enum class Type : uint8_t {
        Null = 0,
        Bool,
        Int,
        Double,
        String,
        Array,
        Object,
    };

    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json, std::less<>>;

    Json() noexcept : data_(std::monostate{}) {}
    Json(std::nullptr_t) noexcept : data_(std::monostate{}) {}
    Json(bool v) noexcept : data_(v) {}
    Json(int v) noexcept : data_(static_cast<int64_t>(v)) {}
    Json(int64_t v) noexcept : data_(v) {}
    Json(uint32_t v) noexcept : data_(static_cast<int64_t>(v)) {}
    Json(uint64_t v) noexcept : data_(static_cast<int64_t>(v)) {}
    Json(double v) noexcept : data_(v) {}
    Json(const char* s) : data_(s ? std::string(s) : std::string()) {}
    Json(std::string s) noexcept : data_(std::move(s)) {}
    Json(std::string_view sv) : data_(std::string(sv)) {}
    Json(Array a) noexcept : data_(std::move(a)) {}
    Json(Object o) noexcept : data_(std::move(o)) {}

    static Json array() { return Json(Array{}); }
    static Json object() { return Json(Object{}); }

    Type type() const noexcept { return static_cast<Type>(data_.index()); }
    bool is_null() const noexcept { return type() == Type::Null; }
    bool is_bool() const noexcept { return type() == Type::Bool; }
    bool is_int() const noexcept { return type() == Type::Int; }
    bool is_number() const noexcept { return type() == Type::Int || type() == Type::Double; }
    bool is_string() const noexcept { return type() == Type::String; }
    bool is_array() const noexcept { return type() == Type::Array; }
    bool is_object() const noexcept { return type() == Type::Object; }

    bool as_bool(bool fallback = false) const noexcept {
        return is_bool() ? std::get<bool>(data_) : fallback;
    }

    int64_t as_int(int64_t fallback = 0) const noexcept {
        if (is_int()) return std::get<int64_t>(data_);
        if (type() == Type::Double) return static_cast<int64_t>(std::get<double>(data_));
        return fallback;
    }

    double as_double(double fallback = 0.0) const noexcept {
        if (type() == Type::Double) return std::get<double>(data_);
        if (is_int()) return static_cast<double>(std::get<int64_t>(data_));
        return fallback;
    }

    const std::string& as_string() const noexcept {
        static const std::string kEmpty;
        return is_string() ? std::get<std::string>(data_) : kEmpty;
    }

    std::string as_string_copy(std::string_view fallback = "") const {
        return is_string() ? std::get<std::string>(data_) : std::string(fallback);
    }

    const Array& as_array() const noexcept {
        static const Array kEmpty;
        return is_array() ? std::get<Array>(data_) : kEmpty;
    }

    Array& as_array_mut() {
        if (!is_array()) data_ = Array{};
        return std::get<Array>(data_);
    }

    const Object& as_object() const noexcept {
        static const Object kEmpty;
        return is_object() ? std::get<Object>(data_) : kEmpty;
    }

    Object& as_object_mut() {
        if (!is_object()) data_ = Object{};
        return std::get<Object>(data_);
    }

    bool contains(std::string_view key) const noexcept {
        if (!is_object()) return false;
        const auto& obj = std::get<Object>(data_);
        return obj.find(key) != obj.end();
    }

    const Json* find(std::string_view key) const noexcept {
        if (!is_object()) return nullptr;
        const auto& obj = std::get<Object>(data_);
        auto it = obj.find(key);
        return (it != obj.end()) ? &it->second : nullptr;
    }

    const Json& operator[](std::string_view key) const noexcept {
        static const Json kNull;
        const Json* p = find(key);
        return p ? *p : kNull;
    }

    Json& operator[](const std::string& key) {
        return as_object_mut()[key];
    }

    void push_back(Json v) {
        as_array_mut().push_back(std::move(v));
    }

    size_t size() const noexcept {
        if (is_array()) return std::get<Array>(data_).size();
        if (is_object()) return std::get<Object>(data_).size();
        if (is_string()) return std::get<std::string>(data_).size();
        return 0;
    }

    bool empty() const noexcept { return size() == 0; }

    // Strict RFC 8259 parser (no exceptions thrown)
    static bool parse(std::string_view input, Json& out, std::string* err = nullptr);

    // Compact (indent == 0) or pretty-printed JSON serializer
    std::string dump(int indent = 0) const;

private:
    std::variant<std::monostate, bool, int64_t, double, std::string, Array, Object> data_;
};

} // namespace ompe
