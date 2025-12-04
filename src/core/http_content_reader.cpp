//
// Created by codeart on 03/12/2025.
//
#include "../include/mantisbase/core/http.h"

namespace mantis {
    MantisContentReader::MantisContentReader(
        const httplib::ContentReader &reader,
        const MantisRequest &req)
        : m_reader(reader),
          m_req(req) { read(); }

    const httplib::ContentReader &MantisContentReader::reader() const { return m_reader; }

    bool MantisContentReader::isMultipartFormData() const { return m_req.isMultipartFormData(); }

    void MantisContentReader::read() {
        if (m_parsed) return;

        if (isMultipartFormData()) {
            readMultipart();
        } else {
            readJson();
        }

        m_parsed = true;
    }

    const std::vector<httplib::FormData> &MantisContentReader::files() const {
        return m_files;
    }

    const json &MantisContentReader::jsonBody() const {
        return m_json;
    }

    json MantisContentReader::formDataToJSON(const Entity &entity) const {
        if (!isMultipartFormData())
            throw MantisException(500, "Expected Form Data request type");

        json json_body{}, json_files{};

        // Process uploaded files and form fields
        for (const auto &file: m_files) {
            //
            if (!file.filename.empty()) {
                if (!entity.hasField(file.name)) {
                    throw MantisException(
                        400,
                        std::format("Unknown field `{}` for file type upload!", file.name)
                    );
                }

                const auto e_field = EntitySchemaField(entity.field(file.name).value());

                // Ensure field is of `file|files` type.
                if (!(e_field.type() == "file" || e_field.type() == "files")) {
                    throw MantisException(
                        400,
                        std::format("Field `{}` is not of type `file` or `files`!", file.name)
                    );
                }

                // Handle file upload
                const auto dir = Files::dirPath(entity.name(), true);
                const auto new_filename = sanitizeFilename(file.filename);

                // Create filepath for writing file contents
                std::string filepath = (fs::path(dir) / new_filename).string();

                // File record to be saved
                json file_record;
                file_record["filename"] = new_filename;
                file_record["path"] = filepath; // Path on disk to write the file
                file_record["name"] = file.name; // Original file name as passed by the user
                file_record["hash"] = MantisContentReader::hashMultipartMetadata(file);

                if (e_field.type() == "file") {
                    // For `file` type
                    json_files[file.name] = file_record;

                    // Add to the req body
                    json_body[file.name] = new_filename;
                } else {
                    try {
                        // Should be a JSON array, lets construct that if necessary
                        if (!json_body.contains(file.name))
                            json_body[file.name] = nullptr;
                        if (!json_files.contains(file.name))
                            json_files[file.name] = nullptr;

                        json_body[file.name].push_back(new_filename);
                        json_files[file.name].push_back(file_record);
                    } catch (std::exception &e) {
                        throw MantisException(
                            500,
                            std::format("Error Parsing Files: {}", e.what())
                        );
                    }
                }
            }

            // This is a regular form field, treat as JSON data
            else {
                try {
                    auto e_field_opt = entity.field(file.name);
                    if (e_field_opt.has_value()) {
                        auto schema_field = EntitySchemaField(e_field_opt.value());

                        try {
                            const auto &type = schema_field.type();

                            // For file types, append the file list to any existing array if any or
                            // parse the array correctly to an array of data
                            if (type == "files") {
                                auto data = trim(file.content).empty() ? nullptr : json::parse(file.content);
                                if (!data.is_array() && !data.is_null()) {
                                    throw MantisException(400, std::format(
                                                              "Error parsing field `{}`, expected an array!",
                                                              file.name));
                                }

                                // Create empty field if it does not exist yet
                                if (!json_body.contains(file.name))
                                    json_body[file.name] = nullptr;

                                // For empty/null values, just continue
                                if (data == nullptr) continue;

                                // Append data content to the body field
                                for (const auto &d: data)
                                    json_body[file.name].push_back(d);
                            } else {
                                // For all other input types, simply add the data to the respective field.
                                // Overwrites any existing data
                                auto v = getValueFromType(type, file.content)["value"];
                                json_body[file.name] = v;
                            }
                        } catch (std::exception &e) {
                            throw MantisException(500, e.what());
                        }
                    }
                } catch (const MantisException &e) {
                    throw;
                } catch (const std::exception &e) {
                    throw MantisException(500, e.what());
                }
            }
        }

        return {{"data", json_body}, {"files", json_files }};
    }

    std::string MantisContentReader::hashMultipartMetadata(const httplib::FormData &data) {
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

    json MantisContentReader::getValueFromType(const std::string &type, const std::string &value) {
        json obj;
        const auto content = trim(value);
        if (content.empty())
        {
            obj["value"] = nullptr;
        }
        else if (type == "xml" || type == "string" || type == "date" || type == "file")
        {
            obj["value"] = content;
        }
        else if (type == "double" || type == "int8" || type == "uint8" || type == "int16" ||
                 type == "uint16" || type == "int32" ||
                 type == "uint32" || type == "int64" || type == "uint64")
        {
            obj["value"] = json::parse(content);
        }
        else if (type == "json" || type == "bool")
        {
            obj["value"] = json::parse(content);
        }
        else
        {
            obj["value"] = content;
        }

        return obj;
    }

    void MantisContentReader::readMultipart() {
        m_reader(
            [&](const httplib::FormData &file) {
                m_files.push_back(file);
                return true;
            },
            [&](const char *data, const size_t len) {
                m_files.back().content.append(data, len);
                return true;
            }
        );
    }

    void MantisContentReader::readJson() {
        std::string body;
        m_reader([&](const char *data, const size_t data_length) -> bool {
            body.append(data, data_length);
            return true;
        });

        // Parse request body to JSON Object, return an error if it fails
        try {
            if (trim(body).empty()) m_json = json::object();
            else m_json = json::parse(body);
        } catch (const std::exception &e) {
            throw MantisException(400, e.what());
        }
    }
}
