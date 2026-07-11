cmake_minimum_required(VERSION 3.14)

project(mantisbase_examples)

# Basic server — minimal MantisBase setup with auth + data entities
add_executable(basic_server basic_server/main.cpp)
target_link_libraries(basic_server PRIVATE mb::static)
target_include_directories(basic_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Multi-instance — two MantisBase instances on different ports in one process
add_executable(multi_instance multi_instance/main.cpp)
target_link_libraries(multi_instance PRIVATE mb::static)
target_include_directories(multi_instance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# OAuth flow — configuring Google OAuth for an auth entity
add_executable(oauth_flow oauth_flow/main.cpp)
target_link_libraries(oauth_flow PRIVATE mb::static)
target_include_directories(oauth_flow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# API keys — issuing, using, and revoking API keys
add_executable(api_keys_example api_keys/main.cpp)
target_link_libraries(api_keys_example PRIVATE mb::static)
target_include_directories(api_keys_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
