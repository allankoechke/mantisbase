/**
 * @file http.h
 * @brief HTTP request/response wrappers and multipart content reader.
 *
 * @ref MantisRequest and @ref MantisResponse wrap Drogon types and expose MantisBase
 * conventions: per-request attributes via `set()`/`getOr()`, auth helpers, and JSON
 * envelope responses via @ref MantisResponse::sendJSON.
 */

#ifndef MB_HTTP_H
#define MB_HTTP_H

#include "exceptions.h"
#include "models/entity.h"
#include "types.h"

#ifdef MB_SCRIPTING_ENABLED
#include <dukglue/dukglue.h>
#endif

#include "context_store.h"
#include "../utils/utils.h"
#include "types.h"
#include <fstream>
#include <unordered_map>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

namespace mb {
    class MantisBase; // forward declaration; MantisRequest holds a reference to it

#ifdef MB_SCRIPTING_ENABLED
    /** Duktape bindings for the JS `console` object in server scripts. */
    class DuktapeImpl {
    public:
        static duk_ret_t nativeConsoleInfo(duk_context *ctx);
        static duk_ret_t nativeConsoleTrace(duk_context *ctx);
        static duk_ret_t nativeConsoleTable(duk_context *ctx);
    };

    /** Fire Duktape lifecycle hooks for server start and record mutations. */
    class ScriptingHooks {
    public:
        static void fireOnServerStart(duk_context *ctx);
        static void fireOnRecordCreated(duk_context *ctx, const std::string &entity, const std::string &recordId);
        static void fireOnRecordUpdated(duk_context *ctx, const std::string &entity, const std::string &recordId);
    };
#endif

    /**
     * @brief HTTP request wrapper bound to a @ref MantisBase instance.
     *
     * Per-request attributes (`auth`, `verification`, etc.) are stored on the
     * underlying Drogon request via `set()` / `getOr()`, not a global context.
     * Use `mbApp()` (from @ref IMantisBase) to reach application services.
     */
    class MantisRequest: public IMantisBase {
        drogon::HttpRequestPtr m_req;
        std::unordered_map<std::string, std::string> m_pathParams;

        /// Lazily-parsed, cached request body. getBodyAsJson() is called
        /// several times per request (access-rule evaluation, then the
        /// handler), so the body is parsed once and reused.
        mutable std::optional<std::pair<nlohmann::json, std::string>> m_bodyJsonCache;

    public:
        explicit MantisRequest(const MantisBase& app, drogon::HttpRequestPtr _req);

        /** Set a single route path parameter (e.g. `:id`). */
        void setPathParam(const std::string &key, const std::string &value);
        /** Replace all route path parameters. */
        void setPathParams(const std::unordered_map<std::string, std::string> &params);

        [[nodiscard]] std::string getMethod() const;
        [[nodiscard]] std::string getPath() const;
        [[nodiscard]] std::string getBody() const;
        [[nodiscard]] std::string getRemoteAddr() const;
        [[nodiscard]] int getRemotePort() const;
        [[nodiscard]] std::string getLocalAddr() const;
        [[nodiscard]] int getLocalPort() const;

        [[nodiscard]] bool hasHeader(const std::string &key) const;
        [[nodiscard]] std::string getHeaderValue(const std::string &key, const char *def = "", size_t id = 0) const;
        [[nodiscard]] size_t getHeaderValueU64(const std::string &key, size_t def = 0, size_t id = 0) const;
        [[nodiscard]] size_t getHeaderValueCount(const std::string &key) const;

        [[nodiscard]] bool hasQueryParam(const std::string &key) const;
        [[nodiscard]] std::string getQueryParamValue(const std::string &key) const;
        [[nodiscard]] std::string getQueryParamValue(const std::string &key, size_t id) const;
        [[nodiscard]] size_t getQueryParamValueCount(const std::string &key) const;

        [[nodiscard]] bool hasPathParams() const;
        [[nodiscard]] bool hasPathParam(const std::string &key) const;
        [[nodiscard]] std::string getPathParamValue(const std::string &key) const;
        [[nodiscard]] size_t getPathParamValueCount(const std::string &key) const;

        /** @return `true` when `Content-Type` is `multipart/form-data`. */
        [[nodiscard]] bool isMultipartFormData() const;

        /** Register request accessor methods on the Duktape `req` object. */
        static void registerDuktapeMethods();

        /** @return `true` if a per-request attribute exists on the Drogon request. */
        [[nodiscard]] bool hasKey(const std::string &key) const;

        /** @return Bearer token from the Authorization header, or empty. */
        [[nodiscard]] std::string getBearerTokenAuth() const;

        [[nodiscard]] std::string getCookieValue(const std::string &key) const;

        /** Bearer token if present, otherwise the auth cookie value. */
        [[nodiscard]] std::string resolveAuthToken() const;

        /**
         * @brief Parse and cache the JSON request body.
         * @return `{body, error}` — error non-empty on parse failure.
         */
        [[nodiscard]] std::pair<nlohmann::json, std::string> getBodyAsJson() const;

        /** @return Underlying Drogon request pointer. */
        [[nodiscard]] const drogon::HttpRequestPtr& drogonRequest() const;

        /** @return `true` when the request auth block is guest/unauthenticated. */
        [[nodiscard]] bool isGuestAuth();

        /** @return `true` when the request auth block is an admin session. */
        [[nodiscard]] bool isAdminAuth();

        /** @return `true` when the request auth block is a regular user session. */
        [[nodiscard]] bool isUserAuth();

        /** Store a per-request attribute on the underlying Drogon request. */
        template<typename T>
        void set(const std::string &key, T value) {
            m_req->attributes()->insert(key, value);
        }

        /** Get a per-request attribute, inserting `default_value` if missing. */
        template<typename T>
        const T &getOr(const std::string &key, T default_value) {
            if (!m_req->attributes()->find(key)) {
                m_req->attributes()->insert(key, std::any(std::move(default_value)));
            }
            return m_req->attributes()->get<T>(key);
        }

    private:
#ifdef MB_SCRIPTING_ENABLED
        DukValue get_duk(const std::string &key);
        DukValue getOr_duk(const std::string &key, const DukValue &default_value);
        void set_duk(const std::string &key, const DukValue &value);
#endif
    };

    /**
     * @brief HTTP response wrapper bound to a @ref MantisBase instance.
     *
     * Constructed by the router for each request; use `sendJSON()`, `send()`,
     * or header helpers to build the outgoing response.
     */
    class MantisResponse: public IMantisBase {
        drogon::HttpResponsePtr m_res;

    public:
        explicit MantisResponse(const MantisBase& app);

        ~MantisResponse() = default;

        /** @return Underlying Drogon response pointer. */
        [[nodiscard]] const drogon::HttpResponsePtr& drogonResponse() const;

        [[nodiscard]] int getStatus() const;
        /** Set HTTP status code. */
        void setStatus(int s) const;

        [[nodiscard]] std::string getVersion() const;
        void setVersion(const std::string &b);

        [[nodiscard]] std::string getBody() const;
        void setBody(const std::string &b);

        [[nodiscard]] std::string getLocation() const;
        void setLocation(const std::string &b);

        [[nodiscard]] std::string getReason() const;
        void setReason(const std::string &b);

        [[nodiscard]] bool hasHeader(const std::string &key) const;
        [[nodiscard]] std::string getHeaderValue(const std::string &key, const char *def = "", size_t id = 0) const;
        [[nodiscard]] size_t getHeaderValueU64(const std::string &key, size_t def = 0, size_t id = 0) const;
        [[nodiscard]] size_t getHeaderValueCount(const std::string &key) const;

        void setHeader(const std::string &key, const std::string &val) const;

        /** Set the HttpOnly auth cookie used by login/refresh handlers. */
        void setAuthTokenCookie(const std::string &token, int max_age_seconds) const;
        /** Clear the HttpOnly auth cookie. */
        void clearAuthTokenCookie() const;

        /** Set `Location` header and status for redirects. */
        void setRedirect(const std::string &url, int status = 302) const;

        /** Set response body with explicit content type. */
        void setContent(const char *s, size_t n, const std::string &content_type) const;
        void setContent(const std::string &s, const std::string &content_type) const;
        void setContent(std::string &&s, const std::string &content_type) const;

        /** Stream a file from disk as the response body. */
        void setFileContent(const std::string &path, const std::string &content_type) const;
        void setFileContent(const std::string &path) const;

        /** Send response with status, body, and content type. */
        void send(int statusCode, const std::string &data = "", const std::string &content_type = "text/plain") const;

        /** Send the standard MantisBase JSON envelope `{ status, error, data }`. */
        void sendJSON(int statusCode = 200, const json &data = json::object()) const;
#ifdef MB_SCRIPTING_ENABLED
        /** Send JSON envelope from a Duktape value. */
        void sendJson(int statusCode, const DukValue &data) const;
#endif
        /** Send plain-text body. */
        void sendText(int statusCode = 200, const std::string &data = "") const;
        /** Send HTML body. */
        void sendHtml(int statusCode = 200, const std::string &data = "") const;
        /** Send empty body (default 204 No Content). */
        void sendEmpty(int statusCode = 204) const;

        /** Register response helper methods on the Duktape `res` object. */
        static void registerDuktapeMethods();
    };

    /** Single field from a parsed `multipart/form-data` body. */
    struct FormDataItem {
        std::string name;
        std::string content;
        std::string filename;
        std::string content_type;
    };

    /**
     * @brief Lazy multipart/JSON body parser for content-reader route handlers.
     *
     * Parses once, then exposes form fields, file metadata, and typed entity binding.
     */
    class MantisContentReader {
        const MantisRequest &m_req;

        std::vector<FormDataItem> m_formData;
        json m_json{}, m_filesMetadata{};
        bool m_parsed = false;

    public:
        explicit MantisContentReader(const MantisRequest &req);

        /** @return `true` when the request body is multipart form data. */
        [[nodiscard]] bool isMultipartFormData() const;

        /** Parsed form fields (text and file parts). */
        [[nodiscard]] const std::vector<FormDataItem> &formData() const;

        /** File field metadata keyed by form field name. */
        [[nodiscard]] const json &filesMetadata() const;

        /** Parsed JSON body (empty object when not JSON). */
        [[nodiscard]] const json &jsonBody() const;

        /** Map parsed form/JSON fields onto an entity for create/update. */
        void parseFormDataToEntity(const Entity &entity);

        /** Persist uploaded files for `entity_name` using stored metadata. */
        void writeFiles(const std::string& entity_name);

        /** Remove files written by @ref writeFiles on validation failure. */
        void undoWrittenFiles(const std::string& entity_name);

        /** Stable hash of multipart metadata for deduplication checks. */
        static std::string hashMultipartMetadata(const FormDataItem& data);

    private:
        void read();
        void readMultipart();
        void readJSON();
        static json getValueFromType(const std::string& type, const std::string& value);
    };
} // mb

#endif //MB_HTTP_H
