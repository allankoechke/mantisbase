#include "../../include/mantisbase/core/route_registry.h"
#include "../../include/mantisbase/core/logger/logger.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/http.h"

namespace mb
{
    size_t RouteKeyHash::operator()(const RouteKey& k) const
    {
        return std::hash<std::string>()(k.first + "#" + k.second);
    }

    void RouteRegistry::add(const std::string& method,
                            const std::string& path,
                            const HandlerFn handler,
                            const Middlewares& middlewares)
    {
        routes[{method, path}] = {middlewares, handler};
    }

    void RouteRegistry::add(const std::string& method,
                            const std::string& path,
                            const HandlerWithContentReaderFn handler,
                            const Middlewares& middlewares)
    {
        routes[{method, path}] = {middlewares, handler};
    }

    const RouteHandler* RouteRegistry::find(const std::string& method, const std::string& path) const
    {
        try {
            const auto it = routes.find({method, path});
            std::cout << "it donee ..." << std::endl;
            return it != routes.end() ? &it->second : nullptr;

        } catch (const std::exception& e) {
            std::cerr << "Finding error: " << e.what();
            return nullptr;
        }
    }

    json RouteRegistry::remove(const std::string& method, const std::string& path)
    {
        json res;
        res["error"] = "";

        const auto it = routes.find({method, path});
        if (it == routes.end())
        {
            const auto err = std::format("Route for {} {} not found!", method, path);
            // We didn't find that route, return error
            res["error"] = err;
            LogOrigin::warn("Route Not Found", fmt::format("{}", err));
            return res;
        }

        // Remove item found at the iterator
        routes.erase(it);
        LogOrigin::info("Route Removed", fmt::format("Route for {} {} erased!", method, path));
        return res;
    }
}
