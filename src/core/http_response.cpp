#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/mantisbase.h"
#include <fstream>

namespace mb {
    MantisResponse::MantisResponse()
        : m_res(drogon::HttpResponse::newHttpResponse()) {}

    const drogon::HttpResponsePtr& MantisResponse::drogonResponse() const { return m_res; }

    int MantisResponse::getStatus() const {
        return static_cast<int>(m_res->statusCode());
    }

    void MantisResponse::setStatus(const int s) {
        m_res->setStatusCode(static_cast<drogon::HttpStatusCode>(s));
    }

    std::string MantisResponse::getVersion() const {
        return m_res->getHeaderBy("version");
    }

    void MantisResponse::setVersion(const std::string& b) {
        // Drogon handles version internally
    }

    std::string MantisResponse::getBody() const {
        return std::string(m_res->body());
    }

    void MantisResponse::setBody(const std::string& b) {
        m_res->setBody(b);
    }

    std::string MantisResponse::getLocation() const {
        return m_res->getHeaderBy("Location");
    }

    void MantisResponse::setLocation(const std::string& b) {
        m_res->addHeader("Location", b);
    }

    std::string MantisResponse::getReason() const {
        return "";
    }

    void MantisResponse::setReason(const std::string& b) {
        // Drogon auto-generates reason from status code
    }

    bool MantisResponse::hasHeader(const std::string& key) const {
        return !m_res->getHeaderBy(key).empty();
    }

    std::string MantisResponse::getHeaderValue(const std::string& key, const char* def, size_t id) const {
        auto val = m_res->getHeaderBy(key);
        return val.empty() ? std::string(def) : val;
    }

    size_t MantisResponse::getHeaderValueU64(const std::string& key, size_t def, size_t id) const {
        auto val = m_res->getHeaderBy(key);
        if (val.empty()) return def;
        try { return std::stoull(val); } catch (...) { return def; }
    }

    size_t MantisResponse::getHeaderValueCount(const std::string& key) const {
        return m_res->getHeaderBy(key).empty() ? 0 : 1;
    }

    void MantisResponse::setHeader(const std::string& key, const std::string& val) const {
        m_res->addHeader(key, val);
    }

    void MantisResponse::setRedirect(const std::string& url, int status) const {
        m_res->setStatusCode(static_cast<drogon::HttpStatusCode>(status));
        m_res->addHeader("Location", url);
    }

    void MantisResponse::setContent(const char* s, size_t n, const std::string& content_type) const {
        m_res->setBody(std::string(s, n));
        m_res->setContentTypeString(content_type);
    }

    void MantisResponse::setContent(const std::string& s, const std::string& content_type) const {
        m_res->setBody(s);
        m_res->setContentTypeString(content_type);
    }

    void MantisResponse::setContent(std::string&& s, const std::string& content_type) const {
        m_res->setBody(std::move(s));
        m_res->setContentTypeString(content_type);
    }

    void MantisResponse::setFileContent(const std::string& path, const std::string& content_type) const {
        std::ifstream file(path, std::ios::binary);
        if (file.is_open()) {
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            m_res->setBody(std::move(content));
            m_res->setContentTypeString(content_type);
        }
    }

    void MantisResponse::setFileContent(const std::string& path) const {
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

    void MantisResponse::send(int statusCode, const std::string& data, const std::string& content_type) const {
        m_res->setBody(data);
        m_res->setContentTypeString(content_type);
        m_res->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));
    }

    void MantisResponse::sendText(const int statusCode, const std::string& data) const {
        send(statusCode, data, "text/plain");
    }

    void MantisResponse::sendJSON(const int statusCode, const json& data) const {
        send(statusCode, data.dump(), "application/json");
    }

    void MantisResponse::sendHtml(const int statusCode, const std::string& data) const {
        send(statusCode, data, "text/html");
    }

    void MantisResponse::sendEmpty(const int statusCode) const {
        m_res->setBody(std::string{});
        m_res->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));
    }

#ifdef MB_SCRIPTING_ENABLED
    void MantisResponse::sendJson(const int statusCode, const DukValue& data) const {
        // TODO: Re-enable after Drogon migration
    }

    void MantisResponse::registerDuktapeMethods() {
        // TODO: Re-enable after Drogon migration
    }
#else
    void MantisResponse::registerDuktapeMethods() {}
#endif
}
