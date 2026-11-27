/**
 * @file duk_json.h
 * @brief Teach dukglue to pass nlohmann::json as a JavaScript value.
 *
 * A returned JSON value is decoded onto the Duktape stack, and a JavaScript
 * argument is encoded back into JSON.
 */

#ifndef MANTISBASE_SCRIPTING_DUK_JSON_H
#define MANTISBASE_SCRIPTING_DUK_JSON_H

#include <dukglue/dukglue.h>
#include <nlohmann/json.hpp>
#include "script_json.h"

namespace dukglue {
    namespace types {
        template<>
        struct DukType<nlohmann::json> {
            typedef std::true_type IsValueType;

            template<typename FullT>
            static nlohmann::json read(duk_context *ctx, duk_idx_t arg_idx) {
                if (!(duk_is_object(ctx, arg_idx) || duk_is_string(ctx, arg_idx) || duk_is_number(ctx, arg_idx) ||
                      duk_is_boolean(ctx, arg_idx) || duk_is_null(ctx, arg_idx))) {
                    duk_error(ctx, DUK_ERR_TYPE_ERROR, "Argument %d: expected a JSON value", static_cast<int>(arg_idx));
                }

                duk_dup(ctx, arg_idx);
                const char *encoded = duk_json_encode(ctx, -1);
                const std::string text = encoded ? encoded : "null";
                duk_pop(ctx);

                try {
                    return nlohmann::json::parse(text);
                } catch (const nlohmann::json::exception &ex) {
                    duk_error(ctx, DUK_ERR_TYPE_ERROR, "%s", ex.what());
                }
                return {};
            }

            template<typename FullT>
            static void push(duk_context *ctx, const nlohmann::json &value) {
                if (value.is_null()) {
                    duk_push_null(ctx);
                    return;
                }
                const std::string text = value.dump();
                duk_push_string(ctx, text.c_str());
                duk_json_decode(ctx, -1);
            }
        };

        // Arguments use MbScriptJson. A nlohmann::json parameter is wrapped in an
        // array by dukglue's tuple conversion; see script_json.h.
        template<>
        struct DukType<mb::MbScriptJson> {
            typedef std::true_type IsValueType;

            template<typename FullT>
            static mb::MbScriptJson read(duk_context *ctx, duk_idx_t arg_idx) {
                return mb::MbScriptJson(DukType<nlohmann::json>::read<nlohmann::json>(ctx, arg_idx));
            }

            template<typename FullT>
            static void push(duk_context *ctx, const mb::MbScriptJson &value) {
                DukType<nlohmann::json>::push<nlohmann::json>(ctx, value.value);
            }
        };
    }
}

#endif
