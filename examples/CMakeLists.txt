cmake_minimum_required(VERSION 3.14)

project ( mantis_example )

# Create test executable
add_executable(mantis_example
        main.cpp
)

# Link libraries
target_link_libraries(mantis_example
        PRIVATE
        mantis
)

# Include directories
target_include_directories(mantis_example
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)