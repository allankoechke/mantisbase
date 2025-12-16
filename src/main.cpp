/**
 * @file main.cpp
 * @brief Standalone `mantisbase` executable entrypoint
 *
 * Created by allan on 08/05/2025.
 */

#include "../include/mantisbase/mantis.h"

/**
 * @brief MantisApp standalone entrypoint
 * @param argc Argument count
 * @param argv Argument list
 * @return Non-zero if the application did not exit correctly
 */
int main(const int argc, char* argv[])
{
    // Create `MantisBase` instance with the passed in arguments
    auto& app = mb::MantisBase::create(argc, argv);

    // Or simply
    // Create the JSON object
    // {
    //     "dev": nullptr/true/etc, // the value here does not matter
    //     "serve": {
    //         "port": 9089
    //     }
    // }
    // const json args{{"dev", nullptr}, {"serve", {{"port", 9089}}}};
    // auto& app = mb::MantisBase::create(args);

    // Run the http server listening loop
    return app.run();
}