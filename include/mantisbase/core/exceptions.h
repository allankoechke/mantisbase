/**
 * @file exceptions.h
 * @brief Custom exception class for MantisBase errors.
 *
 * Provides a structured exception type with HTTP-style error code, message,
 * and optional extended description for error handling.
 */

#ifndef MANTISBASE_EXCEPTIONS_H
#define MANTISBASE_EXCEPTIONS_H

#include <exception>
#include <string>

namespace mb {
    /**
     * @brief Application exception carrying an HTTP status code and messages.
     *
     * Thrown by entity, file, and validation layers; caught by route handlers
     * and converted into JSON error responses.
     */
    class MantisException final : public std::exception {
    public:
        /** @param _code HTTP-style status code (e.g. 400, 404). */
        MantisException(int _code, std::string _msg);

        /** @param _desc Optional longer description for logs or API `error` detail. */
        MantisException(int _code, std::string _msg, std::string _desc);

        [[nodiscard]] const char* what() const noexcept override;

        /** Extended description; empty string when not provided. */
        [[nodiscard]] const char* desc() const noexcept;

        /** HTTP-style status code associated with this error. */
        [[nodiscard]] int code() const noexcept;

    private:
        int m_code = -1;
        std::string m_msg, m_desc;
    };
} // mb

#endif //MANTISBASE_EXCEPTIONS_H
