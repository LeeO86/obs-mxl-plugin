#include <obs-module.h>
#include <obs-frontend-api.h>
#include "mxl-output.h"
#include "mxl-config.h"
#include "mxl-native-dialog.h"
#include <thread>
#include <chrono>

// Version information
#define MXL_OUTPUT_PLUGIN_VERSION "0.0.1-alpha"
#define MXL_BUILD_TIMESTAMP __DATE__ " " __TIME__
#define MXL_BUILD_ID __DATE__ "_" __TIME__

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-mxl-output-plugin", "en-US")

// Global components
obs_output_t* global_mxl_output = nullptr;
static MXLConfig* global_config = nullptr;

// Module information
MODULE_EXPORT const char *obs_module_description(void)
{
    return "MXL Output Plugin - Stream OBS output to MXL flows";
}

MODULE_EXPORT const char *obs_module_name(void)
{
    return "MXL Output Plugin";
}

// Helper functions
namespace {
    void mxl_output_start_if_enabled() {
        if (!global_config) return;
        
        if (global_config->OutputEnabled) {
            blog(LOG_INFO, "MXL Output: Starting output");
            
            if (!global_mxl_output) {
                // Create output instance
                obs_data_t* settings = obs_data_create();
                obs_data_set_string(settings, "domain_path", global_config->DomainPath.c_str());
                obs_data_set_string(settings, "video_flow_id", global_config->VideoFlowId.c_str());
                obs_data_set_bool(settings, "video_enabled", global_config->VideoEnabled);
                obs_data_set_string(settings, "audio_flow_id", global_config->AudioFlowId.c_str());
                obs_data_set_bool(settings, "audio_enabled", global_config->AudioEnabled);
                
                global_mxl_output = obs_output_create("mxl_raw_output", "MXL Output", settings, nullptr);
                
                if (!global_mxl_output) {
                    blog(LOG_ERROR, "MXL Output: Failed to create output");
                    obs_data_release(settings);
                    return;
                }
                
                obs_data_release(settings);
            }
            
            if (global_mxl_output && !obs_output_active(global_mxl_output)) {
                if (obs_output_start(global_mxl_output)) {
                    blog(LOG_INFO, "MXL Output: Output started successfully");
                } else {
                    blog(LOG_ERROR, "MXL Output: Failed to start output");
                }
            }
        }
    }
    
    void mxl_output_stop_global() {
        if (global_mxl_output && obs_output_active(global_mxl_output)) {
            blog(LOG_INFO, "MXL Output: Stopping output");
            obs_output_stop(global_mxl_output);
        }
    }
    
    void apply_output_state_from_config() {
        if (!global_config) {
            global_config = MXLConfig::Current();
            global_config->Load();
        }
        
        if (global_config->OutputEnabled) {
            // Should be running - start if not already running
            if (!global_mxl_output || !obs_output_active(global_mxl_output)) {
                blog(LOG_INFO, "MXL Output: Config enabled - starting output");
                mxl_output_start_if_enabled();
            }
        } else {
            // Should be stopped - stop if currently running
            if (global_mxl_output && obs_output_active(global_mxl_output)) {
                blog(LOG_INFO, "MXL Output: Config disabled - stopping output");
                mxl_output_stop_global();
            }
        }
    }
    
    void show_mxl_status() {
        if (!global_config) {
            global_config = MXLConfig::Current();
            global_config->Load();
        }
        
        blog(LOG_INFO, "=== MXL Output Status ===");
        blog(LOG_INFO, "Plugin Version: %s", MXL_OUTPUT_PLUGIN_VERSION);
        blog(LOG_INFO, "Output Enabled: %s", global_config->OutputEnabled ? "Yes" : "No");
        blog(LOG_INFO, "Domain Path: %s", global_config->DomainPath.c_str());
        blog(LOG_INFO, "Video Enabled: %s", global_config->VideoEnabled ? "Yes" : "No");
        blog(LOG_INFO, "Video Flow ID: %s", global_config->VideoFlowId.c_str());
        blog(LOG_INFO, "Audio Enabled: %s", global_config->AudioEnabled ? "Yes" : "No");
        blog(LOG_INFO, "Audio Flow ID: %s", global_config->AudioFlowId.c_str());
        
        if (global_mxl_output) {
            blog(LOG_INFO, "Output Status: %s", obs_output_active(global_mxl_output) ? "ACTIVE" : "STOPPED");
        } else {
            blog(LOG_INFO, "Output Status: NOT CREATED");
        }
        blog(LOG_INFO, "========================");
        blog(LOG_INFO, "Configuration file: %s", global_config->GetConfigPath().c_str());
        blog(LOG_INFO, "Edit the config file and restart OBS to change settings");
    }
}

// Settings dialog callback - opens native visual configuration dialog
void mxl_output_settings_callback(void *data)
{
    UNUSED_PARAMETER(data);
    
    // Load current configuration
    MXLConfig* config = MXLConfig::Current();
    config->Load();
    
    // Store original settings to detect changes
    bool original_output_enabled = config->OutputEnabled;
    bool original_video_enabled = config->VideoEnabled;
    bool original_audio_enabled = config->AudioEnabled;
    std::string original_domain_path = config->DomainPath;
    std::string original_audio_flow_id = config->AudioFlowId;
    
    // Prepare settings structure
    MXLNativeDialog::Settings settings;
    settings.domain_path = config->DomainPath;
    settings.output_enabled = config->OutputEnabled;
    settings.video_enabled = config->VideoEnabled;
    settings.video_flow_id = config->VideoFlowId;
    settings.audio_enabled = config->AudioEnabled;
    settings.audio_flow_id = config->AudioFlowId;
    
    // Auto-populate UUIDs if empty and streams are enabled
    if (settings.video_enabled && settings.video_flow_id.empty()) {
        settings.video_flow_id = MXLNativeDialog::GenerateUUID();
    }
    if (settings.audio_enabled && settings.audio_flow_id.empty()) {
        settings.audio_flow_id = MXLNativeDialog::GenerateUUID();
    }
    
    // Show the native dialog
    if (MXLNativeDialog::ShowSettingsDialog(settings)) {
        // User clicked OK - apply the settings
        blog(LOG_INFO, "MXL Output: Settings updated - Output: %s, Video: %s, Audio: %s", 
             settings.output_enabled ? "enabled" : "disabled",
             settings.video_enabled ? "enabled" : "disabled",
             settings.audio_enabled ? "enabled" : "disabled");
        
        // Update configuration
        config->DomainPath = settings.domain_path;
        config->OutputEnabled = settings.output_enabled;
        config->VideoEnabled = settings.video_enabled;
        config->VideoFlowId = settings.video_flow_id;
        config->AudioEnabled = settings.audio_enabled;
        config->AudioFlowId = settings.audio_flow_id;
        
        // Save to file
        config->Save();
        
        // Check if we need to restart the output due to stream configuration changes
        bool needs_restart = false;
        if (global_mxl_output && obs_output_active(global_mxl_output)) {
            // Output is currently running - check if stream settings changed
            if (original_video_enabled != settings.video_enabled ||
                original_audio_enabled != settings.audio_enabled ||
                original_output_enabled != settings.output_enabled ||
                original_domain_path != settings.domain_path ||
                original_audio_flow_id != settings.audio_flow_id) {
                needs_restart = true;
                blog(LOG_INFO, "MXL Output: Restarting output due to configuration changes");
            }
        }
        
        if (needs_restart) {
            // Stop and destroy the current output completely
            mxl_output_stop_global();
            if (global_mxl_output) {
                obs_output_release(global_mxl_output);
                global_mxl_output = nullptr;
            }
            // Small delay to ensure clean shutdown
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Apply the new output state based on the updated configuration
        apply_output_state_from_config();
    }
}

// Module load function
MODULE_EXPORT bool obs_module_load(void)
{
    blog(LOG_INFO, "=== LOADING MXL OUTPUT PLUGIN v%s ===", MXL_OUTPUT_PLUGIN_VERSION);
    
    // Initialize configuration
    global_config = MXLConfig::Current();
    global_config->Load();
    
    // Register the output type
    struct obs_output_info mxl_output_info = {};
    mxl_output_info.id = "mxl_raw_output";
    mxl_output_info.flags = OBS_OUTPUT_AV | OBS_OUTPUT_MULTI_TRACK;
    
    // Required callbacks
    mxl_output_info.get_name = mxl_output_get_name;
    mxl_output_info.create = mxl_output_create;
    mxl_output_info.destroy = mxl_output_destroy;
    mxl_output_info.start = mxl_output_start;
    mxl_output_info.stop = mxl_output_stop;
    mxl_output_info.raw_video = mxl_output_raw_video;
    mxl_output_info.raw_audio = mxl_output_raw_audio;
    mxl_output_info.raw_audio2 = mxl_output_raw_audio2;
    
    // Optional callbacks
    mxl_output_info.update = mxl_output_update;
    mxl_output_info.get_total_bytes = mxl_output_get_total_bytes;
    mxl_output_info.get_dropped_frames = mxl_output_get_dropped_frames;
    
    // Register the output type
    obs_register_output(&mxl_output_info);
    blog(LOG_INFO, "MXL Output: Output type registered successfully");
    
    // Add Tools menu item
    blog(LOG_INFO, "MXL Output: Adding Tools menu item");
    obs_frontend_add_tools_menu_item("MXL Output Settings", mxl_output_settings_callback, nullptr);
    blog(LOG_INFO, "MXL Output: Tools menu items added successfully");
    
    // Add frontend event callback
    obs_frontend_add_event_callback([](enum obs_frontend_event event, void* private_data) {
        UNUSED_PARAMETER(private_data);
        
        if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
            blog(LOG_INFO, "MXL Output: OBS finished loading");
            
            // Add a small delay to ensure registration is complete
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (global_config && global_config->OutputEnabled) {
                blog(LOG_INFO, "MXL Output: Auto-starting output");
                mxl_output_start_if_enabled();
            }
        } else if (event == OBS_FRONTEND_EVENT_EXIT) {
            blog(LOG_INFO, "MXL Output: OBS exiting, stopping output");
            mxl_output_stop_global();
            if (global_mxl_output) {
                obs_output_release(global_mxl_output);
                global_mxl_output = nullptr;
            }
        }
    }, nullptr);
    
    blog(LOG_INFO, "=== MXL OUTPUT PLUGIN LOADED SUCCESSFULLY ===");
    blog(LOG_INFO, "Configuration: %s", global_config->GetConfigPath().c_str());
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "Unloading MXL Output Plugin v%s", MXL_OUTPUT_PLUGIN_VERSION);
    
    // Stop output if running
    mxl_output_stop_global();
    
    // Clean up global output
    if (global_mxl_output) {
        obs_output_release(global_mxl_output);
        global_mxl_output = nullptr;
    }
    
    blog(LOG_INFO, "MXL Output Plugin unloaded");
}
