/**
 * Multi-instance MantisBase example.
 *
 * Demonstrates running two MantisBase instances on different ports
 * in the same process. Each instance has its own database and
 * configuration, proving full isolation.
 *
 * Usage:
 *   ./multi_instance
 *
 * Instance A: http://localhost:7070
 * Instance B: http://localhost:7071
 */

#include <mantisbase/mantis.h>
#include <iostream>
#include <thread>

int main()
{
    mb::json configA;
    configA["dev"] = true;
    configA["database"] = "SQLITE";
    configA["dataDir"] = "./data_a";
    configA["publicDir"] = "./www";
    configA["serve"] = {{"port", 7070}, {"host", "0.0.0.0"}, {"skip-admin-setup", true}};

    mb::json configB;
    configB["dev"] = true;
    configB["database"] = "SQLITE";
    configB["dataDir"] = "./data_b";
    configB["publicDir"] = "./www";
    configB["serve"] = {{"port", 7071}, {"host", "0.0.0.0"}, {"skip-admin-setup", true}};

    auto& appA = mb::MantisBase::create(configA);

    std::cout << "Instance A starting on port " << appA.port() << std::endl;
    std::cout << "  Health: http://localhost:" << appA.port() << "/api/v1/health" << std::endl;

    std::thread threadA([&appA]() {
        appA.run();
    });

    auto& appB = mb::MantisBase::create(configB);

    std::cout << "Instance B starting on port " << appB.port() << std::endl;
    std::cout << "  Health: http://localhost:" << appB.port() << "/api/v1/health" << std::endl;
    std::cout << std::endl;
    std::cout << "Both instances are running independently." << std::endl;
    std::cout << "Each has its own SQLite database in separate data directories." << std::endl;

    appB.run();

    threadA.join();
    return 0;
}
