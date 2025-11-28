set(BUILD_TESTS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/bcrypt-cpp)
target_link_libraries ( mantisbase_lib PUBLIC bcrypt_cpp )