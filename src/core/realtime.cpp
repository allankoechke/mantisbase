#include "../../include/mantisbase/core/realtime.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/exceptions.h"

mb::RealtimeDB::RealtimeDB() : mApp(mb::MantisBase::instance()) {}

void mb::RealtimeDB::init() const {
    const auto sql = mApp.db().session();

    // Create rt changelog table
    *sql << R"(
        CREATE TABLE IF NOT EXISTS mb_change_log (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp   DATETIME DEFAULT CURRENT_TIMESTAMP,
            type        TEXT NOT NULL,
            entity      TEXT NOT NULL,
            row_id      TEXT NOT NULL,
            old_data    TEXT,
            new_data    TEXT,
        )
    )";
}

void mb::RealtimeDB::addDbHooks(const std::string &entity_name) {
    if (!mApp.hasEntity(entity_name)) {
        throw MantisException(400, std::format("Expected a valid existing entity name, but `{}` was given instead.",
                                               entity_name));
    }

    // First drop any existing hooks
    dropDbHooks(entity_name);

    // Get entity object
    const auto entity = mApp.entity(entity_name);
    auto old_obj = buildTriggerObject(entity, "OLD");
    auto new_obj = buildTriggerObject(entity, "NEW");

    // Get session instance
    const auto sql = mApp.db().session();

    // Create INSERT trigger
    *sql << std::format(
                "CREATE TRIGGER mb_{}_insert_trigger AFTER INSERT ON {} "
                "BEGIN "
                " INSERT INTO mb_change_log(type, entity, row_id, old_data, new_data) "
                " VALUES ('INSERT', :entity, NEW.id, :new_data) "
                "END", entity_name, entity_name
            ), soci::use(entity_name), soci::use(new_obj);

    // Create UPDATE trigger
    *sql << std::format(
                "CREATE TRIGGER mb_{}_update_trigger AFTER UPDATE ON {} "
                "BEGIN "
                " INSERT INTO mb_change_log(type, entity, row_id, old_data, new_data) "
                " VALUES ('UPDATE', :entity, NEW.id, :old_data, :new_data) "
                "END", entity_name, entity_name
            ), soci::use(entity_name), soci::use(old_obj), soci::use(new_obj);

    // Create DELETE trigger
    *sql << std::format(
                "CREATE TRIGGER mb_{}_delete_trigger AFTER DELETE ON {} "
                "BEGIN "
                " INSERT INTO mb_change_log(type, entity, row_id, old_data) "
                " VALUES ('DELETE', :entity, OLD.id, :old_data) "
                "END", entity_name, entity_name
            ), soci::use(entity_name), soci::use(old_obj);
}

void mb::RealtimeDB::dropDbHooks(const std::string &entity_name) {
    if (!EntitySchema::isValidEntityName(entity_name)) {
        throw MantisException(400, "Invalid Entity name.");
    }

    const auto sql = mApp.db().session();
    *sql << std::format("DROP TRIGGER IF EXISTS mb_{}_insert_trigger", entity_name);
    *sql << std::format("DROP TRIGGER IF EXISTS mb_{}_update_trigger", entity_name);
    *sql << std::format("DROP TRIGGER IF EXISTS mb_{}_delete_trigger", entity_name);
}

std::string mb::RealtimeDB::buildTriggerObject(const Entity &entity, const std::string &action) {
    std::stringstream ss;
    ss << "json_object(";
    bool first = true;
    for (const auto &field: entity.fields()) {
        auto name = field["name"].get<std::string>();
        if (!first) {
            ss << ", ";
            first = false;
        }
        ss << "'" << name << "', " << action << "." << name;
    }
    ss << ")";

    return ss.str();
}
