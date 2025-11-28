
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/duktape)

target_compile_options(duktape PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-w>              # suppress all warnings for GCC/Clang
        $<$<CXX_COMPILER_ID:MSVC>:/w>                   # suppress all warnings for MSVC
)

# Link to libs
target_link_libraries(mantis
        PUBLIC
        duktape
)

# Include directories
target_include_directories(mantisbase_lib
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/duktape
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/dukglue/include
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/dukglue/include)