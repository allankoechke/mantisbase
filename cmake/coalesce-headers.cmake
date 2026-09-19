# coalesce-headers.cmake — install rules that collect every header a
# dev-package consumer needs into <prefix>/include/ via:
#   cmake --install <build> --component mb-dev-headers --prefix <dir>
#
# Included at the end of the top-level CMakeLists.txt. Rules execute at
# install time (after generation), so generated headers (drogon/exports.h,
# trantor/exports.h, soci-config.h, wolfssl/options.h, mantisbase/config.hpp)
# are guaranteed to exist — including soci-config.h, which SOCI only writes
# via file(GENERATE) and is therefore absent during configuration.
#
# Layout produced under <prefix>/include/:
#   mantisbase/          public project headers (+ generated config.hpp)
#   argparse/ dukglue/   duktape.h duk_config.h
#   drogon/ (+ orm/ nosql/) trantor/    (core, ORM, redis client, trantor)
#   nlohmann/ json/      (nlohmann/json single-include, jsoncpp)
#   soci/ (+ soci-config.h) spdlog/ fmt/ wolfssl/ (+ options.h)
#
# Only explicitly listed roots are installed (never system include dirs), and
# only *.h/*.hpp files (source dirs such as wolfssl/ or duktape/ also hold
# .c files and build scripts that must NOT ship).
#
# The tree is per-platform: soci-config.h and wolfssl/options.h are generated
# from build options and differ between Linux and Windows builds. Release CI
# installs each platform's tree and the pack job ships it as the include/
# tree of that OS's dev package (see cmake/package-dev.cmake).
set(MB_DEV_HEADERS_COMPONENT mb-dev-headers)

# Install a source header tree, headers only.
function(_mb_install_tree src_dir dst_subdir)
    if(NOT IS_DIRECTORY "${src_dir}")
        message(FATAL_ERROR "coalesce-headers: missing header root ${src_dir}\n"
            "Was the repo cloned with --recurse-submodules?")
    endif()
    install(DIRECTORY "${src_dir}/"
        DESTINATION "include/${dst_subdir}"
        COMPONENT ${MB_DEV_HEADERS_COMPONENT}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")
endfunction()

# Install one generated header. Existence is (deliberately) NOT checked here:
# generated files appear during generation/install, after configuration, so a
# configure-time check would always fail. A missing file errors visibly at
# install time instead.
function(_mb_install_generated src_file dst_subdir)
    install(FILES "${src_file}"
        DESTINATION "include/${dst_subdir}"
        COMPONENT ${MB_DEV_HEADERS_COMPONENT})
endfunction()

# --- 1. MantisBase public headers + generated config.hpp ----------------------
# This is the exact third-party set reachable from include/mantisbase/**
# (verified by compiling every public header against the installed tree).
# jwt-cpp, bcrypt-cpp, zlib and mbs are compiled into the shipped libraries
# and never #included by public headers, so they are intentionally excluded.
_mb_install_tree("${CMAKE_SOURCE_DIR}/include/mantisbase" "mantisbase")
_mb_install_generated("${CMAKE_BINARY_DIR}/include/mantisbase/config.hpp" "mantisbase")

# --- 2. Third-party source header trees ---------------------------------------
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/argparse/include/argparse" "argparse")
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/dukglue/include/dukglue" "dukglue")
# Drogon splits its public headers: core (lib/inc), ORM (orm_lib/inc) and the
# Redis client (nosql_lib/redis/inc) — e.g. HttpAppFramework.h includes
# drogon/orm/DbClient.h.
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/drogon/lib/inc/drogon" "drogon")
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/drogon/orm_lib/inc/drogon/orm" "drogon/orm")
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/drogon/nosql_lib/redis/inc/drogon/nosql" "drogon/nosql")
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/drogon/trantor/trantor" "trantor")
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/json/single_include/nlohmann" "nlohmann")
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/jsoncpp/include/json" "json")
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/soci/include/soci" "soci")
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/spdlog/include/spdlog" "spdlog")
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/fmt/include/fmt" "fmt")
_mb_install_tree("${CMAKE_SOURCE_DIR}/3rdParty/wolfssl/wolfssl" "wolfssl")

# duktape ships its headers next to sources: pick the two headers, not duktape.c.
foreach(_mb_duk_header duktape.h duk_config.h)
    install(FILES "${CMAKE_SOURCE_DIR}/3rdParty/duktape/${_mb_duk_header}"
        DESTINATION "include"
        COMPONENT ${MB_DEV_HEADERS_COMPONENT})
endforeach()

# --- 3. Generated third-party headers (installed over the source trees) -------
_mb_install_generated("${CMAKE_BINARY_DIR}/3rdParty/drogon/exports/drogon/exports.h" "drogon")
_mb_install_generated("${CMAKE_BINARY_DIR}/3rdParty/drogon/trantor/exports/trantor/exports.h" "trantor")
_mb_install_generated("${CMAKE_BINARY_DIR}/include/soci/soci-config.h" "soci")
_mb_install_generated("${CMAKE_BINARY_DIR}/3rdParty/wolfssl/wolfssl/options.h" "wolfssl")
