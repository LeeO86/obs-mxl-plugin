#include "mxl-output.h"
#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <mxl/time.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <sstream>
#include <random>
#include <iomanip>
#include <inttypes.h>

// Version and build information
#define MXL_OUTPUT_PLUGIN_VERSION "0.0.1"
#define MXL_BUILD_ID __DATE__ "_" __TIME__
#define MXL_BUILD_TIMESTAMP __DATE__ " " __TIME__

// MXL flow directory constants
constexpr auto const FLOW_DIRECTORY_NAME_SUFFIX = ".mxl-flow";
constexpr auto const FLOW_DESCRIPTOR_FILE_NAME = ".json";

// Constructor
mxl_output_data::mxl_output_data()
    : output(nullptr)
    , mxl_instance(nullptr)
    , video_flow_writer(nullptr)
    , video_enabled(true)
    , video_width(0)
    , video_height(0)
    , video_fps_num(30)
    , video_fps_den(1)
    , video_format(VIDEO_FORMAT_NONE)
    , thread_active(false)
    , output_active(false)
    , video_grain_index(0)
    , start_timestamp(0)
    , video_frame_interval_ns(33333333) // Default to ~30fps
{
}

// Destructor
mxl_output_data::~mxl_output_data()
{
    cleanup_mxl();
}

std::string mxl_output_data::generate_uuid()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; i++) {
        ss << dis(gen);
    }
    return ss.str();
}

bool mxl_output_data::initialize_mxl()
{
    cleanup_mxl();
    
    blog(LOG_INFO, "MXL Output Plugin v%s [ID: %s] initializing domain: %s", 
         MXL_OUTPUT_PLUGIN_VERSION, MXL_BUILD_ID, domain_path.c_str());
    
    if (domain_path.empty()) {
        blog(LOG_ERROR, "MXL Output: Domain path not set");
        return false;
    }
    
    // Create domain directory if it doesn't exist
    std::filesystem::path domain_dir(domain_path);
    if (!std::filesystem::exists(domain_dir)) {
        try {
            std::filesystem::create_directories(domain_dir);
            blog(LOG_INFO, "MXL Output: Created domain directory: %s", domain_path.c_str());
        } catch (const std::exception& e) {
            blog(LOG_ERROR, "MXL Output: Failed to create domain directory: %s", e.what());
            return false;
        }
    }
    
    // Create MXL instance
    mxl_instance = mxlCreateInstance(domain_path.c_str(), "");
    if (!mxl_instance) {
        blog(LOG_ERROR, "MXL Output: Failed to create MXL instance for domain: %s", 
             domain_path.c_str());
        return false;
    }
    
    // Generate flow ID if not set
    if (video_enabled && video_flow_id.empty()) {
        video_flow_id = generate_uuid();
        blog(LOG_INFO, "MXL Output: Generated video flow ID: %s", video_flow_id.c_str());
    }
    
    // Create video flow using mxlCreateFlow (which handles descriptor creation)
    if (video_enabled && !create_video_flow()) {
        blog(LOG_ERROR, "MXL Output: Failed to create video flow");
        return false;
    }
    
    blog(LOG_INFO, "MXL Output: Successfully initialized MXL flows");
    return true;
}

void mxl_output_data::cleanup_mxl()
{
    // Stop output thread
    if (thread_active) {
        thread_active = false;
        frame_condition.notify_all();
        if (output_thread.joinable()) {
            output_thread.join();
        }
    }
    
    // Clear frame queues
    {
        std::lock_guard<std::mutex> lock(video_queue_mutex);
        while (!video_queue.empty()) {
            video_queue.pop();
        }
    }
    
    // Release MXL resources
    if (video_flow_writer) {
        mxlReleaseFlowWriter(mxl_instance, video_flow_writer);
        video_flow_writer = nullptr;
    }
    
    // Destroy flows (like GStreamer example)
    if (mxl_instance) {
        if (!video_flow_id.empty()) {
            mxlDestroyFlow(mxl_instance, video_flow_id.c_str());
        }
        mxlDestroyInstance(mxl_instance);
        mxl_instance = nullptr;
    }
}

bool mxl_output_data::create_video_flow()
{
    if (!video_enabled || video_flow_id.empty()) {
        return true;
    }
    
    // Create the flow using the descriptor
    std::string flow_descriptor = generate_flow_descriptor_json(true);
    FlowInfo flow_info_temp;
    
    mxlStatus status = mxlCreateFlow(mxl_instance, flow_descriptor.c_str(), nullptr, &flow_info_temp);
    if (status != MXL_STATUS_OK) {
        blog(LOG_ERROR, "MXL Output: Failed to create video flow (status: %d)", status);
        return false;
    }
    
    // Create the flow writer
    status = mxlCreateFlowWriter(mxl_instance, video_flow_id.c_str(), "", &video_flow_writer);
    if (status != MXL_STATUS_OK) {
        blog(LOG_ERROR, "MXL Output: Failed to create video flow writer for flow: %s (status: %d)", 
             video_flow_id.c_str(), status);
        
        // Clean up the flow we created
        mxlDestroyFlow(mxl_instance, video_flow_id.c_str());
        return false;
    }
    
    return true;
}


bool mxl_output_data::create_video_flow_descriptor()
{
    if (!video_enabled || video_flow_id.empty()) {
        return true;
    }
    
    std::string flow_dir = domain_path + "/" + video_flow_id + FLOW_DIRECTORY_NAME_SUFFIX;
    std::string descriptor_path = flow_dir + "/" + FLOW_DESCRIPTOR_FILE_NAME;
    
    // Create flow directory
    try {
        std::filesystem::create_directories(flow_dir);
    } catch (const std::exception& e) {
        blog(LOG_ERROR, "MXL Output: Failed to create video flow directory: %s", e.what());
        return false;
    }
    
    // Generate and write descriptor JSON
    std::string descriptor_json = generate_flow_descriptor_json(true);
    std::ofstream descriptor_file(descriptor_path);
    if (!descriptor_file.is_open()) {
        blog(LOG_ERROR, "MXL Output: Failed to create video flow descriptor file: %s", descriptor_path.c_str());
        return false;
    }
    
    descriptor_file << descriptor_json;
    descriptor_file.close();
    
    blog(LOG_INFO, "MXL Output: Created video flow descriptor: %s", descriptor_path.c_str());
    return true;
}


std::string mxl_output_data::get_mxl_video_media_type(enum video_format format)
{
    // MXL flows should use v210 format for video
    return "video/v210";
}



size_t mxl_output_data::calculate_video_frame_size(enum video_format format, uint32_t width, uint32_t height)
{
    // For MXL output, we always convert to v210 format
    // v210: Each group of 6 pixels = 16 bytes (4 x 32-bit words)
    uint32_t v210_stride = ((width + 5) / 6) * 16; // Round up to nearest 6-pixel group
    v210_stride = (v210_stride + 3) & ~3; // Ensure 4-byte alignment
    return v210_stride * height;
}


std::string mxl_output_data::generate_flow_descriptor_json(bool is_video)
{
    std::stringstream ss;
    
    ss << "{\n";
    ss << "  \"description\": \"MXL Video Output Flow\",\n";
    ss << "  \"id\": \"" << video_flow_id << "\",\n";
    ss << "  \"tags\": {},\n";
    ss << "  \"format\": \"urn:x-nmos:format:video\",\n";
    ss << "  \"label\": \"MXL Video Output\",\n";
    ss << "  \"parents\": [],\n";
    ss << "  \"media_type\": \"video/v210\",\n";
    ss << "  \"grain_rate\": {\n";
    ss << "    \"numerator\": " << video_fps_num << ",\n";
    ss << "    \"denominator\": " << video_fps_den << "\n";
    ss << "  },\n";
    ss << "  \"frame_width\": " << video_width << ",\n";
    ss << "  \"frame_height\": " << video_height << ",\n";
    ss << "  \"colorspace\": \"BT709\",\n";
    ss << "  \"components\": [\n";
    ss << "    {\n";
    ss << "      \"name\": \"Y\",\n";
    ss << "      \"width\": " << video_width << ",\n";
    ss << "      \"height\": " << video_height << ",\n";
    ss << "      \"bit_depth\": 10\n";
    ss << "    },\n";
    ss << "    {\n";
    ss << "      \"name\": \"Cb\",\n";
    ss << "      \"width\": " << (video_width / 2) << ",\n";
    ss << "      \"height\": " << video_height << ",\n";
        ss << "      \"bit_depth\": 10\n";
        ss << "    },\n";
        ss << "    {\n";
        ss << "      \"name\": \"Cr\",\n";
        ss << "      \"width\": " << (video_width / 2) << ",\n";
        ss << "      \"height\": " << video_height << ",\n";
        ss << "      \"bit_depth\": 10\n";
        ss << "    }\n";
        ss << "  ]\n";
        ss << "}";
    
    return ss.str();
}

void mxl_output_data::output_loop()
{
    blog(LOG_INFO, "MXL Output: Output thread started");
    
    while (thread_active) {
        std::unique_lock<std::mutex> lock(video_queue_mutex);
        
        // Wait for frames or thread termination with timeout
        frame_condition.wait_for(lock, std::chrono::milliseconds(100), [this] {
            return !thread_active || !video_queue.empty();
        });
        
        if (!thread_active) {
            break;
        }
        
        // Process video frames
        while (!video_queue.empty()) {
            auto frame = std::move(video_queue.front());
            video_queue.pop();
            lock.unlock();
            
            if (!process_video_frame(std::move(frame))) {
                blog(LOG_WARNING, "MXL Output: Failed to process video frame");
            }
            
            lock.lock();
        }
    }
    
    blog(LOG_INFO, "MXL Output: Output thread stopped");
}

bool mxl_output_data::process_video_frame(std::unique_ptr<video_frame_data> frame)
{
    if (!video_flow_writer || !frame) {
        return false;
    }
    
    // Use proper MXL timing like GStreamer example - get current time-based index
    Rational frame_rate = {static_cast<int32_t>(video_fps_num), static_cast<int32_t>(video_fps_den)};
    uint64_t grain_index = mxlGetCurrentIndex(&frame_rate);
    
    // Log grain writing occasionally to avoid spam
    static uint64_t last_logged_video_grain = 0;
    if (grain_index % 100 == 0 && grain_index != last_logged_video_grain) {
        blog(LOG_INFO, "MXL Output: Writing video grain %" PRIu64 " (rate: %lld/%lld)", 
             grain_index, (long long)frame_rate.numerator, (long long)frame_rate.denominator);
        last_logged_video_grain = grain_index;
    }
    
    GrainInfo grain_info = {};
    uint8_t* payload = nullptr;
    
    // Open grain for writing
    mxlStatus status = mxlFlowWriterOpenGrain(video_flow_writer, grain_index, &grain_info, &payload);
    if (status != MXL_STATUS_OK) {
        blog(LOG_ERROR, "MXL Output: Failed to open video grain %" PRIu64 " (status: %d)", grain_index, status);
        return false;
    }
    
    // Set grain info
    grain_info.flags = 0;
    
    // Copy frame data to MXL payload
    if (payload && frame->data && frame->size > 0) {
        size_t copy_size = std::min(frame->size, static_cast<size_t>(grain_info.grainSize));
        memcpy(payload, frame->data, copy_size);
        grain_info.commitedSize = copy_size;
        
        blog(LOG_DEBUG, "MXL Output: Copied %zu bytes to video grain %" PRIu64 " (grain size: %u)", 
             copy_size, grain_index, grain_info.grainSize);
    }
    
    // Commit the grain
    status = mxlFlowWriterCommitGrain(video_flow_writer, &grain_info);
    if (status != MXL_STATUS_OK) {
        blog(LOG_ERROR, "MXL Output: Failed to commit video grain %" PRIu64 " (status: %d)", grain_index, status);
        mxlFlowWriterCancelGrain(video_flow_writer);
        return false;
    }
    
    // Update our counter for statistics
    video_grain_index.fetch_add(1);
    
    return true;
}



uint64_t mxl_output_data::get_timestamp_ns()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

bool mxl_output_data::convert_to_v210(uint8_t *src_data, enum video_format src_format, 
                                     uint32_t width, uint32_t height, uint32_t *linesize,
                                     uint8_t *dst_data, size_t dst_size,
                                     uint8_t **data_planes)
{
    if (!src_data || !dst_data) {
        return false;
    }
    
    // v210 format: 10-bit YUV 4:2:2 packed
    // Each group of 6 pixels = 16 bytes (4 x 32-bit words)
    // v210 stride must be calculated correctly for the actual width
    uint32_t v210_stride = ((width + 5) / 6) * 16; // Round up to nearest 6-pixel group, 16 bytes per group
    
    // Ensure stride is aligned to 4-byte boundary for 32-bit access
    v210_stride = (v210_stride + 3) & ~3;
    
    // Clear the destination buffer first
    memset(dst_data, 0, dst_size);
    
    // Only log conversion details for unsupported formats or first conversion
    static bool first_conversion = true;
    if (first_conversion || src_format != VIDEO_FORMAT_NV12) {
        blog(LOG_INFO, "MXL Output: Converting format %d to v210 (%dx%d)", src_format, width, height);
        first_conversion = false;
    }
    
    if (src_format == VIDEO_FORMAT_I420) {
        // Convert I420 (YUV 4:2:0 planar) to v210 (YUV 4:2:2 10-bit packed)
        uint8_t *y_plane = src_data;
        uint8_t *u_plane = src_data + (width * height);
        uint8_t *v_plane = src_data + (width * height) + (width * height / 4);
        
        for (uint32_t y = 0; y < height; y++) {
            uint8_t *dst_line = dst_data + (y * v210_stride);
            uint8_t *y_line = y_plane + (y * width);
            uint8_t *u_line = u_plane + ((y / 2) * (width / 2));
            uint8_t *v_line = v_plane + ((y / 2) * (width / 2));
            
            // Process pixels in groups of 6 for v210 packing
            for (uint32_t x = 0; x < width; x += 6) {
                if ((x / 6) * 16 + 16 > v210_stride) break; // Bounds check
                
                uint32_t *v210_group = (uint32_t*)(dst_line + ((x / 6) * 16));
                
                // Get Y values (convert 8-bit to 10-bit by shifting left 2)
                uint16_t y0 = (x + 0 < width) ? (y_line[x + 0] << 2) : 0;
                uint16_t y1 = (x + 1 < width) ? (y_line[x + 1] << 2) : 0;
                uint16_t y2 = (x + 2 < width) ? (y_line[x + 2] << 2) : 0;
                uint16_t y3 = (x + 3 < width) ? (y_line[x + 3] << 2) : 0;
                uint16_t y4 = (x + 4 < width) ? (y_line[x + 4] << 2) : 0;
                uint16_t y5 = (x + 5 < width) ? (y_line[x + 5] << 2) : 0;
                
                // Get U/V values (convert 8-bit to 10-bit)
                uint16_t u0 = ((x / 2) + 0 < width / 2) ? (u_line[(x / 2) + 0] << 2) : 512;
                uint16_t u1 = ((x / 2) + 1 < width / 2) ? (u_line[(x / 2) + 1] << 2) : 512;
                uint16_t u2 = ((x / 2) + 2 < width / 2) ? (u_line[(x / 2) + 2] << 2) : 512;
                
                uint16_t v0 = ((x / 2) + 0 < width / 2) ? (v_line[(x / 2) + 0] << 2) : 512;
                uint16_t v1 = ((x / 2) + 1 < width / 2) ? (v_line[(x / 2) + 1] << 2) : 512;
                uint16_t v2 = ((x / 2) + 2 < width / 2) ? (v_line[(x / 2) + 2] << 2) : 512;
                
                // Clamp to 10-bit range (0-1023)
                y0 = std::min(y0, (uint16_t)1023); y1 = std::min(y1, (uint16_t)1023);
                y2 = std::min(y2, (uint16_t)1023); y3 = std::min(y3, (uint16_t)1023);
                y4 = std::min(y4, (uint16_t)1023); y5 = std::min(y5, (uint16_t)1023);
                u0 = std::min(u0, (uint16_t)1023); u1 = std::min(u1, (uint16_t)1023); u2 = std::min(u2, (uint16_t)1023);
                v0 = std::min(v0, (uint16_t)1023); v1 = std::min(v1, (uint16_t)1023); v2 = std::min(v2, (uint16_t)1023);
                
                // Pack into v210 format (little-endian, 10 bits per component)
                // v210 packing: Cb0 Y0 Cr0 | Y1 Cb1 Y2 | Cr1 Y3 Cb2 | Y4 Cr2 Y5
                v210_group[0] = (u0 & 0x3FF) | ((y0 & 0x3FF) << 10) | ((v0 & 0x3FF) << 20);
                v210_group[1] = (y1 & 0x3FF) | ((u1 & 0x3FF) << 10) | ((y2 & 0x3FF) << 20);
                v210_group[2] = (v1 & 0x3FF) | ((y3 & 0x3FF) << 10) | ((u2 & 0x3FF) << 20);
                v210_group[3] = (y4 & 0x3FF) | ((v2 & 0x3FF) << 10) | ((y5 & 0x3FF) << 20);
            }
        }
        return true;
    }
    else if (src_format == VIDEO_FORMAT_NV12) {
        // Convert NV12 (YUV 4:2:0 semi-planar) to v210 (YUV 4:2:2 10-bit packed)
        // NV12 has Y plane and interleaved UV plane
        
        uint8_t *y_plane = nullptr;
        uint8_t *uv_plane = nullptr;
        
        // Use proper linesize values from OBS
        uint32_t y_stride = linesize[0];   // Y plane stride (bytes per line)
        uint32_t uv_stride = linesize[1];  // UV plane stride (bytes per line)
        
        // Use the actual image width, not stride (stride includes padding)
        uint32_t actual_width = width;
        uint32_t actual_height = height;
        
        // Check if OBS provides separate data planes (like CDI example)
        if (data_planes && data_planes[0] && data_planes[1]) {
            // OBS provides separate Y and UV planes
            y_plane = data_planes[0];
            uv_plane = data_planes[1];
        } else {
            // Fallback: OBS provides single buffer with Y followed by UV
            y_plane = src_data;
            uv_plane = src_data + (y_stride * actual_height);
        }
        
        for (uint32_t y = 0; y < actual_height; y++) {
            uint8_t *dst_line = dst_data + (y * v210_stride);
            uint8_t *y_line = y_plane + (y * y_stride);
            // NV12 UV plane: UV data is at half vertical resolution
            uint8_t *uv_line = uv_plane + ((y / 2) * uv_stride);
            
            // Process pixels in groups of 6 for v210 packing
            for (uint32_t x = 0; x < actual_width; x += 6) {
                if ((x / 6) * 16 + 16 > v210_stride) break; // Bounds check
                
                uint32_t *v210_group = (uint32_t*)(dst_line + ((x / 6) * 16));
                
                // Get Y values (convert 8-bit to 10-bit)
                uint16_t y0 = (x + 0 < actual_width) ? (y_line[x + 0] << 2) : 0;
                uint16_t y1 = (x + 1 < actual_width) ? (y_line[x + 1] << 2) : 0;
                uint16_t y2 = (x + 2 < actual_width) ? (y_line[x + 2] << 2) : 0;
                uint16_t y3 = (x + 3 < actual_width) ? (y_line[x + 3] << 2) : 0;
                uint16_t y4 = (x + 4 < actual_width) ? (y_line[x + 4] << 2) : 0;
                uint16_t y5 = (x + 5 < actual_width) ? (y_line[x + 5] << 2) : 0;
                
                // Get U/V values from interleaved UV plane
                // NV12: UV plane has U,V,U,V... interleaved at half horizontal resolution
                uint16_t u0, v0, u1, v1, u2, v2;
                
                // For 6 pixels (x to x+5), we need 3 UV pairs
                // UV pair 0: covers pixels x+0, x+1
                uint32_t uv_x0 = (x / 2) * 2;  // UV index for pixel pair
                if (uv_x0 + 1 < uv_stride && uv_x0 < actual_width) {
                    u0 = (uv_line[uv_x0 + 0] << 2);
                    v0 = (uv_line[uv_x0 + 1] << 2);
                } else {
                    u0 = v0 = 512; // Neutral chroma (128 << 2)
                }
                
                // UV pair 1: covers pixels x+2, x+3
                uint32_t uv_x1 = ((x + 2) / 2) * 2;
                if (uv_x1 + 1 < uv_stride && uv_x1 < actual_width) {
                    u1 = (uv_line[uv_x1 + 0] << 2);
                    v1 = (uv_line[uv_x1 + 1] << 2);
                } else {
                    u1 = v1 = 512;
                }
                
                // UV pair 2: covers pixels x+4, x+5
                uint32_t uv_x2 = ((x + 4) / 2) * 2;
                if (uv_x2 + 1 < uv_stride && uv_x2 < actual_width) {
                    u2 = (uv_line[uv_x2 + 0] << 2);
                    v2 = (uv_line[uv_x2 + 1] << 2);
                } else {
                    u2 = v2 = 512;
                }
                
                // Clamp to 10-bit range
                y0 = std::min(y0, (uint16_t)1023); y1 = std::min(y1, (uint16_t)1023);
                y2 = std::min(y2, (uint16_t)1023); y3 = std::min(y3, (uint16_t)1023);
                y4 = std::min(y4, (uint16_t)1023); y5 = std::min(y5, (uint16_t)1023);
                u0 = std::min(u0, (uint16_t)1023); u1 = std::min(u1, (uint16_t)1023); u2 = std::min(u2, (uint16_t)1023);
                v0 = std::min(v0, (uint16_t)1023); v1 = std::min(v1, (uint16_t)1023); v2 = std::min(v2, (uint16_t)1023);
                
                // Pack into v210 format: Cb0 Y0 Cr0 | Y1 Cb1 Y2 | Cr1 Y3 Cb2 | Y4 Cr2 Y5
                // Note: Cb = U, Cr = V
                v210_group[0] = (u0 & 0x3FF) | ((y0 & 0x3FF) << 10) | ((v0 & 0x3FF) << 20);
                v210_group[1] = (y1 & 0x3FF) | ((u1 & 0x3FF) << 10) | ((y2 & 0x3FF) << 20);
                v210_group[2] = (v1 & 0x3FF) | ((y3 & 0x3FF) << 10) | ((u2 & 0x3FF) << 20);
                v210_group[3] = (y4 & 0x3FF) | ((v2 & 0x3FF) << 10) | ((y5 & 0x3FF) << 20);
            }
        }
        return true;
    }
    
    // For unsupported formats, create a simple test pattern
    blog(LOG_WARNING, "MXL Output: Format %d to v210 conversion not fully implemented, creating test pattern", src_format);
    
    // Create a simple gradient test pattern in v210 format
    for (uint32_t y = 0; y < height; y++) {
        uint8_t *dst_line = dst_data + (y * v210_stride);
        
        for (uint32_t x = 0; x < width; x += 6) {
            if ((x / 6) * 16 + 16 > v210_stride) break;
            
            uint32_t *v210_group = (uint32_t*)(dst_line + ((x / 6) * 16));
            
            // Create a gradient pattern
            uint16_t luma = ((x + y) % 256) << 2; // 8-bit to 10-bit
            uint16_t chroma = 512; // Neutral chroma
            
            // Pack the same pattern for all 6 pixels
            v210_group[0] = (chroma & 0x3FF) | ((luma & 0x3FF) << 10) | ((chroma & 0x3FF) << 20);
            v210_group[1] = (luma & 0x3FF) | ((chroma & 0x3FF) << 10) | ((luma & 0x3FF) << 20);
            v210_group[2] = (chroma & 0x3FF) | ((luma & 0x3FF) << 10) | ((chroma & 0x3FF) << 20);
            v210_group[3] = (luma & 0x3FF) | ((chroma & 0x3FF) << 10) | ((luma & 0x3FF) << 20);
        }
    }
    
    return true;
}
