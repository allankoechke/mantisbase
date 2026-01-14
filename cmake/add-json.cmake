
set(JSON_MultipleHeaders OFF CACHE BOOL "Disable compiled mode for nlohmann/json")
set(JSON_Install OFF CACHE INTERNAL "")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/json)

# Link to libs
target_link_libraries(mantisbase
        PUBLIC
        nlohmann_json::nlohmann_json
)

# Include directories
target_include_directories(mantisbase
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/json/single_include
)