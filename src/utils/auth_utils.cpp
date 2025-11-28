#include "../../include/mantisbase/utils/utils.h"
#include "../../include/mantisbase/mantisbase.h"

#include <bcrypt-cpp/bcrypt.h>

namespace mantis
{
    std::string hashPassword(const std::string& password)
    {
        if (password.empty()) throw std::invalid_argument("Password cannot be empty");
        return bcrypt::generateHash(password);
    }

    bool verifyPassword(const std::string& password, const std::string& stored_hash)
    {
        if (password.empty()) throw std::invalid_argument("Password cannot be empty");
        if (stored_hash.empty()) throw std::invalid_argument("Stored password hash cannot be empty");
        return bcrypt::validatePassword(password, stored_hash);
    }
}
