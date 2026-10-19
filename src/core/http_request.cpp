#include <utility>
#include <algorithm>

#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/models/access_rules.h"
#include "../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/core/types.h"

#include <drogon/MultiPart.h>

#ifdef MB_SCRIPTING_ENABLED
#include "../../include/mantisbase/scripting/scripting_engine.h"
#include <dukglue/dukglue.h>
#endif

namespace mb {
    MbRequest::MbRequest(const MantisBase &app, drogon::HttpRequestPtr _req)
        : IMantisBase(app),
          m_req(std::move(_req)) {
    }

    void MbRequest::setPathParam(const std::string &key, const std::string &value) {
        m_pathParams[key] = value;
    }

    void MbRequest::setPathParams(const std::unordered_map<std::string, std::string> &params) {
        m_pathParams = params;
    }

    std::string MbRequest::method() const {
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

    std::string MbRequest::path() const { return m_req->path(); }

    std::string MbRequest::body() const { return std::string(m_req->body()); }

    std::string MbRequest::remoteAddr() const {
        auto direct_ip = m_req->peerAddr().toIp();

        if (hasHeader("X-Forwarded-For")) {
            auto trusted_proxies_str = getEnvOrDefault("MB_TRUSTED_PROXIES", "");
            if (!trusted_proxies_str.empty()) {
                auto proxies = splitString(trusted_proxies_str, ",");
                bool is_trusted = false;
                for (auto &proxy : proxies) {
                    proxy = trim(proxy);
                    if (proxy == direct_ip) {
                        is_trusted = true;
                        break;
                    }
                }

                if (is_trusted) {
                    auto forwarded = headerValue("X-Forwarded-For", "");
                    if (const auto first_comma = forwarded.find(',');
                        first_comma != std::string::npos) {
                        forwarded = forwarded.substr(0, first_comma);
                    }
                    forwarded = trim(forwarded);
                    if (isValidIP(forwarded)) {
                        return forwarded;
                    }
                    mbApp().logger().warn("Invalid IP Header",
                                          fmt::format("Invalid IP address in X-Forwarded-For header: {}", forwarded));
                }
            }
        }

        if (isValidIP(direct_ip)) {
            return direct_ip;
        }

        mbApp().logger().warn("IP Detection Failed", "Unable to determine valid client IP address");
        return direct_ip;
    }

    int MbRequest::remotePort() const { return m_req->peerAddr().toPort(); }

    std::string MbRequest::localAddr() const { return m_req->localAddr().toIp(); }

    int MbRequest::localPort() const { return m_req->localAddr().toPort(); }

    bool MbRequest::hasHeader(const std::string &key) const {
        return !m_req->getHeader(key).empty();
    }

    std::string MbRequest::headerValue(const std::string &key,
                                              const std::string& def) const {
        auto val = m_req->getHeader(key);
        return val.empty() ? std::string(def) : val;
    }

    bool MbRequest::hasQueryParam(const std::string &key) const {
        return m_req->getOptionalParameter<std::string>(key).has_value();
    }

    std::string MbRequest::queryParamValue(const std::string &key) const {
        auto val = m_req->getOptionalParameter<std::string>(key);
        return val.has_value() ? val.value() : "";
    }

    bool MbRequest::hasPathParams() const { return !m_pathParams.empty(); }

    bool MbRequest::hasPathParam(const std::string &key) const {
        return m_pathParams.contains(key);
    }

    std::string MbRequest::pathParamValue(const std::string &key) const {
        if (const auto it = m_pathParams.find(key); it != m_pathParams.end())
            return it->second;
        return "";
    }

    bool MbRequest::isMultipartFormData() const {
        const auto ct = m_req->getHeader("Content-Type");
        return ct.find("multipart/form-data") != std::string::npos;
    }

    bool MbRequest::hasKey(const std::string &key) const {
        return m_req->attributes()->find(key);
    }

    std::string MbRequest::bearerTokenAuth() const {
        if (hasHeader("Authorization")) {
            const auto auth = headerValue("Authorization", "");
            constexpr size_t bearer_prefix_len = 7; // "Bearer "
            return auth.size() > bearer_prefix_len ? auth.substr(bearer_prefix_len) : "";
        }
        return "";
    }

    std::string MbRequest::cookieValue(const std::string &key) const {
        return m_req->getCookie(key);
    }

    std::string MbRequest::resolveAuthToken() const {
        if (hasHeader("Authorization")) {
            const auto bearer = trim(bearerTokenAuth());
            if (!bearer.empty()) {
                return bearer;
            }
        }

        return trim(cookieValue(kAuthTokenCookieName));
    }

    std::pair<nlohmann::json, std::string> MbRequest::bodyAsJson() const {
        try {
            const auto b = body();
            auto obj = b.empty() ? nlohmann::json::object() : nlohmann::json::parse(b);
            return {obj, ""};
        } catch (const std::exception &e) {
            return {nlohmann::json::object(), e.what()};
        }
    }

    const drogon::HttpRequestPtr & MbRequest::drogonRequest() const { return m_req; }

    bool MbRequest::isGuestAuth() {
        return mb::isGuestAuth(get<json>("auth", json::object()));
    }

    bool MbRequest::isAdminAuth() {
        return mb::isAdminAuth(get<json>("auth", json::object()));
    }

    bool MbRequest::isUserAuth() {
        return mb::isUserAuth(get<json>("auth", json::object()));
    }

    const json &MbRequest::jsonBody() const {
        ensureBodyParsed();
        return m_unifiedJson;
    }

    const std::vector<MbFormDataItem> &MbRequest::formData() const {
        ensureBodyParsed();
        return m_formData;
    }

    const json &MbRequest::filesMetadata() const {
        ensureBodyParsed();
        return m_filesMetadata;
    }

    // --- Unified body handling (lazy, parsed once per request) ---

    void MbRequest::ensureBodyParsed() const {
        if (m_bodyParsed) return;

        if (isMultipartFormData()) {
            logger().trace("Content Reading", "Reading request as Multipart!");
            readMultipart();
        } else {
            logger().trace("Content Reading", "Reading request as JSON!");
            readJSON();
        }

        m_bodyParsed = true;
    }

    void MbRequest::readMultipart() const {
        const auto &dReq = drogonRequest();

        drogon::MultiPartParser fileUpload;
        if (fileUpload.parse(dReq) == 0) {
            auto &files = fileUpload.getFiles();
            // Get uploaded files
            for (auto &file: files) {
                MbFormDataItem item;
                item.name = file.getItemName();
                item.filename = file.getFileName();
                item.content_type = file.getContentType();
                item.content = std::string(file.fileData(), file.fileLength());
                m_formData.push_back(std::move(item));
            }
        }

        // Get form parameters (non-file fields)
        auto params = dReq->parameters();
        for (const auto &[key, value]: params) {
            // Skip if this key was already handled as a file upload
            bool is_file = false;
            for (const auto &fd: m_formData) {
                if (fd.name == key && !fd.filename.empty()) {
                    is_file = true;
                    break;
                }
            }
            if (is_file) continue;

            MbFormDataItem item;
            item.name = key;
            item.content = value;
            m_formData.push_back(std::move(item));
        }
    }

    void MbRequest::readJSON() const {
        std::string body = this->body();
        try {
            if (trim(body).empty()) m_unifiedJson = json::object();
            else m_unifiedJson = json::parse(body);
        } catch (const std::exception &) {
            throw MantisException(400, "Invalid JSON in request body.");
        }
    }

    void MbRequest::parseFormDataToEntity(const Entity &entity) const {
        ensureBodyParsed();
        if (!isMultipartFormData()) {
            return;
        }

        const bool uploads_disabled = getEnvOrDefault("MB_DISABLE_FILE_UPLOADS", "0") == "1";
        json json_body = json::object(), json_files = json::object();

        for (const auto &form_data: m_formData) {
            if (!form_data.filename.empty()) {
                if (uploads_disabled) {
                    throw MantisException(503, "File uploads are disabled.");
                }

                if (!entity.hasField(form_data.name)) {
                    throw MantisException(
                        400,
                        fmt::format("Unknown field `{}` for file type upload!", form_data.name)
                    );
                }

                const auto e_field = EntitySchemaField(entity.field(form_data.name).value());

                if (!(e_field.type() == "file" || e_field.type() == "files")) {
                    throw MantisException(
                        400,
                        fmt::format("Field `{}` is not of type `file` or `files`!", form_data.name)
                    );
                }

                // `maxFileSize` is stored in bytes (see KeyValStore defaults).
                const auto max_file_size = mbApp().settings().configs()
                                                .value("maxFileSize", 10 * 1024 * 1024);
                if (max_file_size > 0 && form_data.content.size() > static_cast<size_t>(max_file_size)) {
                    throw MantisException(
                        413,
                        fmt::format("File `{}` exceeds the maximum allowed size of {} bytes.",
                                    form_data.filename, max_file_size)
                    );
                }

                const fs::path upload_path{form_data.filename};
                const std::string ext = upload_path.has_extension()
                    ? upload_path.extension().string() : std::string{};
                if (!ext.empty() && !isAllowedFileExtension(ext)) {
                    throw MantisException(
                        415,
                        fmt::format("File extension `{}` is not allowed.", ext)
                    );
                }

                const auto dir = mbApp().files().dirPath(entity.name(), true);
                const auto new_filename = sanitizeFilename(form_data.filename);
                std::string filepath = (fs::path(dir) / new_filename).string();

                json file_record;
                file_record["filename"] = new_filename;
                file_record["path"] = filepath;
                file_record["name"] = form_data.name;
                file_record["hash"] = MbRequest::hashMultipartMetadata(form_data);

                if (e_field.type() == "file") {
                    json_files[form_data.name] = file_record;
                    json_body[form_data.name] = new_filename;
                } else {
                    try {
                        if (!json_body.contains(form_data.name))
                            json_body[form_data.name] = nullptr;
                        if (!json_files.contains(form_data.name))
                            json_files[form_data.name] = nullptr;

                        json_body[form_data.name].push_back(new_filename);
                        json_files[form_data.name].push_back(file_record);
                    } catch (const std::exception &) {
                        throw MantisException(
                            500,
                            "Error parsing file upload data."
                        );
                    }
                }
            } else {
                try {
                    auto e_field_opt = entity.field(form_data.name);
                    if (e_field_opt.has_value()) {
                        auto schema_field = EntitySchemaField(e_field_opt.value());

                        try {
                            const auto &type = schema_field.type();

                            if (type == "files") {
                                auto data = trim(form_data.content).empty()
                                                ? nullptr
                                                : json::parse(form_data.content);
                                if (!data.is_array() && !data.is_null()) {
                                    throw MantisException(400, fmt::format(
                                                              "Error parsing field `{}`, expected an array!",
                                                              form_data.name));
                                }

                                if (!json_body.contains(form_data.name))
                                    json_body[form_data.name] = nullptr;

                                if (data == nullptr) continue;

                                for (const auto &d: data)
                                    json_body[form_data.name].push_back(d);
                            } else {
                                auto v = valueFromType(type, form_data.content)["value"];
                                json_body[form_data.name] = v;
                            }
                        } catch (const std::exception &) {
                            throw MantisException(500, "Error processing form data.");
                        }
                    }
                } catch (const MantisException &) {
                    throw;
                } catch (const std::exception &) {
                    throw MantisException(500, "Error processing form data.");
                }
            }
        }

        m_unifiedJson = json_body;
        m_filesMetadata = json_files;
    }

    void MbRequest::writeFiles(const std::string &entity_name) const {
        ensureBodyParsed();
        if (!isMultipartFormData()) return;

        for (const auto &formData: m_formData) {
            if (formData.filename.empty()) continue;

            const auto file_list = m_filesMetadata[formData.name].is_array()
                                       ? m_filesMetadata[formData.name]
                                       : json::array({m_filesMetadata[formData.name]});

            auto it = std::ranges::find_if(file_list, [&](const json &f) {
                return f["hash"].get<std::string>() == MbRequest::hashMultipartMetadata(formData);
            });

            if (it == file_list.end()) {
                throw MantisException(500, "Error writing files, hash mismatch!");
            }

            const auto &file_record = *it;
            const auto filepath = file_record["path"].get<std::string>();
            if (std::ofstream ofs(filepath, std::ios::binary); ofs.is_open()) {
                ofs.write(formData.content.data(), static_cast<std::streamsize>(formData.content.size()));
                ofs.close();
            } else {
                undoWrittenFiles(entity_name);
                throw MantisException(500, "Failed to open `" + formData.filename + "` file for writing.");
            }
        }
    }

    void MbRequest::undoWrittenFiles(const std::string &entity_name) const {
        if (!isMultipartFormData()) return;

        for (const auto &file: m_filesMetadata) {
            if (file.contains("filename")) {
                [[maybe_unused]] auto _ = mbApp().files().removeFile(
                    entity_name, file["filename"].get<std::string>());
            }
        }
    }

    std::string MbRequest::hashMultipartMetadata(const MbFormDataItem &data) {
        constexpr std::hash<std::string> hasher;
        const size_t h1 = hasher(data.name);
        const size_t h3 = hasher(data.filename);
        const size_t h4 = hasher(data.content_type);
        const size_t content_size_hash = hasher(std::to_string(data.content.size()));

        size_t result = h1;
        result ^= h3 + 0x9e3779b9 + (result << 6) + (result >> 2);
        result ^= h4 + 0x9e3779b9 + (result << 6) + (result >> 2);
        result ^= content_size_hash + 0x9e3779b9 + (result << 6) + (result >> 2);

        return std::to_string(result);
    }

    json MbRequest::valueFromType(const std::string &type, const std::string &value) {
        json obj;
        const auto content = trim(value);
        if (content.empty()) {
            obj["value"] = nullptr;
        } else if (type == "string" || type == "date" || type == "file") {
            obj["value"] = content;
        } else if (type == "double" || type == "int") {
            obj["value"] = json::parse(content);
        } else if (type == "json" || type == "bool") {
            obj["value"] = json::parse(content);
        } else {
            obj["value"] = content;
        }

        return obj;
    }

#ifdef MB_SCRIPTING_ENABLED
    namespace {
        duk_context *scriptingCtx() {
            if (const auto *engine = ScriptingEngine::active()) {
                return engine->ctx();
            }
            return nullptr;
        }

        template<typename T>
        bool tryPushAttribute(duk_context *ctx,
                              const drogon::HttpRequestPtr &req,
                              const std::string &key) {
            try {
                const T &val = req->attributes()->get<T>(key);
                dukglue_push(ctx, val);
                return true;
            } catch (...) {
                return false;
            }
        }
    }

    void MbRequest::registerDuktapeMethods() {
    }

    DukValue MbRequest::get_duk(const std::string &key) const {
        if (!hasKey(key)) {
            return {};
        }

        auto *ctx = scriptingCtx();
        if (!ctx) {
            return {};
        }

        try {
            const auto &j = m_req->attributes()->get<json>(key);
            duk_push_string(ctx, j.dump().c_str());
            duk_json_decode(ctx, -1);
            return DukValue::take_from_stack(ctx);
        } catch (...) {
        }

        if (tryPushAttribute<std::string>(ctx, m_req, key)) {
            return DukValue::take_from_stack(ctx);
        }
        if (tryPushAttribute<int>(ctx, m_req, key)) {
            return DukValue::take_from_stack(ctx);
        }
        if (tryPushAttribute<double>(ctx, m_req, key)) {
            return DukValue::take_from_stack(ctx);
        }
        if (tryPushAttribute<bool>(ctx, m_req, key)) {
            return DukValue::take_from_stack(ctx);
        }
        if (tryPushAttribute<float>(ctx, m_req, key)) {
            return DukValue::take_from_stack(ctx);
        }

        duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported type stored for key '%s'", key.c_str());
        return {};
    }

    DukValue MbRequest::getOr_duk(const std::string &key, const DukValue &default_value) const {
        if (!hasKey(key)) {
            return default_value;
        }
        auto val = get_duk(key);
        if (val == DukValue{}) {
            return default_value;
        }
        return val;
    }

    void MbRequest::set_duk(const std::string &key, const DukValue &value) {
        auto *ctx = scriptingCtx();
        if (!ctx) {
            return;
        }

        switch (value.type()) {
        case DukValue::NUMBER:
            set(key, value.as_double());
            break;
        case DukValue::STRING:
            set(key, value.as_string());
            break;
        case DukValue::BOOLEAN:
            set(key, value.as_bool());
            break;
        case DukValue::NULLREF:
        case DukValue::UNDEFINED:
            break;
        case DukValue::OBJECT: {
            value.push();
            const char *json_str = duk_json_encode(ctx, -1);
            json json_obj = json::parse(json_str ? json_str : "{}");
            duk_pop(ctx);
            set(key, json_obj);
            break;
        }
        default:
            duk_error(ctx, DUK_ERR_TYPE_ERROR, "Unsupported type stored for key '%s'", key.c_str());
        }
    }
#else
    void MantisRequest::registerDuktapeMethods() {
    }
#endif
} // namespace mb
