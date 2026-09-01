//
// Created by codeart on 10/11/2025.
//

#include "../include/mantisbase/mantisbase.h"
#include "../include/mantisbase/core/logger/logger.h"
#include "../include/mantisbase/mantis.h"

#include <argparse/argparse.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <exception>

#include "mantisbase/core/realtime.h"
#include "mantisbase/core/models/entity_schema.h"
#include "mantisbase/core/models/validators.h"

namespace mb {
    namespace {
        LogLevel logLevelFromString(std::string level) {
            toLowerCase(level);
            if (level == "trace") return LogLevel::TRACE;
            if (level == "debug") return LogLevel::DEBUG;
            if (level == "info") return LogLevel::INFO;
            if (level == "warn" || level == "warning") return LogLevel::WARN;
            if (level == "critical" || level == "error") return LogLevel::CRITICAL;
            throw MantisException(400, std::format("Unsupported log level `{}`", level));
        }

        std::string resolveDbType(const std::optional<std::string> &cli_db) {
            const auto env_db = getEnvOrDefault("MB_DATABASE_TYPE", "");
            const auto db = !env_db.empty() ? env_db : cli_db.value_or("sqlite3");
            std::string normalized = db;
            toLowerCase(normalized);

            if (normalized == "sqlite" || normalized == "sqlite3")
                return "sqlite3";
            if (normalized == "mysql")
                return "mysql";
            if (normalized == "psql" || normalized == "postgresql" || normalized == "postgres")
                return "postgresql";

            throw MantisException(500, std::format("Backend Database `{}` is unsupported!", db));
        }

        std::string resolveDbUrl(const std::optional<std::string> &cli_url) {
            const auto env_url = getEnvOrDefault("MB_DATABASE_URL", "");
            return !env_url.empty() ? env_url : cli_url.value_or("");
        }

        void expandAdminAddFromEnv(std::vector<std::string> &args) {
            for (size_t i = 0; i + 1 < args.size(); ++i) {
                if (args[i] != "admins" || args[i + 1] != "--add")
                    continue;

                const bool has_email = i + 2 < args.size() && !args[i + 2].starts_with('-');
                if (has_email)
                    break;

                const auto email = getEnvOrDefault("MB_DEFAULT_ADMIN_EMAIL", "");
                const auto password = getEnvOrDefault("MB_DEFAULT_ADMIN_PASSWORD", "");
                if (email.empty() || password.empty()) {
                    throw MantisException(400, "admins --add requires email/password arguments or "
                                          "MB_DEFAULT_ADMIN_EMAIL/MB_DEFAULT_ADMIN_PASSWORD env vars.");
                }

                args.insert(args.begin() + static_cast<std::ptrdiff_t>(i + 2), {email, password});
                break;
            }
        }

        std::pair<json, std::string> loadJsonInput(const std::string &value) {
            if (const fs::path path = value; fs::exists(path) && fs::is_regular_file(path)) {
                std::ifstream file(path);
                if (!file.is_open())
                    throw MantisException(400, std::format("Could not open JSON file `{}`", value));

                try {
                    json body;
                    file >> body;
                    return make_pair(body, "");
                } catch (const std::exception &e) {
                    return make_pair(json::object(), e.what());
                }
            }

            return tryParseJsonStr(value);
        }

        std::string schemaIdFromNameOrId(const std::string &entity_name_or_id) {
            if (entity_name_or_id.starts_with("mbt_"))
                return entity_name_or_id;

            if (!EntitySchema::isValidEntityName(entity_name_or_id))
                throw MantisException(400, std::format("Invalid entity name `{}`", entity_name_or_id));

            return EntitySchema::genEntityId(entity_name_or_id);
        }

        void printSchemaList(MantisBase &app) {
            const auto tables = EntitySchema::listTables(app);
            std::cout << std::left
                    << std::setw(24) << "NAME"
                    << std::setw(10) << "TYPE"
                    << std::setw(8) << "SYSTEM"
                    << std::setw(8) << "HAS_API"
                    << "ID"
                    << std::endl;

            for (const auto &row: tables) {
                const auto &schema = row.at("schema");
                std::cout << std::left
                        << std::setw(24) << schema.value("name", "")
                        << std::setw(10) << schema.value("type", "")
                        << std::setw(8) << (schema.value("system", false) ? "true" : "false")
                        << std::setw(8) << (schema.value("has_api", true) ? "true" : "false")
                        << row.at("id").get<std::string>()
                        << std::endl;
            }
        }

        void runMigrationsUp(MantisBase &app) {
            const auto dir = fs::path(app.migrationsDir());
            if (!fs::exists(dir)) {
                app.logger().info("Migrations",
                                  fmt::format("No migrations directory at `{}`, nothing to apply.", dir.string()));
                return;
            }

            std::vector<fs::path> files;
            for (const auto &entry: fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json")
                    files.push_back(entry.path());
            }

            std::ranges::sort(files);
            if (files.empty()) {
                app.logger().info("Migrations", "No migration JSON files found.");
                return;
            }

            for (const auto &file: files) {
                app.logger().info("Migrations", fmt::format("Applying migration `{}`", file.filename().string()));
                const auto [body, parse_err] = loadJsonInput(file.string());

                if (!parse_err.empty()) {
                    app.logger().info("Migrations", "Migration Error", parse_err);
                    app.quit(500);
                }

                auto eSchema = EntitySchema::fromSchema(app, body);
                if (const auto err = eSchema.validate(); err.has_value())
                    throw MantisException(400, err.value());

                EntitySchema::createTable(eSchema);
            }
        }

        void runMigrationsDown(const MantisBase &app) {
            app.logger().warn("Migrations", "Migration rollback (`--down`) is not implemented yet.");
        }

        int countExclusiveFlags(const std::initializer_list<bool> flags) {
            return static_cast<int>(std::ranges::count(flags, true));
        }

        void runMigrateDump(MantisBase &app, const std::string &output_path) {
            fs::path dump_path(output_path);

            const auto tables = EntitySchema::listTables(app);
            json dump = json::array();

            for (const auto &row: tables) {
                const auto &schema = row.at("schema");
                if (schema.value("system", false))
                    continue;

                dump.push_back({
                    {"schema", schema}
                });
            }

            std::ofstream out(dump_path);
            if (!out.is_open())
                throw MantisException(500,
                    std::format("Could not open output file `{}` for migration dump", dump_path.string()));

            out << dump.dump(2);
            out.close();

            app.logger().info("Migrate", fmt::format("Dumped {} entity schemas to `{}`",
                                                     dump.size(), output_path));
        }

        void runMigrateUp(MantisBase &app, const std::string &input_path) {
            const fs::path path = input_path;
            if (!fs::exists(path) || !fs::is_regular_file(path))
                throw MantisException(400, std::format("Dump file `{}` does not exist", input_path));

            std::ifstream in(path);
            if (!in.is_open())
                throw MantisException(500, std::format("Could not open dump file `{}`", input_path));

            json dump;
            in >> dump;
            in.close();

            if (!dump.is_array())
                throw MantisException(400, "Dump file must contain a JSON array");

            // Separate base/auth entities from view entities for dependency ordering
            std::vector<json> base_entities;
            std::vector<json> view_entities;

            for (const auto &entry: dump) {
                if (!entry.contains("schema"))
                    throw MantisException(400, "Each dump entry must contain a `schema` key");

                const auto &schema = entry.at("schema");
                const auto type = schema.value("type", "base");
                if (type == "view")
                    view_entities.push_back(schema);
                else
                    base_entities.push_back(schema);
            }

            int restored = 0;

            // Restore base/auth entities first
            for (const auto &schema: base_entities) {
                const auto name = schema.value("name", "");
                if (EntitySchema::tableExists(app, name)) {
                    app.logger().info("Migrate", fmt::format("Skipping existing entity `{}`", name));
                    continue;
                }

                const auto eSchema = EntitySchema::fromSchema(app, schema);
                if (const auto err = eSchema.validate(); err.has_value())
                    throw MantisException(400, std::format("Validation failed for `{}`: {}", name, err.value()));

                EntitySchema::createTable(eSchema);
                app.logger().info("Migrate", fmt::format("Restored entity `{}`", name));
                ++restored;
            }

            // Then restore view entities
            for (const auto &schema: view_entities) {
                const auto name = schema.value("name", "");
                if (EntitySchema::tableExists(app, name)) {
                    app.logger().info("Migrate", fmt::format("Skipping existing entity `{}`", name));
                    continue;
                }

                const auto eSchema = EntitySchema::fromSchema(app, schema);
                if (const auto err = eSchema.validate(); err.has_value())
                    throw MantisException(400, std::format("Validation failed for `{}`: {}", name, err.value()));

                EntitySchema::createTable(eSchema);
                app.logger().info("Migrate", fmt::format("Restored view entity `{}`", name));
                ++restored;
            }

            app.logger().info("Migrate", fmt::format("Restored {} entity schemas from `{}`",
                                                     restored, input_path));
        }
    }

    void MantisBase::parseArgs() {
        expandAdminAddFromEnv(m_cmdArgs);

        argparse::ArgumentParser program("mantisbase", appVersion());
        program.add_description("MantisBase backend CLI");

        program.add_argument("--migrations-dir", "--migrationsDir")
                .nargs(1)
                .metavar("PATH")
                .help("Migrations directory (default: ./migrations)");
        program.add_argument("--data-dir", "--dataDir")
                .nargs(1)
                .metavar("PATH")
                .help("Data directory (default: ./data)");
        program.add_argument("--public-dir", "--publicDir")
                .nargs(1)
                .metavar("PATH")
                .help("Static files directory (default: ./public)");
        program.add_argument("--scripts-dir", "--scriptsDir")
                .nargs(1)
                .metavar("PATH")
                .help("JavaScript scripts directory (default: ./scripts)");
        program.add_argument("--dev")
                .flag()
                .help("Enable verbose development logging (overridden by MB_LOG_LEVEL)");
#ifdef MB_SCRIPTING_ENABLED
        program.add_argument("--disable-scripting", "--no-scripting")
                .flag()
                .help("Disable JavaScript scripting at runtime (also MB_SCRIPTING_DISABLED=1)");
#endif
        program.add_argument("--db")
                .nargs(1)
                .metavar("TYPE")
                .help("Database type: sqlite3, postgresql, mysql (overridden by MB_DATABASE_TYPE)");
        program.add_argument("--db-url")
                .nargs(1)
                .metavar("URL")
                .help("Database connection URL (overridden by MB_DATABASE_URL)");

        argparse::ArgumentParser serve_command("serve");
        serve_command.add_description("Start the HTTP server");
        serve_command.add_argument("--host")
                .default_value(std::string("0.0.0.0"))
                .help("Server host address (default: 0.0.0.0)");
        serve_command.add_argument("--port")
                .default_value(7070)
                .scan<'i', int>()
                .help("Server port (default: 7070)");
        serve_command.add_argument("--skip-admin-setup")
                .flag()
                .help("Skip first-boot admin browser setup (also MB_SKIP_ADMIN_SETUP=1)");
        serve_command.add_argument("--pool-size", "--poolSize")
                .scan<'i', int>()
                .help("Database connection pool size (default: 4 for sqlite3, 10 for postgresql)");

        // `mantisbase admin` subcommand
        argparse::ArgumentParser admins_command("admins");
        admins_command.add_description("Manage admin accounts");
        admins_command.add_argument("--add")
                .nargs(2)
                .help(
                    "Add admin account (or use MB_DEFAULT_ADMIN_EMAIL/PASSWORD with `--add` alone via env expansion)");
        admins_command.add_argument("--ls")
                .flag()
                .help("List admin accounts");
        admins_command.add_argument("--rm")
                .nargs(1)
                .metavar("IDENTIFIER")
                .help("Remove admin by email or id");

        // `mantisbase schema [OPTIONS]` subcommand
        argparse::ArgumentParser schema_command("schema");
        schema_command.add_description("Manage entity schemas");
        schema_command.add_argument("--ls")
                .flag()
                .help("List entity schemas");
        schema_command.add_argument("--rm")
                .nargs(1)
                .metavar("ENTITY")
                .help("Remove an entity schema");
        schema_command.add_argument("--add")
                .nargs(1)
                .metavar("JSON_OR_FILE")
                .help("Create schema from JSON string or file path");
        schema_command.add_argument("--update")
                .nargs(2)
                .help("Update schema from JSON string or file path");

        // `mantisbase migrate [subcommand]`
        argparse::ArgumentParser migrate_schema_command("schema");
        migrate_schema_command.add_description("Dump or restore entity schema");
        migrate_schema_command.add_argument("--to")
                .nargs(1)
                .metavar("FILE")
                .help("Dump all entity schemas to a JSON file");
        migrate_schema_command.add_argument("--from")
                .nargs(1)
                .metavar("FILE")
                .help("Restore entity schemas from a JSON dump file");

        // `mantisbase migrate apply [--up/--down]`
        argparse::ArgumentParser migrations_command("apply");
        migrations_command.add_description("Apply or rollback database migrations");
        migrations_command.add_argument("--up")
                .flag()
                .help("Apply migrations from the migrations directory");
        migrations_command.add_argument("--down")
                .flag()
                .help("Rollback migrations (not yet implemented)");

        // `mantisbase migrate [subcommand]`
        argparse::ArgumentParser migrate_command("migrate");
        migrate_command.add_description("Handle migration and schema ops");

        // Migrate subcommand parsers
        migrate_command.add_subparser(migrations_command);
        migrate_command.add_subparser(migrate_schema_command);

        // Program sub parsers
        program.add_subparser(serve_command);
        program.add_subparser(admins_command);
        program.add_subparser(schema_command);
        program.add_subparser(migrate_command);

        try {
            std::vector<const char *> argv;
            argv.reserve(m_cmdArgs.size());
            for (const auto &arg: m_cmdArgs)
                argv.push_back(arg.c_str());

            program.parse_args(static_cast<int>(argv.size()), argv.data());
        } catch (const std::exception &err) {
            std::cerr << std::endl << err.what() << std::endl;
            std::cout << program << std::endl;
            quit(500, err.what());
        }

        const auto db_type = resolveDbType(program.present<std::string>("--db"));
        const auto conn_string = resolveDbUrl(program.present<std::string>("--db-url"));
        const auto _dataDir = program.present<std::string>("--data-dir").value_or("data");
        const auto _pubDir = program.present<std::string>("--public-dir").value_or("public");
        const auto _scriptsDir = program.present<std::string>("--scripts-dir").value_or("scripts");
        const auto _migrationsDir = program.present<std::string>("--migrations-dir").value_or("migrations");
        const auto dev_flag = program.get<bool>("--dev");
#ifdef MB_SCRIPTING_ENABLED
        m_scriptingDisabled = program.get<bool>("--disable-scripting");
#endif

        const auto env_level = getEnvOrDefault("MB_LOG_LEVEL", "");
        if (!env_level.empty()) {
            Logger::setLogLevel(logLevelFromString(env_level));
            const auto level = logLevelFromString(env_level);
            if (level == LogLevel::TRACE || level == LogLevel::DEBUG)
                m_isDevMode = true;
        } else if (dev_flag) {
            Logger::setLogLevel(LogLevel::TRACE);
            m_isDevMode = true;
        }

        const auto pub_dir = dirFromPath(_pubDir);
        setPublicDir(pub_dir.empty() ? dirFromPath("public") : pub_dir);

        const auto data_dir = dirFromPath(_dataDir);
        setDataDir(data_dir.empty() ? dirFromPath("data") : data_dir);

        const auto scripts_dir = dirFromPath(_scriptsDir);
        setScriptsDir(scripts_dir.empty() ? dirFromPath("scripts") : scripts_dir);

        const auto migrations_dir = dirFromPath(_migrationsDir);
        setMigrationsDir(migrations_dir.empty() ? dirFromPath("migrations") : migrations_dir);

        // Start logger database thread
        logger().initDb(dataDir());

        // Initialize all modules and start them
        init_units();

        // Set database type
        setDbType(db_type);

        // Set pool size for the db pool
        int pool_size = m_dbType == "sqlite3" ? 4 : 10;
        if (program.is_subcommand_used("serve") && serve_command.is_used("--pool-size")) {
            pool_size = serve_command.get<int>("--pool-size");
        }
        setPoolSize(pool_size);

        // Check if database is connected
        if (!m_database->connect(conn_string)) {
            quit(500, "Database connection failed, exiting!");
        }

        //Check that system tables were created
        if (!m_database->createSysTables()) {
            quit(500, "Database migration failed, exiting!");
        }

        // Check that database connection was established
        if (!m_database->isConnected()) {
            logger().critical("Database", "Database Not Opened", "Database was not opened!");
            quit(500, "Database opening failed!");
        }

        // Check that realtime database was started
        if (!m_realtime->init()) {
            logger().critical("Database", "Database Not Opened", "Realtime Db Mgr failed to instantiate.");
            quit(500, "Realtime Db Mgr failed to instantiate.");
        }

        // Check that the router was initialized
        if (!m_router->init()) {
            quit(500, "Failed to initialize router!");
        }

        // Run settings migrations
        settings().migrate();

        // If we passed the `serve` sub command, set its options
        if (program.is_subcommand_used("serve")) {
            setHost(serve_command.get<std::string>("--host"));
            setPort(serve_command.get<int>("--port"));
            if (serve_command.get<bool>("--skip-admin-setup"))
                setSkipAdminSetup(true);
            m_toStartServer = true;
            return;
        }

        if (program.is_subcommand_used("admins")) {
            const bool do_add = admins_command.is_used("--add");
            const bool do_ls = admins_command.get<bool>("--ls");
            const bool do_rm = admins_command.is_used("--rm");

            if (countExclusiveFlags({do_add, do_ls, do_rm}) != 1) {
                quit(400, "admins requires exactly one of --add, --ls, or --rm.");
            }

            auto admin_entity = entity("mb_admins");

            if (do_add) {
                const auto creds = admins_command.get<std::vector<std::string> >("--add");
                std::string email = creds.at(0);
                std::string password = creds.at(1);

                if (const auto val_err = Validators::validatePreset("email", email); val_err.has_value()) {
                    logger().critical("Auth", "Email Validation Failed",
                                      fmt::format("Error validating admin email: {}", val_err.value()));
                    quit(-1, "Email validation failed!");
                }
                if (const auto val_pswd_err = Validators::validatePreset("password", password);
                    val_pswd_err.has_value()) {
                    logger().critical("Auth", "Password Validation Failed",
                                      fmt::format("Error validating password: {}", val_pswd_err.value()));
                    quit(-1, "Password validation failed!");
                }

                try {
                    const auto admin_user = admin_entity.create({{"email", email}, {"password", password}});
                    logger().info("Auth", "Admin Created", fmt::format(
                                      "Admin account created, use '{}' to access the `/mb` dashboard.",
                                      admin_user.at("email").get<std::string>()));
                    quit(0, "");
                } catch (const std::exception &e) {
                    logger().critical("Auth", "Admin Creation Failed",
                                      fmt::format("Failed to created Admin user: {}", e.what()));
                    quit(500, e.what());
                }
            }

            if (do_ls) {
                const auto admins = admin_entity.list();
                std::cout << std::left << std::setw(40) << "ID" << "EMAIL" << std::endl;
                for (const auto &admin: admins) {
                    std::cout << std::left << std::setw(40) << admin.value("id", "")
                            << admin.value("email", "")
                            << std::endl;
                }
                quit(0, "");
            }

            if (do_rm) {
                const auto identifier = admins_command.get<std::string>("--rm");
                if (identifier.empty()) {
                    logger().critical("Auth", "Invalid Admin Identifier", "Invalid admin `email` or `id` provided!");
                    quit(400, "");
                }

                auto resp = admin_entity.queryFromCols(identifier, {"id", "email"});
                if (!resp.has_value()) {
                    logger().critical("Auth", "Admin Not Found",
                                      fmt::format("Admin not found matching id/email on '{}'", identifier));
                    quit(404, "");
                }

                try {
                    admin_entity.remove(resp.value().at("id").get<std::string>());
                    logger().info("Auth", "Admin Removed", "Admin removed successfully.");
                    quit(0, "");
                } catch (const std::exception &e) {
                    logger().critical("Auth", "Admin Removal Failed",
                                      fmt::format("Failed to remove admin account: {}", e.what()));
                    quit(500, e.what());
                }
            }
        }

        if (program.is_subcommand_used("schema")) {
            const bool do_ls = schema_command.get<bool>("--ls");
            const bool do_rm = schema_command.is_used("--rm");
            const bool do_add = schema_command.is_used("--add");
            const bool do_update = schema_command.is_used("--update");

            if (countExclusiveFlags({do_ls, do_rm, do_add, do_update}) != 1) {
                quit(400, "schema requires exactly one of --ls, --rm, --add, or --update.");
            }

            try {
                if (do_ls) {
                    printSchemaList(*this);
                    quit(0);
                }

                if (do_rm) {
                    const auto entity_name = schema_command.get<std::string>("--rm");
                    const auto schema_id = schemaIdFromNameOrId(entity_name);
                    EntitySchema::dropTable(*this, schema_id);
                    logger().info("Schema",
                        fmt::format("Removed schema `{}`", entity_name));
                    quit(0);
                }

                if (do_add) {
                    const auto [body, parse_err] = loadJsonInput(schema_command.get<std::string>("--add"));

                    if (!parse_err.empty()) {
                        throw MantisException(400, std::format("Failed to parse schema: {}", parse_err));
                    }

                    auto eSchema = EntitySchema::fromSchema(*this, body);
                    if (const auto err = eSchema.validate(); err.has_value())
                        throw MantisException(400, err.value());

                    const auto created = EntitySchema::createTable(eSchema);
                    std::cout << created.dump(2) << std::endl;
                    quit(0, "");
                }

                if (do_update) {
                    const auto parts = schema_command.get<std::vector<std::string> >("--update");

                    // Extract and validate entity name
                    const auto& entity_name = parts.at(0);
                    if (entity_name.empty() || !EntitySchema::isValidEntityName(entity_name)) {
                        throw MantisException(400, "Invalid entity name");
                    }

                    // Extract and validate the JSON body
                    if (parts.at(1).empty()) {
                        throw MantisException(400, "Cannot update entity with an empty schema!");
                    }

                    const auto [body, parse_err] = loadJsonInput(parts.at(1));
                    if (!parse_err.empty()) {
                        throw MantisException(400,
                            std::format("Failed to parse update schema: {}", parse_err));
                    }

                    const auto schema_id = schemaIdFromNameOrId(entity_name);
                    const auto updated = EntitySchema::updateTable(*this, schema_id, body);
                    std::cout << updated.dump(2) << std::endl;
                    quit(0);
                }
            } catch (const MantisException &e) {
                quit(e.code(), e.what());
            } catch (const json::parse_error &e) {
                quit(400, std::string("Invalid JSON: ") + e.what());
            } catch (const std::exception &e) {
                quit(500, e.what());
            }
        }

        // `migrate` takes either `migrate apply` or `migrate schema`
        if (program.is_subcommand_used("migrate")) {
            //Check if `apply` subcommand was invoked
            if (migrate_command.is_subcommand_used("apply")) {
                const bool up = migrations_command.get<bool>("--up");
                const bool down = migrations_command.get<bool>("--down");

                if (countExclusiveFlags({up, down}) != 1) {
                    quit(400, "`migrate apply` requires exactly one of --up or --down.");
                }

                try {
                    if (up)
                        runMigrationsUp(*this);
                    else
                        runMigrationsDown(*this);
                    quit(0, "");
                } catch (const MantisException &e) {
                    quit(e.code(), e.what());
                } catch (const std::exception &e) {
                    quit(500, e.what());
                }
            }

            //Check if `schema` subcommand was invoked
            if (migrate_command.is_subcommand_used("schema")) {
                const bool do_dump = migrate_schema_command.is_used("--to");
                const bool do_load = migrate_schema_command.is_used("--from");

                if (countExclusiveFlags({do_dump, do_load}) != 1) {
                    quit(400, "`migrate schema` requires exactly one of --to <file> or --from <file>.");
                }

                try {
                    if (do_dump)
                        runMigrateDump(*this, migrate_schema_command.get<std::string>("--to"));
                    else if (do_load)
                        runMigrateUp(*this, migrate_schema_command.get<std::string>("--from"));
                    else
                        throw MantisException(400,
                            "`migrate schema` requires exactly one of --to <file> or --from <file>.");

                    quit(0);
                } catch (const MantisException &e) {
                    quit(e.code(), e.what());
                } catch (const json::parse_error &e) {
                    quit(400, std::string("Invalid JSON in dump file: ") + e.what());
                } catch (const std::exception &e) {
                    quit(500, e.what());
                }
            }

            else {
                throw MantisException(400, "`migrate` command expected `apply` or `schema` subcommand!");
            }
        }

        logger().info("", "No subcommand specified. Try: "
                          "\n\t - `mantisbase --help` to see all available commands."
                          "\n\t - `mantisbase serve` to start http server"
                          "\n\t - `mantisbase admins [options]` for admin account management");
        quit(0);
    }

    bool MantisBase::isCreated() const {
        return m_isCreated.load();
    }
}
