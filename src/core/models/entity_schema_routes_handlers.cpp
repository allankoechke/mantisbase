#include "../../include/mantisbase/core/models/entity_schema_routes.h"
#include "../../include/mantisbase/core/models/entity_schema.h"
#include "../../include/mantisbase/mantisbase.h"

namespace mb {
    namespace {
        std::string schemaIdFromPathParam(const std::string &schema_id_or_name) {
            if (schema_id_or_name.starts_with("mbt_"))
                return schema_id_or_name;

            if (!MbEntitySchema::isValidEntityName(schema_id_or_name))
                throw MbException(400, "Invalid entity name/id");

            return MbEntitySchema::genEntityId(schema_id_or_name);
        }
    }

    MbHandlerFn schemaGetOneHandler() {
        return [](const MbRequest &req, const MbResponse &res) {
            try {
                const auto schema_id_or_name = trim(req.pathParamValue("schema_name_or_id"));
                if (schema_id_or_name.empty())
                    throw MbException(400, "MbEntitySchema `id` or `name` is required on the route!");

                const auto schema_id = schemaIdFromPathParam(schema_id_or_name);
                const auto record = MbEntitySchema::getTable(req.mbApp(), schema_id);

                res.sendJSON(200, {
                                 {"data", record},
                                 {"error", ""},
                                 {"status", 200}
                             });
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("Schema", "Handler Error", fmt::format("Schema handler error: {}", e.what()));
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."},
                                 {"status", 500}
                             });
            }
        };
    }

    MbHandlerFn schemaGetManyHandler() {
        return [](MbRequest &req, MbResponse &res) {
            try {
                const auto tables = MbEntitySchema::listTables(req.mbApp());
                res.sendJSON(200, {
                                 {"data", tables},
                                 {"error", ""},
                                 {"status", 200}
                             });
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("Schema", "Handler Error", fmt::format("Schema handler error: {}", e.what()));
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."},
                                 {"status", 500}
                             });
            }
        };
    }

    MbHandlerFn schemaPostHandler() {
        return [](const MbRequest &req, const MbResponse &res) {
            try {
                const auto &[body, err] = req.bodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {
                                     {"data", json::object()},
                                     {"error", err},
                                     {"status", 400}
                                 });
                    return;
                }

                auto eSchema = MbEntitySchema::fromSchema(req.mbApp(), body);
                auto _ = eSchema.dump();

                if (const auto val_err = eSchema.validate(); val_err.has_value())
                    throw MbException(400, val_err.value());

                auto _schema = MbEntitySchema::createTable(eSchema);
                res.sendJSON(201, {
                                 {"data", _schema},
                                 {"error", ""},
                                 {"status", 201}
                             });
            } catch (const MbException &e) {
                req.mbApp().logger().critical("MbEntitySchema", "Schema Creation Error",
                                              fmt::format("Error creating entity schema\n\t— {}", e.what()));
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbEntitySchema", "Schema Creation Error",
                                              fmt::format("Error creating entity schema\n\t— {}", e.what()));
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."},
                                 {"status", 500}
                             });
            }
        };
    }

    MbHandlerFn schemaPatchHandler() {
        return [](MbRequest &req, MbResponse &res) {
            try {
                const auto schema_id_or_name = trim(req.pathParamValue("schema_name_or_id"));
                if (schema_id_or_name.empty())
                    throw MbException(400, "MbEntitySchema `id` or `name` is required on the route!");

                const auto schema_id = schemaIdFromPathParam(schema_id_or_name);

                const auto &[body, err] = req.bodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {
                                     {"data", json::object()},
                                     {"error", err},
                                     {"status", 400}
                                 });
                    return;
                }

                auto _schema = MbEntitySchema::updateTable(req.mbApp(), schema_id, body);
                res.sendJSON(200, {
                                 {"data", _schema},
                                 {"error", ""},
                                 {"status", 200}
                             });
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("Schema", "Handler Error", fmt::format("Schema handler error: {}", e.what()));
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."},
                                 {"status", 500}
                             });
            }
        };
    }

    MbHandlerFn schemaDeleteHandler() {
        return [](const MbRequest &req, const MbResponse &res) {
            try {
                const auto schema_id_or_name = trim(req.pathParamValue("schema_name_or_id"));
                if (schema_id_or_name.empty())
                    throw MbException(400, "MbEntitySchema `id` or `name` is required on the route!");

                const auto schema_id = schemaIdFromPathParam(schema_id_or_name);
                MbEntitySchema::dropTable(req.mbApp(), schema_id);
                res.sendEmpty();
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"status", e.code()},
                                 {"error", e.what()},
                                 {"data", json::object()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("Schema", "Delete Error", fmt::format("Schema delete error: {}", e.what()));
                res.sendJSON(500, {
                                 {"status", 500},
                                 {"error", "An internal error occurred."},
                                 {"data", json::object()}
                             });
            }
        };
    }
}
