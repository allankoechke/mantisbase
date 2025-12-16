/**
 * @file exceptions.h
 * @brief Custom exception class for MantisBase errors.
 *
 * Provides a structured exception type with error code, message,
 * and optional description for error handling.
 */

#ifndef MANTISBASE_EXCEPTIONS_H
#define MANTISBASE_EXCEPTIONS_H

#include <exception>
#include <string>

namespace mb {
    class MantisException final : public std::exception {
    public:
        MantisException(int _code, std::string _msg);

        MantisException(int _code, std::string _msg, std::string _desc);

        [[nodiscard]] const char* what() const noexcept override;

        [[nodiscard]] const char* desc() const noexcept;

        [[nodiscard]] int code() const noexcept;

    private:
        int m_code = -1;
        std::string m_msg, m_desc;
    };
} // mb

#endif //MANTISBASE_EXCEPTIONS_H