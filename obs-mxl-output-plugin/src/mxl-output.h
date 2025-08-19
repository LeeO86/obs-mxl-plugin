#pragma once

#include <obs-module.h>
#include <obs-output.h>
#include <obs.h>
#include <media-io/video-io.h>
#include <media-io/audio-io.h>
#include <util/platform.h>
#include <mxl/mxl.h>
#include <mxl/flow.h>
#include <mxl/flowinfo.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <filesystem>
#include <fstream>

struct video_frame_data {
    uint8_t *data;
    size_t size;
    uint64_t timestamp;
    uint32_t width;
    uint32_t height;
    enum video_format format;
    
    video_frame_data() : data(nullptr), size(0), timestamp(0), width(0), height(0), format(VIDEO_FORMAT_NONE) {}
    ~video_frame_data() {
        if (data) {
            bfree(data);
            data = nullptr;
        }
    }
};

struct audio_frame_data {
    uint8_t *data;
    size_t size;
    uint64_t timestamp;
    uint32_t sample_rate;
    uint32_t channels;
    enum audio_format format;
    
    audio_frame_data() : data(nullptr), size(0), timestamp(0), sample_rate(0), channels(0), format(AUDIO_FORMAT_UNKNOWN) {}
    ~audio_frame_data() {
        if (data) {
            bfree(data);
            data = nullptr;
        }
    }
};

struct mxl_output_data {
    // OBS output
    obs_output_t *output;
    
    // MXL components
    mxlInstance mxl_instance;
    mxlFlowWriter video_flow_writer;
    mxlFlowWriter audio_flow_writer;
    
    // Configuration
    std::string domain_path;
    std::string video_flow_id;
    std::string audio_flow_id;
    bool video_enabled;
    bool audio_enabled;
    
    // Video properties
    uint32_t video_width;
    uint32_t video_height;
    uint32_t video_fps_num;
    uint32_t video_fps_den;
    enum video_format video_format;
    std::string video_media_type;
    
    // Audio properties
    uint32_t audio_sample_rate;
    uint32_t audio_channels;
    enum audio_format audio_format;
    std::string audio_media_type;
    
    // Threading and synchronization
    std::thread output_thread;
    std::atomic<bool> thread_active;
    std::atomic<bool> output_active;
    
    // Frame queues
    std::queue<std::unique_ptr<video_frame_data>> video_queue;
    std::queue<std::unique_ptr<audio_frame_data>> audio_queue;
    std::mutex video_queue_mutex;
    std::mutex audio_queue_mutex;
    std::condition_variable frame_condition;
    
    // Grain indexing
    std::atomic<uint64_t> video_grain_index;
    std::atomic<uint64_t> audio_grain_index;
    
    // Timing
    uint64_t start_timestamp;
    uint64_t video_frame_interval_ns;
    uint64_t audio_frame_interval_ns;
    
    // Constructor/Destructor
    mxl_output_data();
    ~mxl_output_data();
    
    // Methods
    bool initialize_mxl();
    void cleanup_mxl();
    bool create_video_flow();
    bool create_audio_flow();
    void output_loop();
    bool process_video_frame(std::unique_ptr<video_frame_data> frame);
    bool process_audio_frame(std::unique_ptr<audio_frame_data> frame);
    
    // Format conversion helpers
    std::string get_mxl_video_media_type(enum video_format format);
    std::string get_mxl_audio_media_type(enum audio_format format);
    size_t calculate_video_frame_size(enum video_format format, uint32_t width, uint32_t height);
    size_t calculate_audio_frame_size(enum audio_format format, uint32_t channels, uint32_t samples);
    
    // v210 conversion
    bool convert_to_v210(uint8_t *src_data, enum video_format src_format, 
                        uint32_t width, uint32_t height, uint32_t *linesize,
                        uint8_t *dst_data, size_t dst_size,
                        uint8_t **data_planes = nullptr);
    
    // Flow descriptor creation
    bool create_video_flow_descriptor();
    bool create_audio_flow_descriptor();
    std::string generate_flow_descriptor_json(bool is_video);
    
    // Utility methods
    std::string generate_uuid();
    uint64_t get_timestamp_ns();
};

// OBS output callbacks
extern "C" {
    const char *mxl_output_get_name(void *unused);
    void *mxl_output_create(obs_data_t *settings, obs_output_t *output);
    void mxl_output_destroy(void *data);
    bool mxl_output_start(void *data);
    void mxl_output_stop(void *data, uint64_t ts);
    void mxl_output_raw_video(void *data, struct video_data *frame);
    void mxl_output_raw_audio(void *data, struct audio_data *frame);
    void mxl_output_raw_audio2(void *data, size_t idx, struct audio_data *frame);
    void mxl_output_update(void *data, obs_data_t *settings);
    bool mxl_output_pause(void *data, bool pause);
    uint64_t mxl_output_get_total_bytes(void *data);
    int mxl_output_get_dropped_frames(void *data);
}
