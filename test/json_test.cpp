// Unit tests for save::Json — the hand-rolled JSON value type (see
// json.hpp: scoped to exactly what the save format needs, not a
// general-purpose parser).

#include <iostream>

#include "save/json.hpp"

namespace {

int g_failures = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAILED: " << #expr                                   \
                      << " at json_test.cpp:" << __LINE__ << "\n";             \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

} // namespace

int main() {
    using keepsake::save::Json;

    // Scalars round-trip through dump()/parse().
    {
        Json obj = Json::object();
        obj.set("str", "hello \"world\"\n");
        obj.set("num", 42);
        obj.set("frac", 3.5);
        obj.set("yes", true);
        obj.set("no", false);
        obj.set("nothing", nullptr);

        Json parsed;
        CHECK(Json::parse(obj.dump(), parsed));
        CHECK(parsed.isObject());
        CHECK(parsed.find("str") != nullptr);
        CHECK(parsed.find("str")->asString() == "hello \"world\"\n");
        CHECK(parsed.find("num")->asInt() == 42);
        CHECK(parsed.find("frac")->asNumber() == 3.5);
        CHECK(parsed.find("yes")->asBool() == true);
        CHECK(parsed.find("no")->asBool() == false);
        CHECK(parsed.find("nothing")->isNull());
        CHECK(parsed.find("absent") == nullptr);
    }

    // Objects preserve insertion order (vector of pairs, not a map).
    {
        Json obj = Json::object();
        obj.set("z", 1);
        obj.set("a", 2);
        obj.set("m", 3);

        Json parsed;
        CHECK(Json::parse(obj.dump(), parsed));
        std::string dumped = parsed.dump();
        CHECK(dumped.find("\"z\"") < dumped.find("\"a\""));
        CHECK(dumped.find("\"a\"") < dumped.find("\"m\""));
    }

    // set() overwrites an existing key rather than duplicating it.
    {
        Json obj = Json::object();
        obj.set("key", "first");
        obj.set("key", "second");
        CHECK(obj.find("key")->asString() == "second");
    }

    // Arrays round-trip, including empty and nested.
    {
        Json arr = Json::array();
        arr.push(Json("a"));
        arr.push(Json(1));
        Json nested = Json::object();
        nested.set("k", "v");
        arr.push(std::move(nested));

        Json parsed;
        CHECK(Json::parse(arr.dump(), parsed));
        CHECK(parsed.isArray());
        CHECK(parsed.items().size() == 3);
        CHECK(parsed.items()[0].asString() == "a");
        CHECK(parsed.items()[1].asInt() == 1);
        CHECK(parsed.items()[2].find("k")->asString() == "v");

        Json emptyArr = Json::array();
        Json parsedEmpty;
        CHECK(Json::parse(emptyArr.dump(), parsedEmpty));
        CHECK(parsedEmpty.isArray());
        CHECK(parsedEmpty.items().empty());
    }

    // Fallback values on type mismatch.
    {
        Json s("not a number");
        CHECK(s.asInt(-1) == -1);
        CHECK(s.asNumber(-1.0) == -1.0);
        CHECK(s.asBool(true) == true);

        Json n(5);
        CHECK(n.asString("fallback") == "fallback");
    }

    // Malformed input is rejected, not crashed on.
    {
        Json out;
        CHECK(!Json::parse("", out));
        CHECK(!Json::parse("{", out));
        CHECK(!Json::parse("{\"a\":}", out));
        CHECK(!Json::parse("[1,]", out));
        CHECK(!Json::parse("not json at all", out));
        // Trailing garbage after a complete value is rejected.
        CHECK(!Json::parse("{}{}", out));
        // \u escapes are deliberately unsupported.
        CHECK(!Json::parse("\"\\u0041\"", out));
    }

    // Numbers that are integral print without a decimal point.
    {
        Json n(42.0);
        CHECK(n.dump() == "42");
        Json f(3.5);
        CHECK(f.dump() == "3.5");
    }

    std::cout << "json_test " << (g_failures ? "FAILED" : "OK") << "\n";
    return g_failures ? 1 : 0;
}
