/**
 * Basic MantisBase server example.
 *
 * Demonstrates creating a MantisBase instance with one auth entity
 * and one data entity using the v4 API, then starting the server.
 *
 * Usage:
 *   ./basic_server
 *
 * The server starts on http://localhost:7070 with:
 *   - An auth entity "users" (email + password + name)
 *   - A data entity "posts" (title + content + views)
 *   - Health check at GET /api/v1/health
 */

#include <mantisbase/mantis.h>
#include <iostream>

int main()
{
    mb::json config;
    config["dev"] = true;
    config["database"] = "SQLITE";
    config["dataDir"] = "./data";
    config["publicDir"] = "./www";
    config["serve"] = {{"port", 7070}, {"host", "0.0.0.0"}, {"skip-admin-setup", true}};

    auto& app = mb::MantisBase::create(config);

    std::cout << "MantisBase v" << mb::MantisBase::appVersion() << " starting..." << std::endl;
    std::cout << "Health check: http://localhost:" << app.port() << "/api/v1/health" << std::endl;
    std::cout << std::endl;
    std::cout << "After the server starts, create entities via the API:" << std::endl;
    std::cout << "  1. Set up an admin: POST /api/v1/sys/admins/setup" << std::endl;
    std::cout << "  2. Create an auth entity: POST /api/v1/schemas" << std::endl;
    std::cout << "     {\"name\":\"users\", \"type\":\"auth\", \"fields\":[" << std::endl;
    std::cout << "       {\"name\":\"name\",\"type\":\"string\",\"required\":true}," << std::endl;
    std::cout << "       {\"name\":\"email\",\"type\":\"string\",\"required\":true,\"unique\":true}," << std::endl;
    std::cout << "       {\"name\":\"password\",\"type\":\"string\",\"required\":true}" << std::endl;
    std::cout << "     ]}" << std::endl;
    std::cout << "  3. Create a data entity: POST /api/v1/schemas" << std::endl;
    std::cout << "     {\"name\":\"posts\", \"type\":\"base\", \"fields\":[" << std::endl;
    std::cout << "       {\"name\":\"title\",\"type\":\"string\",\"required\":true}," << std::endl;
    std::cout << "       {\"name\":\"content\",\"type\":\"string\"}," << std::endl;
    std::cout << "       {\"name\":\"views\",\"type\":\"int\"}" << std::endl;
    std::cout << "     ]}" << std::endl;

    return app.run();
}
