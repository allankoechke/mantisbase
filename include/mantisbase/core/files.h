/**
 * @file files.h files.cpp
 * @brief File management for record level file records.
 */

#ifndef MANTIS_FILES_H
#define MANTIS_FILES_H

#include <optional>
#include <string>
#include "../utils/utils.h"

namespace mb
{
    /**
     * @brief Handle file management for record type files.
     *
     * These files are the usual file assets stored in either `file` or `files` filed type. The idea here being,
     * the file is stored on disk and the name saved to the database.
     *
     * The class should handle:
     * - Creating & deleting folders
     * - Creating & deleting file resources as needed.
     * - Updating folder names when tables are renamed
     * - Deleting folder contents for dropped
     */
    class Files
    {
    public:
        /**
         * @brief Create a directory matching passed in `table` name.
         *
         * @param entity_name Table name to created directory for
         */
        static void createDir(const std::string& entity_name);

        /**
         * @brief Rename existing directory if found or create it if not found.
         *
         * @param old_name Old table name
         * @param new_name New table name
         */
        static void renameDir(const std::string& old_name, const std::string& new_name);

        /**
         * @brief Delete table directory and its contents
         *
         * @param entity_name Table name
         */
        static void deleteDir(const std::string& entity_name);

        /**
         * @brief Fetch directory path string for a given table
         *
         * @param entity_name Table name
         * @param create_if_missing Bool status to know whether to create directory if missing
         * @return Absolute path to a directory for the passed in table
         */
        static std::string dirPath(const std::string& entity_name, bool create_if_missing = false);

        /**
         * @brief Fetch absolute path to where file is or would be for a given table and filename.
         *
         * @param entity_name Table name
         * @param filename File name
         * @return Absolute path to the given filename and table name.
         */
        static std::string filePath(const std::string& entity_name, const std::string& filename);

        /**
         * @brief Fetch filename absolute path only if the file exists, else, return empty response.
         *
         * @param entity_name Table name
         * @param filename File name
         * @return Absolute filepath or none if filename does not exist in the table directory.
         *
         * @see filePath() above.
         */
        static std::optional<std::string> getFilePath(const std::string& entity_name, const std::string& filename);

        /**
         * @brief Remove existing file given the table and filename.
         *
         * @param entity_name Table Name (Entity Schema Name)
         * @param filename File name
         * @return Status whether file deletion succeeded.
         */
        static bool removeFile(const std::string& entity_name, const std::string& filename);

        /**
         * @brief Remove existing file given the table and filename.
         *
         * @param entity_name Table Name (Entity Schema Name)
         * @param files List of file names to delete.
         * @return Status whether file deletion succeeded.
         */
        static void removeFiles(const std::string& entity_name, const std::vector<std::string>& files);

        /**
         * @brief Remove existing file given the table and filename.
         *
         * @param entity_name Table Name (Entity Schema Name)
         * @param filename File name
         * @return Status whether file exists on disk or not
         */
        static bool fileExists(const std::string& entity_name, const std::string& filename);
    };
} // mantis

#endif // MANTIS_FILES_H
