//
// Created by codeart on 15/11/2025.
//

#include "../../include/mantisbase/core/exceptions.h"

namespace mb {
    MbException::MbException(const int _code, std::string _msg)
        : m_code(_code),
          m_msg(std::move(_msg)) {
    }

    MbException::MbException(const int _code, std::string _msg, std::string _desc)
        : m_code(_code),
          m_msg(std::move(_msg)),
          m_desc(std::move(_desc)) {
    }

    const char *MbException::what() const noexcept {
        return m_msg.c_str();
    }

    const char *MbException::desc() const noexcept {
        return m_desc.c_str();
    }

    int MbException::code() const noexcept {
        if (m_code < 0) return 500;
        return m_code;
    }
} // mantis
