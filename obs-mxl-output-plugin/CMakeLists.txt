cmake_minimum_required(VERSION 3.20)
project(obs-mxl-output-plugin VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Frontend API for Tools menu
set(ENABLE_FRONTEND_API "true")

# Platform detection
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(PLATFORM_NAME "macOS")
elseif(UNIX)
    set(PLATFORM_NAME "Linux")
elseif(WIN32)
    set(PLATFORM_NAME "Windows")
endif()

message(STATUS "Building MXL Output Plugin for ${PLATFORM_NAME}")

# Platform-specific paths
if(APPLE)
    set(OBS_SOURCE_DIR "/Users/samisb/obs-studio" CACHE PATH "Path to OBS Studio source")
    set(MXL_SDK_PREFIX "/Users/samisb/mxl-sdk/usr/local" CACHE PATH "Path to MXL SDK")
    set(OBS_APP_PATH "/Applications/OBS.app")
    set(OBS_FRAMEWORKS_DIR "${OBS_APP_PATH}/Contents/Frameworks")
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

# Create the plugin library
add_library(obs-mxl-output-plugin MODULE)

# Platform-specific OBS setup and linking
if(APPLE)
    set(OBS_INCLUDE_DIRS
        "${OBS_SOURCE_DIR}/libobs"
        "${OBS_SOURCE_DIR}/UI/obs-frontend-api"
    )
    set(OBS_LIBRARIES
        "${OBS_FRAMEWORKS_DIR}/libobs.framework/Versions/A/libobs"
        "${OBS_FRAMEWORKS_DIR}/obs-frontend-api.dylib"
    )
    
    # Include directories
    target_include_directories(obs-mxl-output-plugin PRIVATE ${OBS_INCLUDE_DIRS})
    
    # Link OBS libraries
    target_link_libraries(obs-mxl-output-plugin PRIVATE ${OBS_LIBRARIES})
    
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
    
    # Include directories
    target_include_directories(obs-mxl-output-plugin PRIVATE ${OBS_INCLUDE_DIRS})
    
    # Link OBS libraries
    target_link_libraries(obs-mxl-output-plugin PRIVATE ${OBS_LIBRARIES})
endif()



# Link MXL SDK
target_link_libraries(obs-mxl-output-plugin PRIVATE mxl::mxl)

# Platform-specific UI framework linking
if(APPLE)
    # Link Cocoa framework for native macOS dialogs
    target_link_libraries(obs-mxl-output-plugin PRIVATE "-framework Cocoa")
elseif(UNIX)
    # Link GTK for native Linux dialogs
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(obs-mxl-output-plugin PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(obs-mxl-output-plugin PRIVATE ${GTK3_LIBRARIES})
    target_compile_options(obs-mxl-output-plugin PRIVATE ${GTK3_CFLAGS_OTHER})
elseif(WIN32)
    # Link Windows common controls and shell libraries
    target_link_libraries(obs-mxl-output-plugin PRIVATE comctl32 shell32)
endif()

# Plugin source files
target_sources(obs-mxl-output-plugin PRIVATE
    src/obs-mxl-output.cpp
    src/mxl-output.cpp
    src/mxl-output-callbacks.cpp
    src/mxl-config.cpp
    src/mxl-native-dialog.cpp
    
    PRIVATE FILE_SET HEADERS FILES
    src/mxl-output.h
    src/mxl-config.h
    src/mxl-native-dialog.h
)

# Platform-specific source files
if(APPLE)
    target_sources(obs-mxl-output-plugin PRIVATE
        src/mxl-native-dialog-macos.mm
    )
    # Set Objective-C++ properties for the .mm file
    set_source_files_properties(src/mxl-native-dialog-macos.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
endif()

# Platform-specific compiler flags
if(UNIX AND NOT APPLE)
    # Linux-specific flags
    target_compile_definitions(obs-mxl-output-plugin PRIVATE LINUX)
    set_target_properties(obs-mxl-output-plugin PROPERTIES
        LINK_FLAGS "-Wl,-rpath,\$ORIGIN"
    )
endif()

# Set plugin properties
set_target_properties(obs-mxl-output-plugin PROPERTIES
    PREFIX ""
    FOLDER "plugins"
)

# Platform-specific installation
if(APPLE)
    # macOS bundle installation
    set(PLUGIN_BUNDLE_DIR "$ENV{HOME}/Library/Application Support/obs-studio/plugins/obs-mxl-output-plugin.plugin")
    set(PLUGIN_CONTENTS_DIR "${PLUGIN_BUNDLE_DIR}/Contents")
    set(PLUGIN_MACOS_DIR "${PLUGIN_CONTENTS_DIR}/MacOS")
    
    # Install the binary
    install(TARGETS obs-mxl-output-plugin
        LIBRARY DESTINATION "${PLUGIN_MACOS_DIR}")
    
    # Install Info.plist
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
        DESTINATION "${PLUGIN_CONTENTS_DIR}")
            
elseif(UNIX)
    # Linux installation
    set(PLUGIN_INSTALL_DIR "$ENV{HOME}/.config/obs-studio/plugins/obs-mxl-output-plugin/bin/64bit")
    
    # Create the directory structure
    install(CODE "file(MAKE_DIRECTORY \"${PLUGIN_INSTALL_DIR}\")")
    
    # Install the plugin
    install(TARGETS obs-mxl-output-plugin
        LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}")
            
elseif(WIN32)
    # Windows installation
    set(PLUGIN_INSTALL_DIR "$ENV{APPDATA}/obs-studio/plugins/obs-mxl-output-plugin/bin/64bit")
    install(TARGETS obs-mxl-output-plugin
        LIBRARY DESTINATION "${PLUGIN_INSTALL_DIR}")
endif()

# Print installation info
install(CODE "message(STATUS \"MXL Output Plugin installed to: ${CMAKE_INSTALL_PREFIX}\")")
