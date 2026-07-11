# For configuration help for SOCI,
# check https://github.com/allankoechke/soci/blob/master/docs/installation.md

option(MB_HAS_POSTGRESQL "Has PostgreSQL Backend Support" OFF)
option(MB_DB_MYSQL "Enable MySQL Backend Support" OFF)

if(UNIX)
    message("-- Adding PostgreSQL backend support")
    set(MB_HAS_POSTGRESQL ON CACHE BOOL "" FORCE)
    add_compile_definitions(MB_HAS_POSTGRESQL=1)
else(UNIX)
    # On Windows, allow explicit opt-in for PostgreSQL
    if(MB_HAS_POSTGRESQL)
        message("-- Adding PostgreSQL backend support (Windows)")
        add_compile_definitions(MB_HAS_POSTGRESQL=1)
    else()
        add_compile_definitions(MB_HAS_POSTGRESQL=0)
    endif()
endif()

if(MB_DB_MYSQL)
    message("-- Adding MySQL backend support")
    add_compile_definitions(MB_HAS_MYSQL=1)
else()
    add_compile_definitions(MB_HAS_MYSQL=0)
endif()

# Critical: Set SOCI_SHARED before adding subdirectory
set ( SOCI_SHARED OFF CACHE BOOL "Build SOCI as static library" FORCE )

# Disable all backends except SQLite
set ( SOCI_TESTS OFF CACHE BOOL "Disable SOCI tests" FORCE )
set ( WITH_BOOST OFF CACHE BOOL "Disable Boost dependency" FORCE )
set ( SOCI_SQLITE3 ON CACHE BOOL "Enable SQLite3 backend" FORCE )
set ( SOCI_SQLITE3_BUILTIN ON CACHE BOOL "Use builtin SQLite3" FORCE )

# Explicitly disable other backends
set ( SOCI_ORACLE OFF CACHE BOOL "Disable Oracle backend" FORCE )
set ( SOCI_ODBC OFF CACHE BOOL "Disable ODBC backend" FORCE )
set ( SOCI_DB2 OFF CACHE BOOL "Disable DB2 backend" FORCE )
set ( SOCI_FIREBIRD OFF CACHE BOOL "Disable Firebird backend" FORCE )
set ( SOCI_EMPTY OFF CACHE BOOL "Disable empty backend" FORCE )

if(MB_HAS_POSTGRESQL)
    set ( SOCI_POSTGRESQL ON CACHE BOOL "Enable PostgreSQL backend" FORCE )
else(MB_HAS_POSTGRESQL)
    set ( SOCI_POSTGRESQL OFF CACHE BOOL "Disable PostgreSQL backend" FORCE )
endif(MB_HAS_POSTGRESQL)

if(MB_DB_MYSQL)
    set ( SOCI_MYSQL ON CACHE BOOL "Enable MySQL backend" FORCE )
else()
    set ( SOCI_MYSQL OFF CACHE BOOL "Disable MySQL backend" FORCE )
endif()

# Add SOCI subdirectory - this should generate soci-config.h
add_subdirectory ( ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/soci )

target_link_libraries ( mantisbase
        PUBLIC
        soci_core
        soci_sqlite3
)

if(MB_HAS_POSTGRESQL)
    target_link_libraries ( mantisbase
            PUBLIC
            soci_postgresql
    )
    if(UNIX)
        target_link_libraries ( mantisbase PUBLIC pq dl )
    else()
        find_package(PostgreSQL)
        if(PostgreSQL_FOUND)
            target_link_libraries ( mantisbase PUBLIC PostgreSQL::PostgreSQL )
        else()
            target_link_libraries ( mantisbase PUBLIC libpq )
        endif()
    endif()
endif(MB_HAS_POSTGRESQL)

if(MB_DB_MYSQL)
    target_link_libraries ( mantisbase
            PUBLIC
            soci_mysql
    )
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(MYSQL mysqlclient)
    endif()
    if(MYSQL_FOUND)
        target_link_libraries ( mantisbase PUBLIC ${MYSQL_LIBRARIES} )
        target_include_directories ( mantisbase PUBLIC ${MYSQL_INCLUDE_DIRS} )
    else()
        find_library(MYSQLCLIENT_LIB NAMES mysqlclient mysql libmysql)
        if(MYSQLCLIENT_LIB)
            target_link_libraries ( mantisbase PUBLIC ${MYSQLCLIENT_LIB} )
        endif()
    endif()
endif()

# Include directories
target_include_directories ( mantisbase
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/soci/3rdParty
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/soci/include
        ${CMAKE_BINARY_DIR}/include                 # Generated headers
        ${CMAKE_BINARY_DIR}/3rdParty/soci/include   # Same as above, just in case it ends up here
)
