/**
 * @file files.cpp
 * @brief Implementation for @see files.h
 */

#include "../../include/mantisbase/core/files.h"
#include "../../include/mantisbase/core/logger/logger.h"
#include "../../include/mantisbase/mantisbase.h"

#include <fstream>
#include <filesystem>

namespace mb {
    namespace fs = std::filesystem;

    void Files::createDir(const std::string &entity_name) {
        if (!EntitySchema::isValidEntityName(entity_name)) {
            throw MantisException(500, "Invalid Entity Name",
                std::format("Provided Entity Name: `{}`", entity_name));
        }

        if (entity_name.empty()) {
            LogOrigin::warn("Empty Entity Name", "Attempting to create directory but entity name is empty!");
            return;
        }

        if (const auto dir_path = dirPath(entity_name); !fs::exists(dir_path)) {
            LogOrigin::trace("Directory Creation", fmt::format("Creating Dir: {}", dir_path));
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

        LogOrigin::trace("Directory Rename", fmt::format("Renaming folder name from `files/{}` to `files/{}`",
            old_entity_name, new_entity_name));

        // Rename folder if it exists, else, create it
        if (const auto old_path = dirPath(old_entity_name); fs::exists(old_path))
            fs::rename(old_path, dirPath(new_entity_name));

        else
            createDir(new_entity_name);
    }

    void Files::deleteDir(const std::string &entity_name) {
        LogOrigin::trace("Directory Deletion", fmt::format("Dropping dir files/{}/* started.", entity_name));
        fs::remove_all(dirPath(entity_name));
        LogOrigin::trace("Directory Deletion", fmt::format("Dropping dir files/{}/* completed.", entity_name));
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
        const auto path = filesBaseDir() / entity_name;

        if (!EntitySchema::isValidEntityName(entity_name)) {
            throw MantisException(400, "Invalid entity name `{}`.", entity_name);
        }

        // Ensure base directory exists before canonicalization
        const auto base_dir = filesBaseDir();
        if (!fs::exists(base_dir)) {
            fs::create_directories(base_dir);
        }
        
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

        const auto c_path = getCanonicalPath(filesBaseDir() / entity_name / filename);
        return c_path.string();
    }

    bool Files::removeFile(const std::string &entity_name, const std::string &filename) {
        try {
            // Remove the file, only if it exists
            if (const auto path = filePath(entity_name, filename); fs::exists(path)) {
                LogOrigin::trace("File Removal", fmt::format("Removing file at `<data dir>/{}/{}`", entity_name, filename));
                fs::remove(path);
                return true;
            }

            LogOrigin::warn("File Missing", fmt::format("Missing file: `files/{}/{}`.", entity_name, filename));
        } catch (const std::exception &e) {
            LogOrigin::critical("File Removal Error", fmt::format("Error removing file\n\t{}", e.what()));
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
            LogOrigin::critical("File Error", fmt::format("Error removing file\n\t- {}", e.what()));
            return false;
        }
    }

    fs::path Files::getCanonicalPath(const fs::path &path) {
        // Ensure base directory exists
        const auto base_dir = filesBaseDir();
        if (!fs::exists(base_dir)) {
            fs::create_directories(base_dir);
        }
        
        // Use weakly_canonical for paths that might not exist yet
        // It resolves symlinks and normalizes the path without requiring existence
        std::error_code ec;
        const auto canonical_path = fs::weakly_canonical(path, ec);
        if (ec) {
            throw MantisException(400, "Failed to resolve path: {}", ec.message());
        }
        
        const auto canonical_base = fs::canonical(base_dir); // Base should exist now

        // Get the relative path from base to target
        const auto relative = fs::relative(canonical_path, canonical_base, ec);
        
        // If relative path calculation failed or path is outside base, it's traversal
        if (ec || relative.empty() || relative.string().find("..") != std::string::npos) {
            throw MantisException(400, "Path traversal detected");
        }
        
        // Additional check: ensure the path string starts with base path
        const std::string path_str = canonical_path.string();
        const std::string base_str = canonical_base.string();
        
        if (path_str.length() < base_str.length() || 
            path_str.substr(0, base_str.length()) != base_str) {
            throw MantisException(400, "Path traversal detected");
        }
        
        // Ensure the next character (if any) is a path separator or path ends there
        if (path_str.length() > base_str.length()) {
            const char next_char = path_str[base_str.length()];
            if (next_char != fs::path::preferred_separator && next_char != '/') {
                throw MantisException(400, "Path traversal detected");
            }
        }

        return canonical_path;
    }

    bool Files::isCanonicalPath(const fs::path &path) {
        // Ensure base directory exists
        const auto base_dir = filesBaseDir();
        if (!fs::exists(base_dir)) {
            fs::create_directories(base_dir);
        }
        
        // Use weakly_canonical for paths that might not exist yet
        std::error_code ec;
        const auto canonical_path = fs::weakly_canonical(path, ec);
        if (ec) {
            return false; // Can't resolve path, consider it invalid
        }
        
        const auto canonical_base = fs::canonical(base_dir); // Base should exist now

        // Get the relative path from base to target
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
