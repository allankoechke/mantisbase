#include "../../include/mantisbase/core/router.h"
#include "../../include/mantisbase/utils/utils.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/database.h"
#include "../../include/mantisbase/core/files.h"
#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/kv_store.h"

#include <drogon/drogon.h>
#include <regex>

namespace mb {
    std::string Router::convertPathToDrogon(const std::string &httplib_path) {
        // Convert httplib path params /:param to Drogon format /{param}
        // Also handle regex patterns like R"(/mb(/.*)?)" -> /mb/{1}
        std::string result;
        result.reserve(httplib_path.size());

        for (size_t i = 0; i < httplib_path.size(); ++i) {
            if (httplib_path[i] == ':' && (i == 0 || httplib_path[i - 1] == '/')) {
                result += '{';
                ++i;
                while (i < httplib_path.size() && httplib_path[i] != '/') {
                    result += httplib_path[i];
                    ++i;
                }
                result += '}';
                if (i < httplib_path.size()) {
                    result += httplib_path[i];
                }
            } else {
                result += httplib_path[i];
            }
        }

        return result;
    }

    void Router::executeMiddlewareChain(MbRequest &req, MbResponse &res, const RouteHandler *route) const {
        // Execute global pre-routing middlewares
        for (const auto &g_mw: m_preRoutingMiddlewares) {
            if (g_mw(req, res) == MbHandlerResponse::Handled) return;
        }

        // Execute route-specific middlewares
        if (route) {
            for (const auto &mw: route->middlewares) {
                if (mw(req, res) == MbHandlerResponse::Handled) return;
            }

            // Execute the handler
            route->handler(req, res);
        }

        // Post routing
        for (const auto &p_mw: m_postRoutingMiddlewares) {
            p_mw(req, res);
        }
    }

    void Router::Get(const std::string &path, const MbHandlerFn &handler, const MbMiddlewares &middlewares) {
        logger().debug("Route Created", fmt::format("GET {}", path));
        m_routeRegistry.add("GET", path, handler, middlewares);
        registerDrogonHandler("GET", path);
    }

    void Router::Post(const std::string &path, const MbHandlerFn &handler,
                      const MbMiddlewares &middlewares) {
        logger().debug("Route Created", fmt::format("POST {}", path));
        m_routeRegistry.add("POST", path, handler, middlewares);
        registerDrogonHandler("POST", path);
    }

    void Router::Patch(const std::string &path, const MbHandlerFn &handler,
                       const MbMiddlewares &middlewares) {
        logger().debug("Route Created", fmt::format("PATCH {}", path));
        m_routeRegistry.add("PATCH", path, handler, middlewares);
        registerDrogonHandler("PATCH", path);
    }

    void Router::Put(const std::string &path, const MbHandlerFn &handler,
                       const MbMiddlewares &middlewares) {
        logger().debug("Route Created", fmt::format("PUT {}", path));
        m_routeRegistry.add("PUT", path, handler, middlewares);
        registerDrogonHandler("PUT", path);
    }

    void Router::Delete(const std::string &path, const MbHandlerFn &handler, const MbMiddlewares &middlewares) {
        logger().debug("Route Created", fmt::format("DELETE {}", path));
        m_routeRegistry.add("DELETE", path, handler, middlewares);
        registerDrogonHandler("DELETE", path);
    }

    void Router::redirect(const std::string &initialPath, const std::string &destinationPath, int redirectStatus,
                          const std::vector<std::string> &constraints) const {
        logger().debug("Route Redirect Created", fmt::format("Redirect from {} to {}", initialPath, destinationPath));

        // Create callback handler
        auto _cb = [destinationPath, redirectStatus](const drogon::HttpRequestPtr &,
                                             std::function<void(const drogon::HttpResponsePtr &)> &&cb) {
            cb(drogon::HttpResponse::newRedirectionResponse(
                destinationPath, static_cast<drogon::HttpStatusCode>(redirectStatus)));
        };

        // Get constraint array
        const auto http_constraints = Router::validateConstraints(constraints);

        // Register new path
        drogon::app().registerHandler(initialPath, std::move(_cb), http_constraints);
    }

    static drogon::HttpMethod toDrogonMethod(const std::string &method) {
        if (method == "GET") return drogon::Get;
        if (method == "POST") return drogon::Post;
        if (method == "PATCH") return drogon::Patch;
        if (method == "DELETE") return drogon::Delete;
        if (method == "PUT") return drogon::Put;
        if (method == "OPTIONS") return drogon::Options;
        return drogon::Get;
    }

    void Router::registerDrogonHandler(const std::string &method, const std::string &path) const {
        const auto drogon_path = convertPathToDrogon(path);
        const auto drogon_method = toDrogonMethod(method);

        auto handler = [
                    this,
                    _method = std::string(method),
                    _path = std::string(path)
                ](
            const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            MbRequest ma_req{mbApp(), req};
            MbResponse ma_res{mbApp()};

            const auto param_names = req->getRoutingParameters();

            // Extract path params from the matched path
            // Drogon stores them as positional params in the matched pattern
            if (!param_names.empty()) {
                // Parse path params by comparing actual path with pattern
                const auto actual_parts = splitString(std::string(req->path()), "/");
                const auto pattern_parts = splitString(_path, "/");

                for (size_t i = 0; i < pattern_parts.size() && i < actual_parts.size(); ++i) {
                    if (!pattern_parts[i].empty() && pattern_parts[i][0] == ':') {
                        ma_req.setPathParam(pattern_parts[i].substr(1), actual_parts[i]);
                    }
                }
            }

            const auto route = m_routeRegistry.find(_method, _path);
            if (!route) {
                json response;
                response["status"] = 404;
                response["error"] = std::format("{} {} Route Not Found", _method, _path);
                response["data"] = json::object();
                ma_res.sendJSON(404, response);
                callback(ma_res.drogonResponse());
                return;
            }

            executeMiddlewareChain(ma_req, ma_res, route);
            callback(ma_res.drogonResponse());
        };

        drogon::app().registerHandler(drogon_path, std::move(handler), {drogon_method});
    }
}
