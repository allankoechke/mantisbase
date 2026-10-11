# Collect all admin dashboard static files
file(GLOB_RECURSE ADMIN_STATIC_FILES LIST_DIRECTORIES false "${CMAKE_CURRENT_SOURCE_DIR}/public/*")
file(GLOB_RECURSE MANTIS_ASSET_FILES LIST_DIRECTORIES false "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")

# Combine and escape problematic characters
set(ALL_RESOURCE_FILES ${ADMIN_STATIC_FILES} ${MANTIS_ASSET_FILES})
set(ESCAPED_RESOURCE_FILES "")
foreach(file ${ALL_RESOURCE_FILES})
    list(APPEND ESCAPED_RESOURCE_FILES "${file}")
endforeach()

# Create CMRC resource library with escaped paths
cmrc_add_resource_library(mb-rc NAMESPACE mantis ${ESCAPED_RESOURCE_FILES} "favicon.ico")

# Link resources to admin library - use PUBLIC to expose to downstream targets
target_link_libraries(mantisbase PUBLIC mb-rc)

# Make CMRC headers available to any target linking to mantisbase_admin
target_compile_definitions(mantisbase PUBLIC CMRC_ENABLE)

# Export the library for downstream consumption
set_target_properties(mantisbase PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "$<INSTALL_INTERFACE:include>"
)
