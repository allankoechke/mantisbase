//
// Created by allan on 18/10/2025.
//

#ifndef MANTISAPP_CONTEXTSTORE_H
#define MANTISAPP_CONTEXTSTORE_H

#include <nlohmann/json.hpp>
#include "../utils/utils.h"

#ifdef MANTIS_ENABLE_SCRIPTING
#include <dukglue/dukglue.h>
#endif

namespace mantis
{
    /// Shorten JSON namespace
    using json = nlohmann::json;

    /**
     * The `ContextStore` class provides a means to set/get a key-value data that can be shared uniquely between middlewares
     * and the handler functions. This allows sending data down the chain from the first to the last handler.
     *
     * For instance, the auth middleware will inject user `id` and subsequent middlewares can retrieve it as needed.
     *
     * @code
     * // Create the object
     * Context ctx;
     *
     * // Add values
     * ctx.set<std::string>("key", "Value");
     * ctx.set<int>("id", 967567);
     * ctx.set<bool>("verified", true);
     *
     * // Retrieve values
     * std::optional key = ctx.get<std::string>("key");
     *
     * // From scripting using JS
     * req.set("key", 5") // INT/DOUBLE/FLOATS ...
     * req.set("key2", { a: 5, b: 7}) // Objects/JSON
     * req.set("valid", true) // BOOLs
     *
     * req.get("key") // -> Return 5
     * req.get("nothing") // -> undefined
     * req.getOr("nothing", "Default Value")
     * @endcode
     *
     * The value returned from the `get()` is a std::optional, meaning a std::nullopt if the key was not found.
     * @code
     * std::optional key = ctx.get<std::string>("key");
     * if(key.has_value()) { .... }
     * @endcode
     *
     * Additionally, we have a @see get_or() method that takes in a key and a default value if the key is missing. This
     * unlike @see get() method, returns a `T&` instead of `T*` depending on the usage needs.
     */
    class ContextStore
    {
        std::unordered_map<std::string, std::any> data;
        std::string __class_name__ = "mantis::ContextStore";

    public:
        ContextStore() = default;
        /**
         * @brief Convenience method for dumping context data for debugging.
         */
        void dump();

        /**
         *
         * @param key Key to check in context store
         * @return `true` if key exists, else, `false`
         */
        bool hasKey(const std::string& key) const;

        /**
         * @brief Store a key-value data in the context
         *
         * @tparam T Value data type
         * @param key Value key
         * @param value Value to be stored
         */
        template <typename T>
        void set(const std::string& key, T value)
        {
            data[key] = std::move(value);
        }

        /**
         * @brief Get context value given the key.
         *
         * @tparam T Value data type
         * @param key Value key
         * @return Value wrapped in a std::optional
         */
        template <typename T>
        std::optional<T*> get(const std::string& key)
        {
            const auto it = data.find(key);
            if (it != data.end())
                return std::any_cast<T>(&it->second);

            return std::nullopt;
        }

        /**
         * @brief Get context value given the key.
         *
         * @tparam T Value data type
         * @param key Value key
         * @param default_value Default value if key is missing
         * @return Value or default value
         */
        template <typename T>
        T& getOr(const std::string& key, T default_value)
        {
            if (const auto it = data.find(key); it == data.end())
            {
                data[key] = std::move(default_value);
            }
            return std::any_cast<T&>(data.at(key));
        }

#ifdef MANTIS_ENABLE_SCRIPTING
        DukValue get_duk(const std::string& key);

        /**
         * Get value from context store if available or return default_value instead.
         *
         * @param key Key to check in the store
         * @param default_value Default value to pass if missing
         * @return Value of type `DukValue` corresponding to found value or default value
         */
        DukValue getOr_duk(const std::string& key, DukValue default_value);

        /**
         * @brief Store a value in store with given `key` and `value`
         *
         * @param key Key to store in context store
         * @param value Value to correspond to given `key`
         */
        void set_duk(const std::string& key, const DukValue& value);
#endif
    };

} // mantis

#endif //MANTISAPP_CONTEXTSTORE_H