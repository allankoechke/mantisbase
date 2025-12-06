/**
 * @file files.cpp
 * @brief Implementation for @see files.h
 */

#include "../../include/mantisbase/core/files.h"
#include "../../include/mantisbase/core/logger.h"
#include "../../include/mantisbase/mantisbase.h"

#include <fstream>
#include <filesystem>

namespace mantis {
    namespace fs = std::filesystem;

    void Files::createDir(const std::string &entity_name) {
        logger::trace("Creating directory: {}", dirPath(entity_name));

        if (entity_name.empty()) return;

        if (const auto path = dirPath(entity_name); !fs::exists(path))
            fs::create_directories(path);
    }

    void Files::renameDir(const std::string &old_name, const std::string &new_name) {
        logger::trace("Renaming folder name from '{}' to '{}'", old_name, new_name);

        // Rename folder if it exists, else, create it
        if (const auto old_path = dirPath(old_name); fs::exists(old_path))
            fs::rename(old_path, dirPath(new_name));

        else
            createDir(new_name);
    }

    void Files::deleteDir(const std::string &entity_name) {
        logger::trace("Removing <data dir>/{}/*", entity_name);
        fs::remove_all(dirPath(entity_name));
    }

    std::optional<std::string> Files::getFilePath(const std::string &entity_name, const std::string &filename) {
        // Check if file exists, if so, return the path, else, return std::nullopt
        if (auto path = filePath(entity_name, filename); fs::exists(path)) {
            return path;
        }

        return std::nullopt;
    }

    std::string Files::dirPath(const std::string &entity_name, const bool create_if_missing) {
        auto path = (fs::path(MantisBase::instance().dataDir()) / "files" / entity_name).string();
        if (!fs::exists(path) && create_if_missing) {
            fs::create_directories(path);
        }
        return path;
    }

    std::string Files::filePath(const std::string &entity_name, const std::string &filename) {
        return (fs::path(MantisBase::instance().dataDir()) / "files" / entity_name / filename).string();
    }

    bool Files::removeFile(const std::string &entity_name, const std::string &filename) {
        try {
            if (entity_name.empty() || filename.empty()) {
                logger::warn("Entity name and filename are required!");
                return false;
            }

            const auto path = filePath(entity_name, filename);
            logger::trace("Removing file at `<data dir>/{}/{}`", entity_name, filename);

            // Remove the file, only if it exists
            if (fs::exists(path)) {
                fs::remove(path);
                return true;
            }

            logger::warn("File `<data dir>/{}/{}` seems to be missing!", entity_name, filename);
        } catch (const std::exception &e) {
            logger::critical("Error removing file\n\t{}", e.what());
        }

        return false;
    }

    void Files::removeFiles(const std::string &entity_name, const std::vector<std::string> &files) {
        for (const auto &file: files) {
            if (!removeFile(entity_name, file))
                logger::warn("Could not delete file `{}`, not found on disk.", file);
        }
    }

    bool Files::fileExists(const std::string &entity_name, const std::string &filename) {
        try {
            if (entity_name.empty() || filename.empty()) {
                logger::warn("Entity name and filename are required!");
                return false;
            }

            const auto path = filePath(entity_name, filename);
            return fs::exists(path);
        } catch (const std::exception &e) {
            logger::critical("Error removing file\n\t- {}", e.what());
            return false;
        }
    }
} // mantis
