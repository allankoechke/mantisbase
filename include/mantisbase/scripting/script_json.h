/**
 * @file script_json.h
 * @brief JSON value carried into a Duktape method without going through nlohmann::json.
 *
 * dukglue stores arguments in std::tuple via std::forward_as_tuple. Converting
 * tuple<json&&> to tuple<json> constructs the json from that tuple, and
 * nlohmann::json serializes a tuple as a one-element array. MbScriptJson is not
 * constructible from a tuple, so the argument is moved through unchanged.
 */

#ifndef MANTISBASE_SCRIPTING_SCRIPT_JSON_H
#define MANTISBASE_SCRIPTING_SCRIPT_JSON_H

#include <nlohmann/json.hpp>
#include <type_traits>
#include <utility>

namespace mb {
    struct MbScriptJson {
        nlohmann::json value;

        MbScriptJson() = default;
        MbScriptJson(const MbScriptJson &) = default;
        MbScriptJson(MbScriptJson &&) noexcept = default;
        MbScriptJson &operator=(const MbScriptJson &) = default;
        MbScriptJson &operator=(MbScriptJson &&) noexcept = default;

        template<typename T, typename = std::enable_if_t<std::is_same_v<std::decay_t<T>, nlohmann::json>>>
        explicit MbScriptJson(T &&json) : value(std::forward<T>(json)) {}
    };
}

#endif
