/**
 * @file files.h files.cpp
 * @brief File management for record level file records.
 */

#ifndef MANTIS_FILES_H
#define MANTIS_FILES_H

#include <optional>
#include <string>
#include "../utils/utils.h"

namespace mantis
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
         * @param table Table name to created directory for
         */
        static void createDir(const std::string& table);
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
         * @param table Table name
         */
        static void deleteDir(const std::string& table);

        /**
         * @brief Fetch directory path string for a given table
         *
         * @param table Table name
         * @param create_if_missing Bool status to know whether to create directory if missing
         * @return Absolute path to a directory for the passed in table
         */
        static std::string dirPath(const std::string& table, bool create_if_missing = false);
        /**
         * @brief Fetch absolute path to where file is or would be for a given table and filename.
         *
         * @param table Table name
         * @param filename File name
         * @return Absolute path to the given filename and table name.
         */
        static std::string filePath(const std::string& table, const std::string& filename);
        /**
         * @brief Fetch filename absolute path only if the file exists, else, return empty response.
         *
         * @param table Table name
         * @param filename FIle name
         * @return Absolute filepath or none if filename does not exist in the table directory.
         *
         * @see filePath() above.
         */
        static std::optional<std::string> getFilePath(const std::string& table, const std::string& filename);
        /**
         * @brief Remove existing file given the table and filename.
         *
         * @param table Table name
         * @param filename File name
         * @return Status whether file deletion succeeded.
         */
        static bool removeFile(const std::string& table, const std::string& filename);
    };
} // mantis

#endif // MANTIS_FILES_H
