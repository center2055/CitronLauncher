#include "check.h"

#include "core/Json.h"

using namespace citron;

TEST_CASE(json_parse_object) {
    auto v = json::parse(R"({"a": 1, "b": "two", "c": [true, false, null], "d": {"e": -2.5}})");
    CHECK(v.has_value());
    CHECK(v->isObject());
    CHECK_EQ((*v)["a"].asInt(), 1);
    CHECK_EQ((*v)["b"].asString(), "two");
    CHECK_EQ((*v)["c"].size(), 3u);
    CHECK((*v)["c"].asArray()[0].asBool());
    CHECK((*v)["c"].asArray()[2].isNull());
    CHECK_EQ((*v)["d"]["e"].asNumber(), -2.5);
    CHECK((*v)["missing"].isNull());
}

TEST_CASE(json_parse_strings) {
    auto v = json::parse(R"(["a\"b", "line\nbreak", "é中", "😀"])");
    CHECK(v.has_value());
    const auto& items = v->asArray();
    CHECK_EQ(items[0].asString(), "a\"b");
    CHECK_EQ(items[1].asString(), "line\nbreak");
    CHECK_EQ(items[2].asString(), "\xC3\xA9\xE4\xB8\xAD");
    CHECK_EQ(items[3].asString(), "\xF0\x9F\x98\x80");
}

TEST_CASE(json_parse_errors) {
    CHECK(!json::parse("").has_value());
    CHECK(!json::parse("{").has_value());
    CHECK(!json::parse("[1,]").has_value());
    CHECK(!json::parse("{\"a\" 1}").has_value());
    CHECK(!json::parse("\"unterminated").has_value());
    CHECK(!json::parse("tru").has_value());
    CHECK(!json::parse("1 2").has_value());
    std::string deep(100, '[');
    CHECK(!json::parse(deep).has_value());
}

TEST_CASE(json_round_trip) {
    json::Value v = json::Object{};
    v.set("name", "citron");
    v.set("size", std::uint64_t{2142892032});
    v.set("flag", true);
    json::Value list = json::Array{};
    list.push(1);
    list.push("x");
    v.set("list", std::move(list));
    const std::string text = json::serialize(v, false);
    CHECK_EQ(text, R"({"name":"citron","size":2142892032,"flag":true,"list":[1,"x"]})");
    auto back = json::parse(text);
    CHECK(back.has_value());
    CHECK_EQ((*back)["size"].asUnsigned(), 2142892032ull);
    CHECK_EQ((*back)["list"].size(), 2u);
}

TEST_CASE(json_bom_and_overwrite) {
    auto v = json::parse("\xEF\xBB\xBF{\"k\": 1}");
    CHECK(v.has_value());
    CHECK_EQ((*v)["k"].asInt(), 1);
    json::Value o = json::Object{};
    o.set("k", 1);
    o.set("k", 2);
    CHECK_EQ(o.size(), 1u);
    CHECK_EQ(o["k"].asInt(), 2);
}
