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
            readJSON();
        }

        m_parsed = true;
    }

    const std::vector<httplib::FormData> &MantisContentReader::formData() const {
        return m_formData;
    }

    const json &MantisContentReader::filesMetadata() const {
        return m_filesMetadata;
    }

    const json &MantisContentReader::jsonBody() const {
        return m_json;
    }

    void MantisContentReader::parseFormDataToEntity(const Entity &entity) {
        if (!isMultipartFormData())
            throw MantisException(400, "Expected form data request, but it seems null.");

        json json_body{}, json_files{};

        // Process uploaded files and form fields
        for (const auto &form_data: m_formData) {
            //
            if (!form_data.filename.empty()) {
                if (!entity.hasField(form_data.name)) {
                    throw MantisException(
                        400,
                        std::format("Unknown field `{}` for file type upload!", form_data.name)
                    );
                }

                const auto e_field = EntitySchemaField(entity.field(form_data.name).value());

                // Ensure field is of `file|files` type.
                if (!(e_field.type() == "file" || e_field.type() == "files")) {
                    throw MantisException(
                        400,
                        std::format("Field `{}` is not of type `file` or `files`!", form_data.name)
                    );
                }

                // Handle file upload
                const auto dir = Files::dirPath(entity.name(), true);
                const auto new_filename = sanitizeFilename(form_data.filename);

                // Create filepath for writing file contents
                std::string filepath = (fs::path(dir) / new_filename).string();

                // File record to be saved
                json file_record;
                file_record["filename"] = new_filename;
                file_record["path"] = filepath; // Path on disk to write the file
                file_record["name"] = form_data.name; // Original file name as passed by the user
                file_record["hash"] = MantisContentReader::hashMultipartMetadata(form_data);

                if (e_field.type() == "file") {
                    // For `file` type
                    json_files[form_data.name] = file_record;

                    // Add to the req body
                    json_body[form_data.name] = new_filename;
                } else {
                    try {
                        // Should be a JSON array, lets construct that if necessary
                        if (!json_body.contains(form_data.name))
                            json_body[form_data.name] = nullptr;
                        if (!json_files.contains(form_data.name))
                            json_files[form_data.name] = nullptr;

                        json_body[form_data.name].push_back(new_filename);
                        json_files[form_data.name].push_back(file_record);
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
                    auto e_field_opt = entity.field(form_data.name);
                    if (e_field_opt.has_value()) {
                        auto schema_field = EntitySchemaField(e_field_opt.value());

                        try {
                            const auto &type = schema_field.type();

                            // For file types, append the file list to any existing array if any or
                            // parse the array correctly to an array of data
                            if (type == "files") {
                                auto data = trim(form_data.content).empty()
                                ? nullptr
                                : json::parse(form_data.content);
                                if (!data.is_array() && !data.is_null()) {
                                    throw MantisException(400, std::format(
                                                              "Error parsing field `{}`, expected an array!",
                                                              form_data.name));
                                }

                                // Create empty field if it does not exist yet
                                if (!json_body.contains(form_data.name))
                                    json_body[form_data.name] = nullptr;

                                // For empty/null values, just continue
                                if (data == nullptr) continue;

                                // Append data content to the body field
                                for (const auto &d: data)
                                    json_body[form_data.name].push_back(d);
                            } else {
                                // For all other input types, simply add the data to the respective field.
                                // Overwrites any existing data
                                auto v = getValueFromType(type, form_data.content)["value"];
                                json_body[form_data.name] = v;
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

        m_json = json_body;
        m_filesMetadata = json_files;
    }

    void MantisContentReader::writeFiles(const std::string &entity_name) {
        for (const auto &formData: m_formData) {
            // For non-file types, continue
            if (formData.filename.empty()) continue;

            const auto file_list = m_filesMetadata[formData.name].is_array()
                                       ? m_filesMetadata[formData.name]
                                       : json::array({m_filesMetadata[formData.name]});

            auto it = std::ranges::find_if(file_list, [&](const json &f) {
                return f["hash"].get<std::string>() == MantisContentReader::hashMultipartMetadata(formData);
            });

            if (it == file_list.end()) {
                // Should not happen, but if it does, throw 500 error
                throw MantisException(500, "Error writing files, hash mismatch!");
            }

            auto &file_record = *it;
            const auto filepath = file_record["path"].get<std::string>();
            if (std::ofstream ofs(filepath, std::ios::binary); ofs.is_open()) {
                ofs.write(formData.content.data(), formData.content.size());
                ofs.close();
            } else {
                undoWrittenFiles(entity_name);
                throw MantisException(500, "Failed to open `" + formData.filename + "` file for writing.");
            }
        }
    }

    void MantisContentReader::undoWrittenFiles(const std::string &entity_name) {
        // Remove any written files
        for (const auto &file: m_filesMetadata) {
            if (file.contains("filename")) {
                [[maybe_unused]] auto _ = Files::removeFile(entity_name, file["filename"].get<std::string>());
            }
        }
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
        if (content.empty()) {
            obj["value"] = nullptr;
        } else if (type == "xml" || type == "string" || type == "date" || type == "file") {
            obj["value"] = content;
        } else if (type == "double" || type == "int8" || type == "uint8" || type == "int16" ||
                   type == "uint16" || type == "int32" ||
                   type == "uint32" || type == "int64" || type == "uint64") {
            obj["value"] = json::parse(content);
        } else if (type == "json" || type == "bool") {
            obj["value"] = json::parse(content);
        } else {
            obj["value"] = content;
        }

        return obj;
    }

    void MantisContentReader::readMultipart() {
        m_reader(
            [&](const httplib::FormData &form_data) {
                m_formData.push_back(form_data);
                return true;
            },
            [&](const char *data, const size_t len) {
                m_formData.back().content.append(data, len);
                return true;
            }
        );
    }

    void MantisContentReader::readJSON() {
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
