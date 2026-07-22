cmake_minimum_required(VERSION 3.2)

option(BUILD_TESTS "Build test programs" OFF)

project(bcrypt-cpp)

set(CMAKE_CXX_STANDARD 11)

add_library(bcrypt_cpp src/bcrypt.cpp src/blowfish.cpp)
target_include_directories(bcrypt_cpp PRIVATE src/)
target_include_directories(bcrypt_cpp PUBLIC include/)

if(BUILD_TESTS)
  add_executable(test_bcryptcpp test/main.cpp)
  target_link_libraries(test_bcryptcpp PRIVATE bcrypt_cpp)
endif()
