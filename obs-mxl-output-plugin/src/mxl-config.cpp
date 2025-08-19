/*
-------------------------------------------------------------------------------------------
  Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.

  Licensed under the Apache License, Version 2.0 (the "License").
  You may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
-------------------------------------------------------------------------------------------
*/

#include "mxl-config.h"
#include <util/config-file.h>
#include <util/platform.h>
#include <obs-module.h>
#include <cstdlib>

MXLConfig* MXLConfig::_instance = nullptr;

MXLConfig::MXLConfig() :
    OutputEnabled(false),
    DomainPath(std::string(getenv("HOME")) + "/mxl_domain"),
    VideoEnabled(true),
    AudioEnabled(true),
    VideoFlowId(""),
    AudioFlowId("")
{
    // Constructor - defaults are set above
    // Actual loading happens in Load() method
}

void MXLConfig::Load() {
    std::string config_path = GetConfigPath();
    blog(LOG_INFO, "MXL Config: Loading from: %s", config_path.c_str());
    
    config_t* config = nullptr;
    int result = config_open(&config, config_path.c_str(), CONFIG_OPEN_EXISTING);
    
    if (result == CONFIG_SUCCESS) {
        blog(LOG_INFO, "MXL Config: Successfully opened config file");
        
        OutputEnabled = config_get_bool(config, MXL_SECTION_NAME, MXL_PARAM_OUTPUT_ENABLED);
        DomainPath = config_get_string(config, MXL_SECTION_NAME, MXL_PARAM_DOMAIN_PATH);
        VideoEnabled = config_get_bool(config, MXL_SECTION_NAME, MXL_PARAM_VIDEO_ENABLED);
        AudioEnabled = config_get_bool(config, MXL_SECTION_NAME, MXL_PARAM_AUDIO_ENABLED);
        VideoFlowId = config_get_string(config, MXL_SECTION_NAME, MXL_PARAM_VIDEO_FLOW_ID);
        AudioFlowId = config_get_string(config, MXL_SECTION_NAME, MXL_PARAM_AUDIO_FLOW_ID);
        
        blog(LOG_INFO, "MXL Config: Loaded - Output: %s, Domain: %s, Video: %s, Audio: %s",
             OutputEnabled ? "enabled" : "disabled",
             DomainPath.c_str(),
             VideoEnabled ? "enabled" : "disabled", 
             AudioEnabled ? "enabled" : "disabled");
        
        config_close(config);
    } else {
        blog(LOG_INFO, "MXL Config: Config file doesn't exist or failed to open (result: %d), using defaults", result);
    }
}

void MXLConfig::Save() {
    std::string config_path = GetConfigPath();
    blog(LOG_INFO, "MXL Config: Saving to: %s", config_path.c_str());
    
    config_t* config = nullptr;
    int result = config_open(&config, config_path.c_str(), CONFIG_OPEN_ALWAYS);
    
    if (result == CONFIG_SUCCESS) {
        blog(LOG_INFO, "MXL Config: Successfully opened config file for writing");
        
        config_set_bool(config, MXL_SECTION_NAME, MXL_PARAM_OUTPUT_ENABLED, OutputEnabled);
        config_set_string(config, MXL_SECTION_NAME, MXL_PARAM_DOMAIN_PATH, DomainPath.c_str());
        config_set_bool(config, MXL_SECTION_NAME, MXL_PARAM_VIDEO_ENABLED, VideoEnabled);
        config_set_bool(config, MXL_SECTION_NAME, MXL_PARAM_AUDIO_ENABLED, AudioEnabled);
        config_set_string(config, MXL_SECTION_NAME, MXL_PARAM_VIDEO_FLOW_ID, VideoFlowId.c_str());
        config_set_string(config, MXL_SECTION_NAME, MXL_PARAM_AUDIO_FLOW_ID, AudioFlowId.c_str());
        
        blog(LOG_INFO, "MXL Config: Saving - Output: %s, Domain: %s, Video: %s, Audio: %s",
             OutputEnabled ? "enabled" : "disabled",
             DomainPath.c_str(),
             VideoEnabled ? "enabled" : "disabled", 
             AudioEnabled ? "enabled" : "disabled");
        
        int save_result = config_save(config);
        if (save_result == CONFIG_SUCCESS) {
            blog(LOG_INFO, "MXL Config: Successfully saved config file");
        } else {
            blog(LOG_ERROR, "MXL Config: Failed to save config file (result: %d)", save_result);
        }
        
        config_close(config);
    } else {
        blog(LOG_ERROR, "MXL Config: Failed to open config file for writing (result: %d)", result);
    }
}

MXLConfig* MXLConfig::Current() {
    if (!_instance) {
        _instance = new MXLConfig();
    }
    return _instance;
}

std::string MXLConfig::GetConfigPath() {
    // Get the OBS config directory
    char* obs_config_dir = obs_module_get_config_path(obs_current_module(), "");
    if (!obs_config_dir) {
        // Fallback to a simple path
        std::string fallback = std::string(getenv("HOME")) + "/.config/obs-studio/plugin_config/mxl-output-config.ini";
        blog(LOG_INFO, "MXL Config: Using fallback config path: %s", fallback.c_str());
        return fallback;
    }
    
    std::string config_dir(obs_config_dir);
    bfree(obs_config_dir);
    
    // Create the directory if it doesn't exist
    os_mkdirs(config_dir.c_str());
    
    std::string config_path = config_dir + "/mxl-output-config.ini";
    blog(LOG_INFO, "MXL Config: Using config path: %s", config_path.c_str());
    return config_path;
}
