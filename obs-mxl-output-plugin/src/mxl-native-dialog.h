#pragma once

#include <string>

// Forward declaration for OBS output
struct obs_output;
typedef struct obs_output obs_output_t;

// External reference to global output instance
extern obs_output_t *global_mxl_output;

// Cross-platform native dialog interface
class MXLNativeDialog {
public:
    struct Settings {
        std::string domain_path;
        bool output_enabled;
        bool video_enabled;
        bool audio_enabled;
        std::string video_flow_id;
        std::string audio_flow_id;
    };
    
    // Show the settings dialog and return true if user clicked OK
    static bool ShowSettingsDialog(Settings& settings);
    
    // Show a simple message dialog
    static void ShowMessage(const std::string& title, const std::string& message);
    
    // Generate UUID helper
    static std::string GenerateUUID();
    
private:
    // Platform-specific implementations
#ifdef __APPLE__
    static bool ShowSettingsDialog_macOS(Settings& settings);
    static void ShowMessage_macOS(const std::string& title, const std::string& message);
#elif defined(_WIN32)
    static bool ShowSettingsDialog_Windows(Settings& settings);
    static void ShowMessage_Windows(const std::string& title, const std::string& message);
#elif defined(__linux__)
    static bool ShowSettingsDialog_Linux(Settings& settings);
    static void ShowMessage_Linux(const std::string& title, const std::string& message);
#endif
};
