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
    class DuktapeImpl {
    public:
        static duk_ret_t nativeConsoleInfo(duk_context *ctx);
        static duk_ret_t nativeConsoleTrace(duk_context *ctx);
        static duk_ret_t nativeConsoleTable(duk_context *ctx);
    };

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

        void setPathParam(const std::string &key, const std::string &value);
        void setPathParams(const std::unordered_map<std::string, std::string> &params);

        std::string getMethod() const;
        std::string getPath() const;
        std::string getBody() const;
        std::string getRemoteAddr() const;
        int getRemotePort() const;
        std::string getLocalAddr() const;
        int getLocalPort() const;

        bool hasHeader(const std::string &key) const;
        std::string getHeaderValue(const std::string &key, const char *def = "", size_t id = 0) const;
        size_t getHeaderValueU64(const std::string &key, size_t def = 0, size_t id = 0) const;
        size_t getHeaderValueCount(const std::string &key) const;

        bool hasQueryParam(const std::string &key) const;
        std::string getQueryParamValue(const std::string &key) const;
        std::string getQueryParamValue(const std::string &key, size_t id) const;
        size_t getQueryParamValueCount(const std::string &key) const;

        bool hasPathParams() const;
        bool hasPathParam(const std::string &key) const;
        std::string getPathParamValue(const std::string &key) const;
        size_t getPathParamValueCount(const std::string &key) const;

        bool isMultipartFormData() const;

        static void registerDuktapeMethods();

        bool hasKey(const std::string &key) const;
        std::string getBearerTokenAuth() const;
        std::string getCookieValue(const std::string &key) const;
        /** Bearer token if present, otherwise the auth cookie value. */
        std::string resolveAuthToken() const;
        std::pair<nlohmann::json, std::string> getBodyAsJson() const;

        const drogon::HttpRequestPtr& drogonRequest() const;

        template<typename T>
        void set(const std::string &key, T value) {
            m_req->attributes()->insert(key, value);
        }

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

        [[nodiscard]] const drogon::HttpResponsePtr& drogonResponse() const;

        [[nodiscard]] int getStatus() const;
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
        std::string getHeaderValue(const std::string &key, const char *def = "", size_t id = 0) const;
        [[nodiscard]] size_t getHeaderValueU64(const std::string &key, size_t def = 0, size_t id = 0) const;
        [[nodiscard]] size_t getHeaderValueCount(const std::string &key) const;

        void setHeader(const std::string &key, const std::string &val) const;

        void setAuthTokenCookie(const std::string &token, int max_age_seconds) const;
        void clearAuthTokenCookie() const;

        void setRedirect(const std::string &url, int status = 302) const;

        void setContent(const char *s, size_t n, const std::string &content_type) const;
        void setContent(const std::string &s, const std::string &content_type) const;
        void setContent(std::string &&s, const std::string &content_type) const;

        void setFileContent(const std::string &path, const std::string &content_type) const;
        void setFileContent(const std::string &path) const;

        void send(int statusCode, const std::string &data = "", const std::string &content_type = "text/plain") const;
        void sendJSON(int statusCode = 200, const json &data = json::object()) const;
#ifdef MB_SCRIPTING_ENABLED
        void sendJson(int statusCode, const DukValue &data) const;
#endif
        void sendText(int statusCode = 200, const std::string &data = "") const;
        void sendHtml(int statusCode = 200, const std::string &data = "") const;
        void sendEmpty(int statusCode = 204) const;

        static void registerDuktapeMethods();
    };

    struct FormDataItem {
        std::string name;
        std::string content;
        std::string filename;
        std::string content_type;
    };

    class MantisContentReader {
        const MantisRequest &m_req;

        std::vector<FormDataItem> m_formData;
        json m_json{}, m_filesMetadata{};
        bool m_parsed = false;

    public:
        explicit MantisContentReader(const MantisRequest &req);

        [[nodiscard]] bool isMultipartFormData() const;

        [[nodiscard]] const std::vector<FormDataItem> &formData() const;

        [[nodiscard]] const json &filesMetadata() const;

        [[nodiscard]] const json &jsonBody() const;

        void parseFormDataToEntity(const Entity &entity);

        void writeFiles(const std::string& entity_name);

        void undoWrittenFiles(const std::string& entity_name);

        static std::string hashMultipartMetadata(const FormDataItem& data);

    private:
        void read();
        void readMultipart();
        void readJSON();
        static json getValueFromType(const std::string& type, const std::string& value);
    };
} // mb

#endif //MB_HTTP_H
