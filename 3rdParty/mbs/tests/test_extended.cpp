// Extended suite: cparse-style cases adapted for mbs (@vars, ^ = power, null/true/false),
// multiline / comments, and test() syntax + semantic checks.
// Reference: https://github.com/cparse/cparse/blob/master/test-shunting-yard.cpp

#include <catch2/catch_test_macros.hpp>
#include <mbs/script.hpp>

#include <cmath>
#include <nlohmann/json.hpp>
#include <string>

using mbs::EvalErr;
using mbs::EvalOk;
using mbs::eval_val;
using mbs::test;
using nlohmann::json;

namespace {

/// Same shape as cparse PREPARE_ENVIRONMENT, using JSON + @-prefixed names in scripts.
json cparse_style_context() {
  return json::parse(R"({
    "pi": 3.14,
    "b1": 0.0,
    "b2": 0.86,
    "_b": 0,
    "str1": "foo",
    "str2": "bar",
    "str3": "foobar",
    "str4": "foo10",
    "str5": "10bar",
    "map": {
      "key": "mapped value",
      "key1": "second mapped value",
      "key2": 10,
      "key3": {
        "map1": "inception1",
        "map2": "inception2"
      }
    },
    "emap": { "a": 10, "b": 20 }
  })");
}

json ev(std::string_view src, const json& ctx = json::object()) {
  auto r = eval_val(src, ctx);
  REQUIRE(std::holds_alternative<EvalOk>(r));
  return std::get<EvalOk>(r).value;
}

void require_eval_err(std::string_view src, const json& ctx = json::object()) {
  auto r = eval_val(src, ctx);
  REQUIRE(std::holds_alternative<EvalErr>(r));
}

bool approx_num(const json& j, double expected, double eps = 1e-6) {
  return j.is_number() && std::fabs(j.get<double>() - expected) < eps;
}

void require_test_fail(std::string_view src) {
  auto t = test(src);
  REQUIRE_FALSE(t.ok);
  REQUIRE_FALSE(t.error.empty());
}

void require_test_ok(std::string_view src) {
  auto t = test(src);
  INFO("error: " << t.error);
  REQUIRE(t.ok);
}

}  // namespace

TEST_CASE("cparse-style numeric expressions", "[cparse][numeric]") {
  const json v = cparse_style_context();

  SECTION("negation and context") {
    REQUIRE(approx_num(ev("-@pi + 1", v), -2.14));
    REQUIRE(approx_num(ev("-@pi + 1 * @b1", v), -3.14));
  }

  SECTION("precedence and parens") {
    REQUIRE(approx_num(ev("(20+10)*3/2-3", v), 42.0));
    REQUIRE(approx_num(ev("1+(-2*3)", v), -5.0));
    REQUIRE(approx_num(ev("1+@_b+(-2*3)", v), -5.0));
    REQUIRE(approx_num(ev("4 * -3", v), -12.0));
    REQUIRE(approx_num(ev("10 + - - 10", v), 20.0));
  }

  SECTION("literals") {
    REQUIRE(ev("123", v) == 123.0);
    REQUIRE(ev("0", v) == 0.0);
    REQUIRE(approx_num(ev("-0", v), 0.0));
    REQUIRE(approx_num(ev("0.5", v), 0.5));
    REQUIRE(approx_num(ev("1.5", v), 1.5));
    REQUIRE(approx_num(ev("2e2", v), 200.0));
    REQUIRE(approx_num(ev("2E2", v), 200.0));
    REQUIRE(approx_num(ev("2.5e2", v), 250.0));
    REQUIRE(approx_num(ev("2.5E2", v), 250.0));
  }

  SECTION("^ is exponentiation (cparse uses ^ as XOR)") {
    REQUIRE(approx_num(ev("3 ^ 2", v), 9.0));
    REQUIRE(approx_num(ev("2 ^ 3 ^ 1", v), 8.0));  // right-assoc: 2^(3^1)=8
  }

  SECTION("division by zero yields null") {
    REQUIRE(ev("1 / 0", v).is_null());
  }
}

TEST_CASE("cparse-style boolean expressions", "[cparse][boolean]") {
  const json v = cparse_style_context();

  REQUIRE(ev("3 < 3", v) == false);
  REQUIRE(ev("3 <= 3", v) == true);
  REQUIRE(ev("3 > 3", v) == false);
  REQUIRE(ev("3 >= 3", v) == true);
  REQUIRE(ev("3 == 3", v) == true);
  REQUIRE(ev("3 != 3", v) == false);

  REQUIRE(ev("(3 && true) == true", v) == true);
  REQUIRE(ev("(3 && 0) == true", v) == false);
  REQUIRE(ev("(3 || 0) == true", v) == true);
  REQUIRE(ev("(false || 0) == true", v) == false);

  REQUIRE(ev("10 == null", v) == false);
  REQUIRE(ev("10 != null", v) == true);
  REQUIRE(ev("10 == 'str'", v) == false);
  REQUIRE(ev("10 != 'str'", v) == true);

  REQUIRE(ev("!false", v) == true);
  REQUIRE(ev("!true", v) == false);
}

TEST_CASE("cparse-style string expressions", "[cparse][string]") {
  const json v = cparse_style_context();

  REQUIRE(ev("@str1 + @str2 == @str3", v) == true);
  REQUIRE(ev("@str1 + @str2 != @str3", v) == false);
  REQUIRE(ev("'foo' + \"bar\" == @str3", v) == true);

  REQUIRE(ev("'foo\\'bar'", v) == "foo'bar");
  REQUIRE(ev("\"foo\\\"bar\"", v) == "foo\"bar");
  REQUIRE(ev("'foo\\\\bar'", v) == "foo\\bar");
  REQUIRE(ev("'foo\\nar'", v) == "foo\nar");
  REQUIRE(ev("'foo\\tar'", v) == "foo\tar");
}

TEST_CASE("mbs rejects string + number (null), unlike cparse", "[cparse][diff]") {
  const json v = cparse_style_context();
  REQUIRE(ev("@str1 + 10", v).is_null());
  REQUIRE(ev("10 + @str2", v).is_null());
  // cparse: str1+10==str4 — here null == @str4 is false
  REQUIRE(ev("@str1 + 10 == @str4", v) == false);
}

TEST_CASE("cparse-style collection equality and spacing", "[cparse][collections]") {
  REQUIRE(ev("['list'] == ['list']") == true);
  REQUIRE(ev("['list']== ['list']") == true);
  REQUIRE(ev("['list'] ==['list']") == true);
  REQUIRE(ev("['list']==['list']") == true);

  REQUIRE(ev("{a: 'list'} == {a: 'list'}") == true);
  REQUIRE(ev("{a: 'list'}== {a: 'list'}") == true);
  REQUIRE(ev("{a: 'list'} =={a: 'list'}") == true);
  REQUIRE(ev("{a: 'list'}=={a: 'list'}") == true);
}

TEST_CASE("cparse-style map access with @ context", "[cparse][map]") {
  const json v = cparse_style_context();

  REQUIRE(ev("@map[\"key\"]", v) == "mapped value");
  REQUIRE(ev("@map[\"key\" + \"1\"]", v) == "second mapped value");
  REQUIRE(ev("@map[\"key2\"] + 3 == 13", v) == true);
  REQUIRE(ev("@map.key1", v) == "second mapped value");
  REQUIRE(ev("@map.key3.map1", v) == "inception1");
  REQUIRE(ev("@map.key3[\"map2\"]", v) == "inception2");
  REQUIRE(ev("@emap.a + @emap.b", v) == 30.0);
}

TEST_CASE("object literal member access", "[cparse][map]") {
  REQUIRE(ev("{a: 1}.a") == 1.0);
  REQUIRE(ev("[1, 2].size()", json::object()) == 2.0);
}

TEST_CASE("multiline expressions", "[multiline]") {
  const json v = cparse_style_context();

  REQUIRE(approx_num(ev(
                         R"(
    ( 20
      + 10 )
    * 3 / 2
    - 3
  )",
                         v),
                     42.0));

  REQUIRE(ev(
              R"(
    @str1
    + @str2
    ==
    @str3
  )",
              v) == true);

  REQUIRE(approx_num(ev(
                         R"(
    - @pi
    + ( @b1 * 1 )
  )",
                         v),
                     -3.14));
}

TEST_CASE("comments throughout expression", "[comments]") {
  REQUIRE(ev(
              R"(
    // start
    1 +
    2
  )",
              json::object()) == 3.0);

  REQUIRE(ev(
              R"(
    1 + // middle
    2
  )",
              json::object()) == 3.0);

  REQUIRE(ev("3 + // c\n4", json::object()) == 7.0);
}

TEST_CASE("block comments are not supported (use // only)", "[comments]") {
  // `/` is division; `/*` is not a block comment opener.
  require_test_fail("1 / * 2");
  require_test_ok("// line comment only\n1+1");
}

TEST_CASE("test() semantic: unknown method", "[syntax][test]") {
  require_test_fail("@ctx.unknown()");
  auto t = test("@ctx.unknown()");
  REQUIRE(t.error.find("unknown method") != std::string::npos);
}

TEST_CASE("test() semantic: has() arity", "[syntax][test]") {
  require_test_fail("@ctx.has()");
  REQUIRE(test("@ctx.has()").error.find("has()") != std::string::npos);

  require_test_fail("@ctx.has(1, 2)");
}

TEST_CASE("test() semantic: size() and len() arity", "[syntax][test]") {
  require_test_fail("@ctx.size(1)");
  require_test_fail("@ctx.len(9)");
}

TEST_CASE("test() accepts valid builtins", "[syntax][test]") {
  require_test_ok("@x.has(\"k\")");
  require_test_ok("@x.size()");
  require_test_ok("@x.len()");
  require_test_ok("['a'].has(@r)");
}

TEST_CASE("test() parse: trailing junk after expression", "[syntax][test]") {
  require_test_fail("1 2");
  require_test_fail("5x");
}

TEST_CASE("test() parse: bare identifier", "[syntax][test]") {
  require_test_fail("foo");
  require_test_fail("true && foo");
}

TEST_CASE("test() parse: incomplete operators", "[syntax][test]") {
  require_test_fail("1 +");
  require_test_fail("1 + * 2");
}

TEST_CASE("test() parse: unclosed grouping", "[syntax][test]") {
  require_test_fail("(1 + 2");
  require_test_fail("[1, 2");
  require_test_fail("{a: 1");
}

TEST_CASE("eval_val returns EvalErr on parse failure", "[syntax][eval]") {
  require_eval_err("(");
  require_eval_err(")");
  require_eval_err("");
  // whitespace-only may tokenize to End; parser primary fails
  require_eval_err("   ");
}

TEST_CASE("string character indexing", "[ops][string]") {
  REQUIRE(ev("'foobar'[0]", json::object()) == "f");
  REQUIRE(ev("'foobar'[5]", json::object()) == "r");
  REQUIRE(ev("'foobar'[-1]", json::object()) == "r");
  REQUIRE(ev("'foobar'[6]", json::object()).is_null());
}

TEST_CASE("comparison string ordering", "[ops]") {
  REQUIRE(ev("'a' < 'b'", json::object()) == true);
  REQUIRE(ev("'b' <= 'b'", json::object()) == true);
}

TEST_CASE("null propagation in chain", "[eval]") {
  REQUIRE(ev("@missing.key", json::object()).is_null());
  REQUIRE(ev("@missing[\"x\"]", json::object()).is_null());
}

TEST_CASE("or short-circuit skips rhs", "[eval]") {
  REQUIRE(ev("true || @missing.boom", json::object()) == true);
}

TEST_CASE("nested logical and comparison", "[eval]") {
  const json v = cparse_style_context();
  REQUIRE(ev("@emap.a < @emap.b && @map.key2 == 10", v) == true);
}

// ─── TASK 5: Quick-Win Features ──────────────────────────────────────

TEST_CASE("modulo operator", "[ops][modulo]") {
  REQUIRE(ev("7 % 3") == 1.0);
  REQUIRE(ev("10 % 5") == 0.0);
  REQUIRE(ev("7.5 % 2") == 1.5);
  REQUIRE(ev("7 % 0").is_null());
  REQUIRE(ev("'a' % 2").is_null());
  REQUIRE(ev("-7 % 3") == -1.0);
}

TEST_CASE("negative array indexing", "[ops][index]") {
  json ctx = {{"arr", json::array({10, 20, 30, 40})}};
  REQUIRE(ev("@arr[-1]", ctx) == 40);
  REQUIRE(ev("@arr[-2]", ctx) == 30);
  REQUIRE(ev("@arr[-4]", ctx) == 10);
  REQUIRE(ev("@arr[-5]", ctx).is_null());
  REQUIRE(ev("[1,2,3][-1]") == 3);
}

TEST_CASE(".type() method", "[ops][type]") {
  REQUIRE(ev("(42).type()") == "number");
  REQUIRE(ev("(3.14).type()") == "number");
  REQUIRE(ev("\"hello\".type()") == "string");
  REQUIRE(ev("true.type()") == "boolean");
  REQUIRE(ev("false.type()") == "boolean");
  REQUIRE(ev("null.type()") == "null");
  REQUIRE(ev("[1,2].type()") == "array");
  REQUIRE(ev("{a:1}.type()") == "object");
}

TEST_CASE("null-coalescing operator", "[ops][nullcoalesce]") {
  REQUIRE(ev("null ?? \"default\"") == "default");
  REQUIRE(ev("\"val\" ?? \"default\"") == "val");
  REQUIRE(ev("0 ?? \"default\"") == 0);
  REQUIRE(ev("false ?? \"default\"") == false);
  REQUIRE(ev("\"\" ?? \"default\"") == "");
  json ctx = {{"x", nullptr}};
  REQUIRE(ev("@x ?? 42", ctx) == 42);
  REQUIRE(ev("@missing ?? \"fallback\"", json::object()) == "fallback");
  REQUIRE(ev("null ?? null ?? 3") == 3);
}

TEST_CASE("test() accepts .type() method", "[syntax][test]") {
  auto t = test("@x.type()");
  INFO("error: " << t.error);
  REQUIRE(t.ok);
}

TEST_CASE("test() rejects .type() with args", "[syntax][test]") {
  auto t = test("@x.type(1)");
  REQUIRE_FALSE(t.ok);
}

// --- TASK 6: String Methods ---

TEST_CASE("string contains()", "[string][methods]") {
  REQUIRE(ev(R"("hello world".contains("world"))") == true);
  REQUIRE(ev(R"("hello world".contains("xyz"))") == false);
  REQUIRE(ev(R"("hello".contains(""))") == true);
  REQUIRE(ev(R"("".contains(""))") == true);
  REQUIRE(ev("(42).contains(\"x\")").is_null());
}

TEST_CASE("string startsWith()", "[string][methods]") {
  REQUIRE(ev(R"("hello world".startsWith("hello"))") == true);
  REQUIRE(ev(R"("hello world".startsWith("world"))") == false);
  REQUIRE(ev(R"("hello".startsWith(""))") == true);
  REQUIRE(ev(R"("".startsWith("x"))") == false);
  REQUIRE(ev("(42).startsWith(\"x\")").is_null());
}

TEST_CASE("string endsWith()", "[string][methods]") {
  REQUIRE(ev(R"("hello world".endsWith("world"))") == true);
  REQUIRE(ev(R"("hello world".endsWith("hello"))") == false);
  REQUIRE(ev(R"("hello".endsWith(""))") == true);
  REQUIRE(ev(R"("".endsWith("x"))") == false);
  REQUIRE(ev("(42).endsWith(\"x\")").is_null());
}

TEST_CASE("string toLowerCase()", "[string][methods]") {
  REQUIRE(ev(R"("HELLO".toLowerCase())") == "hello");
  REQUIRE(ev(R"("Hello World".toLowerCase())") == "hello world");
  REQUIRE(ev(R"("already lower".toLowerCase())") == "already lower");
  REQUIRE(ev(R"("".toLowerCase())") == "");
  REQUIRE(ev("(42).toLowerCase()").is_null());
}

TEST_CASE("string toUpperCase()", "[string][methods]") {
  REQUIRE(ev(R"("hello".toUpperCase())") == "HELLO");
  REQUIRE(ev(R"("Hello World".toUpperCase())") == "HELLO WORLD");
  REQUIRE(ev(R"("ALREADY UPPER".toUpperCase())") == "ALREADY UPPER");
  REQUIRE(ev(R"("".toUpperCase())") == "");
  REQUIRE(ev("(42).toUpperCase()").is_null());
}

TEST_CASE("string trim()", "[string][methods]") {
  REQUIRE(ev(R"("  hello  ".trim())") == "hello");
  REQUIRE(ev("\"\\t\\n hello \\n\\t\".trim()") == "hello");
  REQUIRE(ev(R"("hello".trim())") == "hello");
  REQUIRE(ev(R"("   ".trim())") == "");
  REQUIRE(ev(R"("".trim())") == "");
  REQUIRE(ev("(42).trim()").is_null());
}

TEST_CASE("string indexOf()", "[string][methods]") {
  REQUIRE(ev(R"("hello world".indexOf("world"))") == 6);
  REQUIRE(ev(R"("hello".indexOf("x"))") == -1);
  REQUIRE(ev(R"("hello".indexOf(""))") == 0);
  REQUIRE(ev(R"("abcabc".indexOf("bc"))") == 1);
  REQUIRE(ev("(42).indexOf(\"x\")").is_null());
}

TEST_CASE("string replace()", "[string][methods]") {
  REQUIRE(ev(R"("hello world".replace("world", "there"))") == "hello there");
  REQUIRE(ev(R"("aaa".replace("a", "bb"))") == "bbaa");
  REQUIRE(ev(R"("hello".replace("xyz", "abc"))") == "hello");
  REQUIRE(ev(R"("".replace("", "x"))") == "x");
  REQUIRE(ev("(42).replace(\"a\", \"b\")").is_null());
}

TEST_CASE("string substring()", "[string][methods]") {
  REQUIRE(ev(R"("hello world".substring(6))") == "world");
  REQUIRE(ev(R"("hello world".substring(0, 5))") == "hello");
  REQUIRE(ev(R"("hello".substring(0, 0))") == "");
  REQUIRE(ev(R"("hello".substring(10))") == "");
  REQUIRE(ev(R"("hello".substring(-5))") == "hello");
  REQUIRE(ev("(42).substring(0)").is_null());
}

TEST_CASE("string split()", "[string][methods]") {
  json expected = json::array({"a", "b", "c"});
  REQUIRE(ev(R"("a,b,c".split(","))") == expected);

  json expected2 = json::array({"hello"});
  REQUIRE(ev(R"("hello".split(","))") == expected2);

  json expected3 = json::array({"", ""});
  REQUIRE(ev(R"(",".split(","))") == expected3);

  json chars = json::array({"h", "i"});
  REQUIRE(ev(R"("hi".split(""))") == chars);

  REQUIRE(ev("(42).split(\",\")").is_null());
}

// --- TASK 6: Object Introspection ---

TEST_CASE("object keys()", "[object][methods]") {
  json ctx = {{"obj", {{"a", 1}, {"b", 2}}}};
  json result = ev("@obj.keys()", ctx);
  REQUIRE(result.is_array());
  REQUIRE(result.size() == 2);
  REQUIRE(std::find(result.begin(), result.end(), "a") != result.end());
  REQUIRE(std::find(result.begin(), result.end(), "b") != result.end());

  json empty_ctx = {{"obj", json::object()}};
  REQUIRE(ev("@obj.keys()", empty_ctx) == json::array());

  REQUIRE(ev("(42).keys()").is_null());
  REQUIRE(ev(R"("hello".keys())").is_null());
}

TEST_CASE("object values()", "[object][methods]") {
  json ctx = {{"obj", {{"a", 1}, {"b", 2}}}};
  json result = ev("@obj.values()", ctx);
  REQUIRE(result.is_array());
  REQUIRE(result.size() == 2);
  REQUIRE(std::find(result.begin(), result.end(), 1) != result.end());
  REQUIRE(std::find(result.begin(), result.end(), 2) != result.end());

  json empty_ctx = {{"obj", json::object()}};
  REQUIRE(ev("@obj.values()", empty_ctx) == json::array());

  REQUIRE(ev("(42).values()").is_null());
}

TEST_CASE("test() accepts new string methods", "[syntax][test]") {
  require_test_ok(R"(@x.contains("y"))");
  require_test_ok(R"(@x.startsWith("y"))");
  require_test_ok(R"(@x.endsWith("y"))");
  require_test_ok("@x.toLowerCase()");
  require_test_ok("@x.toUpperCase()");
  require_test_ok("@x.trim()");
  require_test_ok(R"(@x.indexOf("y"))");
  require_test_ok(R"(@x.replace("a", "b"))");
  require_test_ok("@x.substring(0)");
  require_test_ok("@x.substring(0, 5)");
  require_test_ok(R"(@x.split(","))");
  require_test_ok("@x.keys()");
  require_test_ok("@x.values()");
}

TEST_CASE("test() rejects wrong arg counts for new methods", "[syntax][test]") {
  require_test_fail("@x.contains()");
  require_test_fail(R"(@x.contains("a", "b"))");
  require_test_fail("@x.toLowerCase(1)");
  require_test_fail("@x.trim(1)");
  require_test_fail("@x.keys(1)");
  require_test_fail("@x.values(1)");
  require_test_fail("@x.replace(\"a\")");
  require_test_fail("@x.substring()");
  require_test_fail("@x.substring(1, 2, 3)");
}

// --- TASK 7: Ternary Operator ---

TEST_CASE("ternary operator basic", "[ternary]") {
  REQUIRE(ev(R"(true ? "yes" : "no")") == "yes");
  REQUIRE(ev(R"(false ? "yes" : "no")") == "no");
  REQUIRE(ev("1 ? 10 : 20") == 10);
  REQUIRE(ev("0 ? 10 : 20") == 20);
  REQUIRE(ev("null ? 10 : 20") == 20);
}

TEST_CASE("ternary operator with context", "[ternary]") {
  json ctx = {{"x", 5}, {"name", "Alice"}};
  REQUIRE(ev("@x > 3 ? \"big\" : \"small\"", ctx) == "big");
  REQUIRE(ev("@x < 3 ? \"big\" : \"small\"", ctx) == "small");
  REQUIRE(ev(R"(@name == "Alice" ? "hi Alice" : "who?")", ctx) == "hi Alice");
}

TEST_CASE("ternary operator nested", "[ternary]") {
  REQUIRE(ev("true ? true ? 1 : 2 : 3") == 1);
  REQUIRE(ev("true ? false ? 1 : 2 : 3") == 2);
  REQUIRE(ev("false ? 1 : true ? 2 : 3") == 2);
  REQUIRE(ev("false ? 1 : false ? 2 : 3") == 3);
}

TEST_CASE("ternary operator lazy evaluation", "[ternary]") {
  json ctx = {{"x", 0}};
  REQUIRE(ev("@x == 0 ? 42 : 1 / @x", ctx) == 42);
}

TEST_CASE("ternary with complex expressions", "[ternary]") {
  REQUIRE(ev("(1 + 1 == 2) ? 100 : 200") == 100);
  json ctx = {{"arr", json::array({1, 2, 3})}};
  REQUIRE(ev("@arr.size() > 0 ? @arr[0] : null", ctx) == 1);
}

TEST_CASE("ternary interacts with null coalescing", "[ternary]") {
  REQUIRE(ev("true ? null ?? 42 : 0") == 42);
  REQUIRE(ev("false ? 0 : null ?? 99") == 99);
}

TEST_CASE("test() validates ternary expressions", "[ternary][syntax]") {
  require_test_ok("@x ? 1 : 2");
  require_test_ok("true ? @x.size() : 0");
}

// --- TASK 7: in Operator ---

TEST_CASE("in operator with arrays", "[in]") {
  json ctx = {{"arr", json::array({1, 2, 3, "hello", true})}};
  REQUIRE(ev("2 in @arr", ctx) == true);
  REQUIRE(ev("5 in @arr", ctx) == false);
  REQUIRE(ev(R"("hello" in @arr)", ctx) == true);
  REQUIRE(ev(R"("world" in @arr)", ctx) == false);
  REQUIRE(ev("true in @arr", ctx) == true);
  REQUIRE(ev("false in @arr", ctx) == false);
}

TEST_CASE("in operator with objects", "[in]") {
  json ctx = {{"obj", {{"a", 1}, {"b", 2}}}};
  REQUIRE(ev(R"("a" in @obj)", ctx) == true);
  REQUIRE(ev(R"("c" in @obj)", ctx) == false);
  REQUIRE(ev(R"("b" in @obj)", ctx) == true);
}

TEST_CASE("in operator with non-string key on object", "[in]") {
  json ctx = {{"obj", {{"a", 1}}}};
  REQUIRE(ev("1 in @obj", ctx).is_null());
}

TEST_CASE("in operator with non-collection", "[in]") {
  REQUIRE(ev("1 in 42").is_null());
  REQUIRE(ev(R"(1 in "hello")").is_null());
  REQUIRE(ev("1 in null").is_null());
}

TEST_CASE("in operator with inline literals", "[in]") {
  REQUIRE(ev("1 in [1, 2, 3]") == true);
  REQUIRE(ev("4 in [1, 2, 3]") == false);
  REQUIRE(ev(R"("x" in {x: 1, y: 2})") == true);
  REQUIRE(ev(R"("z" in {x: 1, y: 2})") == false);
}

TEST_CASE("in operator combined with other operators", "[in]") {
  REQUIRE(ev("1 in [1, 2] && 3 in [3, 4]") == true);
  REQUIRE(ev("1 in [1, 2] && 5 in [3, 4]") == false);
  REQUIRE(ev("5 in [1, 2] || 3 in [3, 4]") == true);
}

TEST_CASE("in operator with ternary", "[in][ternary]") {
  REQUIRE(ev(R"(2 in [1,2,3] ? "found" : "missing")") == "found");
  REQUIRE(ev(R"(9 in [1,2,3] ? "found" : "missing")") == "missing");
}

TEST_CASE("test() validates in expressions", "[in][syntax]") {
  require_test_ok("@x in @arr");
  require_test_ok("1 in [1, 2, 3]");
}
