//
// Created by codeart on 15/11/2025.
//

#ifndef MANTISBASE_EXCEPTIONS_H
#define MANTISBASE_EXCEPTIONS_H

#include <exception>
#include <string>

namespace mantis {
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
} // mantis

#endif //MANTISBASE_EXCEPTIONS_H