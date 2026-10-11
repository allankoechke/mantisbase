#include <iostream>
#include <mbs/script.hpp>

#include <nlohmann/json.hpp>

using mbs::EvalErr;
using mbs::EvalOk;
using mbs::eval_true;
using mbs::eval_val;
using mbs::test;
using nlohmann::json;

int main() {
    const std::string src = "1+1+@pi";
    json ctx = json::object();
    ctx["pi"] = 3.14;
    const auto r = eval_val(src, ctx);
    std::cout << "Res: " << std::holds_alternative<EvalOk>(r) << std::endl;
    std::cout << "Res: " << std::get<EvalOk>(r).value << std::endl;
    std::cout << "@pi = " << std::get<EvalOk>(eval_val("@pi", ctx)).value << std::endl;
    std::cout << "@pi = " << eval_true("@pi", ctx).value << std::endl;
    return 0;
}