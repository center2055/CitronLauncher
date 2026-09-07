#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace citron::json {

class Value;
using Array = std::vector<Value>;
using Object = std::vector<std::pair<std::string, Value>>;

class Value {
public:
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object,
    };

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : data_(b) {}
    Value(double d) : data_(d) {}
    Value(int i) : data_(static_cast<double>(i)) {}
    Value(unsigned int i) : data_(static_cast<double>(i)) {}
    Value(std::int64_t i) : data_(static_cast<double>(i)) {}
    Value(std::uint64_t i) : data_(static_cast<double>(i)) {}
    Value(std::string s) : data_(std::move(s)) {}
    Value(std::string_view s) : data_(std::string(s)) {}
    Value(const char* s) : data_(std::string(s)) {}
    Value(Array a) : data_(std::move(a)) {}
    Value(Object o) : data_(std::move(o)) {}

    Type type() const;
    bool isNull() const { return type() == Type::Null; }
    bool isBool() const { return type() == Type::Bool; }
    bool isNumber() const { return type() == Type::Number; }
    bool isString() const { return type() == Type::String; }
    bool isArray() const { return type() == Type::Array; }
    bool isObject() const { return type() == Type::Object; }

    bool asBool(bool fallback = false) const;
    double asNumber(double fallback = 0.0) const;
    std::int64_t asInt(std::int64_t fallback = 0) const;
    std::uint64_t asUnsigned(std::uint64_t fallback = 0) const;
    const std::string& asString() const;
    std::string asString(std::string_view fallback) const;
    const Array& asArray() const;
    const Object& asObject() const;
    Array& asArray();
    Object& asObject();

    const Value* find(std::string_view key) const;
    const Value& operator[](std::string_view key) const;
    Value& set(std::string key, Value value);
    Value& push(Value value);
    size_t size() const;

private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> data_;
};

std::expected<Value, std::string> parse(std::string_view text);
std::string serialize(const Value& value, bool pretty = true);

}
