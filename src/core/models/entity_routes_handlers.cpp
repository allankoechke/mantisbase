#include "../../include/mantisbase/core/models/entity.h"
// #include "spdlog/fmt/bundled/std.h"
#include "../../include/mantisbase/core/auth.h"
#include <fstream>

namespace mantis {
    HandlerFn Entity::getOneRouteHandler() const {
        // Capture entity name, currently we get an error if we capture `this` directly.
        const std::string entity_name = name();
        logger::trace("Creating GET /api/v1/entities/{}/:id", entity_name);

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
                    res.sendJSON(200,
                                 {
                                     {"data", record},
                                     {"error", ""},
                                     {"status", 200}
                                 }
                    );
                } else {
                    res.sendJSON(404,
                                 {
                                     {"data", json::object()},
                                     {"error", "Resource not found!"},
                                     {"status", 404}
                                 }
                    );
                }
            } catch (const std::exception &e) {
                res.sendJSON(500, {
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
        logger::trace("Creating GET /api/v1/entities/{}", entity_name);

        HandlerFn handler = [entity_name](const MantisRequest &req, const MantisResponse &res) {
            try {
                // Get entity object for given `name`
                const auto entity = MantisBase::instance().entity(entity_name);

                // Page number to query
                auto page = req.hasQueryParam("page") ? std::stoi(req.getQueryParamValue("page")) : 1;

                // Records per request
                auto page_size = req.hasQueryParam("page_size") ? std::stoi(req.getQueryParamValue("page_size")) : 100;

                // Skip counting total items
                bool skip_total_count = req.hasQueryParam("skip_total_count")
                                            ? strToBool(req.getQueryParamValue("skip_total_count"))
                                            : false;
                // Filter parameters
                std::string filter = req.hasQueryParam("filter")
                                         ? req.getQueryParamValue("filter")
                                         : "";

                nlohmann::json opts;
                opts["pagination"] = {
                    {"page", page},
                    {"page_size", page_size},
                    {"skip_total_count", skip_total_count}
                };
                opts["filter"] = filter; // TODO

                // Get total count
                int items_count = skip_total_count ? -1 : entity.countRecords();

                const auto records = entity.list(opts);
                res.sendJSON(200, {
                                 {
                                     "data", {
                                         {"page", page},
                                         {"items_count", records.size()},
                                         {"page_size", page_size},
                                         {"total_count", items_count},
                                         {"items", records}
                                     }
                                 },
                                 {"error", ""},
                                 {"status", 200}
                             }
                );
            } catch (const std::exception &e) {
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", 500}
                             }
                );
            }
        };

        return handler;
    }

    HandlerWithContentReaderFn Entity::postRouteHandler() const {
        // Capture entity name, currently we get an error if we capture `this` directly.
        const std::string entity_name = name();
        logger::trace("Creating POST /api/v1/entities/{}", entity_name);

        HandlerWithContentReaderFn handler = [entity_name](const MantisRequest &req, const MantisResponse &res,
                                                           MantisContentReader &reader) {
            try {
                // Get entity object for given name
                const auto entity = MantisBase::instance().entity(entity_name);

                // Parse form data to JSON body and files metadata
                reader.parseFormDataToEntity(entity);

                if (const auto &val_err = Validators::validateRequestBody(entity, reader.jsonBody());
                    val_err.has_value()) {
                    res.sendJSON(400, {
                                     {"data", json::object()},
                                     {"error", val_err.value()},
                                     {"status", 400}
                                 });
                    return;
                }

                // Write files to disk ...
                reader.writeFiles(entity_name);

                // Create the record
                auto record = entity.create(reader.jsonBody());

                // For auth types, remove the password field from the response
                if (entity.type() == "auth" && record.contains("password")) {
                    record.erase("password");
                }
                res.sendJSON(201, record);
            } catch (const MantisException &e) {
                reader.undoWrittenFiles(entity_name);
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             }
                );
            } catch (const std::exception &e) {
                reader.undoWrittenFiles(entity_name);
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", 500}
                             }
                );
            }
        };

        return handler;
    }

    HandlerWithContentReaderFn Entity::patchRouteHandler() const {
        // Capture entity name, currently we get an error if we capture `this` directly.
        const std::string entity_name = name();
        logger::trace("Creating PATCH /api/v1/entities/{}/:id", entity_name);

        HandlerWithContentReaderFn handler = [entity_name](MantisRequest &req, MantisResponse &res,
                                                           MantisContentReader &reader) {
            try {
                // Get entity object for given name
                const auto entity = MantisBase::instance().entity(entity_name);

                const auto entity_id = trim(req.getPathParamValue("id"));
                if (entity_id.empty())
                    throw MantisException(400, "Entity `id` is required!");

                // Parse form data to JSON body and files metadata
                reader.parseFormDataToEntity(entity);

                // Validate request body
                if (const auto &val_err = Validators::validateUpdateRequestBody(entity, reader.jsonBody());
                    val_err.has_value()) {
                    res.sendJSON(400, {
                                     {"data", json::object()},
                                     {"error", val_err.value()},
                                     {"status", 400}
                                 });
                    return;
                }

                // Write files to disk first
                reader.writeFiles(entity_name);

                // Update records ...
                auto record = entity.update(entity_id, reader.jsonBody());

                // For auth types, remove the password field from the response
                if (entity.type() == "auth" && record.contains("password")) {
                    record.erase("password");
                }

                res.sendJSON(200, record);
            } catch (const MantisException &e) {
                reader.undoWrittenFiles(entity_name);
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             }
                );
            } catch (const std::exception &e) {
                reader.undoWrittenFiles(entity_name);
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", 500}
                             }
                );
            }
        };

        return handler;
    }

    HandlerFn Entity::deleteRouteHandler() const {
        // Capture entity name, currently we get an error if we capture `this` directly.
        const std::string entity_name = name();
        logger::trace("Creating DELETE /api/v1/entities/{}/:id", entity_name);

        HandlerFn handler = [entity_name](const MantisRequest &req, const MantisResponse &res) {
            try {
                // Get entity object for given name
                const auto entity = MantisBase::instance().entity(entity_name);

                const auto entity_id = trim(req.getPathParamValue("id"));
                if (entity_id.empty())
                    throw MantisException(400, "Entity `id` is required!");

                entity.remove(entity_id);
                res.sendEmpty();
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             }
                );
            } catch (const std::exception &e) {
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", 500}
                             }
                );
            }
        };

        return handler;
    }

    void Entity::createEntityRoutes() const {
        auto &router = MantisBase::instance().router();

        std::cout << std::endl;

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
    };
}
