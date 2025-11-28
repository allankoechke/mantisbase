#include "../../include/mantisbase/utils/utils.h"

#include <algorithm>
#include <httplib.h>

namespace mantis
{
    std::optional<json> tryParseJsonStr(const std::string& json_str)
    {
        try
        {
            auto res = json::parse(json_str);
            return res;
        }
        catch (const std::exception& e)
        {
            logger::critical("JSON parse error: {}", e.what());
            return std::nullopt;
        }
    }

    bool strToBool(const std::string& value)
    {
        std::string s = value;
        std::ranges::transform(s, s.begin(), ::tolower);
        return (s == "1" || s == "true" || s == "yes" || s == "on");
    }

    void toLowerCase(std::string& str)
    {
        std::ranges::transform(str, str.begin(),
                               [](const unsigned char c) { return std::tolower(c); });
    }

    void toUpperCase(std::string& str)
    {
        std::ranges::transform(str, str.begin(),
                               [](const unsigned char c) { return std::toupper(c); });
    }

    std::string trim(const std::string& s)
    {
        auto start = std::ranges::find_if_not(s, ::isspace);
        auto end = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
        return (start < end) ? std::string(start, end) : "";
    }

    std::string generateTimeBasedId()
    {
        // Get current time since epoch in milliseconds
        using namespace std::chrono;
        auto now = system_clock::now();
        auto millis = duration_cast<milliseconds>(now.time_since_epoch()).count();

        // Generate 4-digit random suffix
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 9999);

        std::ostringstream oss;
        oss << millis << std::setw(4) << std::setfill('0') << dis(gen);
        return oss.str();
    }

    std::string generateReadableTimeId()
    {
        using namespace std::chrono;
        const auto now = system_clock::now();
        const auto tt = system_clock::to_time_t(now);
        const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        const std::tm tm = *std::localtime(&tt);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%dT%H%M%S");
        oss << std::setw(3) << std::setfill('0') << ms.count();

        // Append 3-character random suffix
        static constexpr char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<> dis(0, 35);
        for (int i = 0; i < 3; ++i)
            oss << charset[dis(gen)];

        return oss.str();
    }

    std::string generateShortId(const size_t length)
    {
        static const std::string chars =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        thread_local std::mt19937_64 rng{std::random_device{}()};
        std::uniform_int_distribution<std::size_t> dist(0, chars.size() - 1);

        std::string id;
        id.reserve(length);
        for (std::size_t i = 0; i < length; ++i)
        {
            id.push_back(chars[dist(rng)]);
        }
        return id;
    }

    std::vector<std::string> splitString(const std::string& input, const std::string& delimiter)
    {
        std::vector<std::string> tokens;
        size_t start = 0;
        size_t end = 0;

        while ((end = input.find(delimiter, start)) != std::string::npos)
        {
            tokens.push_back(input.substr(start, end - start));
            start = end + delimiter.length();
        }

        tokens.push_back(input.substr(start)); // last token
        return tokens;
    }

    std::string getEnvOrDefault(const std::string& key, const std::string& defaultValue)
    {
        const char* value = std::getenv(key.c_str());
        return value ? std::string(value) : defaultValue;
    }

    bool invalidChar(const unsigned char c)
    {
        return c < 0x20 || c == 0x7F ||
            c == '<' || c == '>' || c == ':' || c == '"' ||
            c == '/' || c == '\\' || c == '|' || c == '?' || c == '*' ||
            c == '+' || c == '\t' || c == ' ' || c == '\n' || c == '\r' ||
            c == '%' || c == '=';
    }

    void sanitizeInPlace(std::string& s)
    {
        for (auto& ch : s)
        {
            if (const auto c = static_cast<unsigned char>(ch); invalidChar(c)) ch = '_';
        }

        // collapse consecutive underscores
        s.erase(
            std::ranges::unique(s, [](const char a, const char b)
                                {
                                    return a == '_' && b == '_';
                                }
            ).begin(),
            s.end());

        s.erase(
            std::ranges::unique(s, [](const char a, const char b)
                                {
                                    return a == '_' && b == '-';
                                }
            ).begin(),
            s.end());

        s.erase(
            std::ranges::unique(s, [](const char a, const char b)
                                {
                                    return a == '-' && b == '_';
                                }
            ).begin(),
            s.end());

        // trim leading/trailing spaces and dots
        auto isTrim = [](const unsigned char c) { return c == ' ' || c == '.'; };
        while (!s.empty() && isTrim(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && isTrim(static_cast<unsigned char>(s.back()))) s.pop_back();
        if (s.empty()) s = "unnamed";
    }

    std::string sanitizeFilename(const std::string_view original,
                                 const std::size_t maxLen, const std::size_t idLen,
                                 const std::string_view idSep)
    {
        const fs::path p{std::string{original}};
        std::string stem = p.stem().string();
        std::string ext = p.has_extension() ? p.extension().string() : std::string{};

        sanitizeInPlace(stem);
        const auto max_size = stem.size() + ext.size() + idLen + idSep.size();

        const std::string id = generateShortId(idLen);

        if (max_size <= maxLen)
        {
            const auto& name = stem;
            return ext.empty()
                       ? std::format("{}{}{}", id, idSep, name)
                       : std::format("{}{}{}{}", id, idSep, name, ext);
        }

        auto shorten = [&](const std::string& s, const std::size_t n) -> std::string
        {
            if (n >= s.size()) return "";

            const std::size_t keep = (s.size() - n - 3) / 2;
            const std::size_t front = keep;
            const std::size_t back = s.size() - n - 3 - front;

            return std::format("{}...{}", s.substr(0, front), s.substr(s.size() - back));
        };

        const auto name = shorten(stem, max_size - maxLen);
        return ext.empty() ? std::format("{}{}{}", id, idSep, name) : std::format("{}{}{}{}", id, idSep, name, ext);
    }

    std::string sanitizeFilename_JSWrapper(const std::string& original)
    {
        return sanitizeFilename(original);
    }
}
