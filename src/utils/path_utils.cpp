#include "../../include/mantisbase/utils/utils.h"

namespace mantis
{
    fs::path joinPaths(const std::string& path1, const std::string& path2)
    {
        fs::path result = fs::path(path1) / fs::path(path2);
        return result;
    }

    fs::path resolvePath(const std::string& input_path)
    {
        fs::path path(input_path);

        if (!path.is_absolute())
        {
            // Resolve relative to app binary
            path = fs::absolute(path);
        }

        return path;
    }

    bool createDirs(const fs::path& path)
    {
        try
        {
            if (!fs::exists(path))
            {
                fs::create_directories(path); // creates all missing parent directories too
            }

            return true;
        }
        catch (const fs::filesystem_error& e)
        {
            logger::critical("Filesystem error while creating directory '{}', reason: {}",
                          path.string(), e.what());
            return false;
        }
    }

    std::string dirFromPath(const std::string& path)
    {
        if (const auto dir = resolvePath(path); createDirs(dir))
            return dir.string();

        return "";
    }
}
