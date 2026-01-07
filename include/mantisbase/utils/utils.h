/**
 * @file utils.h
 *
 * @brief Collection of utility functions that are re-used across different files.
 *
 * Created by allan on 11/05/2025.
 */

#ifndef MANTIS_UTILS_H
#define MANTIS_UTILS_H

#include <string>
#include <filesystem>
#include <random>
#include <chrono>
#include <string_view>
#include <algorithm>
#include <soci/row.h>
#include <nlohmann/json.hpp>

#include "../core/logger/logger.h"

#ifdef MANTIS_ENABLE_SCRIPTING
#include "dukglue/dukvalue.h"
#endif


// Wrap around std::format for compilers that do no support
// std::format yet, especially on Windows
#if __has_include(<format>)
#include <format>
#else
#include <spdlog/fmt/bundled/format.h>
namespace std {
    using fmt::format;
}
#endif

namespace mb {
    namespace fs = std::filesystem; ///< Use shorthand `fs` to refer to the `std::filesystem`
    using json = nlohmann::json; ///> JSON convenience for the nlomann::json namespace

    // ----------------------------------------------------------------- //
    // PATH UTILS
    // ----------------------------------------------------------------- //

    /**
     * @brief Joins absolute path and a relative path, relative to the first path.
     * @param path1 The first absolute path or relative path
     * @param path2 The relative path, subject to the first
     * @return An absolute path if successfully joined, else an empty path.
     */
    fs::path joinPaths(const std::string &path1, const std::string &path2);

    /**
     * Resolves given path as a string to an absolute path.
     *
     * Given a relative path, relative to the `cwd`, we can resolve that path to the actual
     * absolute path in our filesystem. This is needed for creating directories and files, especially
     * for database and file-serving.
     *
     * @param input_path The path to resolve
     * @return Returns an absolute filesystem path.
     */
    fs::path resolvePath(const std::string &input_path);

    /**
     * @brief Create directory, given a path
     *
     * Creates the target directory iteratively, including any missing parent directories.
     * his ensures, any parent directory is set up before attempting to create a child directory.
     *
     * @param path The directory path to create like `/foo/bar`.
     * @return True if creation was successful. If the directory exists, it returns false.
     */
    bool createDirs(const fs::path &path);

    /**
     * @brief Returns a created/existing directory from a path.
     *
     * Given a file path, it first gets the file parent directory and ensures the directory
     * together with any missing parent directories are created first using the `createDirs(...)`.
     *
     * @see createDirs() for creating directories.
     *
     * @param path The file path
     * @return Returns the directory path if successful, else an empty string.
     */
    std::string dirFromPath(const std::string &path);

    // ----------------------------------------------------------------- //
    // STRING UTILS
    // ----------------------------------------------------------------- //
    /**
     * @brief Converts a string to its lowercase variant.
     *
     * It converts the string in place.
     *
     * @param str The string to convert.
     * @see toUpperCase() To convert string to uppercase.
     */
    void toLowerCase(std::string &str);

    /**
     * @brief Converts a string to its uppercase variant.
     *
     * It converts the string in place.
     *
     * @param str The string to convert.
     * @see toLowerCase() To convert string to lowercase.
     */
    void toUpperCase(std::string &str);

    /**
     * @brief Trims leading and trailing whitespaces from a string.
     *
     * @param s The string to trim.
     * @return String with all leading and trailing whitespaces removed.
     */
    std::string trim(const std::string &s);

    /**
     * @brief Attempt to parse a JSON string.
     * @param json_str JSON string to parse
     * @return A JSON Object if successful, else a `std::nullopt`
     *
     * @code
     * auto user = tryParseJsonStr("{\"name\": \"John Doe\"}");
     * if(user.has_value()) {
     *      // Do something ...
     * }
     * @endcode
     */
    std::optional<json> tryParseJsonStr(const std::string &json_str);

    /**
     * @brief Convert given string value to boolean type.
     *
     * By default, we check for true equivalents, anything else will be
     * considered as a false value.
     *
     * @param value String value to convert to bool
     * @return true or false value
     */
    bool strToBool(const std::string &value);

    /**
     * @brief Generate a time base UUID
     *
     * The first part is made up of milliseconds since epoch while the last 4 digits a random component.
     * This makes it lexicographically sortable by time
     *
     * Sample Output: `17171692041233276`
     *
     * @return A time based UUID
     *
     * @see generateReadableTimeId() For a readable time-based UUID.
     * @see generateShortId() For a short UUID.
     */
    std::string generateTimeBasedId();

    /**
     * @brief Generates a readable time-based UUID.
     *
     * The first segment is the current time in ISO-formatted time + milliseconds + short random suffix.
     * It is human-readable and sortable, just like
     *
     * Sample Output: `20250531T221944517N3J`
     *
     * @return A readable-time based UUID
     *
     * @see generateTimeBasedId() For a time-based UUID.
     * @see generateShortId() For a short UUID.
    */
    std::string generateReadableTimeId();

    /**
     * @brief Generates a short UUID
     *
     * Similar to what platforms like YouTube use for videos, but in our case, making use of only
     * alphanumeric characters.
     *
     * Sample Output: `Fz8xYc6a7LQw`
     *
     * @return A short alphanumeric UUID
     *
     * @see generateTimeBasedId() For a time-based UUID.
     * @see generateReadableTimeId() For a readable time-based UUID.
     */
    std::string generateShortId(size_t length = 16);

    /**
     * @brief Split given string based on given delimiter
     *
     * @param input Input string to split.
     * @param delimiter The string delimiter to use to split the `input` string.
     * @return A vector of strings.
     *
     * @code
     * auto parts = splitString("Hello, John!", ",");
     * std::cout << parts.size() << std::endl;
     * // > Should be a vector of two strings `Hello` and ` World`
     * @endcode
     */
    std::vector<std::string> splitString(const std::string &input, const std::string &delimiter);

    /**
     * @brief Retrieves a value from an environment variable or a default value if the env variable was not set.
     * @param key Environment variable key.
     * @param defaultValue A default value if the key is not set.
     * @return The env value if found, else the default value passed in.
     */
    std::string getEnvOrDefault(const std::string &key, const std::string &defaultValue);

    /**
     *
     * @brief Check if a character is invalid in a filename.
     *
     * Invalid characters typically include reserved symbols such as
     * slashes, colons, question marks, etc. This function allows
     * filtering and sanitization of filenames.
     *
     * @param c Character to check.
     * @return true if the character is invalid in a filename, false otherwise.
     */
    bool invalidChar(unsigned char c);

    /**
     * @brief Sanitize a string in-place by removing or replacing invalid characters.
     *
     * This modifies the provided string directly. Characters deemed invalid
     * by `invalidChar()` are replaced with an underscore (`_`).
     *
     * @param s Reference to the string to sanitize.
     */
    void sanitizeInPlace(std::string &s);

    /**
     * @brief Sanitize a filename and ensure uniqueness.
     *
     * Creates a safe filename from the given input. Invalid characters are
     * replaced with underscores. The resulting filename is truncated to
     * `maxLen` characters, and a short unique ID is appended to avoid collisions.
     *
     * Example:
     *   Input: "my*illegal:file?.txt"
     *   Output: "my_illegal_file_txt_abC82xM01qP"
     *
     * @param original Original filename (input).
     * @param maxLen Maximum length of the sanitized name before adding ID. Default = 50.
     * @param idLen Length of the unique ID suffix. Default = 12.
     * @param idSep Separator inserted between the name and the ID. Default = "_".
     * @return Sanitized filename with appended unique ID.
     */
    std::string sanitizeFilename(std::string_view original,
                                 std::size_t maxLen = 50,
                                 std::size_t idLen = 12,
                                 std::string_view idSep = "_");

    std::string sanitizeFilename_JSWrapper(const std::string &original);

    // ----------------------------------------------------------------- //
    // AUTH UTILS
    // ----------------------------------------------------------------- //
    /**
     * @brief Digests user password + a generated salt to yield a hashed password.
     * @param password Password input to hash.
     * @return A hash string representation of the password + salt.
     */
    std::string hashPassword(const std::string &password);

    /**
     * @brief Verifies user password if it matches the given hashed password.
     *
     * Given a hashed password from the database (`stored_hash`), the method extracts the salt value,
     * hashes the `password` value with the salt then compares the two hashes if they match.
     *
     * @param password User password input.
     * @param stored_hash Database stored hashed user password.
     * @return boolean indicating whether the verification was successful or not.
     */
    bool verifyPassword(const std::string &password, const std::string &stored_hash);

    // ----------------------------------------------------------------- //
    // AUTH UTILS
    // ----------------------------------------------------------------- //

    /**
     * @brief Convert c++ std::tm date/time value to ISO formatted string.
     * @param t std::tm value
     * @return ISO formatted datetime value
     */
    std::string tmToStr(const std::tm &t);

    /**
     * @brief Convert ISO formatted datetime string to std::tm structure.
     * @param value ISO formatted datetime string
     * @return std::tm structure representing the datetime
     */
    std::tm strToTM(const std::string &value);

    /**
     * @brief Convert database date value from SOCI row to string.
     * @param row SOCI row containing the date value
     * @param index Column index in the row
     * @return String representation of the date
     */
    std::string dbDateToString(const soci::row &row, int index);

    /**
     * @brief Safely convert string to integer with default fallback.
     *
     * Attempts to convert a string to an integer. If conversion fails
     * (invalid format, out of range, etc.), returns the default value
     * instead of throwing an exception.
     *
     * @param s String to convert
     * @param default_val Default value to return if conversion fails
     * @return Converted integer or default value
     *
     * @code
     * int page = safe_stoi(req.getQueryParam("page"), 1);  // Defaults to 1 if invalid
     * int limit = safe_stoi(req.getQueryParam("limit"), 100);  // Defaults to 100 if invalid
     * @endcode
     */
    int safe_stoi(const std::string &s, const int default_val);

    // ----------------------------------------------------------------- //
    // NETWORK UTILS
    // ----------------------------------------------------------------- //

    /**
     * @brief Validates if a string is a valid IPv4 address.
     *
     * Checks if the input string matches the IPv4 format (e.g., "192.168.1.1").
     * Does not validate the actual IP range, only the format.
     *
     * @param ip String to validate as IPv4 address
     * @return true if the string is a valid IPv4 format, false otherwise
     *
     * @code
     * if (isValidIPv4("192.168.1.1")) {
     *     // Valid IPv4 format
     * }
     * @endcode
     */
    bool isValidIPv4(const std::string &ip);

    /**
     * @brief Validates if a string is a valid IPv6 address.
     *
     * Checks if the input string matches the IPv6 format (e.g., "2001:0db8::1").
     * Supports compressed notation (::).
     *
     * @param ip String to validate as IPv6 address
     * @return true if the string is a valid IPv6 format, false otherwise
     */
    bool isValidIPv6(const std::string &ip);

    /**
     * @brief Validates if a string is a valid IP address (IPv4 or IPv6).
     *
     * @param ip String to validate
     * @return true if valid IPv4 or IPv6, false otherwise
     */
    bool isValidIP(const std::string &ip);

#ifdef MANTIS_ENABLE_SCRIPTING
    void registerUtilsToDuktapeEngine();
#endif
}

#endif // MANTIS_UTILS_H
