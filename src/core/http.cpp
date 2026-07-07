#include "../../include/mantisbase/core/router.h"
#include "../../include/mantisbase/utils/utils.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/database.h"
#include "../../include/mantisbase/core/files.h"
#include "../../include/mantisbase/core/http.h"
#include "../../include/mantisbase/core/kv_store.h"

#include <cmrc/cmrc.hpp>
#include <chrono>
#include <thread>

// For thread logger
#include <spdlog/sinks/stdout_color_sinks-inl.h>
#include <spdlog/sinks/ansicolor_sink.h>

#include "../include/mantisbase/core/exceptions.h"
#include "../include/mantisbase/core/middlewares.h"
#include "../include/mantisbase/core/models/entity_schema.h"
#include "../include/mantisbase/core/models/entity_routes.h"
#include "../include/mantisbase/core/models/entity_schema_routes.h"
#include "../include/mantisbase/core/logger/log_database.h"
#include "../include/mantisbase/core/realtime.h"
#include "../include/mantisbase/core/sse.h"

namespace mb {
    void Router::Get(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares) {
        LogOrigin::info("Route Created", fmt::format("Creating route [ GET ] {}", path));
        m_routeRegistry.add("GET", path, handler, middlewares);
        globalRouteHandler("GET", path);
    }

    void Router::Post(const std::string &path, const HandlerWithContentReaderFn &handler,
                      const Middlewares &middlewares) {
        LogOrigin::info("Route Created", fmt::format("Creating route [ POST ] {}", path));
        m_routeRegistry.add("POST", path, handler, middlewares);
        globalRouteHandlerWithReader("POST", path);
    }

    void Router::Post(const std::string &path, const HandlerFn &handler,
                      const Middlewares &middlewares) {
        LogOrigin::info("Route Created", fmt::format("Creating route [ POST ] {}", path));
        m_routeRegistry.add("POST", path, handler, middlewares);
        globalRouteHandler("POST", path);
    }

    void Router::Patch(const std::string &path, const HandlerWithContentReaderFn &handler,
                       const Middlewares &middlewares) {
        m_routeRegistry.add("PATCH", path, handler, middlewares);
        globalRouteHandlerWithReader("PATCH", path);
    }

    void Router::Patch(const std::string &path, const HandlerFn &handler,
                       const Middlewares &middlewares) {
        m_routeRegistry.add("PATCH", path, handler, middlewares);
        globalRouteHandler("PATCH", path);
    }

    void Router::Delete(const std::string &path, const HandlerFn &handler, const Middlewares &middlewares) {
        m_routeRegistry.add("DELETE", path, handler, middlewares);
        globalRouteHandler("DELETE", path);
    }

    void Router::globalRouteHandler(const std::string &method, const std::string &path) {
        const std::function handlerFunc = [this, method, path](const httplib::Request &req, httplib::Response &res) {
            MantisRequest ma_req{req, mApp};
            MantisResponse ma_res{res};

            const auto route = m_routeRegistry.find(method, path);
            if (!route) {
                json response;
                response["status"] = 404;
                response["error"] = std::format("{} {} Route Not Found", method, path);
                response["data"] = json::object();

                ma_res.sendJSON(404, response);
                return;
            }

            // First, execute global middlewares
            for (const auto &g_mw: m_preRoutingMiddlewares) {
                if (g_mw(ma_req, ma_res) == HandlerResponse::Handled) return;
            }

            // Secondly, execute route specific middlewares
            for (const auto &mw: route->middlewares) {
                if (mw(ma_req, ma_res) == HandlerResponse::Handled) return;
            }

            // Finally, execute the handler function
            if (const auto func = std::get_if<HandlerFn>(&route->handler)) {
                (*func)(ma_req, ma_res);
            }

            // Any post routing checks?
            for (const auto &p_mw: m_postRoutingMiddlewares) {
                p_mw(ma_req, ma_res); // Execute all, no return types
            }
        };

        if (method == "GET") {
            svr.Get(path, handlerFunc);
        } else if (method == "PATCH") {
            svr.Patch(path, handlerFunc);
        } else if (method == "POST") {
            svr.Post(path, handlerFunc);
        } else if (method == "DELETE") {
            svr.Delete(path, handlerFunc);
        } else {
            throw MantisException(500, "Router method `" + method + "` is not supported!");
        }
    }

    void Router::globalRouteHandlerWithReader(const std::string &method, const std::string &path) {
        const std::function handlerFuncWithContentReader = [this, method, path](
            const httplib::Request &req, httplib::Response &res, const httplib::ContentReader &cr) {
            MantisRequest ma_req{req, mApp};
            MantisResponse ma_res{res};
            MantisContentReader ma_cr{cr, ma_req};

            const auto route = m_routeRegistry.find(method, path);
            if (!route) {
                ma_res.sendJSON(404, {
                                    {"status", 404},
                                    {"error", std::format("{} {} Route Not Found", method, path)},
                                    {"data", json::object()}
                                });
                return;
            }

            // First, execute global middlewares
            for (const auto &g_mw: m_preRoutingMiddlewares) {
                if (g_mw(ma_req, ma_res) == HandlerResponse::Handled) return;
            }

            // Secondly, execute route specific middlewares
            for (const auto &mw: route->middlewares) {
                if (mw(ma_req, ma_res) == HandlerResponse::Handled) return;
            }

            // Finally, execute the handler function
            if (const auto func = std::get_if<HandlerWithContentReaderFn>(&route->handler)) {
                (*func)(ma_req, ma_res, ma_cr);
            }

            // Any post routing checks?
            for (const auto &p_mw: m_postRoutingMiddlewares) {
                p_mw(ma_req, ma_res); // Execute all, no return types
            }
        };

        if (method == "PATCH") {
            svr.Patch(path, handlerFuncWithContentReader);
        } else if (method == "POST") {
            svr.Post(path, handlerFuncWithContentReader);
        } else {
            throw MantisException(500, "Router method `" + method + "` is not supported!");
        }
    }
}
