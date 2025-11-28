#include "../../include/mantisbase/core/models/entity.h"
// #include "spdlog/fmt/bundled/std.h"
#include "../../include/mantisbase/core/auth.h"

namespace mantis {
    HandlerFn EntitySchema::getOneRouteHandler() const {
        HandlerFn handler = [](const MantisRequest &req, const MantisResponse &res) {
            try {
                // Extract path `id` value [REQUIRED]
                const auto schema_id_or_name = trim(req.getPathParamValue("schema_name_or_id"));
                if (schema_id_or_name.empty())
                    throw MantisException(400, "EntitySchema `id` or `name` is required on the route!");

                // If table name is passed in, get the `id` equivalent
                const auto schema_id = schema_id_or_name.starts_with("mbt_")
                                           ? schema_id_or_name
                                           : EntitySchema::genEntityId(schema_id_or_name);

                // Read for entity matching given `id` if it exists, return it, else error `404`
                const auto record = EntitySchema::getTable(schema_id);
                res.sendJson(200,
                             {
                                 {"data", record},
                                 {"error", ""},
                                 {"status", 200}
                             }
                );
            } catch (const MantisException &e) {
                res.sendJson(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
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

    HandlerFn EntitySchema::getManyRouteHandler() const {
        HandlerFn handler = [](MantisRequest &req, MantisResponse &res) {
            try {
                logger::trace("Listing all tables ...");
                const auto tables = EntitySchema::listTables();
                res.sendJson(200, {
                                 {"data", tables},
                                 {"error", ""},
                                 {"status", 200}
                             }
                );
            } catch (const MantisException &e) {
                res.sendJson(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
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

    HandlerFn EntitySchema::postRouteHandler() const {
        HandlerFn handler = [](MantisRequest &req, MantisResponse &res) {
            try {
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJson(400, {
                                     {"data", json::object()},
                                     {"error", err},
                                     {"status", 400}
                                 }
                    );
                }

                const auto eSchema = EntitySchema::fromSchema(body);
                if (const auto val_err = eSchema.validate(); val_err.has_value())
                    throw MantisException(400, val_err.value());

                auto _schema = EntitySchema::createTable(eSchema);
                res.sendJson(200, {
                                 {"data", _schema},
                                 {"error", ""},
                                 {"status", 200}
                             }
                );
            } catch (const MantisException &e) {
                res.sendJson(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
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

    HandlerFn EntitySchema::patchRouteHandler() const {
        HandlerFn handler = [](MantisRequest &req, MantisResponse &res) {
            const auto schema_id_or_name = trim(req.getPathParamValue("schema_name_or_id"));
            if (schema_id_or_name.empty())
                throw MantisException(400, "EntitySchema `id` or `name` is required on the route!");

            // If table name is passed in, get the `id` equivalent
            const auto schema_id = schema_id_or_name.starts_with("mbt_")
                                       ? schema_id_or_name
                                       : EntitySchema::genEntityId(schema_id_or_name);

            // Check request body if valid ...
            const auto &[body, err] = req.getBodyAsJson();
            if (!err.empty()) {
                res.sendJson(400, {
                                 {"data", json::object()},
                                 {"error", err},
                                 {"status", 400}
                             });
            }

            // if (const auto &val_err = Validators::validateUpdateRequestBody(entity, body);
            //     val_err.has_value()) {
            //     res.sendJson(400, {
            //                      {"data", json::object()},
            //                      {"error", val_err.value()},
            //                      {"status", 400}
            //                  });
            //     return;
            // }

            // const auto record = entity.update(entity_id, body);
            // res.sendJson(200, record);
            // return;
        };

        return handler;
    }

    HandlerFn EntitySchema::deleteRouteHandler() const {
        HandlerFn handler = [](const MantisRequest &req, const MantisResponse &res) {
            try {
                const auto schema_id_or_name = trim(req.getPathParamValue("schema_name_or_id"));
                if (schema_id_or_name.empty())
                    throw MantisException(400, "EntitySchema `id` or `name` is required on the route!");

                // If table name is passed in, get the `id` equivalent
                const auto schema_id = schema_id_or_name.starts_with("mbt_")
                                           ? schema_id_or_name
                                           : EntitySchema::genEntityId(schema_id_or_name);
                EntitySchema::dropTable(schema_id);
                res.sendEmpty();
            } catch (const MantisException &e) {
                res.sendJson(e.code(), {
                                 {"status", e.code()},
                                 {"error", e.what()},
                                 {"data", json::object()}
                             }
                );
            } catch (const std::exception &e) {
                res.sendJson(500, {
                                 {"status", 500},
                                 {"error", e.what()},
                                 {"data", json::object()}
                             }
                );
            }
        };

        return handler;
    }

    void EntitySchema::createEntityRoutes() const {
        auto &router = MantisBase::instance().router();

        router.Get("/api/v1/schemas", getManyRouteHandler(), {requireAdminAuth()});
        router.Post("/api/v1/schemas", postRouteHandler(), {requireAdminAuth()}); // Create Entity
        router.Get("/api/v1/schemas/:schema_name_or_id", getOneRouteHandler(), {requireAdminAuth()});
        router.Patch("/api/v1/schemas/:schema_name_or_id", patchRouteHandler(), {requireAdminAuth()}); // Update Entity
        router.Delete("/api/v1/schemas/:schema_name_or_id", deleteRouteHandler(), {requireAdminAuth()});
        // Delete Entity
    };
}
