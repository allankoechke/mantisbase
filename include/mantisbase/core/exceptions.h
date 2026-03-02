/**
 * @file exceptions.h
 * Custom exception class for MantisBase errors.
 *
 * Provides a structured exception type with error code, message,
 * and optional description for error handling.
 */

#ifndef MANTISBASE_EXCEPTIONS_H
#define MANTISBASE_EXCEPTIONS_H

#include <exception>
#include <string>

namespace mb {
    /**
     * Custom exception class for MantisBase errors.
     *
     * Provides a structured exception type with error code, message,
     * and optional description for error handling.
     * @ingroup cpp_core
     */
    class MantisException final : public std::exception {
    public:
        /**
         * Construct a MantisException with an error code and message.
         * @param _code Error code
         * @param _msg Error message
         */
        MantisException(int _code, std::string _msg);

        /**
         * Construct a MantisException with an error code, message, and description.
         * @param _code Error code
         * @param _msg Error message
         * @param _desc Error description
         */
        MantisException(int _code, std::string _msg, std::string _desc);

        /**
         * Get the error message.
         * @return Error message
         */
        [[nodiscard]] const char* what() const noexcept override;

        /**
         * Get the error description.
         * @return Error description
         */
        [[nodiscard]] const char* desc() const noexcept;

        /**
         * Get the error code.
         * @return Error code
         */
        [[nodiscard]] int code() const noexcept;

    private:
        int m_code = -1;
        std::string m_msg, m_desc;
    };
} // mb

#endif //MANTISBASE_EXCEPTIONS_H