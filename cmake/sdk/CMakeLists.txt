# Adapter for the small subset of the Microsoft SDK used by our native reader
# and CPU camera-calibration projection. All source remains in FetchContent.
set(SDK "${k4a_source_SOURCE_DIR}")
set(CMAKE_MODULE_PATH "${SDK}/cmake;${CMAKE_MODULE_PATH}")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(USB REQUIRED IMPORTED_TARGET libusb-1.0)
add_library(LibUSB::LibUSB ALIAS PkgConfig::USB)
foreach(dep azure_c_shared cjson libjpeg-turbo libuvc spdlog)
    add_subdirectory("${SDK}/extern/${dep}" "extern/${dep}" EXCLUDE_FROM_ALL)
endforeach()
target_link_libraries(uvc_static PUBLIC PkgConfig::USB ${JPEG_LIBRARIES})

set(K4A_PRIV_INCLUDE_DIR "${SDK}/include" "${CMAKE_CURRENT_BINARY_DIR}/src/sdk/include")
set(K4A_INCLUDE_DIR "${SDK}/include")
set(K4A_VERSION_MAJOR 1)
set(K4A_VERSION_MINOR 4)
set(K4A_VERSION_PATCH 0)
set(K4A_VERSION "1.4.0")
set(K4A_VERSION_STR "1.4.0-mac-kinect")
set(K4A_VERSION_RC "${SDK}/version.rc.in")
# SDK installation templates refer to this variable. Keep the override scoped
# to this adapter directory, not the parent project's source tree.
set(PROJECT_SOURCE_DIR "${SDK}")
foreach(component allocator calibration capturesync color color_mcu depth depth_mcu
        deloader dewrapper dynlib global image imu logging math queue rwlock
        sdk tewrapper transformation usbcommand)
    add_subdirectory("${SDK}/src/${component}" "src/${component}" EXCLUDE_FROM_ALL)
endforeach()
