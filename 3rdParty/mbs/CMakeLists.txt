cmake_minimum_required(VERSION 3.16)
project(mbs VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(mbs STATIC
  src/lexer.cpp
  src/parser.cpp
  src/evaluator.cpp
  src/script.cpp
)
target_include_directories(mbs
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(mbs PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(mbs PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

option(BUILD_TESTING "Build tests and examples" ON)
if(BUILD_TESTING)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
  )
  FetchContent_MakeAvailable(Catch2)

  add_executable(mbs-example examples/main.cpp)
  target_link_libraries(mbs-example PRIVATE mbs)

  add_executable(mbs_tests
    tests/test_script.cpp
    tests/test_extended.cpp
  )
  target_link_libraries(mbs_tests PRIVATE mbs Catch2::Catch2WithMain)
  include(CTest)
  add_test(NAME mbs_tests COMMAND $<TARGET_FILE:mbs_tests>)
endif()
