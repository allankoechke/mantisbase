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

    std::vector<std::string> Router::extractParamNames(const std::string &httplib_path) {
        std::vector<std::string> names;
        std::cout << "HTTPLIB PATH: " << httplib_path << std::endl;
        std::cout << "httplib path size? " << httplib_path.size() << std::endl;

        for (size_t i = 0; i < httplib_path.size(); ++i) {
            if (httplib_path[i] == ':' && (i == 0 || httplib_path[i - 1] == '/')) {
                std::string name;
                ++i;
                while (i < httplib_path.size() && httplib_path[i] != '/') {
                    name += httplib_path[i];
                    ++i;
                }
                names.push_back(name);
            }
        }
        return names;
    }

    void Router::executeMiddlewareChain(MantisRequest &req, MantisResponse &res, const RouteHandler *route) const {
        return;

        std::cout << "Exec middleware chain START" << std::endl;
        // Execute global pre-routing middlewares
        for (const auto &g_mw: m_preRoutingMiddlewares) {
            if (g_mw(req, res) == HandlerResponse::Handled) return;
        }

        std::cout << "Pre routing done" << std::endl;

        // Execute route-specific middlewares
        if (route) {
            std::cout << "Route was found!" << std::endl;

            for (const auto &mw: route->middlewares) {
                if (mw(req, res) == HandlerResponse::Handled) return;
            }

            // Execute the handler
            if (const auto func = std::get_if<HandlerFn>(&route->handler)) {
                (*func)(req, res);
            }
        }

        std::cout << "Execute post routing middlewares" << std::endl;

        // Post routing
        for (const auto &p_mw: m_postRoutingMiddlewares) {
            p_mw(req, res);
        }

        std::cout << "Middleware chains done!" << std::endl;
    }

    void Router::Get(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares) {
        LogOrigin::info("Route Created", fmt::format("GET {}", path));
        m_routeRegistry.add("GET", path, handler, middlewares);
        registerDrogonHandler("GET", path);
    }

    void Router::Post(const std::string &path, const HandlerWithContentReaderFn &handler,
                      const Middlewares &middlewares) {
        LogOrigin::info("Route Created", fmt::format("POST {}", path));
        m_routeRegistry.add("POST", path, handler, middlewares);
        registerDrogonHandlerWithReader("POST", path);
    }

    void Router::Post(const std::string &path, const HandlerFn &handler,
                      const Middlewares &middlewares) {
        LogOrigin::info("Route Created", fmt::format("POST {}", path));
        m_routeRegistry.add("POST", path, handler, middlewares);
        registerDrogonHandler("POST", path);
    }

    void Router::Patch(const std::string &path, const HandlerWithContentReaderFn &handler,
                       const Middlewares &middlewares) {
        m_routeRegistry.add("PATCH", path, handler, middlewares);
        registerDrogonHandlerWithReader("PATCH", path);
    }

    void Router::Patch(const std::string &path, const HandlerFn &handler,
                       const Middlewares &middlewares) {
        m_routeRegistry.add("PATCH", path, handler, middlewares);
        registerDrogonHandler("PATCH", path);
    }

    void Router::Delete(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares) {
        m_routeRegistry.add("DELETE", path, handler, middlewares);
        registerDrogonHandler("DELETE", path);
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

    void Router::registerDrogonHandler(const std::string &method, const std::string &path) {
        const auto drogon_path = convertPathToDrogon(path);
        const auto drogon_method = toDrogonMethod(method);

        std::cout << "Registering Drogon Handler [Before] : " << method << " : " << path << std::endl;
        std::cout << "BEFORE -> m = " << &method << ", p = " << &path << std::endl;

        auto handler = [&method, &path](
            const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            std::cout << "AFTER -> m = " << &method << ", p = " << &path << std::endl;
            std::cout << "IN" << std::endl;

            // std::cout << "Registering Drogon Handler [After] : " << method << " : " << path << std::endl;
            // std::cout << "Method: " << method << ", Path: " << path << std::endl;

            // MantisRequest ma_req{mApp, req};
            // MantisResponse ma_res{};
            //
            // const auto param_names = req->getRoutingParameters();
            //
            // // Extract path params from the matched path
            // // Drogon stores them as positional params in the matched pattern
            // if (!param_names.empty()) {
            //     std::cout << "Parameter names not empty!" << std::endl;
            //     // Parse path params by comparing actual path with pattern
            //     auto actual_parts = splitString(std::string(req->path()), "/");
            //     auto pattern_parts = splitString(_path, "/");
            //
            //     for (size_t i = 0; i < pattern_parts.size() && i < actual_parts.size(); ++i) {
            //         if (!pattern_parts[i].empty() && pattern_parts[i][0] == ':') {
            //             ma_req.setPathParam(pattern_parts[i].substr(1), actual_parts[i]);
            //         }
            //     }
            // }
            //
            // std::cout << "Check if route exists in the registry: " << std::endl;
            // std::cout << "Method: " << _method << ", Path: " << _path << std::endl;
            //
            // const auto route = m_routeRegistry.find(_method, _path);
            //
            // std::cout << "Route found? " << (route == nullptr) << std::endl;
            //
            // if (!route) {
            //     json response;
            //     response["status"] = 404;
            //     response["error"] = std::format("{} {} Route Not Found", _method, _path);
            //     response["data"] = json::object();
            //     ma_res.sendJSON(404, response);
            //     callback(ma_res.drogonResponse());
            //     return;
            // }
            //
            // std::cout << "Getting into middleware chains ..." << std::endl;
            //
            // executeMiddlewareChain(ma_req, ma_res, route);
            // std::cout << "Middleware chains ended, calling response!" << std::endl;
            // callback(ma_res.drogonResponse());
            drogon::HttpResponsePtr m_res = drogon::HttpResponse::newHttpResponse();
            m_res->setBody("OK");
            m_res->setContentTypeString("text/plain");
            m_res->setStatusCode(static_cast<drogon::HttpStatusCode>(200));
            callback(m_res);
            std::cout << "Response done!" << std::endl;
        };

        drogon::app().registerHandler(drogon_path, handler, {drogon_method});
    }

    void Router::registerDrogonHandlerWithReader(const std::string &method, const std::string &path) {
        const auto drogon_path = convertPathToDrogon(path);
        const auto param_names = extractParamNames(path);
        const auto drogon_method = toDrogonMethod(method);

        auto handler = [this, method, path, param_names](
            const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            MantisRequest ma_req{mApp, req};
            MantisResponse ma_res{};
            MantisContentReader ma_cr{ma_req};

            // Extract path params
            if (!param_names.empty()) {
                auto actual_parts = splitString(std::string(req->path()), "/");
                auto pattern_parts = splitString(path, "/");

                for (size_t i = 0; i < pattern_parts.size() && i < actual_parts.size(); ++i) {
                    if (!pattern_parts[i].empty() && pattern_parts[i][0] == ':') {
                        ma_req.setPathParam(pattern_parts[i].substr(1), actual_parts[i]);
                    }
                }
            }

            const auto route = m_routeRegistry.find(method, path);
            if (!route) {
                ma_res.sendJSON(404, {
                                    {"status", 404},
                                    {"error", std::format("{} {} Route Not Found", method, path)},
                                    {"data", json::object()}
                                });
                callback(ma_res.drogonResponse());
                return;
            }

            // Execute global middlewares
            for (const auto &g_mw: m_preRoutingMiddlewares) {
                if (g_mw(ma_req, ma_res) == HandlerResponse::Handled) {
                    callback(ma_res.drogonResponse());
                    return;
                }
            }

            // Execute route-specific middlewares
            for (const auto &mw: route->middlewares) {
                if (mw(ma_req, ma_res) == HandlerResponse::Handled) {
                    callback(ma_res.drogonResponse());
                    return;
                }
            }

            // Execute handler with content reader
            if (const auto func = std::get_if<HandlerWithContentReaderFn>(&route->handler)) {
                (*func)(ma_req, ma_res, ma_cr);
            }

            // Post routing
            for (const auto &p_mw: m_postRoutingMiddlewares) {
                p_mw(ma_req, ma_res);
            }

            callback(ma_res.drogonResponse());
        };

        drogon::app().registerHandler(drogon_path, handler, {drogon_method});
    }
}
