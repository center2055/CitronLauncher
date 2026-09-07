set(CITRON_COMMIT "unknown")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE git_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE git_result)
    if(git_result EQUAL 0 AND NOT "${git_commit}" STREQUAL "")
        set(CITRON_COMMIT "${git_commit}")
    endif()
endif()

string(TIMESTAMP CITRON_BUILD_DATE "%Y-%m-%d" UTC)

set(content "#pragma once

#define CITRON_VERSION_STRING \"${VERSION}\"
#define CITRON_VERSION_MAJOR ${VERSION_MAJOR}
#define CITRON_VERSION_MINOR ${VERSION_MINOR}
#define CITRON_VERSION_PATCH ${VERSION_PATCH}
#define CITRON_COMMIT \"${CITRON_COMMIT}\"
#define CITRON_COMPILER \"${COMPILER}\"
#define CITRON_BUILD_DATE \"${CITRON_BUILD_DATE}\"
")

set(existing "")
if(EXISTS "${OUTPUT}")
    file(READ "${OUTPUT}" existing)
endif()
if(NOT "${existing}" STREQUAL "${content}")
    get_filename_component(dir "${OUTPUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${dir}")
    file(WRITE "${OUTPUT}" "${content}")
endif()
