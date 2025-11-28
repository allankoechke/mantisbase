//
// Created by codeart on 15/11/2025.
//

#include "../../include/mantisbase/core/exceptions.h"

namespace mantis {
    MantisException::MantisException(const int code, std::string message): m_code(code), m_msg(std::move(message)) {}

    const char * MantisException::what() const noexcept {
        return m_msg.c_str();
    }

    int MantisException::code() const noexcept {
        if (m_code < 0) return 500;
        return m_code;
    }
} // mantis