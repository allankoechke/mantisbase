# For configuration help for SOCI,
# check https://github.com/allankoechke/soci/blob/master/docs/installation.md

option(MANTIS_HAS_POSTGRESQL "Has PostgreSQL Backend Support" OFF)

if(UNIX)
    message("-- Adding PostgreSQL backend support")
    set(MANTIS_HAS_POSTGRESQL ON CACHE BOOL "" FORCE)
    add_compile_definitions(MANTIS_HAS_POSTGRESQL=1)
else(UNIX)
    add_compile_definitions(MANTIS_HAS_POSTGRESQL=0)
    set(MANTIS_HAS_POSTGRESQL OFF CACHE BOOL "" FORCE)
endif()

# Critical: Set SOCI_SHARED before adding subdirectory
set ( SOCI_SHARED OFF CACHE BOOL "Build SOCI as static library" FORCE )

# Disable all backends except SQLite
set ( SOCI_TESTS OFF CACHE BOOL "Disable SOCI tests" FORCE )
set ( WITH_BOOST OFF CACHE BOOL "Disable Boost dependency" FORCE )
set ( SOCI_SQLITE3 ON CACHE BOOL "Enable SQLite3 backend" FORCE )
set ( SOCI_SQLITE3_BUILTIN ON CACHE BOOL "Use builtin SQLite3" FORCE )

# Explicitly disable other backends
set ( SOCI_MYSQL OFF CACHE BOOL "Disable MySQL backend" FORCE )
set ( SOCI_ORACLE OFF CACHE BOOL "Disable Oracle backend" FORCE )
set ( SOCI_ODBC OFF CACHE BOOL "Disable ODBC backend" FORCE )
set ( SOCI_DB2 OFF CACHE BOOL "Disable DB2 backend" FORCE )
set ( SOCI_FIREBIRD OFF CACHE BOOL "Disable Firebird backend" FORCE )
set ( SOCI_EMPTY OFF CACHE BOOL "Disable empty backend" FORCE )

if(MANTIS_HAS_POSTGRESQL)
    set ( SOCI_POSTGRESQL ON CACHE BOOL "Enable PostgreSQL backend" FORCE )
else(MANTIS_HAS_POSTGRESQL)
    set ( SOCI_POSTGRESQL OFF CACHE BOOL "Disable PostgreSQL backend" FORCE )
endif(MANTIS_HAS_POSTGRESQL)

# Add SOCI subdirectory - this should generate soci-config.h
add_subdirectory ( ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/soci )

target_link_libraries ( mantisbase
        PUBLIC
        soci_core
        soci_sqlite3
)

if(MANTIS_HAS_POSTGRESQL)
    target_link_libraries ( mantisbase
            PUBLIC
            soci_postgresql
            pq
            dl
    )
endif(MANTIS_HAS_POSTGRESQL)

# Include directories
target_include_directories ( mantisbase
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/soci/3rdParty
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/soci/include
        ${CMAKE_BINARY_DIR}/include                 # Generated headers
        ${CMAKE_BINARY_DIR}/3rdParty/soci/include   # Same as above, just in case it ends up here
)
