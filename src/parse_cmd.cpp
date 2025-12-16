//
// Created by codeart on 10/11/2025.
//

#include "../include/mantisbase/mantisbase.h"
#include "../include/mantisbase/core/logger.h"
#include "../include/mantisbase/mantis.h"

#include <argparse/argparse.hpp>

#include "mantisbase/core/models/validators.h"

namespace mb {
    void MantisBase::parseArgs() {
        // Main program parser with global arguments
        argparse::ArgumentParser program("mantisbase", appVersion());
        program.add_argument("--database", "-d")
                .nargs(1)
                .help("<type> Database type ['SQLITE', 'PSQL', 'MYSQL'] (default: SQLITE)");
        program.add_argument("--connection", "-c")
                .nargs(1)
                .help("<conn> Database connection string.");
        program.add_argument("--dataDir")
                .nargs(1)
                .help("<dir> Data directory (default: ./data)");
        program.add_argument("--publicDir")
                .nargs(1)
                .help("<dir> Static files directory (default: ./public).");
        program.add_argument("--scriptsDir")
                .nargs(1)
                .help("<dir> JS script files directory (default: ./scripts).");
        program.add_argument("--dev").flag();

        // Serve subcommand
        argparse::ArgumentParser serve_command("serve");
        serve_command.add_argument("--port", "-p")
                .default_value(7070)
                .scan<'i', int>()
                .help("<port> Server Port (default: 7070)");
        serve_command.add_argument("--host", "-h")
                .nargs(1)
                .default_value("0.0.0.0")
                .help("<host> Server Host (default: 0.0.0.0)");
        serve_command.add_argument("--poolSize")
                .scan<'i', int>()
                .help("<pool size> Size of database connection pools >= 1");

        // Admins subcommand with nested subcommands
        // admins add user@doe.com 123456789
        // admins rm user@doe.com  # by email
        // admins rm 7r6r656-896-8y97587-897AB7587 # by ID
        argparse::ArgumentParser admins_command("admins");
        admins_command.add_description("Admin accounts management commands");

        // Create the 'add' subcommand
        argparse::ArgumentParser add_command("add");
        add_command.add_description("Add a new admin");
        add_command.add_argument("email")
                .help("Admin email address");
        add_command.add_argument("password")
                .help("Admin password");

        // Create the 'rm' subcommand
        argparse::ArgumentParser rm_command("rm");
        rm_command.add_description("Remove an admin");
        rm_command.add_argument("identifier")
                .help("Admin email or GUID");

        // Add subcommands to admins
        admins_command.add_subparser(add_command);
        admins_command.add_subparser(rm_command);

        // Migrations subcommand with nested subcommands
        argparse::ArgumentParser migrate_command("migrate");
        migrate_command.add_description("Migration management commands");

        // Create the 'load' subcommand
        argparse::ArgumentParser load_command("load");
        load_command.add_description("Load admin data from file");
        load_command.add_argument("file")
                .help("File to load (json or zip format)");

        // Create the 'create' subcommand
        argparse::ArgumentParser create_command("create");
        create_command.add_description("Create new admin");
        create_command.add_argument("filename")
                .help("Optional filename for output")
                .nargs(argparse::nargs_pattern::optional);

        // Add subcommands to admins
        migrate_command.add_subparser(load_command);
        migrate_command.add_subparser(create_command);

        // Add main subparsers
        program.add_subparser(serve_command);
        program.add_subparser(admins_command);
        program.add_subparser(migrate_command);

        try {
            // Create a vector of `const char*` pointing to the owned `std::string`s
            std::vector<const char *> argv;
            argv.reserve(m_cmdArgs.size());
            for (const auto &arg: m_cmdArgs) {
                argv.push_back(arg.c_str());
            }

            // Parse safely — strings are now owned by `m_cmdArgs`
            program.parse_args(static_cast<int>(argv.size()), argv.data());
        } catch (const std::exception &err) {
            std::cerr << err.what() << std::endl;
            std::cout << program << std::endl;
            quit(500, err.what());
        }

        // Get main program args
        auto db = program.present<std::string>("--database").value_or("sqlite");
        const auto connString = program.present<std::string>("--connection").value_or("");
        const auto dataDir = program.present<std::string>("--dataDir").value_or("data");
        const auto pubDir = program.present<std::string>("--publicDir").value_or("public");
        const auto scriptsDir = program.present<std::string>("--scriptsDir").value_or("scripts");

        // Set trace mode if flag is set
        if (program.get<bool>("--dev")) {
            // Print developer messages - set it to trace for now
            logger::setLogLevel(LogLevel::TRACE);
            m_isDevMode = true;
        }

        // If directory paths are not valid, we default back to the
        // default directory for the respective items (`public`, `data` and `scripts`)
        // relative to the application binary.
        const auto pub_dir = dirFromPath(pubDir);
        setPublicDir(pub_dir.empty() ? dirFromPath("public") : pub_dir);

        const auto data_dir = dirFromPath(dataDir);
        setDataDir(data_dir.empty() ? dirFromPath("data") : data_dir);

        const auto scripts_dir = dirFromPath(scriptsDir);
        setScriptsDir(scripts_dir.empty() ? dirFromPath("scripts") : scripts_dir);

        // Ensure objects are first created, taking into account the cmd args passed in
        // esp. the directory paths
        init_units();

        // Convert db type to lowercase and set the db type
        toLowerCase(db);
        if (db == "sqlite" || db == "sqlite3") {
            setDbType("sqlite3");
        } else if (db == "mysql") {
            setDbType(db);
        } else if (db == "psql" || db == "postgresql" || db == "postgres") {
            setDbType("postgresql");
        } else {
            quit(-1, std::format("Backend Database `{}` is unsupported!", db));
        }

        // Initialize database connection & Migration
        if (!m_database->connect(connString)) {
            // Connection to database failed
            quit(-1, "Database connection failed, exiting!");
        }
        if (!m_database->createSysTables()) {
            quit(-1, "Database migration failed, exiting!");
        }

        if (!m_database->isConnected()) {
            logger::critical("Database was not opened!");
            quit(-1, "Database opening failed!");
        }

        // Initialize router here to ensure schemas are loaded
        if (!m_router->initialize())
            quit(-1, "Failed to initialize router!");

        // Check which commands were used
        if (program.is_subcommand_used("serve")) {
            const auto host = serve_command.get<std::string>("--host");
            const auto port = serve_command.get<int>("--port");

            int default_pool_size = m_dbType == "sqlite3" ? 4 : 10;
            const auto pools = serve_command.present<int>("--poolSize").value_or(default_pool_size);

            setHost(host);
            setPort(port);
            setPoolSize(pools > 0 ? pools : 1);

            // Set the serve flag to true, will be checked later before
            // running the listen on port & host above.
            m_toStartServer = true;
        } else if (program.is_subcommand_used("admins")) {
            // Check which subcommand was used
            if (admins_command.is_subcommand_used("add")) {
                std::string email = add_command.get("email");
                std::string password = add_command.get("password");

                if (const auto val_err = Validators::validatePreset("email", email);
                    val_err.has_value()) {
                    logger::critical("Error validating admin email: {}", val_err.value());
                    quit(-1, "Email validation failed!");
                }

                if (const auto val_pswd_err = Validators::validatePreset("password", password);
                    val_pswd_err.has_value()) {
                    logger::critical("Error validating email: {}", val_pswd_err.value());
                    quit(-1, "Email validation failed!");
                }

                try {
                    auto admin_entity = entity("mb_admins");

                    // Create new admin user
                    const auto admin_user = admin_entity.create({{"email", email}, {"password", password}});

                    // Admin User was created!
                    logger::info("Admin account created, use '{}' to access the `/admin` dashboard.",
                                 admin_user.at("email").get<std::string>());
                    quit(0, "");
                } catch (const std::exception &e) {
                    logger::critical("Failed to created Admin user: {}", e.what());
                    quit(500, e.what());
                }
            } else if (admins_command.is_subcommand_used("rm")) {
                std::string identifier = rm_command.get("identifier");
                // Process rm command - identifier could be email or GUID
                if (identifier.empty()) {
                    logger::critical("Invalid admin `email` or `id` provided!");
                    quit(400, "");
                }

                auto admin_entity = entity("mb_admins");
                auto resp = admin_entity.queryFromCols(identifier, {"id", "email"});
                if (!resp.has_value()) {
                    logger::critical("Admin not found matching id/email on '{}'",
                                     identifier);
                    quit(404, "");
                }

                try {
                    admin_entity.remove(resp.value().at("id").get<std::string>());
                    logger::info("Admin removed successfully.");
                    quit(0, "");
                } catch (const std::exception &e) {
                    logger::critical("Failed to remove admin account: {}", e.what());
                    quit(500, e.what());
                }
            } else {
                std::cout << std::endl << "Unknown arguments to `admins` subcommand.\n\n" << admins_command << std::endl;
            }
        } else if (program.is_subcommand_used("migrate")) {
            // Do migration stuff here
            logger::info("Migration CMD support has not been implemented yet! ");

            if (admins_command.is_subcommand_used("load")) {
                std::string file = load_command.get("file");
                // Process load command
            } else if (admins_command.is_subcommand_used("create")) {
                bool use_json = create_command.get<bool>("--json");
                bool use_zip = create_command.get<bool>("--zip");

                if (create_command.is_used("filename")) {
                    std::string filename = create_command.get("filename");
                    // Process with filename
                }
                // Process create command
            }
        } else {
            std::cout << "Unknown command, try one of the following:\n" << program;
        }
    }
}
