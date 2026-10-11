#include <catch2/catch_test_macros.hpp>
#include <mbs/script.hpp>

#include <nlohmann/json.hpp>

using mbs::EvalErr;
using mbs::EvalOk;
using mbs::eval_true;
using mbs::eval_val;
using mbs::test;
using nlohmann::json;

TEST_CASE("single-line comments are skipped", "[lexer]") {
  auto r = eval_val(
      R"(
      // comment
      1 + 2 // trailing
      )",
      json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == 3.0);
}

TEST_CASE("string times integer repeats like Python", "[ops]") {
  auto r = eval_val(R"("ab" * 3)", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == "ababab");
}

TEST_CASE("integer times string repeats", "[ops]") {
  auto r = eval_val(R"(4 * "x")", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == "xxxx");
}

TEST_CASE("string times non-integral number is invalid (null)", "[ops]") {
  auto r = eval_val(R"("a" * 2.5)", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value.is_null());
}

TEST_CASE("string times zero yields empty string", "[ops]") {
  auto r = eval_val(R"("hi" * 0)", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == "");
}

TEST_CASE("power requires numeric operands", "[ops]") {
  SECTION("string ^ number -> null") {
    auto r = eval_val(R"("a" ^ 2)", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value.is_null());
  }
  SECTION("number ^ string -> null") {
    auto r = eval_val(R"(2 ^ "b")", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value.is_null());
  }
  SECTION("valid number power") {
    auto r = eval_val(R"(2 ^ 3)", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value == 8.0);
  }
}

TEST_CASE("invalid mixed arithmetic returns null", "[ops]") {
  auto r = eval_val(R"("a" + 1)", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value.is_null());
}

TEST_CASE("string + string concatenation", "[ops]") {
  auto r = eval_val(R"("foo" + "bar")", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == "foobar");
}

TEST_CASE("test() rejects unknown methods", "[test]") {
  auto t = test(R"(@x.unknown())");
  REQUIRE_FALSE(t.ok);
  REQUIRE(t.error.find("unknown method") != std::string::npos);
}

TEST_CASE("test() accepts known script", "[test]") {
  auto t = test(R"(@user.age > 0 && ['a'].has("a"))");
  REQUIRE(t.ok);
}

TEST_CASE("eval_true uses truthiness", "[eval]") {
  json ctx = {{"flag", false}};
  auto b = eval_true("@flag", ctx);
  REQUIRE(b.ok);
  REQUIRE_FALSE(b.value);
}

TEST_CASE("context variable missing is null", "[eval]") {
  auto r = eval_val("@missing", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value.is_null());
}

TEST_CASE("len on number returns number", "[builtin]") {
  auto r = eval_val(R"( (5).len() )", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == 5.0);
}

TEST_CASE("array literal has membership", "[eval]") {
  json ctx = {{"role", "student"}};
  auto r = eval_val(R"(['student','teacher'].has(@role))", ctx);
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == true);
}

TEST_CASE("short-circuit and skips rhs", "[eval]") {
  json ctx = json::object();
  auto r = eval_val(R"(false && @nope.foo)", ctx);
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == false);
}

TEST_CASE("string repeat with huge count returns null", "[security]") {
  auto r = eval_val(R"("x" * 999999999)", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value.is_null());
}

TEST_CASE("number * string with huge count returns null", "[security]") {
  auto r = eval_val(R"(999999999 * "y")", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value.is_null());
}

TEST_CASE("deeply nested parentheses throw parse depth error", "[security]") {
  std::string expr(2000, '(');
  expr += "1";
  expr += std::string(2000, ')');
  auto r = eval_val(expr, json::object());
  REQUIRE(std::holds_alternative<EvalErr>(r));
  REQUIRE(std::get<EvalErr>(r).error.find("deeply nested") != std::string::npos);
}

TEST_CASE("deeply nested unary chains throw parse depth error", "[security]") {
  std::string expr(2000, '!');
  expr += "true";
  auto r = eval_val(expr, json::object());
  REQUIRE(std::holds_alternative<EvalErr>(r));
  REQUIRE(std::get<EvalErr>(r).error.find("deeply nested") != std::string::npos);
}

TEST_CASE("evaluation depth exceeded throws error", "[security]") {
  std::string expr(600, '(');
  expr += "1";
  expr += std::string(600, ')');
  auto r = eval_val(expr, json::object());
  REQUIRE(std::holds_alternative<EvalErr>(r));
  auto& err = std::get<EvalErr>(r).error;
  bool is_depth_error = err.find("deeply nested") != std::string::npos ||
                        err.find("depth exceeded") != std::string::npos;
  REQUIRE(is_depth_error);
}

TEST_CASE("moderate string repeat still works", "[security]") {
  auto r = eval_val(R"("ab" * 100)", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value.get<std::string>().size() == 200);
}

TEST_CASE("NaN/Infinity from arithmetic returns null (S4)", "[security]") {
  SECTION("0 ^ -1 is Inf -> null") {
    auto r = eval_val(R"(0 ^ -1)", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value.is_null());
  }
  SECTION("(-1) ^ 0.5 is NaN -> null") {
    auto r = eval_val(R"((-1) ^ 0.5)", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value.is_null());
  }
  SECTION("1e308 + 1e308 is Inf -> null") {
    json ctx = {{"big", 1e308}};
    auto r = eval_val(R"(@big + @big)", ctx);
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value.is_null());
  }
  SECTION("1e308 * 2 is Inf -> null") {
    json ctx = {{"big", 1e308}};
    auto r = eval_val(R"(@big * 2)", ctx);
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value.is_null());
  }
  SECTION("subtraction overflow -> null") {
    json ctx = {{"big", 1e308}, {"negbig", -1e308}};
    auto r = eval_val(R"(@negbig - @big)", ctx);
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value.is_null());
  }
}

TEST_CASE("string concatenation exceeding MAX_STRING_SIZE returns null (S6)", "[security]") {
  std::string big(600000, 'a');
  json ctx = {{"s1", big}, {"s2", big}};
  auto r = eval_val(R"(@s1 + @s2)", ctx);
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value.is_null());
}

TEST_CASE("max_source_length rejects oversized input (S7)", "[security]") {
  SECTION("eval_val rejects long source") {
    auto r = eval_val("1 + 2", json::object(), 3);
    REQUIRE(std::holds_alternative<EvalErr>(r));
    REQUIRE(std::get<EvalErr>(r).error.find("exceeds maximum length") != std::string::npos);
  }
  SECTION("eval_val accepts source within limit") {
    auto r = eval_val("1 + 2", json::object(), 100);
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value == 3.0);
  }
  SECTION("eval_true rejects long source") {
    auto r = eval_true("true", json::object(), 2);
    REQUIRE_FALSE(r.ok);
    REQUIRE(r.error.find("exceeds maximum length") != std::string::npos);
  }
  SECTION("test rejects long source") {
    auto t = test("1 + 2", 3);
    REQUIRE_FALSE(t.ok);
    REQUIRE(t.error.find("exceeds maximum length") != std::string::npos);
  }
  SECTION("zero max_source_length means unlimited") {
    auto r = eval_val("1 + 2", json::object(), 0);
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value == 3.0);
  }
}

TEST_CASE("out-of-range number literal gives descriptive error (S8)", "[security]") {
  auto r = eval_val("1e99999", json::object());
  REQUIRE(std::holds_alternative<EvalErr>(r));
  REQUIRE(std::get<EvalErr>(r).error.find("out of range") != std::string::npos);
}

TEST_CASE("malformed scientific notation does not eat operators (B1)", "[bugfix]") {
  SECTION("1e+ without trailing digit produces error (not silent consumption)") {
    auto r = eval_val("1e+", json::object());
    REQUIRE(std::holds_alternative<EvalErr>(r));
  }
  SECTION("1e- without trailing digit produces error") {
    auto r = eval_val("1e-", json::object());
    REQUIRE(std::holds_alternative<EvalErr>(r));
  }
  SECTION("1e without trailing digit produces error") {
    auto r = eval_val("1e", json::object());
    REQUIRE(std::holds_alternative<EvalErr>(r));
  }
  SECTION("valid scientific notation still works") {
    auto r = eval_val("1e2", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value == 100.0);
  }
  SECTION("valid scientific notation with sign works") {
    auto r = eval_val("1e+2", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value == 100.0);
  }
  SECTION("valid negative exponent works") {
    auto r = eval_val("1e-2", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    auto val = std::get<EvalOk>(r).value.get<double>();
    REQUIRE(std::fabs(val - 0.01) < 1e-9);
  }
}

TEST_CASE("&& and || always return bool (B2)", "[bugfix]") {
  SECTION("3 && 'hello' returns true (bool)") {
    auto r = eval_val(R"(3 && "hello")", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value == true);
    REQUIRE(std::get<EvalOk>(r).value.is_boolean());
  }
  SECTION("false || 42 returns false (bool)") {
    auto r = eval_val("false || 42", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value == true);
    REQUIRE(std::get<EvalOk>(r).value.is_boolean());
  }
  SECTION("true && false returns false (bool)") {
    auto r = eval_val("true && false", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value == false);
    REQUIRE(std::get<EvalOk>(r).value.is_boolean());
  }
  SECTION("false || false returns false (bool)") {
    auto r = eval_val("false || false", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value == false);
    REQUIRE(std::get<EvalOk>(r).value.is_boolean());
  }
}

// --- Precompilation API (F14) ---

TEST_CASE("Script::compile + eval_val reuses AST", "[precompile]") {
  mbs::Script s;
  auto compiled = s.compile("@x + 1");

  SECTION("evaluates with different contexts") {
    auto r1 = s.eval_val(compiled, json{{"x", 1}});
    REQUIRE(std::holds_alternative<EvalOk>(r1));
    REQUIRE(std::get<EvalOk>(r1).value == 2.0);

    auto r2 = s.eval_val(compiled, json{{"x", 2}});
    REQUIRE(std::holds_alternative<EvalOk>(r2));
    REQUIRE(std::get<EvalOk>(r2).value == 3.0);
  }

  SECTION("eval_true on compiled expression") {
    auto r = s.eval_true(compiled, json{{"x", 1}});
    REQUIRE(r.ok);
    REQUIRE(r.value == true);
  }
}

TEST_CASE("Script::compile rejects invalid expressions", "[precompile]") {
  mbs::Script s;
  REQUIRE_THROWS(s.compile("1 +"));
}

TEST_CASE("Script::compile respects max_source_length", "[precompile]") {
  mbs::Script s;
  REQUIRE_THROWS_AS(s.compile("1 + 2", 3), std::runtime_error);
  REQUIRE_NOTHROW(s.compile("1 + 2", 10));
}

TEST_CASE("Script::eval_val with source string works", "[precompile]") {
  mbs::Script s;
  auto r = s.eval_val("1 + 2", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == 3.0);
}

TEST_CASE("Script::eval_true with source string works", "[precompile]") {
  mbs::Script s;
  auto r = s.eval_true("1 > 0", json::object());
  REQUIRE(r.ok);
  REQUIRE(r.value == true);
}

TEST_CASE("Uncompiled CompiledExpr returns error", "[precompile]") {
  mbs::Script s;
  mbs::CompiledExpr empty;
  REQUIRE(!empty);
  auto r = s.eval_val(empty, json::object());
  REQUIRE(std::holds_alternative<EvalErr>(r));
}

// --- Custom Function Registry (F15) ---

TEST_CASE("register_function: basic custom method", "[custom-func]") {
  mbs::Script s;
  s.register_function("double", [](const json& target, const std::vector<json>&) -> json {
    if (!target.is_number()) return nullptr;
    return target.get<double>() * 2;
  });

  auto r = s.eval_val("(5).double()", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == 10.0);
}

TEST_CASE("register_function: custom method with arguments", "[custom-func]") {
  mbs::Script s;
  s.register_function("add", [](const json& target, const std::vector<json>& args) -> json {
    if (!target.is_number() || args.empty() || !args[0].is_number()) return nullptr;
    return target.get<double>() + args[0].get<double>();
  });

  auto r = s.eval_val("(10).add(5)", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == 15.0);
}

TEST_CASE("register_function: custom method on string", "[custom-func]") {
  mbs::Script s;
  s.register_function("reverse", [](const json& target, const std::vector<json>&) -> json {
    if (!target.is_string()) return nullptr;
    std::string result = target.get<std::string>();
    std::reverse(result.begin(), result.end());
    return result;
  });

  auto r = s.eval_val(R"("hello".reverse())", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == "olleh");
}

TEST_CASE("register_function: unknown method still fails semantic check", "[custom-func]") {
  mbs::Script s;
  REQUIRE_THROWS(s.compile("(5).unknown()"));
}

TEST_CASE("register_function: registered method passes semantic check", "[custom-func]") {
  mbs::Script s;
  s.register_function("custom", [](const json& target, const std::vector<json>&) -> json {
    return target;
  });
  REQUIRE_NOTHROW(s.compile("(5).custom()"));
}

TEST_CASE("register_function: works with compiled expressions", "[custom-func]") {
  mbs::Script s;
  s.register_function("triple", [](const json& target, const std::vector<json>&) -> json {
    if (!target.is_number()) return nullptr;
    return target.get<double>() * 3;
  });

  auto compiled = s.compile("@x.triple()");
  auto r1 = s.eval_val(compiled, json{{"x", 3}});
  REQUIRE(std::holds_alternative<EvalOk>(r1));
  REQUIRE(std::get<EvalOk>(r1).value == 9.0);

  auto r2 = s.eval_val(compiled, json{{"x", 7}});
  REQUIRE(std::holds_alternative<EvalOk>(r2));
  REQUIRE(std::get<EvalOk>(r2).value == 21.0);
}

TEST_CASE("register_function: custom method in nested expression", "[custom-func]") {
  mbs::Script s;
  s.register_function("double", [](const json& target, const std::vector<json>&) -> json {
    if (!target.is_number()) return nullptr;
    return target.get<double>() * 2;
  });

  auto r = s.eval_val("(3).double() + (4).double()", json::object());
  REQUIRE(std::holds_alternative<EvalOk>(r));
  REQUIRE(std::get<EvalOk>(r).value == 14.0);
}

TEST_CASE("register_function: builtin methods still work alongside custom", "[custom-func]") {
  mbs::Script s;
  s.register_function("double", [](const json& target, const std::vector<json>&) -> json {
    if (!target.is_number()) return nullptr;
    return target.get<double>() * 2;
  });

  SECTION("builtin size still works") {
    auto r = s.eval_val(R"("hello".size())", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value == 5);
  }

  SECTION("custom double works") {
    auto r = s.eval_val("(5).double()", json::object());
    REQUIRE(std::holds_alternative<EvalOk>(r));
    REQUIRE(std::get<EvalOk>(r).value == 10.0);
  }
}
