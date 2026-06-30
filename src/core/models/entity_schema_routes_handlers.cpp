#include "../../include/mantisbase/core/models/entity_schema_routes.h"
#include "../../include/mantisbase/core/models/entity_schema.h"
#include "../../include/mantisbase/mantisbase.h"

namespace mb {
    namespace {
        std::string schemaIdFromPathParam(const std::string &schema_id_or_name) {
            if (schema_id_or_name.starts_with("mbt_"))
                return schema_id_or_name;

            if (!EntitySchema::isValidEntityName(schema_id_or_name))
                throw MantisException(400, "Invalid entity name/id");

            return EntitySchema::genEntityId(schema_id_or_name);
        }
    }

    HandlerFn schemaGetOneHandler() {
        return [](const MantisRequest &req, const MantisResponse &res) {
            try {
                const auto schema_id_or_name = trim(req.getPathParamValue("schema_name_or_id"));
                if (schema_id_or_name.empty())
                    throw MantisException(400, "EntitySchema `id` or `name` is required on the route!");

                const auto schema_id = schemaIdFromPathParam(schema_id_or_name);
                const auto record = EntitySchema::getTable(schema_id);

                res.sendJSON(200, {
                    {"data", record},
                    {"error", ""},
                    {"status", 200}
                });
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                    {"data", json::object()},
                    {"error", e.what()},
                    {"status", e.code()}
                });
            } catch (const std::exception &e) {
                res.sendJSON(500, {
                    {"data", json::object()},
                    {"error", e.what()},
                    {"status", 500}
                });
            }
        };
    }

    HandlerFn schemaGetManyHandler() {
        return [](MantisRequest &, MantisResponse &res) {
            try {
                const auto tables = EntitySchema::listTables();
                res.sendJSON(200, {
                    {"data", tables},
                    {"error", ""},
                    {"status", 200}
                });
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                    {"data", json::object()},
                    {"error", e.what()},
                    {"status", e.code()}
                });
            } catch (const std::exception &e) {
                res.sendJSON(500, {
                    {"data", json::object()},
                    {"error", e.what()},
                    {"status", 500}
                });
            }
        };
    }

    HandlerFn schemaPostHandler() {
        return [](const MantisRequest &req, const MantisResponse &res) {
            try {
                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {
                        {"data", json::object()},
                        {"error", err},
                        {"status", 400}
                    });
                    return;
                }

                const auto eSchema = EntitySchema::fromSchema(body);
                auto _ = eSchema.dump();

                if (const auto val_err = eSchema.validate(); val_err.has_value())
                    throw MantisException(400, val_err.value());

                auto _schema = EntitySchema::createTable(eSchema);
                res.sendJSON(201, {
                    {"data", _schema},
                    {"error", ""},
                    {"status", 201}
                });
            } catch (const MantisException &e) {
                LogOrigin::entitySchemaCritical("Schema Creation Error",
                                                fmt::format("Error creating entity schema\n\t— {}", e.what()));
                res.sendJSON(e.code(), {
                    {"data", json::object()},
                    {"error", e.what()},
                    {"status", e.code()}
                });
            } catch (const std::exception &e) {
                LogOrigin::entitySchemaCritical("Schema Creation Error",
                                                fmt::format("Error creating entity schema\n\t— {}", e.what()));
                res.sendJSON(500, {
                    {"data", json::object()},
                    {"error", e.what()},
                    {"status", 500}
                });
            }
        };
    }

    HandlerFn schemaPatchHandler() {
        return [](MantisRequest &req, MantisResponse &res) {
            try {
                const auto schema_id_or_name = trim(req.getPathParamValue("schema_name_or_id"));
                if (schema_id_or_name.empty())
                    throw MantisException(400, "EntitySchema `id` or `name` is required on the route!");

                const auto schema_id = schemaIdFromPathParam(schema_id_or_name);

                const auto &[body, err] = req.getBodyAsJson();
                if (!err.empty()) {
                    res.sendJSON(400, {
                        {"data", json::object()},
                        {"error", err},
                        {"status", 400}
                    });
                    return;
                }

                auto _schema = EntitySchema::updateTable(schema_id, body);
                res.sendJSON(200, {
                    {"data", _schema},
                    {"error", ""},
                    {"status", 200}
                });
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                    {"data", json::object()},
                    {"error", e.what()},
                    {"status", e.code()}
                });
            } catch (const std::exception &e) {
                res.sendJSON(500, {
                    {"data", json::object()},
                    {"error", e.what()},
                    {"status", 500}
                });
            }
        };
    }

    HandlerFn schemaDeleteHandler() {
        return [](const MantisRequest &req, const MantisResponse &res) {
            try {
                const auto schema_id_or_name = trim(req.getPathParamValue("schema_name_or_id"));
                if (schema_id_or_name.empty())
                    throw MantisException(400, "EntitySchema `id` or `name` is required on the route!");

                const auto schema_id = schemaIdFromPathParam(schema_id_or_name);
                EntitySchema::dropTable(schema_id);
                res.sendEmpty();
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                    {"status", e.code()},
                    {"error", e.what()},
                    {"data", json::object()}
                });
            } catch (const std::exception &e) {
                res.sendJSON(500, {
                    {"status", 500},
                    {"error", e.what()},
                    {"data", json::object()}
                });
            }
        };
    }
}

