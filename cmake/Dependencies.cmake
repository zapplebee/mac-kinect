include(FetchContent)

# Do not include the upstream SDK's top-level build: it brings in its viewer,
# recorders, tests, packaging and unrelated Windows/.NET tooling.
FetchContent_Declare(k4a_source
    GIT_REPOSITORY https://github.com/djpiper28/Azure-Kinect-Sensor-SDK.git
    GIT_TAG 59047d3ca78fddde8b3844a50aa3fbb22d7e1f6f
    GIT_SUBMODULES extern/azure_c_shared/src extern/cjson/src
        extern/libjpeg-turbo/src extern/libuvc/src extern/spdlog/src
    SOURCE_SUBDIR unused-top-level)
FetchContent_MakeAvailable(k4a_source)

# Apply narrowly scoped, idempotent portability fixes to the fetched source.
function(patch_text path before after)
    file(READ "${path}" contents)
    string(FIND "${contents}" "${after}" already_patched)
    if(NOT already_patched EQUAL -1)
        return()
    endif()
    string(FIND "${contents}" "${before}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Dependency patch no longer applies: ${path}")
    endif()
    string(REPLACE "${before}" "${after}" contents "${contents}")
    file(WRITE "${path}" "${contents}")
endfunction()

patch_text("${k4a_source_SOURCE_DIR}/src/calibration/calibration.c"
    "#include <locale.h>" "#include <xlocale.h>\n#include <locale.h>")
# The SDK assumes pthread_once_t fits in a zero-initialized pointer. macOS uses
# a larger structure and a nonzero initializer; using the original crashes.
file(READ "${k4a_source_SOURCE_DIR}/include/k4ainternal/global.h" global_header)
if(NOT global_header MATCHES "typedef pthread_once_t k4a_init_once_t;")
    string(FIND "${global_header}" "typedef void *k4a_init_once_t;" start)
    string(FIND "${global_header}" "typedef void(k4a_init_once_function_t)" end)
    if(start LESS 0 OR end LESS 0)
        message(FATAL_ERROR "Cannot patch SDK once initialization")
    endif()
    string(SUBSTRING "${global_header}" 0 ${start} prefix)
    string(SUBSTRING "${global_header}" ${end} -1 suffix)
    file(WRITE "${k4a_source_SOURCE_DIR}/include/k4ainternal/global.h"
        "${prefix}#include <pthread.h>\ntypedef pthread_once_t k4a_init_once_t;\n#define K4A_INIT_ONCE PTHREAD_ONCE_INIT\n\n${suffix}")
endif()
patch_text("${k4a_source_SOURCE_DIR}/src/color/CMakeLists.txt"
    "elseif (\${CMAKE_SYSTEM_NAME} STREQUAL \"Linux\")"
    "elseif (\${CMAKE_SYSTEM_NAME} STREQUAL \"Linux\" OR APPLE)")
patch_text("${k4a_source_SOURCE_DIR}/extern/libuvc/CMakeLists.txt"
    "if(\"\${CMAKE_SYSTEM_NAME}\" STREQUAL \"Linux\")"
    "if(\"\${CMAKE_SYSTEM_NAME}\" STREQUAL \"Linux\" OR APPLE)")

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/sdk" "${PROJECT_BINARY_DIR}/sdk" EXCLUDE_FROM_ALL)
set(KINECT_SDK_INCLUDE "${k4a_source_SOURCE_DIR}/include")
set(KINECT_SDK_GENERATED "${PROJECT_BINARY_DIR}/sdk/src/sdk/include")
