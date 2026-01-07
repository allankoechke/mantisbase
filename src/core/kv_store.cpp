#include "../../include/mantisbase/core/kv_store.h"
#include "../../include/mantisbase/core/route_registry.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/database.h"
#include "../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/utils/utils.h"
#include "../../include/mantisbase/core/types.h"
#include "../../include/mantisbase/core/middlewares.h"
#include "../../include/mantisbase/core/router.h"

#include <soci/soci.h>


namespace mb
{
    bool KVStore::setupRoutes()
    {
        try
        {
            setupConfigRoutes();
        }
        catch (const std::exception& e)
        {
            logger::critical(fmt::format("Error setting up settings routes: {}", e.what()));
            return false;
        }


        return true;
    }

    void KVStore::migrate()
    {
        const auto sql = MantisBase::instance().db().session();

        // Check if we have settings data already, if not so, add base settings
        json settings;
        auto id = std::to_string(std::hash<std::string>{}("configs"));
        *sql << "SELECT value FROM __settings WHERE id = :id LIMIT 1", soci::use(id), soci::into(settings);
        if (sql->got_data())
        {
            m_configs = settings;
            logger::trace(fmt::format("Config Values: ", m_configs.dump()));
        }
        // Create base data to config settings
        else
        {
            // Create default time values
            const std::time_t current_t = time(nullptr);
            std::tm* created_tm = std::localtime(&current_t);

            // Create data since it's missing
            settings.clear();
            settings["appName"] = "ACME Project";
            settings["baseUrl"] = "https://acme.example.com";
            settings["maintenanceMode"] = false;
            settings["maxFileSize"] = 10; // in MB
            settings["allowRegistration"] = true;
            settings["emailVerificationRequired"] = false;
            settings["sessionTimeout"] = 24 * 60 * 60; // 24 hours
            settings["adminSessionTimeout"] = 1 * 60 * 60; // 1 hour
            settings["mode"] = "PROD";

            *sql <<
                "INSERT INTO __settings (id, value, created, updated) VALUES (:id, :value, :created, :updated)"
                ,
                soci::use(id), soci::use(settings),
                soci::use(*created_tm), soci::use(*created_tm);
        }
    }

    HandlerResponse KVStore::hasAccess(MantisRequest& req, MantisResponse& res) const
    {
        // Get the auth var from the context, resort to empty object if it's not set.
        auto& auth = req.getOr<json>("auth", json::object());

        // Ensure auth object is present in the request's context
        if (auth.empty())
        {
            json response;
            response["status"] = 403;
            response["data"] = json::object();
            response["error"] = "Auth token missing";

            res.sendJSON(403, response);
            return REQUEST_HANDLED;
        }

        // Ensure token  is passed in
        if (auth.contains("token") && (auth["token"].is_null() || auth["token"].empty()))
        {
            json response;
            response["status"] = 403;
            response["data"] = json::object();
            response["error"] = "Auth token missing";

            res.sendJSON(403, response);
            return REQUEST_HANDLED;
        }

        // fetch auth token
        const auto& token = auth.at("token").get<std::string>();

        // Expand logged user if token is present
        const auto resp = Auth::verifyToken(token);
        if (!resp.value("verified", false) || !resp.value("error", "").empty())
        {
            json response;
            response["status"] = 403;
            response["data"] = json::object();
            response["error"] = resp.value("error", "");

            res.sendJSON(403, response);
            return REQUEST_HANDLED;
        }

        // Extract and verify that the id and table data is provided, else,
        // return an error
        const auto _id = resp.value("id", "");
        const auto _table = resp.value("table", "");

        if (_id.empty() || _table.empty())
        {
            json response;
            response["status"] = 403;
            response["data"] = json::object();
            response["error"] = "Auth token missing user id or table name";

            res.sendJSON(403, response);
            return REQUEST_HANDLED;
        }

        // Query for user with given ID, this info will be populated to the
        // expression evaluator args as well as available through
        // the session context, queried by:
        //  ` ctx.get<json>("auth").value("id", ""); // returns the user ID
        //  ` ctx.get<json>("auth").value("name", ""); // returns the user's name
        auto sql = MantisBase::instance().db().session();
        soci::row r;
        std::string query = "SELECT * FROM mb_admins WHERE id = :id LIMIT 1";
        *sql << query, soci::use(_id), soci::into(r);

        // Return 404 if user was not found
        if (!sql->got_data())
        {
            json response;
            response["status"] = 404;
            response["data"] = json::object();
            response["error"] = "Auth id was not found.";

            res.sendJSON(404, response);
            return REQUEST_HANDLED;
        }

        // Check if user is logged in as Admin
        if (_table == "mb_admins")
        {
            // If logged in as admin, grant access
            // Admins get unconditional data access
            return REQUEST_PENDING;
        }

        // User was not an admin, lets return access denied error
        json response;
        response["status"] = 403;
        response["data"] = json::object();
        response["error"] = "Admin auth required to access this resource.";

        res.sendJSON(403, response);
        return REQUEST_HANDLED;
    }

    json& KVStore::configs()
    {
        return m_configs;
    }

    json KVStore::initSettingsConfig()
    {
        // Get app session
        const auto sql = MantisBase::instance().db().session();

        // Fetch settings
        json settings;
        auto id = std::to_string(std::hash<std::string>{}("configs"));
        *sql << "SELECT value FROM _settings WHERE id = :id LIMIT 1", soci::use(id), soci::into(settings);
        if (sql->got_data())
        {
            m_configs = settings;
            return settings;
        }

        return json::object();
    }

    void KVStore::setupConfigRoutes()
    {
        // TRACE_CLASS_METHOD()

        // Set up settings get & update endpoints
        MantisBase::instance().router().Get(
            "/api/v1/settings/config",
            [this](MantisRequest& req, MantisResponse& res)
            {
                // If we have a cached config object ...
                if (!m_configs.empty())
                {
                    // Return cached config object
                    json response;
                    response["status"] = 200;
                    response["error"] = "";
                    response["data"] = m_configs;
                    response["data"]["mantisVersion"] = MantisBase::appVersion();

                    res.sendJSON(200, response);
                    return;
                }

                // Get app session
                const auto sql = MantisBase::instance().db().session();
                json response; // Response object

                // Fetch settings
                json settings;
                auto settings_id = std::to_string(std::hash<std::string>{}("settings"));
                *sql << "SELECT value FROM __settings WHERE id = :id LIMIT 1", soci::use(
                    settings_id), soci::into(settings);
                if (sql->got_data())
                {
                    // Update local cache
                    m_configs = settings;

                    // Create response object
                    response["status"] = 200;
                    response["error"] = "";
                    response["data"] = settings;
                    response["data"]["mantisVersion"] = MantisBase::appVersion();

                    res.sendJSON(200, response);
                    return;
                }

                // We didn't find any data, something is wrong!
                response["status"] = 404;
                response["error"] = "Settings object not found!";
                response["data"] = json::object();

                res.sendJSON(404, response);
            }, { });
                // [](MantisRequest& req, MantisResponse& res)-> bool
                // {
                //     return TableUnit::getAuthToken(req, res);
                // },
                // [this](MantisRequest& req, MantisResponse& res)-> bool
                // {
                //     return hasAccess(req, res);
                // }
            // });

        // Update settings config
        MantisBase::instance().router().Patch(
            "/api/v1/settings/config",
            [this](MantisRequest& req, MantisResponse& res)
            {
                // Parse request body
                json body = json::object();
                try
                {
                    body = json::parse(req.getBody());
                }
                catch (const std::exception& e)
                {
                    json response;
                    response["status"] = 400;
                    response["error"] = "Could not parse request body, expected JSON!";
                    response["data"] = json::object();

                    res.sendJSON(400, response);
                    return;
                }

                // Get app session
                const auto sql = MantisBase::instance().db().session();

                // Create base data before we update.
                if (m_configs.empty()) migrate();

                m_configs["appName"] = body.contains("appName")
                                           ? body["appName"]
                                           : m_configs["appName"];
                m_configs["baseUrl"] = body.contains("baseUrl")
                                           ? body["baseUrl"]
                                           : m_configs["baseUrl"];
                m_configs["jwtEnableSetIssuer"] = body.contains("jwtEnableSetIssuer")
                                                      ? body["jwtEnableSetIssuer"]
                                                      : m_configs["jwtEnableSetIssuer"];
                m_configs["jwtEnableSetAudience"] = body.contains("jwtEnableSetAudience")
                                                        ? body["jwtEnableSetAudience"]
                                                        : m_configs["jwtEnableSetAudience"];
                m_configs["maintenanceMode"] = body.contains("maintenanceMode")
                                                   ? body.value("maintenanceMode", false)
                                                   : m_configs.value("maintenanceMode", false);
                m_configs["maxFileSize"] = body.contains("maxFileSize")
                                               ? body.value("maxFileSize", 10)
                                               : m_configs.value("maxFileSize", 10);
                m_configs["allowRegistration"] = body.contains("allowRegistration")
                                                     ? body.value("allowRegistration", false)
                                                     : m_configs.value("allowRegistration", false);
                m_configs["emailVerificationRequired"] = body.contains("emailVerificationRequired")
                                                             ? body.value("emailVerificationRequired", false)
                                                             : m_configs.value("emailVerificationRequired", false);
                m_configs["sessionTimeout"] = body.contains("sessionTimeout")
                                                  ? body.value("sessionTimeout", 24 * 60 * 60)
                                                  : m_configs.value("sessionTimeout", 24 * 60 * 60);
                m_configs["adminSessionTimeout"] = body.contains("adminSessionTimeout")
                                                       ? body.value("adminSessionTimeout", 1 * 60 * 60)
                                                       : m_configs.value("adminSessionTimeout", 1 * 60 * 60);
                ///> TODO [Deprecated], drop it in v0.3.0
                auto mode = body.contains("mode")
                                ? body.value("mode", "PROD")
                                : m_configs.value("mode", "PROD");

                toUpperCase(mode); // Ensure mode is in upper case
                m_configs["mode"] = mode == "TEST" ? "TEST" : "PROD"; // Limit update modes to prod/test only

                // Create default time values
                const std::time_t updated_t = time(nullptr);
                std::tm* updated_tm = std::localtime(&updated_t);

                // Create config admin
                auto id = std::to_string(std::hash<std::string>{}("configs"));

                // Update config values
                *sql << "UPDATE __settings SET value = :value, updated = :updated WHERE id = :id",
                    soci::use(m_configs), soci::use(*updated_tm);

                json response;
                response["status"] = 200;
                response["error"] = "";
                response["data"] = m_configs;
                response["data"]["mantisVersion"] = MantisBase::appVersion();

                res.sendJSON(200, response);
            }, {
            });
    }
} // mantis
