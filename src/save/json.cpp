#include "save/json.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>

namespace keepsake::save {

namespace {

void appendEscaped(std::string &out, const std::string &s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

std::string formatNumber(double n) {
    if (n == static_cast<long long>(n) && std::abs(n) < 1e15) {
        return std::to_string(static_cast<long long>(n));
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", n);
    return buf;
}

// Recursive-descent parser over a fixed input string.
class Parser {
  public:
    explicit Parser(const std::string &text) : text_(text), pos_(0) {}

    bool parseValue(Json &out) {
        skipWhitespace();
        if (pos_ >= text_.size()) return false;
        switch (text_[pos_]) {
            case '{':
                return parseObject(out);
            case '[':
                return parseArray(out);
            case '"':
                return parseString(out);
            case 't':
            case 'f':
                return parseBool(out);
            case 'n':
                return parseNull(out);
            default:
                return parseNumber(out);
        }
    }

    bool atEnd() {
        skipWhitespace();
        return pos_ >= text_.size();
    }

  private:
    const std::string &text_;
    size_t pos_;

    void skipWhitespace() {
        while (pos_ < text_.size() &&
               std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    bool consume(char expected) {
        skipWhitespace();
        if (pos_ >= text_.size() || text_[pos_] != expected) return false;
        ++pos_;
        return true;
    }

    bool parseObject(Json &out) {
        if (!consume('{')) return false;
        Json::Object obj;
        skipWhitespace();
        if (consume('}')) {
            out = Json(std::move(obj));
            return true;
        }
        for (;;) {
            skipWhitespace();
            Json keyJson;
            if (!parseString(keyJson)) return false;
            if (!consume(':')) return false;
            Json value;
            if (!parseValue(value)) return false;
            obj.emplace_back(keyJson.asString(), std::move(value));
            skipWhitespace();
            if (consume(',')) continue;
            if (consume('}')) break;
            return false;
        }
        out = Json(std::move(obj));
        return true;
    }

    bool parseArray(Json &out) {
        if (!consume('[')) return false;
        Json::Array arr;
        skipWhitespace();
        if (consume(']')) {
            out = Json(std::move(arr));
            return true;
        }
        for (;;) {
            Json value;
            if (!parseValue(value)) return false;
            arr.push_back(std::move(value));
            skipWhitespace();
            if (consume(',')) continue;
            if (consume(']')) break;
            return false;
        }
        out = Json(std::move(arr));
        return true;
    }

    bool parseString(Json &out) {
        skipWhitespace();
        if (pos_ >= text_.size() || text_[pos_] != '"') return false;
        ++pos_;
        std::string result;
        while (pos_ < text_.size() && text_[pos_] != '"') {
            char c = text_[pos_];
            if (c == '\\') {
                ++pos_;
                if (pos_ >= text_.size()) return false;
                switch (text_[pos_]) {
                    case '"':
                        result.push_back('"');
                        break;
                    case '\\':
                        result.push_back('\\');
                        break;
                    case '/':
                        result.push_back('/');
                        break;
                    case 'n':
                        result.push_back('\n');
                        break;
                    case 'r':
                        result.push_back('\r');
                        break;
                    case 't':
                        result.push_back('\t');
                        break;
                    default:
                        // \u escapes are deliberately unsupported — see
                        // json.hpp.
                        return false;
                }
                ++pos_;
            } else {
                result.push_back(c);
                ++pos_;
            }
        }
        if (pos_ >= text_.size()) return false;
        ++pos_; // closing quote
        out = Json(std::move(result));
        return true;
    }

    bool parseNumber(Json &out) {
        skipWhitespace();
        size_t start = pos_;
        if (pos_ < text_.size() && text_[pos_] == '-') ++pos_;
        while (pos_ < text_.size() &&
               (std::isdigit(static_cast<unsigned char>(text_[pos_])) ||
                text_[pos_] == '.' || text_[pos_] == 'e' ||
                text_[pos_] == 'E' || text_[pos_] == '+' ||
                text_[pos_] == '-')) {
            ++pos_;
        }
        if (pos_ == start) return false;
        try {
            out = Json(std::stod(text_.substr(start, pos_ - start)));
        } catch (...) {
            return false;
        }
        return true;
    }

    bool parseBool(Json &out) {
        if (text_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            out = Json(true);
            return true;
        }
        if (text_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            out = Json(false);
            return true;
        }
        return false;
    }

    bool parseNull(Json &out) {
        if (text_.compare(pos_, 4, "null") == 0) {
            pos_ += 4;
            out = Json(nullptr);
            return true;
        }
        return false;
    }
};

} // namespace

Json Json::object() {
    return Json(Object{});
}

Json Json::array() {
    return Json(Array{});
}

bool Json::isNull() const {
    return std::holds_alternative<std::nullptr_t>(value_);
}

bool Json::isObject() const {
    return std::holds_alternative<Object>(value_);
}

bool Json::isArray() const {
    return std::holds_alternative<Array>(value_);
}

void Json::set(const std::string &key, Json value) {
    auto &obj = std::get<Object>(value_);
    for (auto &entry : obj) {
        if (entry.first == key) {
            entry.second = std::move(value);
            return;
        }
    }
    obj.emplace_back(key, std::move(value));
}

const Json *Json::find(const std::string &key) const {
    const auto &obj = std::get<Object>(value_);
    for (const auto &entry : obj) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

void Json::push(Json value) {
    std::get<Array>(value_).push_back(std::move(value));
}

const Json::Array &Json::items() const {
    return std::get<Array>(value_);
}

std::string Json::asString(const std::string &fallback) const {
    if (auto *s = std::get_if<std::string>(&value_)) return *s;
    return fallback;
}

double Json::asNumber(double fallback) const {
    if (auto *n = std::get_if<double>(&value_)) return *n;
    return fallback;
}

int Json::asInt(int fallback) const {
    if (auto *n = std::get_if<double>(&value_)) {
        return static_cast<int>(*n);
    }
    return fallback;
}

bool Json::asBool(bool fallback) const {
    if (auto *b = std::get_if<bool>(&value_)) return *b;
    return fallback;
}

std::string Json::dump() const {
    std::string out;
    dumpTo(out, 0);
    return out;
}

void Json::dumpTo(std::string &out, int depth) const {
    const std::string indent(static_cast<size_t>(depth) * 2, ' ');
    const std::string childIndent(static_cast<size_t>(depth + 1) * 2, ' ');

    if (isNull()) {
        out += "null";
    } else if (auto *b = std::get_if<bool>(&value_)) {
        out += *b ? "true" : "false";
    } else if (auto *n = std::get_if<double>(&value_)) {
        out += formatNumber(*n);
    } else if (auto *s = std::get_if<std::string>(&value_)) {
        appendEscaped(out, *s);
    } else if (auto *arr = std::get_if<Array>(&value_)) {
        if (arr->empty()) {
            out += "[]";
            return;
        }
        out += "[\n";
        for (size_t i = 0; i < arr->size(); ++i) {
            out += childIndent;
            (*arr)[i].dumpTo(out, depth + 1);
            if (i + 1 < arr->size()) out += ",";
            out += "\n";
        }
        out += indent + "]";
    } else if (auto *obj = std::get_if<Object>(&value_)) {
        if (obj->empty()) {
            out += "{}";
            return;
        }
        out += "{\n";
        for (size_t i = 0; i < obj->size(); ++i) {
            out += childIndent;
            appendEscaped(out, (*obj)[i].first);
            out += ": ";
            (*obj)[i].second.dumpTo(out, depth + 1);
            if (i + 1 < obj->size()) out += ",";
            out += "\n";
        }
        out += indent + "}";
    }
}

bool Json::parse(const std::string &text, Json &out) {
    Parser parser(text);
    if (!parser.parseValue(out)) return false;
    return parser.atEnd();
}

} // namespace keepsake::save
