#include "../../include/mantisbase/core/expr_evaluator.h"

#include "httplib.h"
#include "../../include/mantisbase/core/logger.h"

namespace mantis
{
    bool ExprMgr::evaluate(const std::string& expr, const std::vector<json>& vars)
    {
        try
        {
            // const packToken result = calculator::calculate(expr.c_str(), vars);
            return false; // result.asBool(); // true/false
        } catch (std::exception& e)
        {
            logger::critical("Error evaluating expression '{}', error: {}", expr, e.what());
            return false;
        }
    }

    bool ExprMgr::evaluate(const std::string& expr, const json& vars)
    {
        // const auto t_vars = jsonToTokenMap(vars);
        return false; //evaluate(expr, t_vars);
    }

    // TokenMap ExprMgr::jsonToTokenMap(const json& j)
    // {
    //     cparse::TokenMap map;
    //
    //     for (const auto& [key, value] : j.items())
    //     {
    //         if (value.is_null())
    //         {
    //             map[key] = cparse::packToken::None();
    //         }
    //         else if (value.is_boolean())
    //         {
    //             map[key] = value.get<bool>();
    //         }
    //         else if (value.is_number_integer())
    //         {
    //             map[key] = value.get<int64_t>();
    //         }
    //         else if (value.is_number_float())
    //         {
    //             map[key] = value.get<double>();
    //         }
    //         else if (value.is_string())
    //         {
    //             map[key] = value.get<std::string>();
    //         }
    //         else if (value.is_object())
    //         {
    //             map[key] = jsonToTokenMap(value); // Recursive conversion
    //         }
    //         else if (value.is_array())
    //         {
    //             cparse::TokenList list;
    //             for (const auto& item : value)
    //             {
    //                 if (item.is_object())
    //                 {
    //                     list.push(jsonToTokenMap(item));
    //                 }
    //                 else
    //                 {
    //                     // Convert primitive types
    //                     if (item.is_string()) list.push(item.get<std::string>());
    //                     else if (item.is_number_integer()) list.push(item.get<int64_t>());
    //                     else if (item.is_number_float()) list.push(item.get<double>());
    //                     else if (item.is_boolean()) list.push(item.get<bool>());
    //                     else if (item.is_null()) list.push(cparse::packToken::None());
    //                 }
    //             }
    //             map[key] = list;
    //         }
    //     }
    //
    //     return map;
    // };
} // mantis
