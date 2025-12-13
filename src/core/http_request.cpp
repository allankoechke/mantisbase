#include "../../include/mantisbase/core/http.h"
#include "../../../include/mantisbase/mantisbase.h"

#define __file__ "duktape_response_wrapper.cpp"

namespace mb
{
    MantisRequest::MantisRequest(const httplib::Request& _req)
        : m_req(_req),
          m_store(ContextStore{})
    {
    }

    std::string MantisRequest::getMethod() const
    {
        return m_req.method;
    }

    std::string MantisRequest::getPath() const
    {
        return m_req.path;
    }

    std::string mb::MantisRequest::getBody() const
    {
        return m_req.body;
    }

    std::string MantisRequest::getRemoteAddr() const
    {
        return m_req.remote_addr;
    }

    int MantisRequest::getRemotePort() const
    {
        return m_req.remote_port;
    }

    std::string MantisRequest::getLocalAddr() const
    {
        return m_req.local_addr;
    }

    int MantisRequest::getLocalPort() const
    {
        return m_req.local_port;
    }

    bool MantisRequest::hasHeader(const std::string& key) const
    {
        return m_req.has_header(key);
    }

    std::string MantisRequest::getHeaderValue(const std::string& key, const char* def, size_t id) const
    {
        return m_req.get_header_value(key, def, id);
    }

    size_t MantisRequest::getHeaderValueU64(const std::string& key, size_t def, size_t id) const
    {
        return m_req.get_header_value_u64(key, def, id);
    }

    size_t MantisRequest::getHeaderValueCount(const std::string& key) const
    {
        return m_req.get_header_value_count(key);
    }

    bool MantisRequest::hasTrailer(const std::string& key) const
    {
        return m_req.has_trailer(key);
    }

    std::string MantisRequest::getTrailerValue(const std::string& key, size_t id) const
    {
        return m_req.get_trailer_value(key, id);
    }

    size_t MantisRequest::getTrailerValueCount(const std::string& key) const
    {
        return m_req.get_trailer_value_count(key);
    }

    httplib::Match MantisRequest::matches() const
    {
        return m_req.matches;
    }

    bool MantisRequest::hasQueryParam(const std::string& key) const
    {
        return m_req.has_param(key);
    }

    std::string MantisRequest::getQueryParamValue(const std::string& key) const
    {
        return m_req.get_param_value(key);
    }

    std::string MantisRequest::getQueryParamValue(const std::string& key, const size_t id) const
    {
        return m_req.get_param_value(key, id);
    }

    size_t MantisRequest::getQueryParamValueCount(const std::string& key) const
    {
        return m_req.get_param_value_count(key);
    }

    bool MantisRequest::hasPathParams() const
    {
        return !m_req.path_params.empty();
    }

    bool MantisRequest::hasPathParam(const std::string& key) const
    {
        return m_req.path_params.contains(key);
    }

    std::string MantisRequest::getPathParamValue(const std::string& key) const
    {
        if (m_req.path_params.contains(key))
            return m_req.path_params.at(key);

        return "";
    }

    size_t MantisRequest::getPathParamValueCount(const std::string& key) const
    {
        if (m_req.path_params.contains(key))
            return m_req.path_params.at(key).size();

        return 0;
    }

    bool MantisRequest::isMultipartFormData() const
    {
        return m_req.is_multipart_form_data();
    }

#ifdef MANTIS_ENABLE_SCRIPTING
    void MantisRequest::registerDuktapeMethods()
    {
        // Get Duktape Context
        const auto ctx = MantisApp::instance().ctx();

        // Register Request methods
        // `req.hasHeader("Authorization")` -> return true/false
        dukglue_register_method(ctx, &MantisRequest::hasHeader, "hasHeader");
        // `req.getHeader("Authorization", "", 0)` -> Return Authorization value or default
        // `req.getHeader("Authorization", "Default Value", 0)` -> Return Authorization value or default
        // `req.getHeader("Some Key", "Default Value", 1)` -> Return 'Some Key' value if exists of index '1' or default
        dukglue_register_method(ctx, &MantisRequest::getHeaderValue, "getHeader");
        dukglue_register_method(ctx, &MantisRequest::getHeaderValueU64, "getHeaderU64");
        // `req.getHeaderCount("key")` -> Count for header values given the header key
        dukglue_register_method(ctx, &MantisRequest::getHeaderValueCount, "getHeaderCount");

        dukglue_register_method(ctx, &MantisRequest::hasTrailer, "hasTrailer");
        dukglue_register_method(ctx, &MantisRequest::getTrailerValue, "getTrailer");
        dukglue_register_method(ctx, &MantisRequest::getTrailerValueCount, "getTrailerCount");

        // `req.hasQueryParam("key")` -> return true/false
        dukglue_register_method(ctx, &MantisRequest::hasQueryParam, "hasQueryParam");
        // `req.getQueryParam("key")` -> Return header value given the key
        dukglue_register_method(
            ctx, static_cast<std::string(MantisRequest::*)(const std::string&) const>(&
                MantisRequest::getQueryParamValue), "getQueryParam");
        // `req.getQueryParamCount("key")` -> Return parameter value count
        dukglue_register_method(ctx, &MantisRequest::getQueryParamValueCount, "getQueryParamCount");

        // `req.hasPathParam("key")` -> return true/false
        dukglue_register_method(ctx, &MantisRequest::hasPathParam, "hasPathParam");
        // `req.getPathParam("key")` -> Return header value given the key
        dukglue_register_method(
            ctx, static_cast<std::string(MantisRequest::*)(const std::string&) const>(&
                MantisRequest::getPathParamValue), "getPathParam");
        // `req.getPathParamCount("key")` -> Return parameter value count
        dukglue_register_method(ctx, &MantisRequest::getPathParamValueCount, "getPathParamCount");

        // `req.isMultipartFormData()` -> Return true if request type is of Multipart/FormData
        dukglue_register_method(ctx, &MantisRequest::isMultipartFormData, "isMultipartFormData");

        // `req.body` -> Get request body data
        dukglue_register_property(ctx, &MantisRequest::getBody, nullptr, "body");
        // `req.method` -> Get request method ('GET', 'POST', ...)
        dukglue_register_property(ctx, &MantisRequest::getMethod, nullptr, "method");
        // `req.path` -> Get request path value
        dukglue_register_property(ctx, &MantisRequest::getPath, nullptr, "path");
        // `req.remoteAddr`
        dukglue_register_property(ctx, &MantisRequest::getRemoteAddr, nullptr, "remoteAddr");
        // `req.remotePort`
        dukglue_register_property(ctx, &MantisRequest::getRemotePort, nullptr, "remotePort");
        // `req.localAddr`
        dukglue_register_property(ctx, &MantisRequest::getLocalAddr, nullptr, "localAddr");
        // `req.localPort`
        dukglue_register_property(ctx, &MantisRequest::getLocalPort, nullptr, "localPort");


        // `req.hasKey("key")` -> return true if key is in the context store
        dukglue_register_method(ctx, &MantisRequest::hasKey, "hasKey");
        // `req.set("key", value)` // Store in the context store the given key and value. Note only int, float, double, str
        dukglue_register_method(ctx, &MantisRequest::set_duk, "set");
        // `req.get("key")`
        dukglue_register_method(ctx, &MantisRequest::get_duk, "get");
        // `req.getOr("key", default_value)`
        dukglue_register_method(ctx, &MantisRequest::getOr_duk, "getOr");
    }
#endif

    bool MantisRequest::hasKey(const std::string& key) const
    {
        return m_store.hasKey(key);
    }

    std::string MantisRequest::getBearerTokenAuth() const
    {
        return get_bearer_token_auth(m_req);
    }

    std::pair<nlohmann::json, std::string> MantisRequest::getBodyAsJson() const {
        try {
            auto b = getBody();
            auto obj = b.empty() ? nlohmann::json::object() : nlohmann::json::parse(b);
            return {obj, ""};
        } catch (const std::exception &e) {
            return {nlohmann::json::object(), e.what()};
        }
    }

#ifdef MANTIS_ENABLE_SCRIPTING
    DukValue MantisRequest::get_duk(const std::string& key)
    {
        return m_store.get_duk(key);
    }

    DukValue MantisRequest::getOr_duk(const std::string& key, const DukValue& default_value)
    {
        return m_store.getOr_duk(key, default_value);
    }

    void MantisRequest::set_duk(const std::string& key, const DukValue& value)
    {
        m_store.set_duk(key, value);
    }
#endif
}
