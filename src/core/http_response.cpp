#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/mantisbase.h"
#ifdef MB_SCRIPTING_ENABLED
#include "../../include/mantisbase/scripting/scripting_engine.h"
#endif
#include <drogon/Cookie.h>
#include <fstream>

namespace mb {
    MbResponse::MbResponse(const MantisBase &app)
        : IMantisBase(app),
          m_res(drogon::HttpResponse::newHttpResponse()) {
    }

    const drogon::HttpResponsePtr &MbResponse::drogonResponse() const { return m_res; }

    int MbResponse::getStatus() const {
        return static_cast<int>(m_res->statusCode());
    }

    void MbResponse::setStatus(const int s) const {
        m_res->setStatusCode(static_cast<drogon::HttpStatusCode>(s));
    }

    std::string MbResponse::getVersion() const {
        return m_res->getHeader("version");
    }

    void MbResponse::setVersion(const std::string &b) {
        // Drogon handles version internally
    }

    std::string MbResponse::getBody() const {
        return std::string(m_res->body());
    }

    void MbResponse::setBody(const std::string &b) {
        m_res->setBody(b);
    }

    std::string MbResponse::getLocation() const {
        return m_res->getHeader("Location");
    }

    void MbResponse::setLocation(const std::string &b) {
        m_res->addHeader("Location", b);
    }

    std::string MbResponse::getReason() const {
        return "";
    }

    void MbResponse::setReason(const std::string &b) {
        // Drogon auto-generates reason from status code
    }

    bool MbResponse::hasHeader(const std::string &key) const {
        return !m_res->getHeader(key).empty();
    }

    std::string MbResponse::getHeaderValue(const std::string &key, const char *def, size_t id) const {
        auto val = m_res->getHeader(key);
        return val.empty() ? std::string(def) : val;
    }

    size_t MbResponse::getHeaderValueU64(const std::string &key, size_t def, size_t id) const {
        auto val = m_res->getHeader(key);
        if (val.empty()) return def;
        try { return std::stoull(val); } catch (...) { return def; }
    }

    size_t MbResponse::getHeaderValueCount(const std::string &key) const {
        return m_res->getHeader(key).empty() ? 0 : 1;
    }

    void MbResponse::setHeader(const std::string &key, const std::string &val) const {
        m_res->addHeader(key, val);
    }

    void MbResponse::setAuthTokenCookie(const std::string &token, const int max_age_seconds) const {
        drogon::Cookie cookie(kAuthTokenCookieName, token);
        cookie.setHttpOnly(true);
        cookie.setPath("/");
        cookie.setMaxAge(max_age_seconds);
        m_res->addCookie(std::move(cookie));
    }

    void MbResponse::clearAuthTokenCookie() const {
        drogon::Cookie cookie(kAuthTokenCookieName, "");
        cookie.setPath("/");
        cookie.setMaxAge(0);
        m_res->addCookie(std::move(cookie));
    }

    void MbResponse::setRedirect(const std::string &url, int status) const {
        m_res->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
        m_res->addHeader("Location", url);
    }

    void MbResponse::setContent(const char *s, size_t n, const std::string &content_type) const {
        m_res->setBody(std::string(s, n));
        m_res->setContentTypeString(content_type);
    }

    void MbResponse::setContent(const std::string &s, const std::string &content_type) const {
        m_res->setBody(s);
        m_res->setContentTypeString(content_type);
    }

    void MbResponse::setContent(std::string &&s, const std::string &content_type) const {
        m_res->setBody(std::move(s));
        m_res->setContentTypeString(content_type);
    }

    void MbResponse::setFileContent(const std::string &path, const std::string &content_type) const {
        std::ifstream file(path, std::ios::binary);
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            m_res->setBody(std::move(content));
            m_res->setContentTypeString(content_type);
        }
    }

    void MbResponse::setFileContent(const std::string &path) const {
        std::string content_type = "application/octet-stream";
        if (path.ends_with(".html")) content_type = "text/html";
        else if (path.ends_with(".css")) content_type = "text/css";
        else if (path.ends_with(".js")) content_type = "application/javascript";
        else if (path.ends_with(".json")) content_type = "application/json";
        else if (path.ends_with(".png")) content_type = "image/png";
        else if (path.ends_with(".jpg") || path.ends_with(".jpeg")) content_type = "image/jpeg";
        else if (path.ends_with(".svg")) content_type = "image/svg+xml";
        setFileContent(path, content_type);
    }

    void MbResponse::send(int statusCode, const std::string &data, const std::string &content_type) const {
        m_res->setBody(data);
        m_res->setContentTypeString(content_type);
        m_res->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));
    }

    void MbResponse::sendText(const int statusCode, const std::string &data) const {
        send(statusCode, data, "text/plain");
    }

    void MbResponse::sendJSON(const int statusCode, const json &data) const {
        send(statusCode, data.dump(), "application/json");
    }

    void MbResponse::sendHtml(const int statusCode, const std::string &data) const {
        send(statusCode, data, "text/html");
    }

    void MbResponse::sendEmpty(const int statusCode) const {
        m_res->setBody(std::string{});
        m_res->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));
    }

#ifdef MB_SCRIPTING_ENABLED
    void MbResponse::sendJson(const int statusCode, const DukValue &data) const {
        auto *engine = ScriptingEngine::active();
        if (!engine || !engine->ctx()) {
            send(statusCode, "{}", "application/json");
            return;
        }
        auto *ctx = engine->ctx();
        data.push();
        const char *json_str = duk_json_encode(ctx, -1);
        const std::string body = json_str ? json_str : "{}";
        duk_pop(ctx);
        send(statusCode, body, "application/json");
    }

    void MbResponse::registerDuktapeMethods() {
    }
#else
    void MantisResponse::registerDuktapeMethods() {
    }
#endif
}
