//
// Created by allan on 07/10/2025.
//

#ifndef MANTISAPP_DUKTAPE_CUSTOM_TYPES_H
#define MANTISAPP_DUKTAPE_CUSTOM_TYPES_H
#include "exceptions.h"
#include "models/entity.h"

#ifdef MANTIS_ENABLE_SCRIPTING
#include <dukglue/dukglue.h>
#endif

#include <mantisbase/core/route_registry.h>
#include <mantisbase/core/context_store.h>
#include "../utils/utils.h"
#include "types.h"

#include <fstream>

namespace mb {
#ifdef MANTIS_ENABLE_SCRIPTING
    class DuktapeImpl {
    public:
        static duk_ret_t nativeConsoleInfo(duk_context *ctx);

        static duk_ret_t nativeConsoleTrace(duk_context *ctx);

        static duk_ret_t nativeConsoleTable(duk_context *ctx);
    };
#endif

    /**
     * @brief A wrapper class around `httplib::Request` offering a
     * consistent API and allowing for easy wrapper methods compatible
     * with `Duktape` API requirements for scripting.
     *
     * Additionally, `MantisRequest` adds a context object for storing
     * some `key`-`value` data for sharing across middlewares and
     * request handlers.
     */
    class MantisRequest {
        const httplib::Request &m_req;
        ContextStore m_store;

        const std::string __class_name__ = "mb::MantisRequest";

    public:
        /**
         * @brief Wrapper class around the httplib Request object and
         * our context library.
         *
         * @param _req httplib::Request& object
         */
        explicit MantisRequest(const httplib::Request &_req);

        ///> Get request method
        std::string getMethod() const;

        ///> Get request path
        std::string getPath() const;

        ///> Get request body
        std::string getBody() const;

        ///> Get remote address
        std::string getRemoteAddr() const;

        ///> Get remote port
        int getRemotePort() const;

        ///> Get local address
        std::string getLocalAddr() const;

        ///> Get local port
        int getLocalPort() const;

        bool hasHeader(const std::string &key) const;

        std::string getHeaderValue(const std::string &key, const char *def, size_t id) const;

        size_t getHeaderValueU64(const std::string &key, size_t def, size_t id) const;

        size_t getHeaderValueCount(const std::string &key) const;

        bool hasTrailer(const std::string &key) const;

        std::string getTrailerValue(const std::string &key, size_t id) const;

        size_t getTrailerValueCount(const std::string &key) const;

        ///> Fetch matches for the request
        httplib::Match matches() const;


        bool hasQueryParam(const std::string &key) const;

        std::string getQueryParamValue(const std::string &key) const;

        std::string getQueryParamValue(const std::string &key, size_t id) const;

        size_t getQueryParamValueCount(const std::string &key) const;

        ///> Check if the request has path parameter values
        bool hasPathParams() const;

        ///> Check if a path parameter matching given key
        bool hasPathParam(const std::string &key) const;

        ///> Get path param value for a given key
        std::string getPathParamValue(const std::string &key) const;

        ///> Get path parameter value count for a given `key`
        size_t getPathParamValueCount(const std::string &key) const;

        ///> Check whether the request is of multipart form data
        bool isMultipartFormData() const;

        ///> Register MantisRequest methods to duktape.
        static void registerDuktapeMethods();

        bool hasKey(const std::string &key) const;

        /**
         * @brief Get bearer token from the `Authorization` header value.
         *
         * @return Token value as a string
         */
        std::string getBearerTokenAuth() const;

        std::pair<nlohmann::json, std::string> getBodyAsJson() const;

        template<typename T>
        void set(const std::string &key, T value) {
            m_store.set(key, value);
        }

        // template<typename T>
        // std::optional<T *> get(const std::string &key) {
        //     return m_store.get<T>(key);
        // }

        template<typename T>
        T &getOr(const std::string &key, T default_value) {
            return m_store.getOr<T>(key, default_value);
        }

    private:
#ifdef MANTIS_ENABLE_SCRIPTING
        // Context Methods for setting and getting context values
        DukValue get_duk(const std::string &key);
        DukValue getOr_duk(const std::string &key, const DukValue &default_value);
        void set_duk(const std::string &key, const DukValue &value);
#endif
    };

    /**
     * @brief A wrapper class around `httplib::Response` offering a
     * consistent API and allowing for easy wrapper methods compatible
     * with `Duktape` API requirements for scripting.
     */
    class MantisResponse {
        httplib::Response &m_res;

        const std::string __class_name__ = "mb::MantisResponse";

    public:
        explicit MantisResponse(httplib::Response &_resp);

        ~MantisResponse() = default;

        ///> Get Response Status Code
        [[nodiscard]] int getStatus() const;

        ///> Set Response Status Code
        void setStatus(int s);

        [[nodiscard]] std::string getVersion() const;

        void setVersion(const std::string &b);

        ///> Get Response Body
        [[nodiscard]] std::string getBody() const;

        ///> Set Response Body
        void setBody(const std::string &b);

        ///> Get Response redirect location
        [[nodiscard]] std::string getLocation() const;

        ///> Set Response redirect location
        void setLocation(const std::string &b);

        [[nodiscard]] std::string getReason() const;

        void setReason(const std::string &b);

        ///> Check if response has a given header set
        [[nodiscard]] bool hasHeader(const std::string &key) const;

        ///> Get Response header with given header `key`, with an optional default value `def` and index `id` if it's an array
        std::string getHeaderValue(const std::string &key, const char *def = "", size_t id = 0) const;

        ///> Same as @see getHeaderValue() but returns responses as `u64`
        [[nodiscard]] size_t getHeaderValueU64(const std::string &key, size_t def = 0, size_t id = 0) const;

        ///> Get count of the values in a given header entry defined by `key`.
        [[nodiscard]] size_t getHeaderValueCount(const std::string &key) const;

        ///> Set header value with given `key` and `val`
        void setHeader(const std::string &key, const std::string &val) const;

        [[nodiscard]] bool hasTrailer(const std::string &key) const;

        [[nodiscard]] std::string getTrailerValue(const std::string &key, size_t id = 0) const;

        [[nodiscard]] size_t getTrailerValueCount(const std::string &key) const;

        void setRedirect(const std::string &url, int status = httplib::StatusCode::Found_302) const;

        void setContent(const char *s, size_t n, const std::string &content_type) const;

        void setContent(const std::string &s, const std::string &content_type) const;

        void setContent(std::string &&s, const std::string &content_type) const;

        void setFileContent(const std::string &path, const std::string &content_type) const;

        void setFileContent(const std::string &path) const;

        void send(int statusCode, const std::string &data = "", const std::string &content_type = "text/plain") const;

        void sendJSON(int statusCode = 200, const json &data = json::object()) const;
#ifdef MANTIS_ENABLE_SCRIPTING
        void sendJson(int statusCode, const DukValue &data) const;
#endif
        void sendText(int statusCode = 200, const std::string &data = "") const;

        void sendHtml(int statusCode = 200, const std::string &data = "") const;

        void sendEmpty(int statusCode = 204) const;

        static void registerDuktapeMethods();
    };

    ///> Middleware shorthand for the content reader
    class MantisContentReader {
        const httplib::ContentReader &m_reader;
        const MantisRequest &m_req;

        std::vector<httplib::FormData> m_formData;
        json m_json{}, m_filesMetadata{};
        bool m_parsed = false;

    public:
        explicit MantisContentReader(const httplib::ContentReader &reader, const MantisRequest &req);

        [[nodiscard]] const httplib::ContentReader &reader() const;

        [[nodiscard]] bool isMultipartFormData() const;

        [[nodiscard]] const std::vector<httplib::FormData> &formData() const;

        [[nodiscard]] const json &filesMetadata() const;

        [[nodiscard]] const json &jsonBody() const;

        void parseFormDataToEntity(const Entity &entity);

        void writeFiles(const std::string& entity_name);

        void undoWrittenFiles(const std::string& entity_name);

        static std::string hashMultipartMetadata(const httplib::FormData& data);

    private:
        void read();

        void readMultipart();

        void readJSON();

        static json getValueFromType(const std::string& type, const std::string& value);
    };
} // mb

#endif //MANTISAPP_DUKTAPE_CUSTOM_TYPES_H
