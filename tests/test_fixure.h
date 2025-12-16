#ifndef MANTISBASE_TEST_FIXTURE_H
#define MANTISBASE_TEST_FIXTURE_H

#include <thread>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <atomic>
#include <mutex>

#include "../include/mantisbase/mantis.h"
#include "common/test_config.h"

namespace fs = std::filesystem;

inline fs::path getBaseDir()
{
    // Base test directory for files and SQLite data
    // Use temp_directory_path() consistently for cross-platform compatibility
    auto base_path = fs::temp_directory_path() / "mantisbase_tests" / mb::generateShortId();

    try
    {
        fs::create_directories(base_path);
        // Set restrictive permissions (owner read/write/execute only)
        fs::permissions(base_path, fs::perms::owner_all, fs::perm_options::replace);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FS Create] " << e.what() << std::endl;
    }

    return base_path;
}

struct TestFixture
{
    int port;
    mb::MantisBase& mApp;
    static std::atomic<bool> server_ready;
    static std::mutex server_mutex;

private:
    TestFixture(const mb::json& config)
        : port(TestConfig::getTestPort()),
          mApp(mb::MantisBase::create(config))
    {
        std::cout << "[TestFixture] Setting up DB and starting server on port " << port << "...\n";
    }

public:
    static TestFixture& instance(const mb::json& config)
    {
        static TestFixture _instance{config};
        return _instance;
    }

    static mb::MantisBase& app()
    {
        return mb::MantisBase::instance();
    }

    // Wait until the server port responds with exponential backoff
    [[nodiscard]] bool waitForServer(const int max_retries = 20, const int initial_delay_ms = 50) const
    {
        // If server is already ready, return immediately
        if (server_ready.load()) {
            return true;
        }
        
        httplib::Client cli("http://localhost", port);
        int delay_ms = initial_delay_ms;
        
        for (int i = 0; i < max_retries; ++i)
        {
            if (auto res = cli.Get("/api/v1/health"); res && res->status == 200) {
                server_ready.store(true);
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            // Exponential backoff, max 500ms
            delay_ms = std::min(delay_ms * 2, 500);
        }
        return false;
    }

    void teardownOnce(const fs::path& base_path) const
    {
        std::lock_guard<std::mutex> lock(server_mutex);
        
        std::cout << "[TestFixture] Shutting down server...\n";

        // Graceful shutdown
        app().close();
        server_ready.store(false);

        // Cleanup the temporary directory & files
        try
        {
            if (fs::exists(base_path)) {
                fs::remove_all(base_path);
                std::cout << "[TestFixture] Removed directory " << base_path.string() << "...\n";
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[TestFixture] Error removing directory: " << e.what() << "\n";
        }

        std::cout << "[TestFixture] Server stopped.\n";
    }

    httplib::Client client() const
    {
        return httplib::Client("http://localhost", port);
    }
    
    static httplib::Client staticClient()
    {
        return httplib::Client("http://localhost", TestConfig::getTestPort());
    }
};

#endif //MANTISBASE_TEST_FIXTURE_H
