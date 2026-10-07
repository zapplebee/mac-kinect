include("${CMAKE_ROOT}/Modules/FetchContent.cmake")
FetchContent_Declare(openk4a_cpu
    GIT_REPOSITORY https://github.com/LukeSchoen/OpenK4A.git
    GIT_TAG f192ef34b3df2845d944c7065ce6d1ec4b7bdb7a)
FetchContent_MakeAvailable(openk4a_cpu)
set(OPENK4A_SOURCE "${openk4a_cpu_SOURCE_DIR}/src")
enable_language(OBJCXX)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/filter.metal" METAL_SOURCE)
configure_file(metal_source.h.in metal_source.h @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS filter.metal)
add_library(native_depth_metal STATIC metal_filter.mm)
target_include_directories(native_depth_metal PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(native_depth_metal PRIVATE -fobjc-arc)
target_link_libraries(native_depth_metal PUBLIC "-framework Metal" "-framework Foundation")
# Keep upstream CPU arithmetic intact; extract its portable declarations only.
file(READ "${OPENK4A_SOURCE}/openk4a.h" HEADER)
string(FIND "${HEADER}" "#define OPENK4A_FRAME_HEADER_BYTES" START)
string(FIND "${HEADER}" "/* Parses the device's own calibration document. */" END)
math(EXPR LENGTH "${END}-${START}")
string(SUBSTRING "${HEADER}" ${START} ${LENGTH} PORTABLE)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/portable_types.h" "${PORTABLE}")
add_library(native_depth_cpu STATIC
    "${OPENK4A_SOURCE}/openk4a_frame.c"
    accelerated.c
    portable.c)
target_include_directories(native_depth_cpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
    "${KINECT_SDK_INCLUDE}" "${KINECT_SDK_GENERATED}")
target_compile_options(native_depth_cpu PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/portable.h" -Wno-error)
target_include_directories(native_depth_cpu PRIVATE "${OPENK4A_SOURCE}")
target_link_libraries(native_depth_cpu PUBLIC native_depth_metal)
add_executable(benchmark_depth benchmark.c)
target_link_libraries(benchmark_depth PRIVATE native_depth_cpu)
add_executable(decode_depth decode.c)
target_link_libraries(decode_depth PRIVATE native_depth_cpu)
add_executable(live_depth live.c)
target_link_libraries(live_depth PRIVATE native_depth_cpu k4ainternal::depth_mcu k4ainternal::image k4ainternal::allocator)
target_link_libraries(live_depth PRIVATE k4a::k4a)
