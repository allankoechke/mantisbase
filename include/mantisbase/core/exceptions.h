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
        MantisException(int code, std::string message);

        const char* what() const noexcept override;

        int code() const noexcept;

    private:
        int m_code = -1;
        std::string m_msg;
    };
} // mantis

#endif //MANTISBASE_EXCEPTIONS_H