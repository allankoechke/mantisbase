# Inject semantic versioning
# Try to get the current Git commit hash
execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

# Suffix for pre-release or metadata (e.g., "-dev", "+gabcdef")
set(MB_VERSION_SUFFIX "-dev+g${GIT_COMMIT_HASH}")
set(MB_GIT_COMMIT "${GIT_COMMIT_HASH}")

# Fallback if not in a Git repo
if(NOT GIT_COMMIT_HASH)
    set(MB_VERSION_SUFFIX "")
    set(MB_GIT_COMMIT "unknown")
endif()