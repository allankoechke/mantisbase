# -------------------------------------------------------------------
# fmt: the single fmt implementation for the entire project
# -------------------------------------------------------------------

set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_DOC OFF CACHE BOOL "" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/fmt)
target_link_libraries(mantisbase PUBLIC fmt::fmt)