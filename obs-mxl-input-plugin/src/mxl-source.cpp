#include "mxl-source.h"
#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <graphics/image-file.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <sstream>
#include <sys/file.h>
#include <unistd.h>

// Version and build information
#define MXL_PLUGIN_VERSION "0.0.1-alpha"
#define MXL_BUILD_ID __DATE__ "_" __TIME__
#define MXL_BUILD_TIMESTAMP __DATE__ " " __TIME__

// MXL flow directory constants (from PathUtils.hpp)
constexpr auto const FLOW_DIRECTORY_NAME_SUFFIX = ".mxl-flow";
constexpr auto const FLOW_DESCRIPTOR_FILE_NAME = "flow_def.json";

// Simple JSON parser for flow descriptor
class SimpleJsonParser {
public:
    SimpleJsonParser(const std::string& json) : json_str(json) {}
    
    std::string getString(const std::string& key) {
        size_t key_pos = json_str.find("\"" + key + "\"");
        if (key_pos == std::string::npos) return "";
        
        size_t colon_pos = json_str.find(":", key_pos);
        if (colon_pos == std::string::npos) return "";
        
        size_t start = json_str.find("\"", colon_pos);
        if (start == std::string::npos) return "";
        start++;
        
        size_t end = json_str.find("\"", start);
        if (end == std::string::npos) return "";
        
        return json_str.substr(start, end - start);
    }
    
    double getNumber(const std::string& key) {
        size_t key_pos = json_str.find("\"" + key + "\"");
        if (key_pos == std::string::npos) return 0.0;
        
        size_t colon_pos = json_str.find(":", key_pos);
        if (colon_pos == std::string::npos) return 0.0;
        
        size_t start = colon_pos + 1;
        while (start < json_str.length() && (json_str[start] == ' ' || json_str[start] == '\t')) start++;
        
        size_t end = start;
        while (end < json_str.length() && (std::isdigit(json_str[end]) || json_str[end] == '.')) end++;
        
        if (start == end) return 0.0;
        
        return std::stod(json_str.substr(start, end - start));
    }
    
private:
    std::string json_str;
};

// Constructor
mxl_source_data::mxl_source_data()
    : source(nullptr)
    , mxl_instance(nullptr)
    , flow_reader(nullptr)
    , thread_active(false)
    , frame_data(nullptr)
    , frame_size(0)
    , audio_buffer(nullptr)
    , width(0)
    , height(0)
    , format(VIDEO_FORMAT_NONE)
    , current_grain_index(0)
    , frame_interval_ns(33333333) // Default to ~30fps
{
    memset(&flow_info, 0, sizeof(flow_info));
}

// Destructor
mxl_source_data::~mxl_source_data()
{
    cleanup_mxl();
    if (frame_data) {
        bfree(frame_data);
        frame_data = nullptr;
    }
}

bool mxl_source_data::initialize_mxl()
{
    cleanup_mxl();
    
    // Log version information
    blog(LOG_INFO, "MXL Source Plugin v%s [ID: %s] initializing flow %s", 
         MXL_PLUGIN_VERSION, MXL_BUILD_ID, flow_id.c_str());
    
    if (domain_path.empty() || flow_id.empty()) {
        blog(LOG_ERROR, "MXL Source: Domain path or flow ID not set");
        return false;
    }
    
    // Create MXL instance
    mxl_instance = mxlCreateInstance(domain_path.c_str(), "");
    if (!mxl_instance) {
        blog(LOG_ERROR, "MXL Source: Failed to create MXL instance for domain: %s", 
             domain_path.c_str());
        return false;
    }
    
    // Create flow reader
    mxlStatus status = mxlCreateFlowReader(mxl_instance, flow_id.c_str(), "", &flow_reader);
    if (status != MXL_STATUS_OK) {
        blog(LOG_ERROR, "MXL Source: Failed to create flow reader for flow: %s (status: %d)", 
             flow_id.c_str(), status);
        return false;
    }
    
    // Get flow info
    status = mxlFlowReaderGetInfo(flow_reader, &flow_info);
    if (status != MXL_STATUS_OK) {
        blog(LOG_ERROR, "MXL Source: Failed to get flow info (status: %d)", status);
        return false;
    }
    
    // Data flows is not supported
    if (flow_info.common.format == MXL_DATA_FORMAT_DATA) {
        blog(LOG_ERROR, "MXL Source: Data flows is not supported");
        return false;
    }

    // Read flow descriptor to get flow-specific information
    std::string descriptor_path = domain_path + "/" + flow_id + FLOW_DIRECTORY_NAME_SUFFIX + "/" + FLOW_DESCRIPTOR_FILE_NAME;
    std::ifstream descriptor_file(descriptor_path);
    if (!descriptor_file.is_open()) {
        blog(LOG_ERROR, "MXL Source: Failed to open flow descriptor: %s", descriptor_path.c_str());
        return false;
    }
    
    std::string flow_descriptor((std::istreambuf_iterator<char>(descriptor_file)), 
                               std::istreambuf_iterator<char>());
    descriptor_file.close();
    
    // Initialize audio
    if (flow_info.common.format == MXL_DATA_FORMAT_AUDIO) {
        if (! initialize_audio(flow_descriptor)) {
            return false;
        }
        // Start capture thread
        thread_active = true;
        capture_thread = std::thread(&mxl_source_data::capture_loop_audio, this);
        return true;
    }

    // Initialize video
    if (flow_info.common.format == MXL_DATA_FORMAT_VIDEO) {
        if (! initialize_video(flow_descriptor)) {
            return false;
        }
        // Start capture thread
        thread_active = true;
        capture_thread = std::thread(&mxl_source_data::capture_loop_video, this);
    }
    
    return true;
}

bool mxl_source_data::initialize_video(std::string& flow_descriptor) 
{
    SimpleJsonParser parser(flow_descriptor);
    // Parse video information
    width = static_cast<uint32_t>(parser.getNumber("frame_width"));
    height = static_cast<uint32_t>(parser.getNumber("frame_height"));
    std::string media_type = parser.getString("media_type");
    
    if (width == 0 || height == 0) {
        blog(LOG_ERROR, "MXL Source: Invalid video dimensions: %dx%d", width, height);
        return false;
    }
    
    // Calculate frame interval from grain rate
    if (flow_info.discrete.grainRate.numerator > 0) {
        frame_interval_ns = (1000000000ULL * flow_info.discrete.grainRate.denominator) / 
                           flow_info.discrete.grainRate.numerator;
    }
    
    // Determine video format based on media type
    format = get_obs_format_from_mxl(media_type);
    
    blog(LOG_INFO, "MXL Source: Initialized video flow %dx%d, format: %s, fps: %.2f", 
         width, height, media_type.c_str(), 
         (double)flow_info.discrete.grainRate.numerator / flow_info.discrete.grainRate.denominator);
    
    // Calculate proper frame buffer size based on format
    frame_size = calculate_frame_size(format, width, height);
    frame_data = (uint8_t*)bmalloc(frame_size);
    if (!frame_data) {
        blog(LOG_ERROR, "MXL Source: Failed to allocate frame buffer");
        return false;
    }

    // Disable audio on video flows
    obs_source_set_audio_active(source, false);
    
    return true;
}

bool mxl_source_data::initialize_audio(std::string& flow_descriptor) 
{
    obs_source_set_audio_active(source, true);
    obs_source_output_video(source, nullptr);

    return true;
}

void mxl_source_data::cleanup_mxl()
{
    // Stop capture thread
    if (thread_active) {
        thread_active = false;
        if (capture_thread.joinable()) {
            capture_thread.join();
        }
    }
     
    // Release MXL resources
    if (flow_reader) {
        mxlReleaseFlowReader(mxl_instance, flow_reader);
        flow_reader = nullptr;
    }
    
    if (mxl_instance) {
        mxlDestroyInstance(mxl_instance);
        mxl_instance = nullptr;
    }
}

void mxl_source_data::capture_loop_audio()
{
    // Huge thx for mxl-gst tools from Riedel developers
    blog(LOG_INFO, "MXL Audio Source: Capture thread started %u", sample_amount);
    
    mxlRational const& rational_rate = flow_info.continuous.sampleRate;
    sample_rate = rational_rate.numerator / rational_rate.denominator;
    audio_buffer_size = sample_amount * sizeof(float);

    mxlStatus status = mxlFlowReaderGetInfo(flow_reader, &flow_info);    
    current_grain_index = mxlGetCurrentIndex(&rational_rate);
    blog(LOG_INFO, "MXL Audio Source: Starting from grain index %lu", current_grain_index);

    uint64_t last_logged_index = 0;
    while (thread_active) {
        mxlWrappedMultiBufferSlice payload;
        status = mxlFlowReaderGetSamples(flow_reader, current_grain_index - sample_amount, sample_amount, &payload);
        if (status == MXL_ERR_OUT_OF_RANGE_TOO_EARLY) {
            // We are too early somehow, keep trying the same index
            if (current_grain_index != last_logged_index) {
                mxlFlowReaderGetInfo(flow_reader, &flow_info);
                blog(LOG_WARNING, "MXL Audio Source: Failed to get samples at index %lu: TOO EARLY. Last published %lu", 
                    current_grain_index,
                    flow_info.continuous.headIndex
                );
                last_logged_index = current_grain_index;
            }
            continue;
        }
        else if (status == MXL_ERR_OUT_OF_RANGE_TOO_LATE) {
            // We are too late, that's too bad. Time travel!
            if (current_grain_index != last_logged_index) {
                blog(LOG_WARNING, "MXL Audio Source: Failed to get samples at index %lu: TOO LATE", current_grain_index);
                last_logged_index = current_grain_index;
            }
            current_grain_index = mxlGetCurrentIndex(&rational_rate);
            continue;
        }
        else if (status != MXL_STATUS_OK) {
            blog(LOG_ERROR, "MXL Audio Source: Unexpected error when reading the grain %lu with status '%d'",
                current_grain_index,
                static_cast<int>(status)
            );
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        struct obs_source_audio audio = {};
        audio.frames = sample_amount;
        audio.speakers = SPEAKERS_MONO;
        audio.format = AUDIO_FORMAT_FLOAT;
        audio.samples_per_sec = sample_rate;
        audio.timestamp = os_gettime_ns();

        if (audio_buffer) {
            bfree(audio_buffer);
        }
        audio_buffer = static_cast<uint8_t*>(bmalloc(audio_buffer_size));
        audio.data[0] = audio_buffer;

        uint8_t channel = selected_channel < payload.count ? selected_channel : 0;
        auto current_write_ptr{audio_buffer};
        for (int i = 0; i < 2; ++i){
            auto const read_ptr{static_cast<std::byte const*>(payload.base.fragments[i].pointer) + channel * payload.stride};
            auto const read_size{payload.base.fragments[i].size};
            ::memcpy(current_write_ptr, read_ptr, read_size);
            current_write_ptr += read_size;
        }
        obs_source_output_audio(source, &audio);

        current_grain_index += sample_amount;
        mxlSleepForNs(mxlGetNsUntilIndex(current_grain_index, &rational_rate));
    }
    
    obs_source_output_audio(source, nullptr);
    blog(LOG_INFO, "MXL Audio Source: Capture thread stopped");
}

void mxl_source_data::capture_loop_video()
{
    blog(LOG_INFO, "MXL Source: Capture thread started");
    
    // Get current grain index - start from current head
    mxlStatus status = mxlFlowReaderGetInfo(flow_reader, &flow_info);
    if (status == MXL_STATUS_OK) {
        current_grain_index = flow_info.discrete.headIndex;
        blog(LOG_INFO, "MXL Source: Starting from grain index %lu", current_grain_index);
    } else {
        blog(LOG_WARNING, "MXL Source: Failed to get initial flow info, starting from 0");
        current_grain_index = 0;
    }
    
    while (thread_active) {
        mxlGrainInfo grain_info;
        uint8_t *payload = nullptr;
        
        // Try to get the next grain with timeout
        status = mxlFlowReaderGetGrain(flow_reader, current_grain_index, 
                                      frame_interval_ns + 1000000, // Add 1ms margin
                                      &grain_info, &payload);
        
        if (status == MXL_STATUS_OK && payload) {
            if (process_grain_video(grain_info, payload)) {
                // Create video frame structure for OBS
                struct obs_source_frame frame = {};
                
                // Set up RGBA frame data
                frame.data[0] = frame_data;
                frame.linesize[0] = width * 4; // RGBA is 4 bytes per pixel
                frame.width = width;
                frame.height = height;
                frame.format = VIDEO_FORMAT_RGBA;
                frame.timestamp = os_gettime_ns();
                frame.full_range = true;
                
                // Debug: log frame setup details once
                static bool frame_debug_logged = false;
                if (!frame_debug_logged) {
                    blog(LOG_INFO, "MXL Source: OBS Frame setup - width:%d height:%d format:RGBA", 
                         frame.width, frame.height);
                    blog(LOG_INFO, "MXL Source: Frame data pointer: %p, linesize: %d", 
                         frame.data[0], frame.linesize[0]);
                    frame_debug_logged = true;
                }
                
                // Add some debug logging
                static uint64_t frame_count = 0;
                frame_count++;
                if (frame_count % 50 == 0) { // Log every 50 frames
                    blog(LOG_INFO, "MXL Source: Processed frame %lu, grain %lu", 
                         frame_count, current_grain_index);
                }
                
                // Signal OBS that new frame is available
                obs_source_output_video(source, &frame);
            } else {
                blog(LOG_WARNING, "MXL Source: Failed to process grain %lu", current_grain_index);
            }
            current_grain_index++;
        } else if (status == MXL_ERR_TIMEOUT) {
            // No new frame available, continue
            static int timeout_count = 0;
            timeout_count++;
            if (timeout_count % 100 == 0) { // Log every 100 timeouts
                blog(LOG_DEBUG, "MXL Source: Timeout waiting for grain %lu (count: %d)", 
                     current_grain_index, timeout_count);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            blog(LOG_WARNING, "MXL Source: Failed to get grain %lu (status: %d)", 
                 current_grain_index, status);
            // Don't increment grain index on error, try the same grain again
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    blog(LOG_INFO, "MXL Source: Capture thread stopped");
}

bool mxl_source_data::process_grain_video(const mxlGrainInfo &grain_info, uint8_t *payload)
{
    if (!payload || grain_info.validSlices != grain_info.totalSlices) {
        return false;
    }
    
    // Check if grain is marked as invalid
    if (grain_info.flags & MXL_GRAIN_FLAG_INVALID) {
        blog(LOG_DEBUG, "MXL Source: Received invalid grain, skipping");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(frame_mutex);
    
    // Add debug info for first few frames
    static int debug_count = 0;
    if (debug_count < 5) {
        blog(LOG_INFO, "MXL Source: Processing grain size %u, first bytes: %02x %02x %02x %02x", 
             grain_info.grainSize, payload[0], payload[1], payload[2], payload[3]);
        debug_count++;
    }
    
    // Convert v210 to RGBA
    static bool logged_conversion = false;
    if (!logged_conversion) {
        blog(LOG_INFO, "MXL Source: Converting v210 data to RGBA format");
        logged_conversion = true;
    }
    convert_v210_to_rgba(payload, grain_info.grainSize, frame_data, frame_size);
    
    return true;
}



enum video_format mxl_source_data::get_obs_format_from_mxl(const std::string &media_type)
{
    // All formats are converted to RGBA for OBS compatibility
    blog(LOG_INFO, "MXL Source: Converting media type '%s' to RGBA format", media_type.c_str());
    return VIDEO_FORMAT_RGBA;
}

size_t mxl_source_data::calculate_frame_size(enum video_format format, uint32_t width, uint32_t height)
{
    // Only RGBA format is supported
    if (format == VIDEO_FORMAT_RGBA) {
        return width * height * 4; // 4 bytes per pixel (RGBA)
    }
    
    // Fallback (should not happen)
    blog(LOG_WARNING, "MXL Source: Unsupported format %d, using RGBA fallback", format);
    return width * height * 4;
}

// OBS Source Callbacks
const char *mxl_source_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return "MXL Flow Source";
}

void *mxl_source_create(obs_data_t *settings, obs_source_t *source)
{
    mxl_source_data *data = new mxl_source_data();
    data->source = source;
    
    mxl_source_update(data, settings);
    
    return data;
}

void mxl_source_destroy(void *data)
{
    mxl_source_data *mxl_data = static_cast<mxl_source_data*>(data);
    delete mxl_data;
}

void mxl_source_update(void *data, obs_data_t *settings)
{
    mxl_source_data *mxl_data = static_cast<mxl_source_data*>(data);
    
    const char *domain = obs_data_get_string(settings, "domain_path");
    const char *flow_id = obs_data_get_string(settings, "flow_id");
    uint8_t selected_channel = obs_data_get_int(settings, "selected_channel"); 
    uint32_t sample_amount = obs_data_get_int(settings, "sample_amount");   
    bool needs_restart = false;
    
    if (mxl_data->domain_path != domain) {
        mxl_data->domain_path = domain ? domain : "";
        needs_restart = true;
    }
    
    if (mxl_data->flow_id != flow_id) {
        mxl_data->flow_id = flow_id ? flow_id : "";
        needs_restart = true;
    }

    if (mxl_data->selected_channel != selected_channel) {
        mxl_data->selected_channel = selected_channel;
        needs_restart = true;
    }

    if (mxl_data->sample_amount != sample_amount) {
        mxl_data->sample_amount = sample_amount;
        needs_restart = true;
    }
    
    if (needs_restart && !mxl_data->domain_path.empty() && !mxl_data->flow_id.empty()) {
        mxl_data->initialize_mxl();
    }
}

// Callback function for when domain path changes
static bool domain_path_changed(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
    UNUSED_PARAMETER(property);
    
    const char *domain_path = obs_data_get_string(settings, "domain_path");
    obs_property_t *flow_list = obs_properties_get(props, "flow_id");
    
    if (!flow_list) {
        return false;
    }
    
    // Clear existing items
    obs_property_list_clear(flow_list);
    blog(LOG_INFO, "DOMAIN PATH: %s (%ld)", domain_path, strlen(domain_path));
    if (domain_path && strlen(domain_path) > 0) {
        // Create a temporary mxl_source_data to use the discovery methods
        mxl_source_data temp_data;
        std::vector<mxl_flow_info> flows = temp_data.discover_flows(std::string(domain_path));
        
        // Add flows to the dropdown
        for (const auto& flow : flows) {
            std::string display_name;
            if (!flow.label.empty() && flow.label != flow.id) {
                display_name = flow.label + " (" + flow.id + ")";
            } else {
                display_name = flow.id;
            }
            
            if (flow.active) {
                display_name += " [Active]";
            }
            
            if (!flow.description.empty()) {
                display_name += " - " + flow.description;
            }
            
            obs_property_list_add_string(flow_list, display_name.c_str(), flow.id.c_str());
        }
        
        if (flows.empty()) {
            obs_property_list_add_string(flow_list, "No flows found", "");
        }
    } else {
        obs_property_list_add_string(flow_list, "Enter domain path first", "");
    }
    
    return true;
}

// Callback function for refresh button
static bool refresh_flows_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
    UNUSED_PARAMETER(property);
    
    // Get the current settings to access domain path
    obs_property_t *domain_prop = obs_properties_get(props, "domain_path");
    struct mxl_source_data *mxl_data = (struct mxl_source_data *)data;
    if (!mxl_data || !mxl_data->source) {
        return false;
    }
    obs_data_t *settings = obs_source_get_settings(mxl_data->source);
    domain_path_changed(props, domain_prop, settings);
    return true;
}

static bool restart_flow_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
    struct mxl_source_data *mxl_data = (struct mxl_source_data *)data;
    if (!mxl_data || !mxl_data->source) {
        return false;
    }
    obs_data_t *settings = obs_source_get_settings(mxl_data->source);
    if (mxl_data->domain_path.empty() || mxl_data->flow_id.empty()) {
        return false;
    }
    mxl_data->initialize_mxl();
    return true;
}

obs_properties_t *mxl_source_get_properties(void *data)
{
    UNUSED_PARAMETER(data);
    
    obs_properties_t *props = obs_properties_create();
    
    // Add version information display
    char version_info[256];
    snprintf(version_info, sizeof(version_info), 
             "MXL Plugin v%s (Build: %s)", 
             MXL_PLUGIN_VERSION, MXL_BUILD_ID);
    obs_properties_add_text(props, "version_info", "Plugin Version", OBS_TEXT_INFO);
    
    // Domain path input with callback
    obs_property_t *domain_prop = obs_properties_add_text(props, "domain_path", "MXL Domain Path", OBS_TEXT_DEFAULT);
    obs_property_set_modified_callback(domain_prop, domain_path_changed);
    
    // Flow selection dropdown
    obs_property_t *flow_prop = obs_properties_add_list(props, "flow_id", "Available Flows", 
                                                       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    
    // Add initial placeholder
    obs_property_list_add_string(flow_prop, "Enter domain path first", "");
    
    // Add refresh button
    obs_properties_add_button(props, "refresh_flows", "Refresh Flow List", refresh_flows_clicked);
    
    // For audio flows only. Channel selection
    obs_properties_add_text(props, "audio_header", "Audio Settings. Only one channel is supported", OBS_TEXT_INFO);
    obs_property_t *channel_prop = obs_properties_add_list(props, "selected_channel", "Selected audio channel", 
                                                    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    for (int i = 0; i < 16; ++i){
        char channel_name[16];
        snprintf(channel_name, sizeof(channel_name), "Channel %i", i + 1);
        obs_property_list_add_int(channel_prop, channel_name, i);
    }

    // Amount of sample per batch
    obs_property_t *samples_per_batch = obs_properties_add_int(
        props,
        "sample_amount",
        "Number of audio samples per batch/buffer", 
        1,
        4096,
        1
    );

    // Add restart button
    obs_properties_add_button(props, "restart_flow_capture", "Restart flow capture", restart_flow_clicked);
    
    return props;
}

void mxl_source_get_defaults(obs_data_t *settings)
{
    // Set version information
    char version_info[256];
    snprintf(version_info, sizeof(version_info), 
             "MXL Plugin v%s (Build: %s)", 
             MXL_PLUGIN_VERSION, MXL_BUILD_ID);
    obs_data_set_default_string(settings, "version_info", version_info);
    
    obs_data_set_default_string(settings, "domain_path", "/tmp/mxl_domain");
    obs_data_set_default_string(settings, "flow_id", "5fbec3b1-1b0f-417d-9059-8b94a47197ef");
    obs_data_set_default_int(settings, "selected_channel", 0);
    obs_data_set_default_int(settings, "sample_amount", 128);
}

uint32_t mxl_source_get_width(void *data)
{
    mxl_source_data *mxl_data = static_cast<mxl_source_data*>(data);
    return mxl_data->width;
}

uint32_t mxl_source_get_height(void *data)
{
    mxl_source_data *mxl_data = static_cast<mxl_source_data*>(data);
    return mxl_data->height;
}

void mxl_source_video_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(seconds);
    // Frame timing is handled by the capture thread
}

void mxl_source_video_render(void *data, gs_effect_t *effect)
{
    // Frame rendering is now handled by obs_source_output_video in capture thread
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(effect);
}

void mxl_source_data::convert_v210_to_rgba(uint8_t *v210_data, size_t v210_size, 
                                           uint8_t *rgba_data, size_t rgba_size)
{
    // Convert v210 (10-bit YUV 4:2:2 packed) to RGBA
    static bool debug_logged = false;
    if (!debug_logged) {
        blog(LOG_INFO, "MXL Source: Converting v210 (%zu bytes) to RGBA (%zu bytes), dimensions %dx%d", 
             v210_size, rgba_size, width, height);
        debug_logged = true;
    }
    
    uint32_t *v210_words = (uint32_t*)v210_data;
    uint32_t *rgba_pixels = (uint32_t*)rgba_data;
    
    // v210 packing: 4 32-bit words contain 6 pixels
    size_t v210_words_per_line = ((width + 5) / 6) * 4;
    
    for (uint32_t line = 0; line < height; line++) {
        uint32_t *line_start = v210_words + (line * v210_words_per_line);
        
        for (uint32_t x = 0; x < width; x += 6) {
            uint32_t word_group = x / 6;
            
            if (word_group * 4 + 3 >= v210_words_per_line) break;
            
            uint32_t w0 = line_start[word_group * 4 + 0];
            uint32_t w1 = line_start[word_group * 4 + 1];
            uint32_t w2 = line_start[word_group * 4 + 2];
            uint32_t w3 = line_start[word_group * 4 + 3];
            
            // Extract YUV values from v210 packing
            // v210 format: Cb0 Y0 Cr0 | Y1 Cb1 Y2 | Cr1 Y3 Cb2 | Y4 Cr2 Y5
            uint16_t cb0 = (w0 >> 0) & 0x3FF;   // U
            uint16_t y0 = (w0 >> 10) & 0x3FF;   // Y
            uint16_t cr0 = (w0 >> 20) & 0x3FF;  // V
            
            uint16_t y1 = (w1 >> 0) & 0x3FF;
            uint16_t cb1 = (w1 >> 10) & 0x3FF;
            uint16_t y2 = (w1 >> 20) & 0x3FF;
            
            uint16_t cr1 = (w2 >> 0) & 0x3FF;
            uint16_t y3 = (w2 >> 10) & 0x3FF;
            uint16_t cb2 = (w2 >> 20) & 0x3FF;
            
            uint16_t y4 = (w3 >> 0) & 0x3FF;
            uint16_t cr2 = (w3 >> 10) & 0x3FF;
            uint16_t y5 = (w3 >> 20) & 0x3FF;
            
            // Convert 10-bit to 8-bit
            uint8_t y_vals[6] = {
                (uint8_t)(y0 >> 2), (uint8_t)(y1 >> 2), (uint8_t)(y2 >> 2),
                (uint8_t)(y3 >> 2), (uint8_t)(y4 >> 2), (uint8_t)(y5 >> 2)
            };
            
            // Convert chroma values to 8-bit
            uint8_t u_vals[3] = { (uint8_t)(cb0 >> 2), (uint8_t)(cb1 >> 2), (uint8_t)(cb2 >> 2) };
            uint8_t v_vals[3] = { (uint8_t)(cr0 >> 2), (uint8_t)(cr1 >> 2), (uint8_t)(cr2 >> 2) };
            
            // Convert YUV to RGBA for each pixel
            for (int i = 0; i < 6 && (x + i) < width; i++) {
                uint32_t pixel_idx = line * width + x + i;
                if (pixel_idx < width * height) {
                    uint8_t y = y_vals[i];
                    uint8_t u = u_vals[i / 2]; // 4:2:2 subsampling - 2 Y per UV
                    uint8_t v = v_vals[i / 2];
                    
                    // Swap U and V to fix red/blue color swap
                    uint8_t temp = u;
                    u = v;
                    v = temp;
                    
                    // YUV to RGB conversion using BT.709 coefficients
                    float yf = (float)y;
                    float uf = (float)u - 128.0f;
                    float vf = (float)v - 128.0f;
                    
                    int r = (int)(yf + 1.5748f * vf);
                    int g = (int)(yf - 0.1873f * uf - 0.4681f * vf);
                    int b = (int)(yf + 1.8556f * uf);
                    
                    // Clamp to 0-255 range
                    r = (r < 0) ? 0 : (r > 255) ? 255 : r;
                    g = (g < 0) ? 0 : (g > 255) ? 255 : g;
                    b = (b < 0) ? 0 : (b > 255) ? 255 : b;
                    
                    // RGBA format: A=255, R, G, B
                    rgba_pixels[pixel_idx] = (255 << 24) | (r << 16) | (g << 8) | b;
                }
            }
        }
    }
}

// Flow discovery implementation
std::vector<mxl_flow_info> mxl_source_data::discover_flows(const std::string &domain_path)
{
    std::vector<mxl_flow_info> flows;
    
    if (domain_path.empty()) {
        return flows;
    }
    
    try {
        std::filesystem::path base(domain_path);
        
        if (!std::filesystem::exists(base) || !std::filesystem::is_directory(base)) {
            blog(LOG_WARNING, "MXL Source: Domain path does not exist or is not a directory: %s", domain_path.c_str());
            return flows;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(base)) {
            if (std::filesystem::is_directory(entry) && 
                entry.path().extension() == FLOW_DIRECTORY_NAME_SUFFIX) {
                
                std::string flow_id = entry.path().stem().string();
                
                // Validate UUID format (basic check)
                if (flow_id.length() == 36 && flow_id[8] == '-' && flow_id[13] == '-' && 
                    flow_id[18] == '-' && flow_id[23] == '-') {
                    
                    std::string descriptor_path = entry.path() / FLOW_DESCRIPTOR_FILE_NAME;
                    mxl_flow_info flow_info = get_flow_info_from_descriptor(flow_id, descriptor_path);
                    flow_info.active = is_flow_active(domain_path, flow_id);
                    
                    flows.push_back(flow_info);
                }
            }
        }
        
        blog(LOG_INFO, "MXL Source: Discovered %zu flows in domain %s", flows.size(), domain_path.c_str());
        
    } catch (const std::exception& e) {
        blog(LOG_ERROR, "MXL Source: Error discovering flows: %s", e.what());
    }
    
    return flows;
}

mxl_flow_info mxl_source_data::get_flow_info_from_descriptor(const std::string &flow_id, const std::string &descriptor_path)
{
    mxl_flow_info info;
    info.id = flow_id;
    info.label = flow_id; // Default to ID if no label found
    info.description = "";
    info.format = "";
    info.active = false;
    
    try {
        if (std::filesystem::exists(descriptor_path)) {
            std::ifstream file(descriptor_path);
            if (file.is_open()) {
                std::string json_content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                file.close();
                
                // Use our simple JSON parser
                SimpleJsonParser parser(json_content);
                
                std::string label = parser.getString("label");
                if (!label.empty()) {
                    info.label = label;
                }
                
                std::string description = parser.getString("description");
                if (!description.empty()) {
                    info.description = description;
                }
                
                std::string format = parser.getString("format");
                if (!format.empty()) {
                    info.format = format;
                }
            }
        }
    } catch (const std::exception& e) {
        blog(LOG_WARNING, "MXL Source: Error reading flow descriptor for %s: %s", flow_id.c_str(), e.what());
    }
    
    return info;
}

bool mxl_source_data::is_flow_active(const std::string &domain_path, const std::string &flow_id)
{
    try {
        std::filesystem::path flow_dir = std::filesystem::path(domain_path) / (flow_id + FLOW_DIRECTORY_NAME_SUFFIX);
        std::filesystem::path data_file = flow_dir / "data";
        
        if (!std::filesystem::exists(data_file)) {
            return false;
        }
        
        // Try to obtain an exclusive lock on the flow data file
        // If we can obtain one, it means no other process is writing to the flow
        int fd = open(data_file.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            return false;
        }
        
        // Try to obtain an exclusive lock on the file descriptor
        // Do not block if the lock cannot be obtained
        bool active = flock(fd, LOCK_EX | LOCK_NB) < 0;
        close(fd);
        
        return active;
        
    } catch (const std::exception& e) {
        blog(LOG_WARNING, "MXL Source: Error checking flow activity for %s: %s", flow_id.c_str(), e.what());
        return false;
    }
}
