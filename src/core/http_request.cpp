#include <utility>

#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/models/access_rules.h"
#include "../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/core/types.h"

#ifdef MB_SCRIPTING_ENABLED
#include "../../include/mantisbase/scripting/scripting_engine.h"
#include <dukglue/dukglue.h>
#endif

namespace mb {
    MantisRequest::MantisRequest(const MantisBase &app, drogon::HttpRequestPtr _req)
        : IMantisBase(app),
          m_req(std::move(_req)) {
    }

    void MantisRequest::setPathParam(const std::string &key, const std::string &value) {
        m_pathParams[key] = value;
    }

    void MantisRequest::setPathParams(const std::unordered_map<std::string, std::string> &params) {
        m_pathParams = params;
    }

    std::string MantisRequest::getMethod() const {
        switch (m_req->method()) {
            case drogon::Get: return "GET";
            case drogon::Post: return "POST";
            case drogon::Put: return "PUT";
            case drogon::Delete: return "DELETE";
            case drogon::Patch: return "PATCH";
            case drogon::Options: return "OPTIONS";
            case drogon::Head: return "HEAD";
            default: return "UNKNOWN";
        }
    }

    std::string MantisRequest::getPath() const { return m_req->path(); }

    std::string MantisRequest::getBody() const { return std::string(m_req->body()); }

    std::string MantisRequest::getRemoteAddr() const {
        auto direct_ip = m_req->peerAddr().toIp();

        if (hasHeader("X-Forwarded-For")) {
            auto trusted_proxies_str = getEnvOrDefault("MB_TRUSTED_PROXIES", "");
            if (!trusted_proxies_str.empty()) {
                auto proxies = splitString(trusted_proxies_str, ",");
                bool is_trusted = false;
                for (auto &proxy : proxies) {
                    proxy = trim(proxy);
                    if (proxy == direct_ip) {
                        is_trusted = true;
                        break;
                    }
                }

                if (is_trusted) {
                    auto forwarded = getHeaderValue("X-Forwarded-For", "", 0);
                    if (const auto first_comma = forwarded.find(',');
                        first_comma != std::string::npos) {
                        forwarded = forwarded.substr(0, first_comma);
                    }
                    forwarded = trim(forwarded);
                    if (isValidIP(forwarded)) {
                        return forwarded;
                    }
                    mbApp().logger().warn("Invalid IP Header",
                                          fmt::format("Invalid IP address in X-Forwarded-For header: {}", forwarded));
                }
            }
        }

        if (isValidIP(direct_ip)) {
            return direct_ip;
        }

        mbApp().logger().warn("IP Detection Failed", "Unable to determine valid client IP address");
        return direct_ip;
    }

    int MantisRequest::getRemotePort() const { return m_req->peerAddr().toPort(); }

    std::string MantisRequest::getLocalAddr() const { return m_req->localAddr().toIp(); }

    int MantisRequest::getLocalPort() const { return m_req->localAddr().toPort(); }

    bool MantisRequest::hasHeader(const std::string &key) const {
        return !m_req->getHeader(key).empty();
    }

    std::string MantisRequest::getHeaderValue(const std::string &key,
                                              const char *def, size_t id) const {
        auto val = m_req->getHeader(key);
        return val.empty() ? std::string(def) : val;
    }

    size_t MantisRequest::getHeaderValueU64(const std::string &key, size_t def,
                                            size_t id) const {
        auto val = m_req->getHeader(key);
        if (val.empty()) return def;
        try { return std::stoull(val); } catch (...) { return def; }
    }

    size_t MantisRequest::getHeaderValueCount(const std::string &key) const {
        return m_req->getHeader(key).empty() ? 0 : 1;
    }

    bool MantisRequest::hasQueryParam(const std::string &key) const {
        return m_req->getOptionalParameter<std::string>(key).has_value();
    }

    std::string MantisRequest::getQueryParamValue(const std::string &key) const {
        auto val = m_req->getOptionalParameter<std::string>(key);
        return val.has_value() ? val.value() : "";
    }

    std::string MantisRequest::getQueryParamValue(const std::string &key,
                                                  const size_t id) const {
        return getQueryParamValue(key);
    }

    size_t MantisRequest::getQueryParamValueCount(const std::string &key) const {
        return hasQueryParam(key) ? 1 : 0;
    }

    bool MantisRequest::hasPathParams() const { return !m_pathParams.empty(); }

    bool MantisRequest::hasPathParam(const std::string &key) const {
        return m_pathParams.contains(key);
    }

    std::string MantisRequest::getPathParamValue(const std::string &key) const {
        if (auto it = m_pathParams.find(key); it != m_pathParams.end())
            return it->second;
        return "";
    }

    size_t MantisRequest::getPathParamValueCount(const std::string &key) const {
        return m_pathParams.contains(key) ? 1 : 0;
    }

    bool MantisRequest::isMultipartFormData() const {
        auto ct = m_req->getHeader("Content-Type");
        return ct.find("multipart/form-data") != std::string::npos;
    }

    bool MantisRequest::hasKey(const std::string &key) const {
        return m_req->attributes()->find(key);
    }

    std::string MantisRequest::getBearerTokenAuth() const {
        if (hasHeader("Authorization")) {
            const auto auth = getHeaderValue("Authorization", "", 0);
            constexpr size_t bearer_prefix_len = 7; // "Bearer "
            return auth.size() > bearer_prefix_len ? auth.substr(bearer_prefix_len) : "";
        }
        return "";
    }

    std::string MantisRequest::getCookieValue(const std::string &key) const {
        return m_req->getCookie(key);
    }

    std::string MantisRequest::resolveAuthToken() const {
        if (hasHeader("Authorization")) {
            const auto bearer = trim(getBearerTokenAuth());
            if (!bearer.empty()) {
                return bearer;
            }
        }

        return trim(getCookieValue(kAuthTokenCookieName));
    }

    std::pair<nlohmann::json, std::string> MantisRequest::getBodyAsJson() const {
        try {
            const auto b = getBody();
            auto obj = b.empty() ? nlohmann::json::object() : nlohmann::json::parse(b);
            return {obj, ""};
        } catch (const std::exception &e) {
            return {nlohmann::json::object(), e.what()};
        }
    }

    const drogon::HttpRequestPtr & MantisRequest::drogonRequest() const { return m_req; }

    bool MantisRequest::isGuestAuth() {
        return mb::isGuestAuth(getOr<json>("auth", json::object()));
    }

    bool MantisRequest::isAdminAuth() {
        return mb::isAdminAuth(getOr<json>("auth", json::object()));
    }

    bool MantisRequest::isUserAuth() {
        return mb::isUserAuth(getOr<json>("auth", json::object()));
    }

#ifdef MB_SCRIPTING_ENABLED
    namespace {
        duk_context *scriptingCtx() {
            if (auto *engine = ScriptingEngine::active()) {
                return engine->ctx();
            }
            return nullptr;
        }

        template<typename T>
        bool tryPushAttribute(duk_context *ctx,
                              const drogon::HttpRequestPtr &req,
                              const std::string &key) {
            try {
                const T &val = req->attributes()->get<T>(key);
                dukglue_push(ctx, val);
                return true;
            } catch (...) {
                return false;
            }
        }
    }

    void MantisRequest::registerDuktapeMethods() {
    }

    DukValue MantisRequest::get_duk(const std::string &key) {
        if (!hasKey(key)) {
            return {};
        }

        auto *ctx = scriptingCtx();
        if (!ctx) {
            return {};
        }

        try {
            const auto &j = m_req->attributes()->get<json>(key);
            duk_push_string(ctx, j.dump().c_str());
            duk_json_decode(ctx, -1);
            return DukValue::take_from_stack(ctx);
        } catch (...) {
        }

        if (tryPushAttribute<std::string>(ctx, m_req, key)) {
            return DukValue::take_from_stack(ctx);
        }
        if (tryPushAttribute<int>(ctx, m_req, key)) {
            return DukValue::take_from_stack(ctx);
        }
        if (tryPushAttribute<double>(ctx, m_req, key)) {
            return DukValue::take_from_stack(ctx);
        }
        if (tryPushAttribute<bool>(ctx, m_req, key)) {
            return DukValue::take_from_stack(ctx);
        }
        if (tryPushAttribute<float>(ctx, m_req, key)) {
            return DukValue::take_from_stack(ctx);
        }

        duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported type stored for key '%s'", key.c_str());
        return {};
    }

    DukValue MantisRequest::getOr_duk(const std::string &key, const DukValue &default_value) {
        if (!hasKey(key)) {
            return default_value;
        }
        auto val = get_duk(key);
        if (val == DukValue{}) {
            return default_value;
        }
        return val;
    }

    void MantisRequest::set_duk(const std::string &key, const DukValue &value) {
        auto *ctx = scriptingCtx();
        if (!ctx) {
            return;
        }

        switch (value.type()) {
        case DukValue::NUMBER:
            set(key, value.as_double());
            break;
        case DukValue::STRING:
            set(key, value.as_string());
            break;
        case DukValue::BOOLEAN:
            set(key, value.as_bool());
            break;
        case DukValue::NULLREF:
        case DukValue::UNDEFINED:
            break;
        case DukValue::OBJECT: {
            value.push();
            const char *json_str = duk_json_encode(ctx, -1);
            json json_obj = json::parse(json_str ? json_str : "{}");
            duk_pop(ctx);
            set(key, json_obj);
            break;
        }
        default:
            duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported type stored for key '%s'", key.c_str());
        }
    }
#else
    void MantisRequest::registerDuktapeMethods() {
    }
#endif
} // namespace mb
