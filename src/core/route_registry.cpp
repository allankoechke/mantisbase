#include "../../include/mantisbase/core/route_registry.h"
#include "../../include/mantisbase/core/logger/logger.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/http.h"

namespace mb {
    size_t MbRouteKeyHash::operator()(const MbRouteKey &k) const {
        return std::hash<std::string>()(k.first + "#" + k.second);
    }

    void MbRouteRegistry::add(const std::string &method,
                            const std::string &path,
                            const MbHandlerFn &handler,
                            const MbMiddlewares &middlewares) {
        routes[{method, path}] = {.middlewares = middlewares, .handler = handler};
    }

    const MbRouteHandler *MbRouteRegistry::find(const std::string &method, const std::string &path) const {
        const auto it = routes.find({method, path});
        return it != routes.end() ? &it->second : nullptr;
    }

    json MbRouteRegistry::remove(const std::string &method, const std::string &path) {
        json res;
        res["error"] = "";

        const auto it = routes.find({method, path});
        if (it == routes.end()) {
            const auto err = std::format("Route for {} {} not found!", method, path);
            // We didn't find that route, return error
            res["error"] = err;
            // LogOrigin::warn("Route Not Found", fmt::format("{}", err));
            return res;
        }

        // Remove item found at the iterator
        routes.erase(it);

        // LogOrigin::info("Route Removed", fmt::format("Route for {} {} erased!", method, path));
        return res;
    }
}
