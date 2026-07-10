#ifndef MB_HTTP_H
#define MB_HTTP_H

#include "exceptions.h"
#include "models/entity.h"

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
#ifdef MB_SCRIPTING_ENABLED
    class DuktapeImpl {
    public:
        static duk_ret_t nativeConsoleInfo(duk_context *ctx);
        static duk_ret_t nativeConsoleTrace(duk_context *ctx);
        static duk_ret_t nativeConsoleTable(duk_context *ctx);
    };
#endif

    class MantisRequest {
        drogon::HttpRequestPtr m_req;
        ContextStore m_store;
        std::unordered_map<std::string, std::string> m_pathParams;

        const std::string __class_name__ = "mb::MantisRequest";

    public:
        explicit MantisRequest(const drogon::HttpRequestPtr &_req);

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
        std::pair<nlohmann::json, std::string> getBodyAsJson() const;

        const drogon::HttpRequestPtr& drogonRequest() const { return m_req; }

        template<typename T>
        void set(const std::string &key, T value) {
            m_store.set(key, value);
        }

        template<typename T>
        T &getOr(const std::string &key, T default_value) {
            return m_store.getOr<T>(key, default_value);
        }

    private:
#ifdef MB_SCRIPTING_ENABLED
        DukValue get_duk(const std::string &key);
        DukValue getOr_duk(const std::string &key, const DukValue &default_value);
        void set_duk(const std::string &key, const DukValue &value);
#endif
    };

    class MantisResponse {
        drogon::HttpResponsePtr m_res;

        const std::string __class_name__ = "mb::MantisResponse";

    public:
        explicit MantisResponse();

        ~MantisResponse() = default;

        const drogon::HttpResponsePtr& drogonResponse() const;

        [[nodiscard]] int getStatus() const;
        void setStatus(int s);

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
