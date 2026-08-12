#ifndef KEEPSAKE_SAVE_JSON_HPP
#define KEEPSAKE_SAVE_JSON_HPP

#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace keepsake::save {

// A minimal JSON value, scoped to exactly what the save format needs:
// object, array, string, number, bool, null. Objects preserve insertion
// order (a vector of pairs, not a map) so a written save reads back in the
// same field order it was written in. This is not a general-purpose JSON
// library — extend it narrowly if a new field needs a type it doesn't
// support yet, rather than reaching for a third-party dependency.
class Json {
  public:
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;

    Json() : value_(nullptr) {}
    Json(std::nullptr_t) : value_(nullptr) {}
    Json(bool b) : value_(b) {}
    Json(double n) : value_(n) {}
    Json(int n) : value_(static_cast<double>(n)) {}
    Json(std::string s) : value_(std::move(s)) {}
    Json(const char *s) : value_(std::string(s)) {}
    Json(Array a) : value_(std::move(a)) {}
    Json(Object o) : value_(std::move(o)) {}

    static Json object();
    static Json array();

    bool isNull() const;
    bool isObject() const;
    bool isArray() const;

    // Requires isObject(). Overwrites an existing key if present.
    void set(const std::string &key, Json value);
    // Requires isObject(). Returns nullptr if the key is absent.
    const Json *find(const std::string &key) const;

    // Requires isArray().
    void push(Json value);
    // Requires isArray().
    const Array &items() const;

    std::string asString(const std::string &fallback = "") const;
    double asNumber(double fallback = 0.0) const;
    int asInt(int fallback = 0) const;
    bool asBool(bool fallback = false) const;

    // Pretty-prints with 2-space indentation.
    std::string dump() const;

    // Parses `text` into `out`. Returns false on malformed input (including
    // a bare `\u` escape, which this parser deliberately does not support —
    // save data never contains one). `out` is left in an unspecified state
    // on failure.
    static bool parse(const std::string &text, Json &out);

  private:
    void dumpTo(std::string &out, int depth) const;

    std::variant<std::nullptr_t, bool, double, std::string, Array, Object>
        value_;
};

} // namespace keepsake::save

#endif
