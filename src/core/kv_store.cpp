#include "../../include/mantisbase/core/kv_store.h"
#include "../../include/mantisbase/core/route_registry.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/database.h"
#include "../../include/mantisbase/core/exceptions.h"
#include "../../include/mantisbase/utils/utils.h"
#include "../../include/mantisbase/core/types.h"
#include "../../include/mantisbase/core/middlewares.h"
#include "../../include/mantisbase/core/router.h"

#include <soci/soci.h>

namespace mb
{
    namespace
    {
        constexpr int kDefaultMaxFileSize = 10 * 1024 * 1024;
        constexpr int kDefaultLogRetentionDays = 5;

        std::string configRowId()
        {
            return std::to_string(std::hash<std::string>{}("configs"));
        }

        json defaultSmtp()
        {
            return {
                {"host", ""},
                {"port", 587},
                {"user", ""},
                {"password", ""},
                {"from", ""},
                {"tls", true}
            };
        }

        json defaultSettings()
        {
            return {
                {"orgName", "ACME Corp"},
                {"siteDomain", "https://acme.example.com"},
                {"corsAllowedOrigins", json::array({
                    "http://localhost:3000",
                    "http://127.0.0.1:3000"
                })},
                {"maxFileSize", kDefaultMaxFileSize},
                {"logRetentionDays", kDefaultLogRetentionDays},
                {"disableAdminRegistration", false},
                {"disableSchemaMutations", false},
                {"emailVerificationRequired", false},
                {"sessionTimeout", 24 * 60 * 60},
                {"adminSessionTimeout", 1 * 60 * 60},
                {"jwtEnableSetIssuer", false},
                {"jwtEnableSetAudience", false},
                {"smtp", defaultSmtp()}
            };
        }
    }

    KeyValStore::KeyValStore(MantisBase &app) : mApp(app) {}

    bool KeyValStore::setupRoutes()
    {
        try
        {
            setupConfigRoutes();
        }
        catch (const std::exception &e)
        {
            mApp.logger().critical("Route Setup Error",
                                   fmt::format("Error setting up settings routes: {}", e.what()));
            return false;
        }

        return true;
    }

    void KeyValStore::migrate()
    {
        const auto sql = mApp.db().session();
        const auto id = configRowId();

        json settings;
        *sql << "SELECT value FROM mb_store WHERE id = :id LIMIT 1", soci::use(id), soci::into(settings);
        if (sql->got_data())
        {
            m_configs = settings;
            mApp.logger().trace("Config Loaded", "Application settings loaded from database.");
            return;
        }

        const std::time_t current_t = time(nullptr);
        const std::tm created_tm = toUtcTime(current_t);
        settings = defaultSettings();

        *sql << "INSERT INTO mb_store (id, value, created, updated) VALUES (:id, :value, :created, :updated)",
            soci::use(id), soci::use(settings), soci::use(created_tm), soci::use(created_tm);

        m_configs = settings;
        mApp.logger().info("Config Initialized", "Created default application settings.");
    }

    json &KeyValStore::configs()
    {
        return m_configs;
    }

    json KeyValStore::getScriptingValue(const std::string &key) const
    {
        if (m_configs.contains(key)) {
            return m_configs.at(key);
        }
        return json();
    }

    void KeyValStore::setScriptingValue(const std::string &key, const json &value)
    {
        applyPatch(json{{key, value}});
    }

    json KeyValStore::getScriptingConfigsCopy() const
    {
        return m_configs;
    }

    void KeyValStore::reloadScriptingConfigs()
    {
        m_configs = loadFromDb();
    }

    std::string KeyValStore::getScriptingJson(const std::string &key) const
    {
        return getScriptingValue(key).dump();
    }

    void KeyValStore::setScriptingJson(const std::string &key, const std::string &json_value)
    {
        setScriptingValue(key, json::parse(json_value));
    }

    std::string KeyValStore::configsScriptingJson() const
    {
        return getScriptingConfigsCopy().dump();
    }

    json KeyValStore::loadFromDb()
    {
        if (!m_configs.empty())
        {
            return m_configs;
        }

        const auto sql = mApp.db().session();
        const auto id = configRowId();
        json settings;
        *sql << "SELECT value FROM mb_store WHERE id = :id LIMIT 1", soci::use(id), soci::into(settings);
        if (sql->got_data())
        {
            m_configs = settings;
            return settings;
        }

        return json::object();
    }

    json KeyValStore::redactForResponse(const json &configs) const
    {
        auto data = configs;
        if (data.contains("smtp") && data["smtp"].is_object())
        {
            auto &smtp = data["smtp"];
            if (smtp.contains("password") && smtp["password"].is_string()
                && !smtp["password"].get<std::string>().empty())
            {
                smtp["password"] = "********";
            }
        }
        return data;
    }

    void KeyValStore::applyPatch(const json &body)
    {
        if (m_configs.empty())
        {
            migrate();
        }

        const auto merge_scalar = [&]<typename T>(const char *key, T default_val) {
            if (body.contains(key))
            {
                m_configs[key] = body.value(key, default_val);
            }
        };

        merge_scalar("orgName", std::string{});
        merge_scalar("siteDomain", std::string{});
        merge_scalar("disableAdminRegistration", false);
        merge_scalar("disableSchemaMutations", false);
        merge_scalar("emailVerificationRequired", false);
        merge_scalar("sessionTimeout", 24 * 60 * 60);
        merge_scalar("adminSessionTimeout", 1 * 60 * 60);
        merge_scalar("jwtEnableSetIssuer", false);
        merge_scalar("jwtEnableSetAudience", false);

        if (body.contains("maxFileSize"))
        {
            const auto size = body.value("maxFileSize", kDefaultMaxFileSize);
            if (!body["maxFileSize"].is_number_integer() || size <= 0)
            {
                throw MantisException(400, "maxFileSize must be a positive integer (bytes).");
            }
            m_configs["maxFileSize"] = size;
        }

        if (body.contains("logRetentionDays"))
        {
            const auto days = body.value("logRetentionDays", kDefaultLogRetentionDays);
            if (!body["logRetentionDays"].is_number_integer() || days <= 0)
            {
                throw MantisException(400, "logRetentionDays must be a positive integer (days).");
            }
            m_configs["logRetentionDays"] = days;
        }

        if (body.contains("smtp") && body["smtp"].is_object())
        {
            if (!m_configs.contains("smtp") || !m_configs["smtp"].is_object())
            {
                m_configs["smtp"] = defaultSmtp();
            }

            const auto &patch_smtp = body["smtp"];
            auto &smtp = m_configs["smtp"];

            for (const auto &[key, value]: patch_smtp.items())
            {
                if (key == "password")
                {
                    if (value.is_string() && value.get<std::string>() == "********")
                    {
                        continue;
                    }
                }
                smtp[key] = value;
            }
        }

        if (body.contains("corsAllowedOrigins"))
        {
            if (!body["corsAllowedOrigins"].is_array())
            {
                throw MantisException(400, "corsAllowedOrigins must be an array of origin strings.");
            }

            json validated = json::array();
            for (const auto &item : body["corsAllowedOrigins"])
            {
                if (!item.is_string())
                {
                    throw MantisException(400, "corsAllowedOrigins must contain only strings.");
                }

                const auto value = trim(item.get<std::string>());
                if (value.empty())
                {
                    throw MantisException(400, "corsAllowedOrigins entries must be non-empty strings.");
                }

                validated.push_back(value);
            }

            m_configs["corsAllowedOrigins"] = validated;
        }
    }

    void KeyValStore::setupConfigRoutes()
    {
        const Middlewares adminAuth = {requireAdminAuth()};
        const Middlewares patchMiddleware = {
            requireAdminAuth(),
            envGateMiddleware("MB_DISABLE_CONFIG_MUTATIONS", true)
        };

        mApp.router().Get(
            "/api/v1/sys/settings/config",
            [this](MantisRequest &req, MantisResponse &res)
            {
                const auto settings = loadFromDb();
                if (settings.empty())
                {
                    res.sendJSON(404, {
                                     {"status", 404},
                                     {"error", "Settings object not found!"},
                                     {"data", json::object()}
                                 });
                    return;
                }

                auto data = redactForResponse(settings);
                data["mantisVersion"] = MantisBase::appVersion();

                res.sendJSON(200, {
                                 {"status", 200},
                                 {"error", ""},
                                 {"data", data}
                             });
            },
            adminAuth);

        mApp.router().Patch(
            "/api/v1/sys/settings/config",
            [this](MantisRequest &req, MantisResponse &res)
            {
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty())
                {
                    res.sendJSON(400, {
                                     {"status", 400},
                                     {"error", err},
                                     {"data", json::object()}
                                 });
                    return;
                }

                try
                {
                    applyPatch(body);

                    const auto& sql = mApp.db().session();
                    const std::time_t updated_t = time(nullptr);
                    const std::tm updated_tm = toUtcTime(updated_t);
                    const auto id = configRowId();

                    *sql << "UPDATE mb_store SET value = :value, updated = :updated WHERE id = :id",
                        soci::use(m_configs), soci::use(updated_tm), soci::use(id);

                    mApp.router().reloadCorsOrigins();

                    auto data = redactForResponse(m_configs);
                    data["mantisVersion"] = MantisBase::appVersion();

                    res.sendJSON(200, {
                                     {"status", 200},
                                     {"error", ""},
                                     {"data", data}
                                 });
                }
                catch (const MantisException &e)
                {
                    res.sendJSON(e.code(), {
                                     {"status", e.code()},
                                     {"error", e.what()},
                                     {"data", json::object()}
                                 });
                }
                catch (const std::exception &e)
                {
                    res.sendJSON(500, {
                                     {"status", 500},
                                     {"error", e.what()},
                                     {"data", json::object()}
                                 });
                }
            },
            patchMiddleware);
    }
} // mb
