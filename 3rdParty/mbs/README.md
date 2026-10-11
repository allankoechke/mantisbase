# mbs-script

**mbs** is a small, safe expression language for C++ applications. You write a single expression as a string; the engine evaluates it against a JSON context object and returns a JSON value. It is designed for configuration rules, feature flags, templating conditions, and similar use cases where you need logic without embedding a full scripting runtime.

This document covers both **how to use the language** (for newcomers) and **how to embed it in C++**.

---

## Build

Requirements: CMake 3.16+, C++17 compiler.

```bash
cmake -B build -S .
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure
```

| Target | Description |
|--------|-------------|
| `mbs` | Static library |
| `mbs-example` | Sample program ([`examples/main.cpp`](examples/main.cpp)) |
| `mbs_tests` | Catch2 test suite |

Run the example:

```bash
./build/mbs-example
# MSVC multi-config:  build/Release/mbs-example.exe
```

Disable tests/examples: `cmake -B build -S . -DBUILD_TESTING=OFF`

---

## Language overview

An **mbs script** is always **one expression**. There are no statements, assignments, loops, or user-defined functions inside the language itself (custom methods are registered from C++). Whitespace and newlines are ignored except inside strings.

Every script reads data from a **context**: a JSON object passed from C++. Context fields are referenced with an `@` prefix (e.g. `@user`, `@count`).

**Results** are JSON values: numbers, strings, booleans, `null`, arrays, or objects.

**Invalid operations** (wrong types, division by zero, overflow, etc.) produce JSON `null` rather than throwing at runtime. Parse errors and semantic errors (unknown methods, bad arity) are reported separately via the C++ API.

---

## Literals

| Literal | Example | Result type |
|---------|---------|-------------|
| Number | `42`, `-3.14`, `2e2`, `1.5E-3` | number |
| String | `"hello"`, `'world'` | string |
| Boolean | `true`, `false` | boolean |
| Null | `null` | null |
| Array | `[1, 2, "three"]` | array |
| Object | `{name: "Alice", age: 30}` | object |

Object keys may be unquoted identifiers (`name:`) or quoted strings (`"key":`).

### Strings

Both single and double quotes work. Escape sequences:

| Escape | Meaning |
|--------|---------|
| `\n` | newline |
| `\t` | tab |
| `\r` | carriage return |
| `\\` | backslash |
| `\'` | single quote |
| `\"` | double quote |

Examples:

```
'foo\'bar'          →  foo'bar
"foo\"bar"          →  foo"bar
'foo\\bar'          →  foo\bar
'foo\nar'           →  foo + newline + ar
```

---

## Context variables (`@name`)

Context variables read fields from the JSON context object supplied by C++.

```
@pi                 →  context["pi"]
@user.name          →  context["user"]["name"]  (null if missing)
@map["key"]         →  bracket access on objects/arrays
@missing            →  null (field not in context)
```

Rules:

- Only `@`-prefixed names are allowed. Bare identifiers like `foo` are a **parse error**.
- Missing context fields evaluate to `null`.
- Chained access on `null` propagates `null` (`@missing.key` → `null`).

Example context:

```json
{
  "pi": 3.14,
  "user": { "name": "Alice", "age": 30 },
  "roles": ["student", "teacher"]
}
```

| Expression | Result |
|------------|--------|
| `@pi + 1` | `4.14` |
| `@user.name` | `"Alice"` |
| `@user.age > 18` | `true` |
| `@roles[0]` | `"student"` |
| `@roles[-1]` | `"teacher"` |

---

## Operators

### Precedence (lowest to highest)

| Level | Operators | Associativity | Notes |
|-------|-----------|---------------|-------|
| 1 | `??` | left | null-coalescing |
| 2 | `\|\|` | left | short-circuit OR; result is always `bool` |
| 3 | `&&` | left | short-circuit AND; result is always `bool` |
| 4 | `==` `!=` | left | deep JSON equality |
| 5 | `<` `<=` `>` `>=` `in` | left | `in` checks membership |
| 6 | `+` `-` | left | `+` also concatenates strings |
| 7 | `*` `/` `%` | left | `*` also repeats strings |
| 8 | `^` | **right** | exponentiation (not XOR) |
| 9 | `!` `-` (unary) | right | logical NOT, numeric negation |
| 10 | `.` `[ ]` `()` | left | member, index, method call |
| — | `? :` | right | ternary (between `??` and `\|\|`) |

Use parentheses to override precedence: `(20 + 10) * 3 / 2 - 3` → `42`.

### Arithmetic

| Operator | Valid operands | Result |
|----------|----------------|--------|
| `+` | number + number | number |
| `+` | string + string | concatenated string |
| `-` `*` `/` `%` | number op number | number |
| `^` | number ^ number | number (right-associative: `2 ^ 3 ^ 1` = `2 ^ (3 ^ 1)` = `8`) |
| `*` | string * integer | string repeated N times |
| `*` | integer * string | same as above |

Mixed-type arithmetic (e.g. `"a" + 1`) returns `null`.

Special cases:

- Division or modulo by zero → `null`
- Non-finite results (NaN, Infinity) → `null`
- String repeat multiplier must be a whole number; `"a" * 2.5` → `null`
- String repeat with count ≤ 0 → `""`

Examples:

```
1 + 2 * 3           →  7
2 ^ 10              →  1024
"hi" * 3            →  "hihihi"
4 * "x"             →  "xxxx"
"foo" + "bar"       →  "foobar"
7 % 3               →  1
1 / 0               →  null
```

### Comparison

| Operator | Valid operands |
|----------|----------------|
| `==` `!=` | any JSON values (deep equality) |
| `<` `<=` `>` `>=` | number vs number, or string vs string |

Comparing incompatible types (e.g. number vs string) → `null`.

```
3 < 3               →  false
3 <= 3              →  true
'a' < 'b'           →  true
10 == null          →  false
['list'] == ['list'] →  true
```

### Logical

| Operator | Behavior |
|----------|----------|
| `&&` | Short-circuit AND. If left is falsy, right is **not** evaluated. Returns `bool`. |
| `\|\|` | Short-circuit OR. If left is truthy, right is **not** evaluated. Returns `bool`. |
| `!` | Logical NOT; returns `bool`. |

```
false && @missing.foo   →  false  (rhs never evaluated)
true || @missing.boom   →  true   (rhs never evaluated)
3 && "hello"            →  true   (both truthy → true)
```

### Null-coalescing (`??`)

Returns the left value unless it is `null`; then returns the right value.

```
null ?? "default"       →  "default"
"val" ?? "default"      →  "val"
0 ?? "default"          →  0        (0 is not null)
false ?? "default"      →  false
@missing ?? 42          →  42
null ?? null ?? 3       →  3
```

### Ternary (`? :`)

```
condition ? thenExpr : elseExpr
```

The condition uses **truthiness** rules (see below). Only the chosen branch is evaluated (lazy).

```
@x > 3 ? "big" : "small"
true ? 42 : 1 / 0       →  42  (else branch skipped)
false ? 1 : 2           →  2
```

### Membership (`in`)

```
value in collection
```

| Right operand | Behavior |
|---------------|----------|
| array | `true` if any element equals `value` (deep equality) |
| object | `true` if `value` is a string key that exists |
| other | `null` |

Object key checks require a string on the left: `1 in {a: 1}` → `null`.

```
2 in [1, 2, 3]              →  true
"hello" in ["hello", "world"] →  true
"a" in {a: 1, b: 2}         →  true
"c" in {a: 1, b: 2}         →  false
```

---

## Truthiness

Used by `&&`, `||`, `!`, `? :`, and `eval_true()`.

| Value | Truthy? |
|-------|---------|
| `null` | false |
| `false` | false |
| `true` | true |
| number `0` | false |
| other numbers | true |
| empty string `""` | false |
| non-empty string | true |
| empty array `[]` | false |
| non-empty array | true |
| empty object `{}` | false |
| non-empty object | true |

---

## Collections and access

### Array literals

```
[1, 2, 3]
['a', 'b', @role]
```

### Object literals

```
{a: 1, b: 2}
{key: "value", nested: {x: 10}}
```

### Indexing

Works on **arrays** and **strings**. Supports negative indices (Python-style, from the end).

```
[10, 20, 30][0]     →  10
[10, 20, 30][-1]    →  30
'foobar'[0]         →  "f"
'foobar'[-1]        →  "r"
@arr[6]             →  null  (out of range)
```

### Member access (`.name`)

On **objects**, reads a field. Missing keys → `null`.

On **any value**, `.empty` is a built-in property (not a method):

| Type | `.empty` |
|------|----------|
| `null` | `true` |
| `false`, numbers | `false` |
| `""` | `true` |
| non-empty string | `false` |
| `[]`, `{}` | `true` |
| non-empty array/object | `false` |

```
@map.key1
@map.key3.map1
{size: 5}.empty     →  false
```

### Chaining

Access patterns chain left-to-right:

```
@map["key" + "1"]
@user.profile.name
['student','teacher'].has(@role)
(5).len()
"hello world".contains("world")
```

---

## Built-in methods

Methods are called on a value: `value.method(args)`.

Wrong receiver type or wrong argument count/type → `null` at runtime. Unknown methods are caught at **compile/validation** time by `test()` and `Script::compile()`.

### General

| Method | Args | Description |
|--------|------|-------------|
| `.type()` | 0 | Returns `"null"`, `"boolean"`, `"number"`, `"string"`, `"array"`, or `"object"`. |
| `.size()` | 0 | Length of string, array, or object. |
| `.len()` | 0 | String/array length; on numbers, returns the number itself. |
| `.has(x)` | 1 | Object: key exists? Array: element equals `x`? |

### String methods

| Method | Args | Description |
|--------|------|-------------|
| `.contains(s)` | 1 | Substring present? |
| `.startsWith(s)` | 1 | Prefix check. |
| `.endsWith(s)` | 1 | Suffix check. |
| `.toLowerCase()` | 0 | Lowercase copy. |
| `.toUpperCase()` | 0 | Uppercase copy. |
| `.trim()` | 0 | Strip leading/trailing whitespace. |
| `.indexOf(s)` | 1 | Index of substring, or `-1`. |
| `.replace(from, to)` | 2 | Replace **first** occurrence. |
| `.substring(start)` | 1 | From `start` to end. |
| `.substring(start, len)` | 2 | Substring of given length. |
| `.split(delim)` | 1 | Split into array; empty delimiter splits into characters. |

Examples:

```
"hello world".contains("world")           →  true
"HELLO".toLowerCase()                     →  "hello"
"  hello  ".trim()                        →  "hello"
"hello world".replace("world", "there")  →  "hello there"
"a,b,c".split(",")                        →  ["a","b","c"]
"hi".split("")                            →  ["h","i"]
"hello".substring(0, 2)                   →  "he"
```

### Object methods

| Method | Args | Description |
|--------|------|-------------|
| `.keys()` | 0 | Array of key strings. |
| `.values()` | 0 | Array of values. |

---

## Comments

Only **line comments** with `//` are supported. They run to end of line.

```
// full-line comment
1 + 2   // trailing comment

// multiline scripts work:
// (20 + 10) * 3 / 2 - 3
```

`/* block comments */` are **not** supported (`/` is division).

---

## Common patterns (cookbook)

**Feature flag**

```
@features.new_ui ?? false
```

**Role check**

```
@role in ['admin', 'moderator']
```

**Safe nested access with default**

```
@user.profile.name ?? "Anonymous"
```

**Conditional message**

```
@score >= 60 ? "Pass" : "Fail"
```

**Validate list membership**

```
['student', 'teacher'].has(@role)
```

**String template-style repeat**

```
"-" * 40
```

**Type guard**

```
@value.type() == "string" && @value.size() > 0
```

---

## Error handling and `null`

| Situation | Outcome |
|-----------|---------|
| Parse error (bad syntax) | C++ API returns `EvalErr` / `TestResult.ok = false` |
| Unknown method / wrong arity | Caught by `test()` or `Script::compile()` before evaluation |
| Type mismatch at runtime | JSON `null` |
| Missing `@variable` | `null` |
| Access on `null` | `null` |
| Division by zero | `null` |
| Overflow / NaN / Infinity | `null` |

Use `??` for defaults and `test()` to validate scripts before deployment.

---

## Security limits

Built-in guardrails for untrusted input:

| Limit | Value |
|-------|-------|
| Max string result size | 1 MiB |
| Max parse nesting depth | 512 |
| Max evaluation depth | 512 |
| String repeat | Rejected if result would exceed max size |
| `max_source_length` parameter | Optional cap on source string length (0 = unlimited) |

---

## C++ API

Header: `#include <mbs/script.hpp>`

Link: `mbs` (transitively pulls in `nlohmann_json`).

### Free functions (one-shot evaluation)

```cpp
#include <mbs/script.hpp>
#include <nlohmann/json.hpp>

nlohmann::json ctx = {{"pi", 3.14}, {"ok", true}};

// Validate syntax + semantics (no execution)
mbs::TestResult t = mbs::test("@user.age > 0 && ['a'].has('a')");
// t.ok, t.error, t.loc (line/column)

// Evaluate to JSON
mbs::EvalValueResult r = mbs::eval_val("1 + 1 + @pi", ctx);
if (std::holds_alternative<mbs::EvalOk>(r)) {
    nlohmann::json value = std::get<mbs::EvalOk>(r).value;  // ~5.14
} else {
    auto& err = std::get<mbs::EvalErr>(r);
    // err.error, err.loc
}

// Evaluate to bool (truthiness)
mbs::EvalBoolResult b = mbs::eval_true("@ok", ctx);
// b.ok, b.value, b.error, b.loc
```

All three accept an optional `max_source_length` as the last argument.

### `Script` class (precompile + custom methods)

Use `Script` when you evaluate the **same expression many times** or need **custom methods**:

```cpp
mbs::Script script;

// Register a custom method: (target).double()
script.register_function("double", [](const nlohmann::json& target,
                                      const std::vector<nlohmann::json>&) -> nlohmann::json {
    if (!target.is_number()) return nullptr;
    return target.get<double>() * 2;
});

// Compile once
mbs::CompiledExpr expr = script.compile("@x.double() + 1");

// Evaluate many times with different contexts
auto r1 = script.eval_val(expr, nlohmann::json{{"x", 5}});   // 11
auto r2 = script.eval_val(expr, nlohmann::json{{"x", 10}});  // 21

// Or one-shot through Script (uses registered functions)
auto r3 = script.eval_val("(5).double()", {});
```

| Type | Purpose |
|------|---------|
| `Script::compile(source)` | Parse + validate; returns `CompiledExpr` (throws on error) |
| `Script::eval_val(compiled, ctx)` | Run precompiled expression |
| `Script::eval_true(compiled, ctx)` | Boolean result |
| `Script::register_function(name, fn)` | Add custom method callable as `.name(args)` |

Custom function signature:

```cpp
nlohmann::json fn(
    const nlohmann::json& target,           // value before the dot
    const std::vector<nlohmann::json>& args   // method arguments
);
```

Return `nullptr` (JSON null) for invalid input, matching built-in method behavior.

### Result types

| Type | Fields |
|------|--------|
| `TestResult` | `ok`, `error`, `loc` |
| `EvalOk` | `value` |
| `EvalErr` | `error`, `loc` |
| `EvalBoolResult` | `ok`, `value`, `error`, `loc` |
| `SourceLocation` | `line`, `column` |

---

## Example program

See [`examples/main.cpp`](examples/main.cpp):

```cpp
#include <iostream>
#include <mbs/script.hpp>
#include <nlohmann/json.hpp>

int main() {
    nlohmann::json ctx = {{"pi", 3.14}};
    auto r = mbs::eval_val("1 + 1 + @pi", ctx);
    std::cout << std::get<mbs::EvalOk>(r).value << std::endl;
}
```

Build and run: `cmake --build build --config Release && build/Release/mbs-example.exe`

---

## Further reference

Exhaustive behavior is covered by the test suites:

- [`tests/test_script.cpp`](tests/test_script.cpp) — core language, security, precompile, custom functions
- [`tests/test_extended.cpp`](tests/test_extended.cpp) — strings, collections, ternary, `in`, methods, edge cases

---

## Quick reference card

```
Literals:     42  3.14  2e2  "str"  'str'  true  false  null  [1,2]  {a:1}
Context:      @name  @obj.field  @arr[0]  @arr[-1]
Arithmetic:   + - * / % ^        "s" * 3  "a" + "b"
Compare:      == != < <= > >=
Logical:      && || !            (short-circuit, return bool)
Other:        ??  ? :  in
Access:       .member  [index]  .method(args)  .empty
Comments:     // line only
```
