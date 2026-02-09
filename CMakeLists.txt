cmake_minimum_required(VERSION 3.20)
project(obs-mxl-plugins)

set(MXL_SDK_PREFIX "" CACHE PATH "Install prefix for MXL SDK")
set(OBS_SOURCE_DIR "" CACHE PATH "Path to OBS Studio source directory (macOS only)")
set(VCPKG_ROOT "" CACHE PATH "Path to vcpkg root (used by MXL)")

if(APPLE)
    set(PLATFORM_SCRIPT "build-macos.sh")
elseif(UNIX)
    set(PLATFORM_SCRIPT "build-linux.sh")
else()
    message(FATAL_ERROR "Unsupported platform for build scripts.")
endif()

set(ENV_ARGS "")
if(MXL_SDK_PREFIX)
    list(APPEND ENV_ARGS "MXL_SDK_PREFIX=${MXL_SDK_PREFIX}")
endif()
if(OBS_SOURCE_DIR)
    list(APPEND ENV_ARGS "OBS_SOURCE_DIR=${OBS_SOURCE_DIR}")
endif()
if(VCPKG_ROOT)
    list(APPEND ENV_ARGS "VCPKG_ROOT=${VCPKG_ROOT}")
endif()

add_custom_target(build-all
    COMMAND ${CMAKE_COMMAND} -E env ${ENV_ARGS}
            "${CMAKE_SOURCE_DIR}/${PLATFORM_SCRIPT}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    USES_TERMINAL
    COMMENT "Building MXL SDK and both OBS plugins"
)
