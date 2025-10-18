#pragma once

#include <obs-module.h>
#include <obs-source.h>
#include <obs.h>
#include <graphics/graphics.h>
#include <util/platform.h>
#include <mxl/mxl.h>
#include <mxl/flow.h>
#include <mxl/flowinfo.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <filesystem>

struct mxl_flow_info {
    std::string id;
    std::string label;
    std::string description;
    std::string format;
    bool active;
};

struct mxl_source_data {
    // OBS source
    obs_source_t *source;
    
    // MXL components
    mxlInstance mxl_instance;
    mxlFlowReader flow_reader;
    mxlFlowInfo flow_info;
    
    // Configuration
    std::string domain_path;
    std::string flow_id;
    
    // Threading
    std::thread capture_thread;
    std::atomic<bool> thread_active;
    std::mutex frame_mutex;
    
    // Frame data
    uint8_t *frame_data;
    size_t frame_size;
    uint32_t width;
    uint32_t height;
    enum video_format format;
    
    // Timing
    uint64_t current_grain_index;
    uint64_t frame_interval_ns;
    
    // Constructor/Destructor
    mxl_source_data();
    ~mxl_source_data();
    
    // Methods
    bool initialize_mxl();
    void cleanup_mxl();
    void capture_loop();
    bool process_grain(const mxlGrainInfo &grain_info, uint8_t *payload);
    enum video_format get_obs_format_from_mxl(const std::string &media_type);
    size_t calculate_frame_size(enum video_format format, uint32_t width, uint32_t height);
    void convert_v210_to_rgba(uint8_t *v210_data, size_t v210_size, 
                             uint8_t *rgba_data, size_t rgba_size);
    
    // Flow discovery methods
    std::vector<mxl_flow_info> discover_flows(const std::string &domain_path);
    mxl_flow_info get_flow_info_from_descriptor(const std::string &flow_id, const std::string &descriptor_path);
    bool is_flow_active(const std::string &domain_path, const std::string &flow_id);
};

// OBS source callbacks
extern "C" {
    const char *mxl_source_get_name(void *unused);
    void *mxl_source_create(obs_data_t *settings, obs_source_t *source);
    void mxl_source_destroy(void *data);
    void mxl_source_update(void *data, obs_data_t *settings);
    obs_properties_t *mxl_source_get_properties(void *data);
    void mxl_source_get_defaults(obs_data_t *settings);
    uint32_t mxl_source_get_width(void *data);
    uint32_t mxl_source_get_height(void *data);
    void mxl_source_video_tick(void *data, float seconds);
    void mxl_source_video_render(void *data, gs_effect_t *effect);
}
