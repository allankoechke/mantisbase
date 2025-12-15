/**
 * @file files.cpp
 * @brief Implementation for @see files.h
 */

#include "../../include/mantisbase/core/files.h"
#include "../../include/mantisbase/core/logger.h"
#include "../../include/mantisbase/mantisbase.h"

#include <fstream>
#include <filesystem>

#include "../../tests/test_fixure.h"

namespace mb {
    namespace fs = std::filesystem;

    void Files::createDir(const std::string &entity_name) {
        if (!EntitySchema::isValidEntityName(entity_name)) {
            throw MantisException(500, "Invalid Entity Name",
                std::format("Provided Entity Name: `{}`", entity_name));
        }

        if (entity_name.empty()) {
            logger::warn("Attempting to create directory but entity name is empty!");
            return;
        }

        if (const auto dir_path = dirPath(entity_name); !fs::exists(dir_path)) {
            logger::trace("Creating Dir: {}", dir_path);
            fs::create_directories(dir_path);
        }
    }

    void Files::renameDir(const std::string &old_entity_name, const std::string &new_entity_name) {
        if (old_entity_name == new_entity_name) return;

        if (!EntitySchema::isValidEntityName(old_entity_name)) {
            throw MantisException(500, "Invalid Entity Name",
                std::format("Old entity name `{}` is not a valid SQL table name format!", old_entity_name));
        }

        if (!EntitySchema::isValidEntityName(new_entity_name)) {
            throw MantisException(500, "Invalid Entity Name",
                std::format("New entity name `{}` is not a valid SQL table name format!", new_entity_name));
        }

        logger::trace("Renaming folder name from `files/{}` to `files/{}`", old_entity_name, new_entity_name);

        // Rename folder if it exists, else, create it
        if (const auto old_path = dirPath(old_entity_name); fs::exists(old_path))
            fs::rename(old_path, dirPath(new_entity_name));

        else
            createDir(new_entity_name);
    }

    void Files::deleteDir(const std::string &entity_name) {
        logger::trace("Removing <data dir>/{}/*", entity_name);
        fs::remove_all(dirPath(entity_name));
    }

    std::optional<std::string> Files::getFilePath(const std::string &entity_name, const std::string &filename) {
        if (!EntitySchema::isValidEntityName(entity_name)) {
            throw MantisException(500, "Invalid Entity Name",
                std::format("Entity name `{}` is not a valid SQL table name format!", entity_name));
        }

        // Check if file exists, if so, return the path, else, return std::nullopt
        if (auto path = filePath(entity_name, filename); fs::exists(path)) {
            return path;
        }

        return std::nullopt;
    }

    std::string Files::dirPath(const std::string &entity_name, const bool create_if_missing) {
        // Create entity file directory
        const auto path = getBaseDir() / entity_name;

        if (!isCanonicalPath(path)) {
            throw MantisException(500, "Path transversal detected.",
                std::format("Entity name `{}` results in path transversal.", entity_name));
        }

        if (!fs::exists(path) && create_if_missing) {
            fs::create_directories(path);
        }

        return path.string();
    }

    std::string Files::filePath(const std::string &entity_name, const std::string &filename) {
        if (!EntitySchema::isValidEntityName(entity_name)) {
            throw MantisException(500, "Invalid Entity Name",
                std::format("Entity name `{}` is not a valid SQL table name format!", entity_name));
        }

        // TODO add proper filename check?
        if (filename.empty()) {
            throw MantisException(500, "Invalid File Name",
                std::format("File name `{}` is empty or invalid!", filename));
        }

        const auto c_path = getCanonicalPath(getBaseDir() / entity_name / filename);
        return c_path.string();
    }

    bool Files::removeFile(const std::string &entity_name, const std::string &filename) {
        try {
            // Remove the file, only if it exists
            if (const auto path = filePath(entity_name, filename); fs::exists(path)) {
                logger::trace("Removing file at `<data dir>/{}/{}`", entity_name, filename);
                fs::remove(path);
                return true;
            }

            logger::warn("Missing file: `files/{}/{}`.", entity_name, filename);
        } catch (const std::exception &e) {
            logger::critical("Error removing file\n\t{}", e.what());
        }

        return false;
    }

    void Files::removeFiles(const std::string &entity_name, const std::vector<std::string> &files) {
        for (const auto &file: files) {
            // Removing may throw an error, let the caller handle
            removeFile(entity_name, file);
        }
    }

    bool Files::fileExists(const std::string &entity_name, const std::string &filename) {
        try {
            const auto path = filePath(entity_name, filename);
            return fs::exists(path);
        } catch (const std::exception &e) {
            logger::critical("Error removing file\n\t- {}", e.what());
            return false;
        }
    }

    fs::path Files::getCanonicalPath(const fs::path &path) {
        // Canonicalize and verify it's within base_path
        const auto canonical_path = fs::canonical(path);
        const auto canonical_base = fs::canonical(filesBaseDir());

        // Check if canonical_path is within canonical_base
        // We need to ensure the base path is a complete prefix, not just a substring match
        // Using path comparison is more reliable than string comparison
        
        // Get the relative path from base to target
        std::error_code ec;
        const auto relative = fs::relative(canonical_path, canonical_base, ec);
        
        // If relative path calculation failed or path is outside base, it's traversal
        if (ec || relative.empty() || relative.string().find("..") != std::string::npos) {
            throw MantisException(400, "Path traversal detected");
        }
        
        // Additional check: ensure the path string starts with base path
        // This catches edge cases where fs::relative might not work as expected
        const std::string path_str = canonical_path.string();
        const std::string base_str = canonical_base.string();
        
        if (path_str.length() < base_str.length() || 
            path_str.substr(0, base_str.length()) != base_str) {
            throw MantisException(400, "Path traversal detected");
        }
        
        // Ensure the next character (if any) is a path separator or path ends there
        // This prevents matching "/data/files" with "/data/files_backup/..."
        if (path_str.length() > base_str.length()) {
            const char next_char = path_str[base_str.length()];
            if (next_char != fs::path::preferred_separator && next_char != '/') {
                throw MantisException(400, "Path traversal detected");
            }
        }

        return canonical_path;
    }

    bool Files::isCanonicalPath(const fs::path &path) {
        // Canonicalize and verify it's within base_path
        const auto canonical_path = fs::canonical(path);
        const auto canonical_base = fs::canonical(filesBaseDir());

        // Get the relative path from base to target
        std::error_code ec;
        const auto relative = fs::relative(canonical_path, canonical_base, ec);
        
        // If relative path calculation failed or contains "..", it's not within base
        if (ec || relative.empty() || relative.string().find("..") != std::string::npos) {
            return false;
        }
        
        // Additional string-based check for safety
        const std::string path_str = canonical_path.string();
        const std::string base_str = canonical_base.string();
        
        if (path_str.length() < base_str.length() || 
            path_str.substr(0, base_str.length()) != base_str) {
            return false;
        }
        
        // Ensure the next character (if any) is a path separator or path ends there
        if (path_str.length() > base_str.length()) {
            const char next_char = path_str[base_str.length()];
            return (next_char == fs::path::preferred_separator || next_char == '/');
        }
        
        // Paths are equal or path is exactly the base path
        return true;
    }

    fs::path Files::filesBaseDir() {
        return fs::path(MantisBase::instance().dataDir()) / "files";
    }
} // mantis
