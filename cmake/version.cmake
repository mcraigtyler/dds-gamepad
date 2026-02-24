# Runs at build time to regenerate version files from current git state.
# Called via add_custom_target in CMakeLists.txt.
# Required variables (passed via -D):
#   GIT_EXECUTABLE, SOURCE_DIR, VERSION_MAJOR, VERSION_MINOR,
#   HEADER_TEMPLATE, RC_TEMPLATE, OUTPUT_DIR, CMAKE_COMMAND

execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_COMMIT_COUNT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_COUNT)
    set(GIT_COMMIT_COUNT 0)
endif()
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()

set(APP_VERSION_MAJOR ${VERSION_MAJOR})
set(APP_VERSION_MINOR ${VERSION_MINOR})
set(APP_VERSION_PATCH ${GIT_COMMIT_COUNT})
set(APP_VERSION "${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH}")
set(APP_VERSION_FULL "${APP_VERSION}+${GIT_COMMIT_HASH}")

# Write to a temp file and use copy_if_different so that source files
# including version.h are not recompiled when the version hasn't changed.
configure_file("${HEADER_TEMPLATE}" "${OUTPUT_DIR}/version.h.tmp" @ONLY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${OUTPUT_DIR}/version.h.tmp" "${OUTPUT_DIR}/version.h")
file(REMOVE "${OUTPUT_DIR}/version.h.tmp")

configure_file("${RC_TEMPLATE}" "${OUTPUT_DIR}/version.rc.tmp" @ONLY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${OUTPUT_DIR}/version.rc.tmp" "${OUTPUT_DIR}/version.rc")
file(REMOVE "${OUTPUT_DIR}/version.rc.tmp")
