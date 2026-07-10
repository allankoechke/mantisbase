#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/http.h"

namespace mb {
MantisRequest::MantisRequest(const drogon::HttpRequestPtr &_req)
    : m_req(_req), m_store(ContextStore{}) {}

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
    if (hasHeader("X-Forwarded-For")) {
        auto forwarded = getHeaderValue("X-Forwarded-For", "", 0);
        if (const auto first_comma = forwarded.find(',');
            first_comma != std::string::npos) {
            forwarded = forwarded.substr(0, first_comma);
        }
        if (isValidIP(forwarded)) {
            return forwarded;
        }
        LogOrigin::warn("Invalid IP Header", fmt::format("Invalid IP address in X-Forwarded-For header: {}", forwarded));
    }
    const auto direct_ip = m_req->peerAddr().toIp();
    if (isValidIP(direct_ip)) {
        return direct_ip;
    }
    LogOrigin::warn("IP Detection Failed", "Unable to determine valid client IP address");
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
    return m_store.hasKey(key);
}

std::string MantisRequest::getBearerTokenAuth() const {
    if (hasHeader("Authorization")) {
        const auto auth = getHeaderValue("Authorization", "", 0);
        constexpr size_t bearer_prefix_len = 7; // "Bearer "
        return auth.size() > bearer_prefix_len ? auth.substr(bearer_prefix_len) : "";
    }
    return "";
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

#ifdef MB_SCRIPTING_ENABLED
void MantisRequest::registerDuktapeMethods() {
    // TODO: Re-enable after Drogon migration
}

DukValue MantisRequest::get_duk(const std::string &key) {
    return m_store.get_duk(key);
}

DukValue MantisRequest::getOr_duk(const std::string &key,
                                  const DukValue &default_value) {
    return m_store.getOr_duk(key, default_value);
}

void MantisRequest::set_duk(const std::string &key, const DukValue &value) {
    m_store.set_duk(key, value);
}
#else
void MantisRequest::registerDuktapeMethods() {}
#endif
} // namespace mb
