#include "mxl-output.h"
#include "mxl-config.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <inttypes.h>

namespace {
    uint32_t speaker_layout_to_channels(enum speaker_layout layout)
    {
        switch (layout) {
        case SPEAKERS_MONO:
            return 1;
        case SPEAKERS_STEREO:
            return 2;
        case SPEAKERS_2POINT1:
            return 3;
        case SPEAKERS_4POINT0:
            return 4;
        case SPEAKERS_4POINT1:
            return 5;
        case SPEAKERS_5POINT1:
            return 6;
        case SPEAKERS_7POINT1:
            return 8;
        default:
            return 2;
        }
    }
}

// Forward declaration for callback functions
extern "C" {
    const char *mxl_output_get_name(void *unused);
    void *mxl_output_create(obs_data_t *settings, obs_output_t *output);
    void mxl_output_destroy(void *data);
    bool mxl_output_start(void *data);
    void mxl_output_stop(void *data, uint64_t ts);
    void mxl_output_raw_video(void *data, struct video_data *frame);
    void mxl_output_raw_audio(void *data, struct audio_data *frames);
    void mxl_output_raw_audio2(void *data, size_t idx, struct audio_data *frames);
    void mxl_output_update(void *data, obs_data_t *settings);
    bool mxl_output_pause(void *data, bool pause);
    uint64_t mxl_output_get_total_bytes(void *data);
    int mxl_output_get_dropped_frames(void *data);
}

// OBS output callback implementations
extern "C" {

const char *mxl_output_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return "MXL Output";
}

void *mxl_output_create(obs_data_t *settings, obs_output_t *output)
{
    blog(LOG_INFO, "MXL Output: Creating output instance");
    
    mxl_output_data *data = new mxl_output_data();
    data->output = output;
    
    // Get settings
    data->domain_path = obs_data_get_string(settings, "domain_path");
    data->video_flow_id = obs_data_get_string(settings, "video_flow_id");
    data->video_enabled = obs_data_get_bool(settings, "video_enabled");
    data->audio_flow_id = obs_data_get_string(settings, "audio_flow_id");
    data->audio_enabled = obs_data_get_bool(settings, "audio_enabled");
    
    // Get video info from OBS
    obs_video_info ovi;
    if (obs_get_video_info(&ovi)) {
        // Use output resolution because raw frames are delivered at output size
        data->video_width = ovi.output_width;
        data->video_height = ovi.output_height;
        data->video_fps_num = ovi.fps_num;
        data->video_fps_den = ovi.fps_den;
        data->video_format = ovi.output_format;
        data->video_media_type = data->get_mxl_video_media_type(ovi.output_format);
        
        // Calculate video frame interval
        if (ovi.fps_num > 0) {
            data->video_frame_interval_ns = (1000000000ULL * ovi.fps_den) / ovi.fps_num;
        }
    }

    // Get audio info from OBS
    obs_audio_info aoi;
    if (obs_get_audio_info(&aoi)) {
        data->audio_sample_rate = aoi.samples_per_sec;
        data->audio_channel_count = speaker_layout_to_channels(aoi.speakers);
    }

    
    blog(LOG_INFO, "MXL Output: Created output instance - Video: %dx%d@%.2ffps, Audio: %u Hz, %u ch",
         data->video_width, data->video_height,
         (double)data->video_fps_num / data->video_fps_den,
         data->audio_sample_rate, data->audio_channel_count);
    
    return data;
}

void mxl_output_destroy(void *data)
{
    blog(LOG_INFO, "MXL Output: Destroying output instance");
    
    mxl_output_data *output_data = static_cast<mxl_output_data*>(data);
    if (output_data) {
        delete output_data;
    }
}

bool mxl_output_start(void *data)
{
    blog(LOG_INFO, "MXL Output: Starting output");
    
    mxl_output_data *output_data = static_cast<mxl_output_data*>(data);
    if (!output_data) {
        blog(LOG_ERROR, "MXL Output: Start failed - output_data is null");
        return false;
    }
    
    // Initialize MXL
    if (!output_data->initialize_mxl()) {
        blog(LOG_ERROR, "MXL Output: Failed to initialize MXL");
        return false;
    }
    
    // Connect to OBS video sources
    obs_output_t *output = output_data->output;
    
    // Connect to video
    video_t *video = obs_get_video();
    if (video) {
        obs_output_set_video_conversion(output, nullptr);
    }

    
    // Start data capture
    if (video) {
        obs_output_begin_data_capture(output, 0);
    }
    
    // Start output thread
    output_data->thread_active = true;
    output_data->output_active = true;
    output_data->start_timestamp = output_data->get_timestamp_ns();
    output_data->video_grain_index = 0;
    output_data->video_grain_index = 0;
    output_data->last_grain_index_valid = false;
    output_data->last_audio_index_valid = false;
    output_data->audio_has_time_offset = false;
    output_data->has_time_offset = false;
    
    try {
        output_data->output_thread = std::thread(&mxl_output_data::output_loop, output_data);
        blog(LOG_INFO, "MXL Output: Output started successfully - Video: %s", 
             output_data->video_enabled ? "enabled" : "disabled");
    } catch (const std::exception& e) {
        blog(LOG_ERROR, "MXL Output: Failed to start output thread: %s", e.what());
        output_data->thread_active = false;
        output_data->output_active = false;
        return false;
    }
    
    return true;
}

void mxl_output_stop(void *data, uint64_t ts)
{
    UNUSED_PARAMETER(ts);
    blog(LOG_INFO, "MXL Output: Stopping output");
    
    mxl_output_data *output_data = static_cast<mxl_output_data*>(data);
    if (!output_data) {
        return;
    }
    
    // Stop data capture first
    if (output_data->output) {
        obs_output_end_data_capture(output_data->output);
    }
    
    output_data->output_active = false;
    output_data->cleanup_mxl();
    
    blog(LOG_INFO, "MXL Output: Output stopped");
}

void mxl_output_raw_video(void *data, struct video_data *frame)
{
    mxl_output_data *output_data = static_cast<mxl_output_data*>(data);
    if (!output_data || !output_data->output_active || !output_data->video_enabled || !frame) {
        return;
    }
    
    static uint64_t frame_count = 0;
    frame_count++;
    
    if (frame_count % 300 == 1) { // Log every 300th frame (every 10 seconds at 30fps)
        blog(LOG_DEBUG, "MXL Output: Received video frame %" PRIu64 " (timestamp: %" PRIu64 ")", 
             frame_count, frame->timestamp);
    }
    
    // Create video frame data
    auto video_frame = std::make_unique<video_frame_data>();
    video_frame->width = output_data->video_width;
    video_frame->height = output_data->video_height;
    video_frame->format = output_data->video_format;
    video_frame->timestamp = frame->timestamp;
    
    // Calculate v210 frame size
    video_frame->size = output_data->calculate_video_frame_size(
        video_frame->format, video_frame->width, video_frame->height);
    
    // Allocate v210 frame buffer
    video_frame->data = static_cast<uint8_t*>(bmalloc(video_frame->size));
    if (!video_frame->data) {
        blog(LOG_ERROR, "MXL Output: Failed to allocate video frame buffer");
        return;
    }
    
    // Convert to v210 format
    
    if (!output_data->convert_to_v210(frame->data[0], video_frame->format,
                                     video_frame->width, video_frame->height,
                                     frame->linesize, video_frame->data, video_frame->size,
                                     frame->data)) {
        blog(LOG_ERROR, "MXL Output: Failed to convert video frame to v210");
        return;
    }
    
    // Add to queue
    {
        std::lock_guard<std::mutex> lock(output_data->video_queue_mutex);
        output_data->video_queue.push(std::move(video_frame));
        
        if (frame_count % 300 == 1) {
            blog(LOG_DEBUG, "MXL Output: Video queue size: %zu", output_data->video_queue.size());
        }
    }
    
    output_data->frame_condition.notify_one();
}



bool mxl_output_pause(void *data, bool pause)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(pause);
    // Pause not supported
    return true;
}

uint64_t mxl_output_get_total_bytes(void *data)
{
    mxl_output_data *output_data = static_cast<mxl_output_data*>(data);
    if (!output_data) {
        return 0;
    }
    
    // Return approximate bytes based on grain count
    return output_data->video_grain_index * 1920 * 1080 * 3;
}

int mxl_output_get_dropped_frames(void *data)
{
    UNUSED_PARAMETER(data);
    // For now, we don't track dropped frames
    return 0;
}

void mxl_output_raw_audio2(void *data, size_t idx, struct audio_data *frames)
{
    UNUSED_PARAMETER(idx);
    mxl_output_data *output_data = static_cast<mxl_output_data*>(data);
    if (!output_data || !output_data->output_active || !output_data->audio_enabled || !frames) {
        return;
    }
    output_data->write_audio_samples(frames);
}

void mxl_output_raw_audio(void *data, struct audio_data *frames)
{
    mxl_output_data *output_data = static_cast<mxl_output_data*>(data);
    if (!output_data || !output_data->output_active || !output_data->audio_enabled || !frames) {
        return;
    }
    output_data->write_audio_samples(frames);
}

void mxl_output_update(void *data, obs_data_t *settings)
{
    mxl_output_data *output_data = static_cast<mxl_output_data*>(data);
    
    // Update configuration from settings
    MXLConfig* config = MXLConfig::Current();
    
    config->DomainPath = obs_data_get_string(settings, "domain_path");
    config->OutputEnabled = obs_data_get_bool(settings, "output_enabled");
    config->VideoEnabled = obs_data_get_bool(settings, "video_enabled");
    config->VideoFlowId = obs_data_get_string(settings, "video_flow_id");
    config->AudioEnabled = obs_data_get_bool(settings, "audio_enabled");
    config->AudioFlowId = obs_data_get_string(settings, "audio_flow_id");
    
    // Save to file
    config->Save();
    
    // Update the output data if it exists
    if (output_data) {
        output_data->domain_path = config->DomainPath;
        output_data->video_flow_id = config->VideoFlowId;
        output_data->video_enabled = config->VideoEnabled;
        output_data->audio_flow_id = config->AudioFlowId;
        output_data->audio_enabled = config->AudioEnabled;
    }
    
    // In a full implementation, you might want to:
    // - Create separate MXL flows for each video track
    // - Mix multiple tracks into a single output
    // - Allow user configuration of which tracks to use
}

} // extern "C"
