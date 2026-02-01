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
    /**
     * @brief Manages application settings
     */
    class KeyValStore
    {
    public:
        KeyValStore() = default;

        void close() {}

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
         * @brief Evaluate if request is authenticated and has permission to access this route.
         *
         * This route is exclusive to admin login only!
         *
         * @param req HTTP request
         * @param res HTTP response
         * @param ctx HTTP context
         * @return `true` if access is granted, else, `false`
         */
        HandlerResponse hasAccess([[maybe_unused]] MantisRequest& req, MantisResponse& res) const;

        // Getter sections
        /**
         * @brief Get the current config data instance.
         *
         * @return Config data as a JSON object
         */
        json& configs();

    private:
        /**
         * @brief Fetch config data from database, initializing it to defaults if not existing yet!
         *
         * @return json object having the config, or empty json object.
         */
        json initSettingsConfig();

        /**
         * @brief Called by @see setupRoutes() to initialize routes specific to settings config only!
         */
        void setupConfigRoutes();

        // Cache settings config on create/read/update cycles to reduce database reads
        // may not be that significant though...!
        json m_configs;
    };
} // mb

#endif // KV_STORE_H
