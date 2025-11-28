#ifndef MANTISBASE_TEST_FIXTURE_H
#define MANTISBASE_TEST_FIXTURE_H

#include <thread>
#include <chrono>
#include <iostream>
#include <filesystem>

#include "../include/mantisbase/mantis.h"

namespace fs = std::filesystem;

inline fs::path getBaseDir()
{
    // Base test directory for files and SQLite data
#ifdef _WIN32
    auto base_path = fs::temp_directory_path() / "mantisbase_tests" / mantis::generateShortId();
#else
    auto base_path = fs::path("/tmp") / "mantisbase_tests" / mantis::generateShortId();
#endif

    try
    {
        fs::create_directories(base_path);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FS Create] " << e.what() << std::endl;
    }

    return base_path;
}

struct TestFixture
{
    int port = 7075;
    mantis::MantisBase& mApp;

private:
    TestFixture(const mantis::json& config)
        : mApp(mantis::MantisBase::create(config))
    {
        std::cout << "[TestFixture] Setting up DB and starting server...\n";
    }

public:
    static TestFixture& instance(const mantis::json& config)
    {
        static TestFixture _instance{config};
        return _instance;
    }

    static mantis::MantisBase& app()
    {
        return mantis::MantisBase::instance();
    }

    // Wait until the server port responds
    [[nodiscard]] bool waitForServer(const int retries = 50, const int delayMs = 500) const
    {
        httplib::Client cli("http://localhost", port);
        for (int i = 0; i < retries; ++i)
        {
            if (auto res = cli.Get("/api/v1/health"); res && res->status == 200)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
        return false;
    }

    void teardownOnce(const fs::path& base_path) const
    {
        std::cout << "[TestFixture] Shutting down server...\n";

        // Graceful shutdown
        app().close();

        // Cleanup the temporary directory & files
        try
        {
            std::filesystem::remove_all(base_path);
            std::cout << "[TestFixture] Removing directory " << base_path.string() << "...\n";
        }
        catch (const std::exception& e)
        {
            std::cerr << "[TestFixture] Error removing file: " << e.what() << "\n";
        }

        std::cout << "[TestFixture] Server stopped.\n";
    }

    static httplib::Client client()
    {
        return httplib::Client("http://localhost", 7075);
    }
};

#endif //MANTISBASE_TEST_FIXTURE_H
