/**
 * @file files.h
 * @brief File management for record-level file assets.
 *
 * Provides secure file operations for entity file storage with path traversal protection.
 * Files are stored in entity-specific directories under the application's data directory.
 */

#ifndef MANTIS_FILES_H
#define MANTIS_FILES_H

#include <optional>
#include <string>
#include <vector>
#include "../utils/utils.h"

namespace mb
{
    /**
     * @brief File management for entity file assets.
     *
     * Handles file storage and retrieval for entities with `file` or `files` field types.
     * Files are stored on disk in entity-specific directories, with filenames saved to the database.
     *
     * Security features:
     * - Entity name validation to prevent SQL injection
     * - Path canonicalization to prevent directory traversal attacks
     * - Automatic sanitization of filenames
     *
     * The class handles:
     * - Creating and deleting entity directories
     * - Creating and deleting file resources
     * - Updating folder names when entities are renamed
     * - Deleting folder contents when entities are dropped
     *
     * @code
     * // Create directory for an entity
     * Files::createDir("products");
     *
     * // Get file path (validates entity name and prevents path traversal)
     * std::string path = Files::filePath("products", "image.jpg");
     *
     * // Check if file exists
     * if (Files::fileExists("products", "image.jpg")) {
     *     // File exists
     * }
     * @endcode
     */
    class Files
    {
    public:
        /**
         * @brief Create a directory for the given entity.
         *
         * Validates the entity name and creates the directory if it doesn't exist.
         * Throws MantisException if entity name is invalid.
         *
         * @param entity_name Entity/table name (must be valid alphanumeric + underscore)
         * @throws MantisException if entity name is invalid
         */
        static void createDir(const std::string& entity_name);

        /**
         * @brief Rename an entity directory.
         *
         * Renames the directory if it exists, otherwise creates a new one.
         * Validates both old and new entity names.
         *
         * @param old_entity_name Current entity name
         * @param new_entity_name New entity name
         * @throws MantisException if either entity name is invalid
         */
        static void renameDir(const std::string& old_entity_name, const std::string& new_entity_name);

        /**
         * @brief Delete an entity directory and all its contents.
         *
         * @param entity_name Entity/table name
         */
        static void deleteDir(const std::string& entity_name);

        /**
         * @brief Get the directory path for an entity.
         *
         * Returns the absolute path to the entity's file directory. Validates entity name
         * and checks for path traversal attacks before returning the path.
         *
         * @param entity_name Entity/table name (must be valid)
         * @param create_if_missing If true, creates the directory if it doesn't exist
         * @return Absolute path string to the entity's file directory
         * @throws MantisException if entity name is invalid or path traversal is detected
         */
        static std::string dirPath(const std::string& entity_name, bool create_if_missing = false);

        /**
         * @brief Get the absolute file path for an entity and filename.
         *
         * Constructs and canonicalizes the file path, ensuring it's within the entity's
         * directory to prevent path traversal attacks. Validates entity name and filename.
         *
         * @param entity_name Entity/table name (must be valid)
         * @param filename File name (must not be empty)
         * @return Absolute canonical path string to the file
         * @throws MantisException if entity name is invalid, filename is empty, or path traversal is detected
         */
        static std::string filePath(const std::string& entity_name, const std::string& filename);

        /**
         * @brief Get file path only if the file exists.
         *
         * Returns the file path if the file exists on disk, otherwise returns std::nullopt.
         * Validates entity name before checking file existence.
         *
         * @param entity_name Entity/table name
         * @param filename File name
         * @return Absolute file path if file exists, std::nullopt otherwise
         * @throws MantisException if entity name is invalid
         */
        static std::optional<std::string> getFilePath(const std::string& entity_name, const std::string& filename);

        /**
         * @brief Remove a file from an entity's directory.
         *
         * @param entity_name Entity/table name
         * @param filename File name to remove
         * @return true if file was successfully removed, false otherwise
         */
        static bool removeFile(const std::string& entity_name, const std::string& filename);

        /**
         * @brief Remove multiple files from an entity's directory.
         *
         * @param entity_name Entity/table name
         * @param files Vector of file names to remove
         */
        static void removeFiles(const std::string& entity_name, const std::vector<std::string>& files);

        /**
         * @brief Check if a file exists in an entity's directory.
         *
         * @param entity_name Entity/table name
         * @param filename File name to check
         * @return true if file exists, false otherwise
         */
        static bool fileExists(const std::string& entity_name, const std::string& filename);

        /**
         * @brief Get canonical path and verify it's within the files base directory.
         *
         * Canonicalizes the given path (resolves symlinks, removes "..", etc.) and verifies
         * that it's within the files base directory to prevent path traversal attacks.
         *
         * @param path Path to canonicalize and validate
         * @return Canonical path if valid
         * @throws MantisException if path traversal is detected (400 status)
         */
        static fs::path getCanonicalPath(const fs::path& path);

        /**
         * @brief Check if a path is canonical and within the files base directory.
         *
         * Canonicalizes the path and checks if it's within the files base directory
         * without throwing an exception.
         *
         * @param path Path to check
         * @return true if path is canonical and within base directory, false otherwise
         */
        static bool isCanonicalPath(const fs::path& path);

        /**
         * @brief Get the base directory for all entity files.
         *
         * Returns the path to the files directory (typically `<dataDir>/files`).
         *
         * @return Filesystem path to the base files directory
         */
        static fs::path filesBaseDir();
    };
} // mb

#endif // MANTIS_FILES_H
