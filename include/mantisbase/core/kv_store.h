/**
 * @file kv_store.h
 * @brief Key-value store for application settings.
 *
 * Manages application-wide settings stored in the database with
 * REST API endpoints for configuration management.
 */

#ifndef KV_STORE_H
#define KV_STORE_H

#include <mantisbase/core/route_registry.h>
#include <nlohmann/json.hpp>
#include "../utils/utils.h"
#include "http.h"

namespace mb
{
    class MantisBase; // forward declaration; KeyValStore holds a reference to it

    /**
     * @brief Manages application settings
     */
    class KeyValStore
    {
    public:
        /**
         * @brief Construct the settings store bound to an application.
         * @param app Owning application (db + router access). Stored by
         *        reference and must outlive this store.
         */
        explicit KeyValStore(MantisBase &app);

        /**
         * @brief Initialize and set up routes for fetching settings data
         * @return `true` if setting up routes succeeded.
         */
        bool setupRoutes();

        /**
         * @brief Initialize migration, create base data for setting fields
         */
        void migrate();

        /**
         * @brief Get the current config data instance.
         *
         * @return Config data as a JSON object
         */
        json& configs();

        /** Read a top-level config key (scripting). Returns null json when missing. */
        [[nodiscard]] json getScriptingValue(const std::string &key) const;

        /** Patch a top-level config key and persist (scripting). */
        void setScriptingValue(const std::string &key, const json &value);

        /** Return a copy of current configs (scripting). */
        [[nodiscard]] json getScriptingConfigsCopy() const;

        /** Reload configs from the database. */
        void reloadScriptingConfigs();

        [[nodiscard]] std::string getScriptingJson(const std::string &key) const;
        void setScriptingJson(const std::string &key, const std::string &json_value);
        [[nodiscard]] std::string configsScriptingJson() const;

    private:
        void setupConfigRoutes();

        json loadFromDb();
        json redactForResponse(const json &configs) const;
        void applyPatch(const json &body);

        json m_configs;

        MantisBase &mApp; ///< Owning application (injected)
    };
} // mb

#endif // KV_STORE_H
