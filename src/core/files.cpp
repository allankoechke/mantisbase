/**
 * @file files.cpp
 * @brief Implementation for @see files.h
 */

#include "../../include/mantisbase/core/files.h"
#include "../../include/mantisbase/core/logger.h"
#include "../../include/mantisbase/mantisbase.h"

#include <fstream>
#include <filesystem>

namespace mantis
{
    namespace fs = std::filesystem;

    void Files::createDir(const std::string& table)
    {
        logger::trace("Creating directory: {}", dirPath(table));

        if (table.empty()) return;

        if (const auto path = dirPath(table); !fs::exists(path))
            fs::create_directories(path);
    }

    void Files::renameDir(const std::string& old_name, const std::string& new_name)
    {
        logger::trace("Renaming folder name from '{}' to '{}'", old_name, new_name);

        // Rename folder if it exists, else, create it
        if (const auto old_path = dirPath(old_name); fs::exists(old_path))
            fs::rename(old_path, dirPath(new_name));

        else
            createDir(new_name);
    }

    void Files::deleteDir(const std::string& table)
    {
        logger::trace("Removing {}", dirPath(table));
        fs::remove_all(dirPath(table));
    }

    std::optional<std::string> Files::getFilePath(const std::string& table, const std::string& filename)
    {
        // Check if file exists, if so, return the path, else, return std::nullopt
        if (auto path = filePath(table, filename); fs::exists(path))
        {
            return path;
        }

        return std::nullopt;
    }

    std::string Files::dirPath(const std::string& table, const bool create_if_missing)
    {
        auto path = (fs::path(MantisBase::instance().dataDir()) / "files" / table).string();
        if (!fs::exists(path) && create_if_missing)
        {
            fs::create_directories(path);
        }
        return path;
    }

    std::string Files::filePath(const std::string& table, const std::string& filename)
    {
        return (fs::path(MantisBase::instance().dataDir()) / "files" / table / filename).string();
    }

    bool Files::removeFile(const std::string& table, const std::string& filename)
    {
        try
        {
            if (table.empty() || filename.empty())
            {
                logger::warn("Table name and filename are required!");
                return false;
            }

            const auto path = filePath(table, filename);
            logger::trace("Removing file at `{}`", path);

            // Remove the file, only if it exists
            if (fs::exists(path))
            {
                fs::remove(path);
                return true;
            }

            logger::warn("Could not remove file at `{}`, seems to be missing!", path);
        }
        catch (const std::exception& e)
        {
            logger::critical("Error removing file: {}", e.what());
        }

        return false;
    }
} // mantis
