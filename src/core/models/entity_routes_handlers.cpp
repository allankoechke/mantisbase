//
// Created by codeart on 16/11/2025.
//

#include "../../include/mantisbase/core/models/entity.h"
// #include "spdlog/fmt/bundled/std.h"
#include "../../include/mantisbase/core/auth.h"

namespace mantis {
    HandlerFn Entity::getOneRouteHandler() const {
        // Capture entity name, currently we get an error if we capture `this` directly.
        const std::string entity_name = name();

        HandlerFn handler = [entity_name](const MantisRequest &req, const MantisResponse &res) {
            try {
                // Get entity object for given name
                const auto entity = MantisBase::instance().entity(entity_name);

                // Extract path `id` value [REQUIRED]
                const auto entity_id = trim(req.getPathParamValue("id"));
                if (entity_id.empty())
                    throw MantisException(400, "Entity `id` is required!");

                // Read for entity matching given `id` if it exists, return it, else error `404`
                if (const auto record = entity.read(entity_id); record.has_value()) {
                    res.sendJson(200,
                                 {
                                     {"data", record},
                                     {"error", ""},
                                     {"status", 200}
                                 }
                    );
                } else {
                    res.sendJson(404,
                                 {
                                     {"data", json::object()},
                                     {"error", "Resource not found!"},
                                     {"status", 404}
                                 }
                    );
                }
            } catch (const std::exception &e) {
                res.sendJson(500, {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", 500}
                             }
                );
            }
        };

        return handler;
    }

    HandlerFn Entity::getManyRouteHandler() const {
        // Capture entity name, currently we get an error if we capture `this` directly.
        const std::string entity_name = name();

        HandlerFn handler = [entity_name](MantisRequest &req, MantisResponse &res) {
            try {
                // Get entity object for given `name`
                const auto entity = MantisBase::instance().entity(entity_name);

                // nlohmann::json opts;
                // opts["pagination"] = {
                //     {"page_index", 1},
                //     {"per_page", 100},
                //     {"skip_total_count", false}
                // };
                std::cout << "List for `" << entity.name() << "`: \n";

                const auto records = entity.list();
                res.sendJson(200, {
                                 {"data", records},
                                 {"error", ""},
                                 {"status", 200}
                             }
                );
            } catch (const std::exception &e) {
                res.sendJson(500, {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", 500}
                             }
                );
            }
        };

        return handler;
    }

    HandlerFn Entity::postRouteHandler() const {
        // Capture entity name, currently we get an error if we capture `this` directly.
        const std::string entity_name = name();

        HandlerFn handler = [entity_name](MantisRequest &req, MantisResponse &res) {
            // Get entity object for given name
            const auto entity = MantisBase::instance().entity(entity_name);
        };

        return handler;
    }

    HandlerFn Entity::patchRouteHandler() const {
        // Capture entity name, currently we get an error if we capture `this` directly.
        const std::string entity_name = name();

        HandlerFn handler = [entity_name](MantisRequest &req, MantisResponse &res) {
            // Get entity object for given name
            const auto entity = MantisBase::instance().entity(entity_name);

            const auto entity_id = trim(req.getPathParamValue("id"));

            if (entity_id.empty())
                throw MantisException(400, "Entity `id` is required!");

            const auto &[body, err] = req.getBodyAsJson();
            if (!err.empty()) {
                res.sendJson(500, {
                                 {"data", json::object()},
                                 {"error", err},
                                 {"status", 400}
                             });
            }

            if (const auto &val_err = Validators::validateRequestBody(entity, body);
                val_err.has_value()) {
                res.sendJson(400, {
                                 {"data", json::object()},
                                 {"error", val_err.value()},
                                 {"status", 400}
                             });
                return;
            }

            const auto record = entity.update(entity_id, body);
            res.sendJson(200, record);
            return;
        };

        return handler;
    }

    HandlerFn Entity::deleteRouteHandler() const {
        // Capture entity name, currently we get an error if we capture `this` directly.
        const std::string entity_name = name();

        HandlerFn handler = [entity_name](MantisRequest &req, MantisResponse &res) {
            // Get entity object for given name
            const auto entity = MantisBase::instance().entity(entity_name);

            const auto entity_id = trim(req.getPathParamValue("id"));
            if (entity_id.empty())
                throw MantisException(400, "Entity `id` is required!");

            entity.remove(entity_id);
            res.sendEmpty();
        };

        return handler;
    }

    HandlerFn Entity::authRouteHandler() const {
        // Capture entity name, currently we get an error if we capture `this` directly.
        const std::string entity_name = name();

        HandlerFn handler = [entity_name](MantisRequest &req, MantisResponse &res) {
            try {
                // Get entity object for given name
                const auto entity = MantisBase::instance().entity(entity_name);

                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJson(400, {
                                     {"data", json::object()},
                                     {"error", err},
                                     {"status", 400}
                                 });
                }

                // Validate auth fields, `email` & `password`
                if (!body.contains("email") || !body.contains("password")) {
                    res.sendJson(400, {
                                     {"data", json::object()},
                                     {"error", "Expected both email & password input fields."},
                                     {"status", 400}
                                 });
                    return;
                }

                // Validate email field
                if (auto er = Validators::validatePreset("email", body["email"].get<std::string>());
                    er.has_value()) {
                    res.sendJson(400, {
                                     {"data", json::object()},
                                     {"error", er.value()},
                                     {"status", 400}
                                 });
                    return;
                }

                auto user = entity.queryFromCols(body["email"].get<std::string>(), {"email"}).value_or(json::object());
                if (user.empty()) {
                    res.sendJson(404, {
                                     {"data", json::object()},
                                     {"error", "User not found!"},
                                     {"status", 404}
                                 });
                    return;
                }

                // Verify user password against stored password
                if (!verifyPassword(body["password"].get<std::string>(), user["password"].get<std::string>())) {
                    res.sendJson(404, {
                                     {"data", json::object()},
                                     {"error", "User not found!"},
                                     {"status", 404}
                                 });
                    return;
                }

                // Create token and return to user ...
                auto token = Auth::createToken({
                    {"id", user["id"].get<std::string>()},
                    {"table", entity.name()}
                });

                // Redact password in response ...
                if (user.contains("password")) user.erase("password");

                // Return token and auth user
                res.sendJson(200, {
                                 {
                                     "data", {
                                         {"token", token},
                                         {"user", user}
                                     }
                                 },
                                 {"error", ""},
                                 {"status", 200}
                             });
            } catch (const std::exception &e) {
                res.sendJson(500, {
                                 {"data", json::object()},
                                 {"error", "Error logging you in, try again later!"},
                                 {"status", 500}
                             });
                logger::critical("Error logging in, \n\t> {}", e.what());
            }
        };

        return handler;
    }

    void Entity::createEntityRoutes() const {
        // logger::trace("Entity::createEntityRoutes() for Entity `{}`", name());

        auto &router = MantisBase::instance().router();

        // List Entities
        router.Get("/api/v1/entities/" + name(), getManyRouteHandler(),
                   {hasAccess(name())});

        // Fetch Entity
        router.Get("/api/v1/entities/" + name() + "/:id", getOneRouteHandler(),
                   {hasAccess(name())});

        // base & auth /post/:id /delete/:id
        if (type() == "base" || type() == "auth") {
            // Create Entity
            router.Post("/api/v1/entities/" + name(), postRouteHandler(),
                        {hasAccess(name())});

            // Update Entity
            router.Patch("/api/v1/entities/" + name() + "/:id", patchRouteHandler(),
                         {hasAccess(name())});

            // Delete Entity
            router.Delete("/api/v1/entities/" + name() + "/:id", deleteRouteHandler(),
                          {hasAccess(name())});
        }

        // auth /login
        if (type() == "auth") {
            router.Post("/api/v1/entities/" + name() + "/auth/token", authRouteHandler());
            // router.Post("/api/v1/" + name() + "/auth/logout", authRouteHandler());
            // router.Post("/api/v1/" + name() + "/auth/reset-password", authRouteHandler());
            // router.Post("/api/v1/" + name() + "/auth/login", authRouteHandler());
        }
    };
}
