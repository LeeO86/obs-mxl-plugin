cmake_minimum_required(VERSION 3.20)
project(obs-mxl-plugin VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform detection
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(PLATFORM_NAME "macOS")
elseif(UNIX)
    set(PLATFORM_NAME "Linux")
elseif(WIN32)
    set(PLATFORM_NAME "Windows")
endif()

message(STATUS "Building for ${PLATFORM_NAME}")

# Platform-specific paths
if(APPLE)
    set(OBS_SOURCE_DIR "$ENV{HOME}/obs-studio" CACHE PATH "Path to OBS Studio source")
    set(MXL_SDK_PREFIX "$ENV{HOME}/mxl-sdk/usr/local" CACHE PATH "Path to MXL SDK")
    set(OBS_APP_PATH "/Applications/OBS.app")
    set(OBS_FRAMEWORKS_DIR "${OBS_APP_PATH}/Contents/Frameworks")
    set(OBS_BUILD_DIR "" CACHE PATH "Path to OBS Studio build directory (for obsconfig.h)")

    # Optional SIMDe headers (required by OBS headers on macOS)
    find_path(SIMDE_INCLUDE_DIR simde/x86/sse2.h
        PATHS
            /opt/homebrew/include
            /usr/local/include
    )
elseif(UNIX)
    # Linux paths - try multiple common locations
    set(OBS_SOURCE_DIR "/usr/include/obs" CACHE PATH "Path to OBS Studio headers")
    set(MXL_SDK_PREFIX "$ENV{HOME}/mxl-sdk/usr/local" CACHE PATH "Path to MXL SDK")
    
    # Find OBS headers in common locations
    find_path(OBS_INCLUDE_DIR obs.h
        PATHS
            /usr/include/obs
            /usr/local/include/obs
            /usr/include/obs-studio
            /usr/local/include/obs-studio
            /usr/include/obs-studio/libobs
            /usr/local/include/obs-studio/libobs
        PATH_SUFFIXES libobs
    )
    
    if(NOT OBS_INCLUDE_DIR)
        message(FATAL_ERROR "OBS headers not found. Please install OBS Studio or build from source.")
    endif()
    
    message(STATUS "Found OBS headers at: ${OBS_INCLUDE_DIR}")
    
    # Try to find OBS libraries
    find_library(OBS_LIB obs 
        PATHS 
            /usr/lib 
            /usr/local/lib 
            /usr/lib/x86_64-linux-gnu
            /usr/lib64
        REQUIRED)
    
    find_library(OBS_FRONTEND_LIB obs-frontend-api
        PATHS 
            /usr/lib 
            /usr/local/lib 
            /usr/lib/x86_64-linux-gnu
            /usr/lib64)
    
    message(STATUS "Found OBS library at: ${OBS_LIB}")
    if(OBS_FRONTEND_LIB)
        message(STATUS "Found OBS frontend API at: ${OBS_FRONTEND_LIB}")
    endif()
endif()

# Find MXL SDK
set(CMAKE_PREFIX_PATH "${MXL_SDK_PREFIX}" ${CMAKE_PREFIX_PATH})
find_package(mxl CONFIG REQUIRED)

# Platform-specific OBS setup
if(APPLE)
    set(OBS_INCLUDE_DIRS
        "${OBS_SOURCE_DIR}/libobs"
        "${OBS_SOURCE_DIR}/UI/obs-frontend-api"
    )
    if(OBS_BUILD_DIR)
        find_path(OBS_CONFIG_DIR obsconfig.h
            PATHS
                "${OBS_BUILD_DIR}"
                "${OBS_BUILD_DIR}/config"
        )
        if(OBS_CONFIG_DIR)
            list(APPEND OBS_INCLUDE_DIRS "${OBS_CONFIG_DIR}")
            message(STATUS "Found OBS config headers at: ${OBS_CONFIG_DIR}")
        endif()
    endif()
    if(NOT OBS_CONFIG_DIR)
        message(FATAL_ERROR "obsconfig.h not found. Configure OBS Studio to generate it and set OBS_BUILD_DIR to the build directory.")
    endif()
    if(SIMDE_INCLUDE_DIR)
        list(APPEND OBS_INCLUDE_DIRS "${SIMDE_INCLUDE_DIR}")
        message(STATUS "Found SIMDe headers at: ${SIMDE_INCLUDE_DIR}")
    endif()
    set(OBS_LIBRARIES
        "${OBS_FRAMEWORKS_DIR}/libobs.framework/Versions/A/libobs"
        "${OBS_FRAMEWORKS_DIR}/obs-frontend-api.dylib"
    )
elseif(UNIX)
    # Linux OBS setup
    set(OBS_INCLUDE_DIRS ${OBS_INCLUDE_DIR})
    
    # Add parent directory if we found headers in a subdirectory
    get_filename_component(OBS_PARENT_DIR ${OBS_INCLUDE_DIR} DIRECTORY)
    if(EXISTS "${OBS_PARENT_DIR}/obs.h")
        list(APPEND OBS_INCLUDE_DIRS ${OBS_PARENT_DIR})
    endif()
    
    set(OBS_LIBRARIES ${OBS_LIB})
    if(OBS_FRONTEND_LIB)
        list(APPEND OBS_LIBRARIES ${OBS_FRONTEND_LIB})
    endif()
endif()

# Plugin source files
set(PLUGIN_SOURCES
    src/obs-mxl-source.cpp
    src/mxl-source.cpp
    src/mxl-source.h
)

# Create the plugin
add_library(obs-mxl-plugin MODULE ${PLUGIN_SOURCES})

# Include directories
target_include_directories(obs-mxl-plugin PRIVATE ${OBS_INCLUDE_DIRS})

# Link libraries
target_link_libraries(obs-mxl-plugin
    PRIVATE
    mxl::mxl
    ${OBS_LIBRARIES}
)

# Platform-specific compiler flags
if(UNIX AND NOT APPLE)
    # Linux-specific flags
    target_compile_definitions(obs-mxl-plugin PRIVATE LINUX)
    set_target_properties(obs-mxl-plugin PROPERTIES
        LINK_FLAGS "-Wl,-rpath,\$ORIGIN"
    )
endif()

# Set plugin properties
set_target_properties(obs-mxl-plugin PROPERTIES
    PREFIX ""
    FOLDER "plugins"
)

# Platform-specific installation
if(APPLE)
    # macOS bundle installation
    set(PLUGIN_BUNDLE_DIR "$ENV{HOME}/Library/Application Support/obs-studio/plugins/obs-mxl-plugin.plugin")
    set(PLUGIN_CONTENTS_DIR "${PLUGIN_BUNDLE_DIR}/Contents")
    set(PLUGIN_MACOS_DIR "${PLUGIN_CONTENTS_DIR}/MacOS")
    
    # Install the binary
    install(TARGETS obs-mxl-plugin
        LIBRARY DESTINATION "${PLUGIN_MACOS_DIR}")
    
    # Install Info.plist
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
        DESTINATION "${PLUGIN_CONTENTS_DIR}")
            
elseif(UNIX)
    # Linux installation
    set(PLUGIN_INSTALL_DIR "$ENV{HOME}/.config/obs-studio/plugins/obs-mxl-plugin/bin/64bit")
    
    # Create the directory structure
    install(CODE "file(MAKE_DIRECTORY \"${PLUGIN_INSTALL_DIR}\")")
    
    # Install the plugin
    install(TARGETS obs-mxl-plugin
        LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}")
            
elseif(WIN32)
    # Windows installation
    set(PLUGIN_INSTALL_DIR "$ENV{APPDATA}/obs-studio/plugins/obs-mxl-plugin/bin/64bit")
    install(TARGETS obs-mxl-plugin
        LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}")
endif()

# Print installation info
install(CODE "message(STATUS \"Plugin installed to: ${CMAKE_INSTALL_PREFIX}\")")
