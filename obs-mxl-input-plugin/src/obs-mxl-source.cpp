#include <obs-module.h>
#include "mxl-source.h"

// Version information
#define MXL_PLUGIN_VERSION "1.0.0"
#define MXL_BUILD_TIMESTAMP __DATE__ " " __TIME__
#define MXL_BUILD_ID __DATE__ "_" __TIME__

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-mxl-plugin", "en-US")

bool obs_module_load(void)
{
    blog(LOG_INFO, "Loading MXL Plugin v%s (built %s) [ID: %s]", 
         MXL_PLUGIN_VERSION, MXL_BUILD_TIMESTAMP, MXL_BUILD_ID);
    
    struct obs_source_info mxl_source_info = {};
    
    mxl_source_info.id = "mxl_source";
    mxl_source_info.type = OBS_SOURCE_TYPE_INPUT;
    mxl_source_info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_ASYNC;
    
    mxl_source_info.get_name = mxl_source_get_name;
    mxl_source_info.create = mxl_source_create;
    mxl_source_info.destroy = mxl_source_destroy;
    mxl_source_info.update = mxl_source_update;
    mxl_source_info.get_properties = mxl_source_get_properties;
    mxl_source_info.get_defaults = mxl_source_get_defaults;
    mxl_source_info.get_width = mxl_source_get_width;
    mxl_source_info.get_height = mxl_source_get_height;
    mxl_source_info.video_tick = mxl_source_video_tick;
    // For async video sources, don't set video_render - OBS handles rendering
    mxl_source_info.video_render = nullptr;
    
    obs_register_source(&mxl_source_info);
    
    blog(LOG_INFO, "MXL Plugin loaded successfully");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "Unloading MXL Plugin v%s", MXL_PLUGIN_VERSION);
}
