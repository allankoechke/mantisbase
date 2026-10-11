#include "evaluator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace mbs {

namespace {

thread_local int t_eval_depth = 0;
thread_local const FunctionRegistry* t_func_registry = nullptr;

struct RegistryGuard {
  const FunctionRegistry* prev;
  explicit RegistryGuard(const FunctionRegistry* r) : prev(t_func_registry) { t_func_registry = r; }
  ~RegistryGuard() { t_func_registry = prev; }
  RegistryGuard(const RegistryGuard&) = delete;
  RegistryGuard& operator=(const RegistryGuard&) = delete;
};

struct DepthGuard {
  DepthGuard() {
    if (++t_eval_depth > MAX_EVAL_DEPTH) {
      --t_eval_depth;
      throw EvalError("evaluation depth exceeded (limit 512)");
    }
  }
  ~DepthGuard() { --t_eval_depth; }
  DepthGuard(const DepthGuard&) = delete;
  DepthGuard& operator=(const DepthGuard&) = delete;
};

bool is_integral_number(const nlohmann::json& j, std::int64_t* out) {
  if (!j.is_number()) {
    return false;
  }
  double d = j.get<double>();
  if (std::isnan(d) || std::isinf(d)) {
    return false;
  }
  double intpart = 0;
  if (std::modf(d, &intpart) != 0.0) {
    return false;
  }
  if (intpart < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
      intpart >= static_cast<double>(std::numeric_limits<std::int64_t>::max()) + 1.0) {
    return false;
  }
  *out = static_cast<std::int64_t>(intpart);
  return true;
}

std::optional<std::int64_t> repeat_count(const nlohmann::json& j) {
  std::int64_t v = 0;
  if (!is_integral_number(j, &v)) {
    return std::nullopt;
  }
  return v;
}

nlohmann::json repeat_string(const std::string& s, std::int64_t c) {
  if (c <= 0) {
    return std::string();
  }
  if (s.empty()) {
    return std::string();
  }
  if (c > 0 && s.size() > MAX_STRING_SIZE / static_cast<std::size_t>(c)) {
    return nullptr;
  }
  std::string out;
  out.reserve(s.size() * static_cast<std::size_t>(c));
  for (std::int64_t i = 0; i < c; ++i) {
    out += s;
  }
  return out;
}

nlohmann::json json_empty_prop(const nlohmann::json& v) {
  if (v.is_null()) {
    return true;
  }
  if (v.is_boolean()) {
    return false;
  }
  if (v.is_number()) {
    return false;
  }
  if (v.is_string()) {
    return v.get_ref<const std::string&>().empty();
  }
  if (v.is_array()) {
    return v.empty();
  }
  if (v.is_object()) {
    return v.empty();
  }
  return nullptr;
}

nlohmann::json builtin_len(const nlohmann::json& v) {
  if (v.is_string()) {
    return static_cast<nlohmann::json::number_unsigned_t>(v.get_ref<const std::string&>().size());
  }
  if (v.is_array()) {
    return static_cast<nlohmann::json::number_unsigned_t>(v.size());
  }
  if (v.is_number()) {
    return v;
  }
  return nullptr;
}

nlohmann::json builtin_size(const nlohmann::json& v) {
  if (v.is_object() || v.is_array()) {
    return static_cast<nlohmann::json::number_unsigned_t>(v.size());
  }
  if (v.is_string()) {
    return static_cast<nlohmann::json::number_unsigned_t>(v.get_ref<const std::string&>().size());
  }
  return nullptr;
}

nlohmann::json builtin_has(const nlohmann::json& v, const nlohmann::json& arg) {
  if (v.is_object()) {
    if (!arg.is_string()) {
      return nullptr;
    }
    const std::string& k = arg.get_ref<const std::string&>();
    return v.contains(k);
  }
  if (v.is_array()) {
    for (const auto& el : v) {
      if (el == arg) {
        return true;
      }
    }
    return false;
  }
  return nullptr;
}

nlohmann::json call_method(const nlohmann::json& v, const std::string& name,
                           const std::vector<nlohmann::json>& args) {
  if (name == "has") {
    if (args.size() != 1U) {
      return nullptr;
    }
    return builtin_has(v, args[0]);
  }
  if (name == "size") {
    if (args.size() != 0U) {
      return nullptr;
    }
    return builtin_size(v);
  }
  if (name == "len") {
    if (args.size() != 0U) {
      return nullptr;
    }
    return builtin_len(v);
  }
  if (name == "type") {
    if (args.size() != 0U) {
      return nullptr;
    }
    if (v.is_null()) return std::string("null");
    if (v.is_boolean()) return std::string("boolean");
    if (v.is_number()) return std::string("number");
    if (v.is_string()) return std::string("string");
    if (v.is_array()) return std::string("array");
    if (v.is_object()) return std::string("object");
    return nullptr;
  }
  if (name == "contains") {
    if (args.size() != 1U || !v.is_string() || !args[0].is_string()) {
      return nullptr;
    }
    const auto& s = v.get_ref<const std::string&>();
    const auto& needle = args[0].get_ref<const std::string&>();
    return s.find(needle) != std::string::npos;
  }
  if (name == "startsWith") {
    if (args.size() != 1U || !v.is_string() || !args[0].is_string()) {
      return nullptr;
    }
    const auto& s = v.get_ref<const std::string&>();
    const auto& prefix = args[0].get_ref<const std::string&>();
    if (prefix.size() > s.size()) return false;
    return s.compare(0, prefix.size(), prefix) == 0;
  }
  if (name == "endsWith") {
    if (args.size() != 1U || !v.is_string() || !args[0].is_string()) {
      return nullptr;
    }
    const auto& s = v.get_ref<const std::string&>();
    const auto& suffix = args[0].get_ref<const std::string&>();
    if (suffix.size() > s.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
  }
  if (name == "toLowerCase") {
    if (args.size() != 0U || !v.is_string()) {
      return nullptr;
    }
    std::string result = v.get<std::string>();
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
  }
  if (name == "toUpperCase") {
    if (args.size() != 0U || !v.is_string()) {
      return nullptr;
    }
    std::string result = v.get<std::string>();
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
  }
  if (name == "trim") {
    if (args.size() != 0U || !v.is_string()) {
      return nullptr;
    }
    std::string result = v.get<std::string>();
    auto start = std::find_if_not(result.begin(), result.end(),
                                  [](unsigned char c) { return std::isspace(c); });
    auto end = std::find_if_not(result.rbegin(), result.rend(),
                                [](unsigned char c) { return std::isspace(c); }).base();
    if (start >= end) return std::string();
    return std::string(start, end);
  }
  if (name == "indexOf") {
    if (args.size() != 1U || !v.is_string() || !args[0].is_string()) {
      return nullptr;
    }
    const auto& s = v.get_ref<const std::string&>();
    const auto& needle = args[0].get_ref<const std::string&>();
    auto pos = s.find(needle);
    if (pos == std::string::npos) return static_cast<std::int64_t>(-1);
    return static_cast<std::int64_t>(pos);
  }
  if (name == "replace") {
    if (args.size() != 2U || !v.is_string() || !args[0].is_string() || !args[1].is_string()) {
      return nullptr;
    }
    std::string result = v.get<std::string>();
    const auto& from = args[0].get_ref<const std::string&>();
    const auto& to = args[1].get_ref<const std::string&>();
    auto pos = result.find(from);
    if (pos != std::string::npos) {
      result.replace(pos, from.size(), to);
    }
    return result;
  }
  if (name == "substring") {
    if (!v.is_string()) {
      return nullptr;
    }
    const auto& s = v.get_ref<const std::string&>();
    if (args.size() == 1U) {
      std::int64_t start = 0;
      if (!is_integral_number(args[0], &start)) return nullptr;
      if (start < 0) start = 0;
      if (static_cast<std::size_t>(start) >= s.size()) return std::string();
      return s.substr(static_cast<std::size_t>(start));
    }
    if (args.size() == 2U) {
      std::int64_t start = 0;
      std::int64_t length = 0;
      if (!is_integral_number(args[0], &start)) return nullptr;
      if (!is_integral_number(args[1], &length)) return nullptr;
      if (start < 0) start = 0;
      if (length < 0) return std::string();
      if (static_cast<std::size_t>(start) >= s.size()) return std::string();
      return s.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(length));
    }
    return nullptr;
  }
  if (name == "split") {
    if (args.size() != 1U || !v.is_string() || !args[0].is_string()) {
      return nullptr;
    }
    const auto& s = v.get_ref<const std::string&>();
    const auto& delim = args[0].get_ref<const std::string&>();
    nlohmann::json arr = nlohmann::json::array();
    if (delim.empty()) {
      for (char c : s) {
        arr.push_back(std::string(1, c));
      }
      return arr;
    }
    std::size_t pos = 0;
    std::size_t found = 0;
    while ((found = s.find(delim, pos)) != std::string::npos) {
      arr.push_back(s.substr(pos, found - pos));
      pos = found + delim.size();
    }
    arr.push_back(s.substr(pos));
    return arr;
  }
  if (name == "keys") {
    if (args.size() != 0U || !v.is_object()) {
      return nullptr;
    }
    nlohmann::json arr = nlohmann::json::array();
    for (auto it = v.begin(); it != v.end(); ++it) {
      arr.push_back(it.key());
    }
    return arr;
  }
  if (name == "values") {
    if (args.size() != 0U || !v.is_object()) {
      return nullptr;
    }
    nlohmann::json arr = nlohmann::json::array();
    for (auto it = v.begin(); it != v.end(); ++it) {
      arr.push_back(it.value());
    }
    return arr;
  }
  if (t_func_registry) {
    auto it = t_func_registry->find(name);
    if (it != t_func_registry->end()) {
      return it->second(v, args);
    }
  }
  return nullptr;
}

nlohmann::json member_access(const nlohmann::json& v, const std::string& name) {
  if (v.is_null()) {
    return nullptr;
  }
  if (name == "empty") {
    return json_empty_prop(v);
  }
  if (v.is_object()) {
    if (!v.contains(name)) {
      return nullptr;
    }
    return v.at(name);
  }
  return nullptr;
}

nlohmann::json index_access(const nlohmann::json& v, const nlohmann::json& key) {
  if (v.is_null()) {
    return nullptr;
  }
  if (v.is_array()) {
    std::int64_t i = 0;
    if (!is_integral_number(key, &i)) {
      return nullptr;
    }
    if (i < 0) {
      i = static_cast<std::int64_t>(v.size()) + i;
    }
    if (i < 0 || static_cast<std::size_t>(i) >= v.size()) {
      return nullptr;
    }
    return v[static_cast<nlohmann::json::size_type>(i)];
  }
  if (v.is_string()) {
    std::int64_t i = 0;
    if (!is_integral_number(key, &i)) {
      return nullptr;
    }
    const std::string& s = v.get_ref<const std::string&>();
    if (i < 0) {
      i = static_cast<std::int64_t>(s.size()) + i;
    }
    if (i < 0 || static_cast<std::size_t>(i) >= s.size()) {
      return nullptr;
    }
    return std::string(1, s[static_cast<std::size_t>(i)]);
  }
  if (v.is_object() && key.is_string()) {
    const std::string& k = key.get_ref<const std::string&>();
    if (!v.contains(k)) {
      return nullptr;
    }
    return v.at(k);
  }
  return nullptr;
}

nlohmann::json eval_chain(const ast::Expr& expr, const nlohmann::json& context) {
  nlohmann::json v = eval_expr(*expr.chainBase, context);
  for (const ast::Segment& seg : expr.segments) {
    if (seg.kind == ast::Segment::Kind::Member) {
      v = member_access(v, seg.name);
    } else if (seg.kind == ast::Segment::Kind::Method) {
      std::vector<nlohmann::json> args;
      args.reserve(seg.args.size());
      for (const auto& a : seg.args) {
        args.push_back(eval_expr(*a, context));
      }
      v = call_method(v, seg.name, args);
    } else if (seg.kind == ast::Segment::Kind::Index) {
      nlohmann::json key = eval_expr(*seg.indexExpr, context);
      v = index_access(v, key);
    }
  }
  return v;
}

nlohmann::json eval_binary_shortcircuit(ast::BinOp op, const ast::Expr& expr, const nlohmann::json& context) {
  nlohmann::json l = eval_expr(*expr.left, context);
  if (op == ast::BinOp::And) {
    if (!json_truthy(l)) {
      return false;
    }
    return nlohmann::json(json_truthy(eval_expr(*expr.right, context)));
  }
  if (op == ast::BinOp::Or) {
    if (json_truthy(l)) {
      return true;
    }
    return nlohmann::json(json_truthy(eval_expr(*expr.right, context)));
  }
  if (op == ast::BinOp::NullCoalesce) {
    if (!l.is_null()) {
      return l;
    }
    return eval_expr(*expr.right, context);
  }
  nlohmann::json r = eval_expr(*expr.right, context);
  switch (op) {
    case ast::BinOp::Eq:
      return l == r;
    case ast::BinOp::Ne:
      return l != r;
    case ast::BinOp::Lt:
      if (l.is_number() && r.is_number()) {
        return l.get<double>() < r.get<double>();
      }
      if (l.is_string() && r.is_string()) {
        return l.get_ref<const std::string&>() < r.get_ref<const std::string&>();
      }
      return nullptr;
    case ast::BinOp::Le:
      if (l.is_number() && r.is_number()) {
        return l.get<double>() <= r.get<double>();
      }
      if (l.is_string() && r.is_string()) {
        return l.get_ref<const std::string&>() <= r.get_ref<const std::string&>();
      }
      return nullptr;
    case ast::BinOp::Gt:
      if (l.is_number() && r.is_number()) {
        return l.get<double>() > r.get<double>();
      }
      if (l.is_string() && r.is_string()) {
        return l.get_ref<const std::string&>() > r.get_ref<const std::string&>();
      }
      return nullptr;
    case ast::BinOp::Ge:
      if (l.is_number() && r.is_number()) {
        return l.get<double>() >= r.get<double>();
      }
      if (l.is_string() && r.is_string()) {
        return l.get_ref<const std::string&>() >= r.get_ref<const std::string&>();
      }
      return nullptr;
    case ast::BinOp::Add:
      if (l.is_number() && r.is_number()) {
        double result = l.get<double>() + r.get<double>();
        if (!std::isfinite(result)) return nullptr;
        return result;
      }
      if (l.is_string() && r.is_string()) {
        std::string result = l.get_ref<const std::string&>() + r.get_ref<const std::string&>();
        if (result.size() > MAX_STRING_SIZE) return nullptr;
        return result;
      }
      return nullptr;
    case ast::BinOp::Sub:
      if (l.is_number() && r.is_number()) {
        double result = l.get<double>() - r.get<double>();
        if (!std::isfinite(result)) return nullptr;
        return result;
      }
      return nullptr;
    case ast::BinOp::Mul: {
      if (l.is_number() && r.is_number()) {
        double result = l.get<double>() * r.get<double>();
        if (!std::isfinite(result)) return nullptr;
        return result;
      }
      if (l.is_string() && r.is_number()) {
        auto n = repeat_count(r);
        if (!n.has_value()) return nullptr;
        return repeat_string(l.get_ref<const std::string&>(), *n);
      }
      if (l.is_number() && r.is_string()) {
        auto n = repeat_count(l);
        if (!n.has_value()) return nullptr;
        return repeat_string(r.get_ref<const std::string&>(), *n);
      }
      return nullptr;
    }
    case ast::BinOp::Div:
      if (l.is_number() && r.is_number()) {
        double rd = r.get<double>();
        if (rd == 0.0) {
          return nullptr;
        }
        double result = l.get<double>() / rd;
        if (!std::isfinite(result)) return nullptr;
        return result;
      }
      return nullptr;
    case ast::BinOp::Mod:
      if (l.is_number() && r.is_number()) {
        double rd = r.get<double>();
        if (rd == 0.0) {
          return nullptr;
        }
        double result = std::fmod(l.get<double>(), rd);
        if (!std::isfinite(result)) return nullptr;
        return result;
      }
      return nullptr;
    case ast::BinOp::Pow:
      if (l.is_number() && r.is_number()) {
        double result = std::pow(l.get<double>(), r.get<double>());
        if (!std::isfinite(result)) return nullptr;
        return result;
      }
      return nullptr;
    case ast::BinOp::In:
      if (r.is_array()) {
        for (const auto& el : r) {
          if (el == l) {
            return true;
          }
        }
        return false;
      }
      if (r.is_object()) {
        if (!l.is_string()) {
          return nullptr;
        }
        return r.contains(l.get_ref<const std::string&>());
      }
      return nullptr;
    default:
      return nullptr;
  }
}

}  // namespace

bool json_truthy(const nlohmann::json& j) {
  if (j.is_null()) {
    return false;
  }
  if (j.is_boolean()) {
    return j.get<bool>();
  }
  if (j.is_number()) {
    return j.get<double>() != 0.0;
  }
  if (j.is_string()) {
    return !j.get_ref<const std::string&>().empty();
  }
  if (j.is_array()) {
    return !j.empty();
  }
  if (j.is_object()) {
    return !j.empty();
  }
  return false;
}

nlohmann::json eval_expr(const ast::Expr& expr, const nlohmann::json& context) {
  DepthGuard dg;
  switch (expr.kind) {
    case ast::Expr::Kind::Literal:
      return expr.literal;
    case ast::Expr::Kind::ContextVar: {
      if (!context.is_object()) {
        return nullptr;
      }
      if (!context.contains(expr.ctxName)) {
        return nullptr;
      }
      return context.at(expr.ctxName);
    }
    case ast::Expr::Kind::Unary: {
      nlohmann::json c = eval_expr(*expr.child, context);
      if (expr.unOp == ast::UnOp::Not) {
        return !json_truthy(c);
      }
      if (expr.unOp == ast::UnOp::Neg) {
        if (!c.is_number()) {
          return nullptr;
        }
        double result = -c.get<double>();
        if (!std::isfinite(result)) return nullptr;
        return result;
      }
      return nullptr;
    }
    case ast::Expr::Kind::Binary:
      return eval_binary_shortcircuit(expr.binOp, expr, context);
    case ast::Expr::Kind::Ternary: {
      nlohmann::json cond = eval_expr(*expr.left, context);
      if (json_truthy(cond)) {
        return eval_expr(*expr.right, context);
      }
      return eval_expr(*expr.elseExpr, context);
    }
    case ast::Expr::Kind::Chain:
      return eval_chain(expr, context);
    case ast::Expr::Kind::Array: {
      nlohmann::json arr = nlohmann::json::array();
      for (const auto& el : expr.arrayElems) {
        arr.push_back(eval_expr(*el, context));
      }
      return arr;
    }
    case ast::Expr::Kind::Object: {
      nlohmann::json obj = nlohmann::json::object();
      for (const auto& p : expr.objectPairs) {
        obj[p.key] = eval_expr(*p.value, context);
      }
      return obj;
    }
    default:
      return nullptr;
  }
}

namespace {

std::optional<std::string> semantic_check_impl(const ast::Expr& expr,
                                               const FunctionRegistry* funcs) {
  switch (expr.kind) {
    case ast::Expr::Kind::Literal:
    case ast::Expr::Kind::ContextVar:
      return std::nullopt;
    case ast::Expr::Kind::Unary: {
      return semantic_check_impl(*expr.child, funcs);
    }
    case ast::Expr::Kind::Binary: {
      auto a = semantic_check_impl(*expr.left, funcs);
      if (a.has_value()) {
        return a;
      }
      return semantic_check_impl(*expr.right, funcs);
    }
    case ast::Expr::Kind::Ternary: {
      auto a = semantic_check_impl(*expr.left, funcs);
      if (a.has_value()) return a;
      auto b = semantic_check_impl(*expr.right, funcs);
      if (b.has_value()) return b;
      return semantic_check_impl(*expr.elseExpr, funcs);
    }
    case ast::Expr::Kind::Chain: {
      if (expr.chainBase) {
        auto b = semantic_check_impl(*expr.chainBase, funcs);
        if (b.has_value()) {
          return b;
        }
      }
      for (const ast::Segment& seg : expr.segments) {
        if (seg.kind == ast::Segment::Kind::Method) {
          static const std::vector<std::string> zero_arg_methods = {
            "size", "len", "type", "toLowerCase", "toUpperCase", "trim", "keys", "values"
          };
          static const std::vector<std::string> one_arg_methods = {
            "has", "contains", "startsWith", "endsWith", "indexOf", "split"
          };
          static const std::vector<std::string> two_arg_methods = {
            "replace"
          };
          static const std::vector<std::string> variable_arg_methods = {
            "substring"
          };
          bool known = false;
          for (const auto& m : zero_arg_methods) {
            if (seg.name == m) {
              known = true;
              if (seg.args.size() != 0U) {
                return seg.name + "() expects no arguments";
              }
              break;
            }
          }
          if (!known) {
            for (const auto& m : one_arg_methods) {
              if (seg.name == m) {
                known = true;
                if (seg.args.size() != 1U) {
                  return seg.name + "() expects 1 argument, got " + std::to_string(seg.args.size());
                }
                break;
              }
            }
          }
          if (!known) {
            for (const auto& m : two_arg_methods) {
              if (seg.name == m) {
                known = true;
                if (seg.args.size() != 2U) {
                  return seg.name + "() expects 2 arguments, got " + std::to_string(seg.args.size());
                }
                break;
              }
            }
          }
          if (!known) {
            for (const auto& m : variable_arg_methods) {
              if (seg.name == m) {
                known = true;
                if (seg.args.size() < 1U || seg.args.size() > 2U) {
                  return seg.name + "() expects 1 or 2 arguments, got " + std::to_string(seg.args.size());
                }
                break;
              }
            }
          }
          if (!known && funcs && funcs->count(seg.name)) {
            known = true;
          }
          if (!known) {
            return std::string("unknown method: ") + seg.name;
          }
          for (const auto& a : seg.args) {
            auto e = semantic_check_impl(*a, funcs);
            if (e.has_value()) {
              return e;
            }
          }
        } else if (seg.kind == ast::Segment::Kind::Index) {
          return semantic_check_impl(*seg.indexExpr, funcs);
        }
      }
      return std::nullopt;
    }
    case ast::Expr::Kind::Array: {
      for (const auto& el : expr.arrayElems) {
        auto e = semantic_check_impl(*el, funcs);
        if (e.has_value()) {
          return e;
        }
      }
      return std::nullopt;
    }
    case ast::Expr::Kind::Object: {
      for (const auto& p : expr.objectPairs) {
        auto e = semantic_check_impl(*p.value, funcs);
        if (e.has_value()) {
          return e;
        }
      }
      return std::nullopt;
    }
    default:
      return std::nullopt;
  }
}

}  // namespace

std::optional<std::string> semantic_check(const ast::Expr& expr) {
  return semantic_check_impl(expr, nullptr);
}

std::optional<std::string> semantic_check(const ast::Expr& expr, const FunctionRegistry& funcs) {
  return semantic_check_impl(expr, &funcs);
}

nlohmann::json eval_expr(const ast::Expr& expr, const nlohmann::json& context,
                         const FunctionRegistry& funcs) {
  RegistryGuard rg(&funcs);
  return eval_expr(expr, context);
}

}  // namespace mbs
