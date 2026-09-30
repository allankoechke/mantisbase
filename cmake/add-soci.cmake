# For configuration help for SOCI,
# check https://github.com/allankoechke/soci/blob/master/docs/installation.md

option(MB_HAS_POSTGRESQL "Has PostgreSQL Backend Support" OFF)

if(UNIX)
    message("-- Adding PostgreSQL backend support")
    set(MB_HAS_POSTGRESQL ON CACHE BOOL "" FORCE)
    add_compile_definitions(MB_HAS_POSTGRESQL=1)
else(UNIX)
    add_compile_definitions(MB_HAS_POSTGRESQL=0)
    set(MB_HAS_POSTGRESQL OFF CACHE BOOL "" FORCE)
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
set ( SOCI_FMT_BUILTIN OFF CACHE STRING "" FORCE )

if(MB_HAS_POSTGRESQL)
    set ( SOCI_POSTGRESQL ON CACHE BOOL "Enable PostgreSQL backend" FORCE )
else(MB_HAS_POSTGRESQL)
    set ( SOCI_POSTGRESQL OFF CACHE BOOL "Disable PostgreSQL backend" FORCE )
endif(MB_HAS_POSTGRESQL)

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
            pq
            dl
    )
endif(MB_HAS_POSTGRESQL)

# Include directories
target_include_directories ( mantisbase
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/soci/3rdParty
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/soci/include
        ${CMAKE_BINARY_DIR}/include                 # Generated headers
        ${CMAKE_BINARY_DIR}/3rdParty/soci/include   # Same as above, just in case it ends up here
)

# --- Dev-package shared library: export SOCI *core* symbols ------------------
# `Database` exposes soci types in its public API (`session()` returns
# `std::shared_ptr<soci::session>`, `connectionPool()` returns
# `soci::connection_pool&`, `MantisLoggerImpl` derives `soci::logger_impl`)
# and dev consumers call soci functions directly
# (`*sql << "...", soci::use(x), soci::into(y)`).
# SOCI compiles its static libs with hidden visibility
# (see 3rdParty/soci/CMakeLists.txt: `set(CMAKE_CXX_VISIBILITY_PRESET hidden)`),
# so without this the shared dev library would not provide soci symbols and
# dev consumers would fail to link (`undefined reference to soci::...`).
#
# SOCI splits into core vs backends: once a session is open, everything devs
# touch (prepare/streaming, use/into, row, statement, transaction, pool)
# dispatches through `soci_core` interfaces (`session_backend` /
# `statement_backend` polymorphism). The backend libs (`soci_sqlite3`,
# `soci_postgresql`) hold only the factories plus backend impls, which
# `Database::connect()` uses internally. Devs work with sessions handed out
# by `Database`, so only the core needs exporting: default visibility on
# `soci_core` alone. Backends stay hidden (still embedded for internal use),
# which also keeps the sqlite amalgamation's `sqlite3_*` symbols from leaking
# and interposing with a system sqlite3. Deliberately no `--whole-archive`:
# normal linking already pulls every core object `Database` references — the
# same surface devs use — while whole-archive doubled the .so (15MB -> 30MB).
#
# Constraint this implies (documented in doc/cpp-dev-package.md): devs must
# obtain sessions from `Database::session()`/`connectionPool()`, not open
# backend sessions directly (`soci::session(soci::sqlite3, ...)`), and catch
# `soci::soci_error` rather than backend-specific error types.
if(TARGET soci_core)
    set_target_properties(soci_core PROPERTIES
        CXX_VISIBILITY_PRESET default
        VISIBILITY_INLINES_HIDDEN NO
    )
endif()
