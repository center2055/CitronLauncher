#include "core/Json.h"

#include <charconv>
#include <cmath>
#include <format>

namespace citron::json {

namespace {

const std::string g_emptyString;
const Array g_emptyArray;
const Object g_emptyObject;
const Value g_nullValue;

constexpr int kMaxDepth = 64;

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    std::expected<Value, std::string> run() {
        skipSpace();
        auto value = parseValue(0);
        if (!value) {
            return value;
        }
        skipSpace();
        if (pos_ != text_.size()) {
            return fail("unexpected trailing characters");
        }
        return value;
    }

private:
    std::string_view text_;
    size_t pos_ = 0;

    std::unexpected<std::string> fail(std::string_view what) const {
        return std::unexpected(std::format("{} at offset {}", what, pos_));
    }

    void skipSpace() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool consume(std::string_view literal) {
        if (text_.substr(pos_, literal.size()) == literal) {
            pos_ += literal.size();
            return true;
        }
        return false;
    }

    std::expected<Value, std::string> parseValue(int depth) {
        if (depth > kMaxDepth) {
            return fail("nesting too deep");
        }
        if (pos_ >= text_.size()) {
            return fail("unexpected end of input");
        }
        const char c = text_[pos_];
        switch (c) {
        case '{': return parseObject(depth);
        case '[': return parseArray(depth);
        case '"': {
            auto s = parseString();
            if (!s) {
                return std::unexpected(s.error());
            }
            return Value(std::move(*s));
        }
        case 't':
            if (consume("true")) {
                return Value(true);
            }
            break;
        case 'f':
            if (consume("false")) {
                return Value(false);
            }
            break;
        case 'n':
            if (consume("null")) {
                return Value(nullptr);
            }
            break;
        default:
            if (c == '-' || (c >= '0' && c <= '9')) {
                return parseNumber();
            }
            break;
        }
        return fail("unexpected character");
    }

    std::expected<Value, std::string> parseNumber() {
        const size_t start = pos_;
        if (text_[pos_] == '-') {
            ++pos_;
        }
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                ++pos_;
            } else {
                break;
            }
        }
        double value = 0.0;
        const auto* begin = text_.data() + start;
        const auto* end = text_.data() + pos_;
        const auto result = std::from_chars(begin, end, value);
        if (result.ec != std::errc() || result.ptr != end || !std::isfinite(value)) {
            return fail("invalid number");
        }
        return Value(value);
    }

    static void appendUtf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    std::expected<std::uint32_t, std::string> parseHex4() {
        if (pos_ + 4 > text_.size()) {
            return fail("truncated escape");
        }
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            value <<= 4;
            if (c >= '0' && c <= '9') {
                value |= static_cast<std::uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                value |= static_cast<std::uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                value |= static_cast<std::uint32_t>(c - 'A' + 10);
            } else {
                return fail("invalid escape");
            }
        }
        return value;
    }

    std::expected<std::string, std::string> parseString() {
        ++pos_;
        std::string out;
        while (true) {
            if (pos_ >= text_.size()) {
                return fail("unterminated string");
            }
            const char c = text_[pos_++];
            if (c == '"') {
                return out;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return fail("control character in string");
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos_ >= text_.size()) {
                return fail("unterminated escape");
            }
            const char e = text_[pos_++];
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
                auto cp = parseHex4();
                if (!cp) {
                    return std::unexpected(cp.error());
                }
                std::uint32_t code = *cp;
                if (code >= 0xD800 && code <= 0xDBFF) {
                    if (!consume("\\u")) {
                        return fail("missing low surrogate");
                    }
                    auto low = parseHex4();
                    if (!low || *low < 0xDC00 || *low > 0xDFFF) {
                        return fail("invalid low surrogate");
                    }
                    code = 0x10000 + ((code - 0xD800) << 10) + (*low - 0xDC00);
                }
                appendUtf8(out, code);
                break;
            }
            default:
                return fail("invalid escape");
            }
        }
    }

    std::expected<Value, std::string> parseArray(int depth) {
        ++pos_;
        Array items;
        skipSpace();
        if (consume("]")) {
            return Value(std::move(items));
        }
        while (true) {
            skipSpace();
            auto item = parseValue(depth + 1);
            if (!item) {
                return item;
            }
            items.push_back(std::move(*item));
            skipSpace();
            if (consume(",")) {
                continue;
            }
            if (consume("]")) {
                return Value(std::move(items));
            }
            return fail("expected , or ]");
        }
    }

    std::expected<Value, std::string> parseObject(int depth) {
        ++pos_;
        Object members;
        skipSpace();
        if (consume("}")) {
            return Value(std::move(members));
        }
        while (true) {
            skipSpace();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                return fail("expected object key");
            }
            auto key = parseString();
            if (!key) {
                return std::unexpected(key.error());
            }
            skipSpace();
            if (!consume(":")) {
                return fail("expected :");
            }
            skipSpace();
            auto value = parseValue(depth + 1);
            if (!value) {
                return value;
            }
            members.emplace_back(std::move(*key), std::move(*value));
            skipSpace();
            if (consume(",")) {
                continue;
            }
            if (consume("}")) {
                return Value(std::move(members));
            }
            return fail("expected , or }");
        }
    }
};

void writeString(std::string& out, std::string_view s) {
    out.push_back('"');
    for (const char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out += std::format("\\u{:04x}", static_cast<unsigned char>(c));
            } else {
                out.push_back(c);
            }
        }
    }
    out.push_back('"');
}

void writeNumber(std::string& out, double d) {
    if (std::isfinite(d) && d == std::floor(d) && std::fabs(d) < 9007199254740992.0) {
        out += std::format("{}", static_cast<std::int64_t>(d));
    } else if (std::isfinite(d)) {
        out += std::format("{}", d);
    } else {
        out += "null";
    }
}

void writeIndent(std::string& out, int depth) {
    out.push_back('\n');
    out.append(static_cast<size_t>(depth) * 2, ' ');
}

void writeValue(std::string& out, const Value& value, bool pretty, int depth) {
    switch (value.type()) {
    case Value::Type::Null:
        out += "null";
        break;
    case Value::Type::Bool:
        out += value.asBool() ? "true" : "false";
        break;
    case Value::Type::Number:
        writeNumber(out, value.asNumber());
        break;
    case Value::Type::String:
        writeString(out, value.asString());
        break;
    case Value::Type::Array: {
        const auto& items = value.asArray();
        if (items.empty()) {
            out += "[]";
            break;
        }
        out.push_back('[');
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) {
                out.push_back(',');
            }
            if (pretty) {
                writeIndent(out, depth + 1);
            }
            writeValue(out, items[i], pretty, depth + 1);
        }
        if (pretty) {
            writeIndent(out, depth);
        }
        out.push_back(']');
        break;
    }
    case Value::Type::Object: {
        const auto& members = value.asObject();
        if (members.empty()) {
            out += "{}";
            break;
        }
        out.push_back('{');
        for (size_t i = 0; i < members.size(); ++i) {
            if (i > 0) {
                out.push_back(',');
            }
            if (pretty) {
                writeIndent(out, depth + 1);
            }
            writeString(out, members[i].first);
            out += pretty ? ": " : ":";
            writeValue(out, members[i].second, pretty, depth + 1);
        }
        if (pretty) {
            writeIndent(out, depth);
        }
        out.push_back('}');
        break;
    }
    }
}

}

Value::Type Value::type() const {
    return static_cast<Type>(data_.index());
}

bool Value::asBool(bool fallback) const {
    if (const auto* b = std::get_if<bool>(&data_)) {
        return *b;
    }
    return fallback;
}

double Value::asNumber(double fallback) const {
    if (const auto* d = std::get_if<double>(&data_)) {
        return *d;
    }
    return fallback;
}

std::int64_t Value::asInt(std::int64_t fallback) const {
    if (const auto* d = std::get_if<double>(&data_)) {
        return static_cast<std::int64_t>(*d);
    }
    return fallback;
}

std::uint64_t Value::asUnsigned(std::uint64_t fallback) const {
    if (const auto* d = std::get_if<double>(&data_)) {
        return *d < 0 ? fallback : static_cast<std::uint64_t>(*d);
    }
    return fallback;
}

const std::string& Value::asString() const {
    if (const auto* s = std::get_if<std::string>(&data_)) {
        return *s;
    }
    return g_emptyString;
}

std::string Value::asString(std::string_view fallback) const {
    if (const auto* s = std::get_if<std::string>(&data_)) {
        return *s;
    }
    return std::string(fallback);
}

const Array& Value::asArray() const {
    if (const auto* a = std::get_if<Array>(&data_)) {
        return *a;
    }
    return g_emptyArray;
}

const Object& Value::asObject() const {
    if (const auto* o = std::get_if<Object>(&data_)) {
        return *o;
    }
    return g_emptyObject;
}

Array& Value::asArray() {
    if (!std::holds_alternative<Array>(data_)) {
        data_ = Array{};
    }
    return std::get<Array>(data_);
}

Object& Value::asObject() {
    if (!std::holds_alternative<Object>(data_)) {
        data_ = Object{};
    }
    return std::get<Object>(data_);
}

const Value* Value::find(std::string_view key) const {
    if (const auto* o = std::get_if<Object>(&data_)) {
        for (const auto& [k, v] : *o) {
            if (k == key) {
                return &v;
            }
        }
    }
    return nullptr;
}

const Value& Value::operator[](std::string_view key) const {
    const Value* found = find(key);
    return found != nullptr ? *found : g_nullValue;
}

Value& Value::set(std::string key, Value value) {
    auto& members = asObject();
    for (auto& [k, v] : members) {
        if (k == key) {
            v = std::move(value);
            return v;
        }
    }
    members.emplace_back(std::move(key), std::move(value));
    return members.back().second;
}

Value& Value::push(Value value) {
    auto& items = asArray();
    items.push_back(std::move(value));
    return items.back();
}

size_t Value::size() const {
    if (const auto* a = std::get_if<Array>(&data_)) {
        return a->size();
    }
    if (const auto* o = std::get_if<Object>(&data_)) {
        return o->size();
    }
    return 0;
}

std::expected<Value, std::string> parse(std::string_view text) {
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }
    return Parser(text).run();
}

std::string serialize(const Value& value, bool pretty) {
    std::string out;
    writeValue(out, value, pretty, 0);
    if (pretty) {
        out.push_back('\n');
    }
    return out;
}

}
