#include "../../include/mantisbase/core/http.h"
#include "../../../include/mantisbase/mantisbase.h"

#define __file__ "duktape_response_wrapper.cpp"

namespace mb
{
    MantisResponse::MantisResponse(httplib::Response& _resp) : m_res(_resp)
    {
    }

    int MantisResponse::getStatus() const
    {
        return m_res.status;
    }

    void MantisResponse::setStatus(const int s)
    {
        m_res.status = s;
    }

    std::string MantisResponse::getVersion() const
    {
        return m_res.version;
    }

    void MantisResponse::setVersion(const std::string& b)
    {
        m_res.version = b;
    }

    std::string MantisResponse::getBody() const
    {
        return m_res.body;
    }

    void MantisResponse::setBody(const std::string& b)
    {
        m_res.body = b;
    }

    std::string MantisResponse::getLocation() const
    {
        return m_res.location;
    }

    void MantisResponse::setLocation(const std::string& b)
    {
        m_res.location = b;
    }

    std::string MantisResponse::getReason() const
    {
        return m_res.reason;
    }

    void MantisResponse::setReason(const std::string& b)
    {
        m_res.reason = b;
    }

    bool MantisResponse::hasHeader(const std::string& key) const
    {
        return m_res.has_header(key);
    }

    std::string MantisResponse::getHeaderValue(const std::string& key, const char* def, size_t id) const
    {
        return m_res.get_header_value(key, def, id);
    }

    size_t MantisResponse::getHeaderValueU64(const std::string& key, size_t def, size_t id) const
    {
        return m_res.get_header_value_u64(key, def, id);
    }

    size_t MantisResponse::getHeaderValueCount(const std::string& key) const
    {
        return m_res.get_header_value_count(key);
    }

    void MantisResponse::setHeader(const std::string& key, const std::string& val) const
    {
        m_res.set_header(key, val);
    }

    bool MantisResponse::hasTrailer(const std::string& key) const
    {
        return m_res.has_trailer(key);
    }

    std::string MantisResponse::getTrailerValue(const std::string& key, size_t id) const
    {
        return m_res.get_trailer_value(key, id);
    }

    size_t MantisResponse::getTrailerValueCount(const std::string& key) const
    {
        return m_res.get_trailer_value_count(key);
    }

    void MantisResponse::setRedirect(const std::string& url, int status) const
    {
        m_res.set_redirect(url, status);
    }

    void MantisResponse::setContent(const char* s, size_t n, const std::string& content_type) const
    {
        m_res.set_content(s, n, content_type);
    }

    void MantisResponse::setContent(const std::string& s, const std::string& content_type) const
    {
        m_res.set_content(s, content_type);
    }

    void MantisResponse::setContent(std::string&& s, const std::string& content_type) const
    {
        m_res.set_content(s, content_type);
    }

    void MantisResponse::setFileContent(const std::string& path, const std::string& content_type) const
    {
        m_res.set_file_content(path, content_type);
    }

    void MantisResponse::setFileContent(const std::string& path) const
    {
        m_res.set_file_content(path);
    }

    void MantisResponse::send(int statusCode = 200, const std::string& data, const std::string& content_type) const
    {
        m_res.set_content(data, content_type);
        m_res.status = statusCode;
    }

    void MantisResponse::sendText(const int statusCode, const std::string& data) const
    {
        send(statusCode, data, "text/plain");
    }

    void MantisResponse::sendJSON(const int statusCode, const json& data) const
    {
        send(statusCode, data.dump(), "application/json");
    }

#ifdef MANTIS_ENABLE_SCRIPTING
    void MantisResponse::sendJson(const int statusCode, const DukValue& data) const
    {
        const auto ctx = MantisApp::instance().ctx();

        if (data.type() == DukValue::OBJECT)
        {
            data.push();
            const char* json_str = duk_json_encode(ctx, -1);
            nlohmann::json json_obj = nlohmann::json::parse(json_str);
            duk_pop(ctx);

            sendJson(statusCode, json_obj);
            return;
        }

        // Unsupported type - throw error
        Log::warn("Could not parse data to JSON");
        duk_error(ctx, DUK_ERR_TYPE_ERROR, "Could not parse data to JSON");
    }
#endif

    void MantisResponse::sendHtml(const int statusCode, const std::string& data) const
    {
        send(statusCode, data, "application/json");
    }

    void MantisResponse::sendEmpty(const int statusCode) const
    {
        m_res.set_content(std::string{}, std::string{});
        m_res.status = statusCode;
    }

#ifdef MANTIS_ENABLE_SCRIPTING
    void MantisResponse::registerDuktapeMethods()
    {
        // Get Duktape context
        const auto ctx = MantisApp::instance().ctx();

        // Register Response methods
        // `res.hasHeader("Authorization")` -> return true/false
        dukglue_register_method(ctx, &MantisResponse::hasHeader, "hasHeader");
        // `res.getHeader("Authorization", "", 0)` -> Return Authorization value or default
        // `res.getHeader("Authorization", "Default Value", 0)` -> Return Authorization value or default
        // `res.getHeader("Some Key", "Default Value", 1)` -> Return 'Some Key' value if exists of index '1' or default
        dukglue_register_method(ctx, &MantisResponse::getHeaderValue, "getHeader");
        dukglue_register_method(ctx, &MantisResponse::getHeaderValueU64, "getHeaderU64");
        // `res.getHeaderCount("key")` -> Count for header values given the header key
        dukglue_register_method(ctx, &MantisResponse::getHeaderValueCount, "getHeaderCount");
        // res.setHeader("Cow", "Cow Value")
        dukglue_register_method(ctx, &MantisResponse::setHeader, "setHeader");

        dukglue_register_method(ctx, &MantisResponse::hasTrailer, "hasTrailer");
        dukglue_register_method(ctx, &MantisResponse::getTrailerValue, "getTrailer");
        dukglue_register_method(ctx, &MantisResponse::getTrailerValueCount, "getTrailerCount");

        // `res.redirect("http://some-url.com", 302)`
        dukglue_register_method(ctx, &MantisResponse::setRedirect, "redirect");

        // `res.setContent("something here", "text/plain")`
        dukglue_register_method(ctx, static_cast<void(MantisResponse::*)(const std::string&, const std::string&) const>(&MantisResponse::setContent), "setContent");
        // `res.setFileContent("/foo/bar.txt")`
        dukglue_register_method(ctx, static_cast<void(MantisResponse::*)(const std::string&) const>(&MantisResponse::setFileContent), "setFileContent");

        // `res.send(200, "some data here", "text/plain")`
        dukglue_register_method(ctx, &MantisResponse::send, "send");
        // `res.json(200, "{\"a\": 5}")`
        dukglue_register_method(ctx, static_cast<void(MantisResponse::*)(int, const DukValue&) const>(&MantisResponse::sendJson), "json");
        // `res.html(200, "<html> ... </html>")`
        dukglue_register_method(ctx, &MantisResponse::sendHtml, "html");
        // `res.text(200, "some text response")`
        dukglue_register_method(ctx, &MantisResponse::sendText, "text");
        // `res.empty(204)`
        dukglue_register_method(ctx, &MantisResponse::sendEmpty, "empty");

        // `res.body = "Some Data"`
        // `res.body` -> returns `Some Data`
        dukglue_register_property(ctx, &MantisResponse::getBody, &MantisResponse::setBody, "body");
        // `res.status` (get or set status data)
        dukglue_register_property(ctx, &MantisResponse::getStatus, &MantisResponse::setStatus, "status");
        // `res.version` (get or set version value)
        dukglue_register_property(ctx, &MantisResponse::getVersion, &MantisResponse::setVersion, "version");
        // `res.location` (get or set redirect location value)
        dukglue_register_property(ctx, &MantisResponse::getLocation, &MantisResponse::setLocation, "location");
        // `res.reason` (get or set reason value)
        dukglue_register_property(ctx, &MantisResponse::getReason, &MantisResponse::setReason, "reason");
    }
#endif
}
