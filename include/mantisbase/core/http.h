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
        /**
         * @brief Construct request wrapper.
         * @param _req Reference to httplib::Request object
         */
        explicit MantisRequest(const httplib::Request &_req);

        /**
         * @brief Get HTTP request method (GET, POST, etc.).
         * @return Method string
         */
        std::string getMethod() const;

        /**
         * @brief Get request path.
         * @return Path string
         */
        std::string getPath() const;

        /**
         * @brief Get raw request body.
         * @return Body string
         */
        std::string getBody() const;

        /**
         * @brief Get client remote address.
         * @return IP address string
         */
        std::string getRemoteAddr() const;

        /**
         * @brief Get client remote port.
         * @return Port number
         */
        int getRemotePort() const;

        /**
         * @brief Get server local address.
         * @return IP address string
         */
        std::string getLocalAddr() const;

        /**
         * @brief Get server local port.
         * @return Port number
         */
        int getLocalPort() const;

        /**
         * @brief Check if header exists.
         * @param key Header name
         * @return true if header exists
         */
        bool hasHeader(const std::string &key) const;

        /**
         * @brief Get header value with default and index.
         * @param key Header name
         * @param def Default value if not found
         * @param id Index if multiple values (0-based)
         * @return Header value or default
         */
        std::string getHeaderValue(const std::string &key, const char *def, size_t id) const;

        /**
         * @brief Get header value as unsigned 64-bit integer.
         * @param key Header name
         * @param def Default value
         * @param id Index if multiple values
         * @return Header value as size_t
         */
        size_t getHeaderValueU64(const std::string &key, size_t def, size_t id) const;

        /**
         * @brief Get count of header values.
         * @param key Header name
         * @return Number of values
         */
        size_t getHeaderValueCount(const std::string &key) const;

        /**
         * @brief Check if trailer exists.
         * @param key Trailer name
         * @return true if trailer exists
         */
        bool hasTrailer(const std::string &key) const;

        /**
         * @brief Get trailer value.
         * @param key Trailer name
         * @param id Index if multiple values
         * @return Trailer value
         */
        std::string getTrailerValue(const std::string &key, size_t id) const;

        /**
         * @brief Get count of trailer values.
         * @param key Trailer name
         * @return Number of values
         */
        size_t getTrailerValueCount(const std::string &key) const;

        /**
         * @brief Get route match information.
         * @return httplib::Match object with path parameters
         */
        httplib::Match matches() const;

        /**
         * @brief Check if query parameter exists.
         * @param key Parameter name
         * @return true if parameter exists
         */
        bool hasQueryParam(const std::string &key) const;

        /**
         * @brief Get query parameter value (first occurrence).
         * @param key Parameter name
         * @return Parameter value or empty string
         */
        std::string getQueryParamValue(const std::string &key) const;

        /**
         * @brief Get query parameter value by index.
         * @param key Parameter name
         * @param id Index if multiple values (0-based)
         * @return Parameter value
         */
        std::string getQueryParamValue(const std::string &key, size_t id) const;

        /**
         * @brief Get count of query parameter values.
         * @param key Parameter name
         * @return Number of values
         */
        size_t getQueryParamValueCount(const std::string &key) const;

        /**
         * @brief Check if request has path parameters.
         * @return true if path parameters exist
         */
        bool hasPathParams() const;

        /**
         * @brief Check if path parameter exists.
         * @param key Parameter name (from route pattern like /:id)
         * @return true if parameter exists
         */
        bool hasPathParam(const std::string &key) const;

        /**
         * @brief Get path parameter value.
         * @param key Parameter name
         * @return Parameter value
         * @code
         * // Route: GET /api/v1/users/:id
         * // Request: GET /api/v1/users/123
         * std::string id = req.getPathParamValue("id"); // "123"
         * @endcode
         */
        std::string getPathParamValue(const std::string &key) const;

        /**
         * @brief Get count of path parameter values.
         * @param key Parameter name
         * @return Number of values
         */
        size_t getPathParamValueCount(const std::string &key) const;

        /**
         * @brief Check if request is multipart/form-data.
         * @return true if multipart form data
         */
        bool isMultipartFormData() const;

        /**
         * @brief Register MantisRequest methods for JavaScript/Duktape.
         */
        static void registerDuktapeMethods();

        /**
         * @brief Check if context store has key.
         * @param key Key to check
         * @return true if key exists in context
         */
        bool hasKey(const std::string &key) const;

        /**
         * @brief Extract Bearer token from Authorization header.
         * @return Token string (without "Bearer " prefix)
         */
        std::string getBearerTokenAuth() const;

        /**
         * @brief Parse request body as JSON.
         * @return Pair of (JSON object, error message)
         *   - If parsing succeeds: (json object, "")
         *   - If parsing fails: (empty json, error message)
         */
        std::pair<nlohmann::json, std::string> getBodyAsJson() const;

        /**
         * @brief Store value in request context (for middleware communication).
         * @tparam T Value type
         * @param key Context key
         * @param value Value to store
         * @code
         * req.set<std::string>("user_id", "123");
         * req.set<int>("count", 42);
         * @endcode
         */
        template<typename T>
        void set(const std::string &key, T value) {
            m_store.set(key, value);
        }

        // template<typename T>
        // std::optional<T *> get(const std::string &key) {
        //     return m_store.get<T>(key);
        // }

        /**
         * @brief Get value from context store or return default.
         * @tparam T Value type
         * @param key Context key
         * @param default_value Default value if key not found
         * @return Reference to value (or default if not found)
         * @code
         * int count = req.getOr<int>("count", 0);
         * std::string name = req.getOr<std::string>("name", "Unknown");
         * @endcode
         */
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
     * @brief Wrapper around httplib::Response for consistent API.
     *
     * Provides convenient methods for setting response status, headers, and body
     * with support for JSON, text, HTML, and file responses.
     *
     * @code
     * // Send JSON response
     * res.sendJSON(200, {{"message", "Success"}});
     *
     * // Send text response
     * res.sendText(200, "Hello World");
     *
     * // Set custom header
     * res.setHeader("X-Custom", "value");
     * @endcode
     */
    class MantisResponse {
        httplib::Response &m_res;

        const std::string __class_name__ = "mb::MantisResponse";

    public:
        /**
         * @brief Construct response wrapper.
         * @param _resp Reference to httplib::Response object
         */
        explicit MantisResponse(httplib::Response &_resp);

        /**
         * @brief Destructor.
         */
        ~MantisResponse() = default;

        /**
         * @brief Get HTTP status code.
         * @return Status code (e.g., 200, 404, 500)
         */
        [[nodiscard]] int getStatus() const;

        /**
         * @brief Set HTTP status code.
         * @param s Status code
         */
        void setStatus(int s);

        /**
         * @brief Get HTTP version.
         * @return Version string
         */
        [[nodiscard]] std::string getVersion() const;

        /**
         * @brief Set HTTP version.
         * @param b Version string
         */
        void setVersion(const std::string &b);

        /**
         * @brief Get response body.
         * @return Body string
         */
        [[nodiscard]] std::string getBody() const;

        /**
         * @brief Set response body.
         * @param b Body string
         */
        void setBody(const std::string &b);

        /**
         * @brief Get redirect location.
         * @return Location URL
         */
        [[nodiscard]] std::string getLocation() const;

        /**
         * @brief Set redirect location.
         * @param b Location URL
         */
        void setLocation(const std::string &b);

        /**
         * @brief Get status reason phrase.
         * @return Reason string
         */
        [[nodiscard]] std::string getReason() const;

        /**
         * @brief Set status reason phrase.
         * @param b Reason string
         */
        void setReason(const std::string &b);

        /**
         * @brief Check if header exists.
         * @param key Header name
         * @return true if header exists
         */
        [[nodiscard]] bool hasHeader(const std::string &key) const;

        /**
         * @brief Get header value.
         * @param key Header name
         * @param def Default value if not found
         * @param id Index if multiple values (0-based)
         * @return Header value or default
         */
        std::string getHeaderValue(const std::string &key, const char *def = "", size_t id = 0) const;

        /**
         * @brief Get header value as unsigned 64-bit integer.
         * @param key Header name
         * @param def Default value
         * @param id Index if multiple values
         * @return Header value as size_t
         */
        [[nodiscard]] size_t getHeaderValueU64(const std::string &key, size_t def = 0, size_t id = 0) const;

        /**
         * @brief Get count of header values.
         * @param key Header name
         * @return Number of values
         */
        [[nodiscard]] size_t getHeaderValueCount(const std::string &key) const;

        /**
         * @brief Set response header.
         * @param key Header name
         * @param val Header value
         */
        void setHeader(const std::string &key, const std::string &val) const;

        /**
         * @brief Check if trailer exists.
         * @param key Trailer name
         * @return true if trailer exists
         */
        [[nodiscard]] bool hasTrailer(const std::string &key) const;

        /**
         * @brief Get trailer value.
         * @param key Trailer name
         * @param id Index if multiple values
         * @return Trailer value
         */
        [[nodiscard]] std::string getTrailerValue(const std::string &key, size_t id = 0) const;

        /**
         * @brief Get count of trailer values.
         * @param key Trailer name
         * @return Number of values
         */
        [[nodiscard]] size_t getTrailerValueCount(const std::string &key) const;

        /**
         * @brief Set redirect response.
         * @param url Redirect URL
         * @param status HTTP status code (default: 302 Found)
         */
        void setRedirect(const std::string &url, int status = httplib::StatusCode::Found_302) const;

        /**
         * @brief Set response content from buffer.
         * @param s Content buffer
         * @param n Content length
         * @param content_type MIME type
         */
        void setContent(const char *s, size_t n, const std::string &content_type) const;

        /**
         * @brief Set response content from string.
         * @param s Content string
         * @param content_type MIME type
         */
        void setContent(const std::string &s, const std::string &content_type) const;

        /**
         * @brief Set response content from moved string.
         * @param s Content string (moved)
         * @param content_type MIME type
         */
        void setContent(std::string &&s, const std::string &content_type) const;

        /**
         * @brief Set response content from file with MIME type.
         * @param path File path
         * @param content_type MIME type
         */
        void setFileContent(const std::string &path, const std::string &content_type) const;

        /**
         * @brief Set response content from file (auto-detect MIME type).
         * @param path File path
         */
        void setFileContent(const std::string &path) const;

        /**
         * @brief Send response with status, data, and content type.
         * @param statusCode HTTP status code
         * @param data Response data
         * @param content_type MIME type (default: "text/plain")
         */
        void send(int statusCode, const std::string &data = "", const std::string &content_type = "text/plain") const;

        /**
         * @brief Send JSON response.
         * @param statusCode HTTP status code (default: 200)
         * @param data JSON object to send
         * @code
         * res.sendJSON(200, {{"success", true}, {"data", result}});
         * @endcode
         */
        void sendJSON(int statusCode = 200, const json &data = json::object()) const;
#ifdef MANTIS_ENABLE_SCRIPTING
        /**
         * @brief Send JSON response from Duktape value (for JavaScript).
         * @param statusCode HTTP status code
         * @param data DukValue object
         */
        void sendJson(int statusCode, const DukValue &data) const;
#endif
        /**
         * @brief Send text response.
         * @param statusCode HTTP status code (default: 200)
         * @param data Text content
         */
        void sendText(int statusCode = 200, const std::string &data = "") const;

        /**
         * @brief Send HTML response.
         * @param statusCode HTTP status code (default: 200)
         * @param data HTML content
         */
        void sendHtml(int statusCode = 200, const std::string &data = "") const;

        /**
         * @brief Send empty response (no body).
         * @param statusCode HTTP status code (default: 204 No Content)
         */
        void sendEmpty(int statusCode = 204) const;

        /**
         * @brief Register MantisResponse methods for JavaScript/Duktape.
         */
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
