#include "mxl-native-dialog.h"
#include <random>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#elif defined(__linux__)
#include <gtk/gtk.h>
#endif

// Generate UUID helper
std::string MXLNativeDialog::GenerateUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) ss << "-";
        if (i == 12) ss << "4"; // Version 4
        else if (i == 16) ss << (dis(gen) & 0x3 | 0x8); // Variant bits
        else ss << dis(gen);
    }
    
    return ss.str();
}

// Public interface implementations
bool MXLNativeDialog::ShowSettingsDialog(Settings& settings) {
#ifdef __APPLE__
    return ShowSettingsDialog_macOS(settings);
#elif defined(_WIN32)
    return ShowSettingsDialog_Windows(settings);
#elif defined(__linux__)
    return ShowSettingsDialog_Linux(settings);
#else
    return false; // Unsupported platform
#endif
}

void MXLNativeDialog::ShowMessage(const std::string& title, const std::string& message) {
#ifdef __APPLE__
    ShowMessage_macOS(title, message);
#elif defined(_WIN32)
    ShowMessage_Windows(title, message);
#elif defined(__linux__)
    ShowMessage_Linux(title, message);
#endif
}

#ifdef _WIN32
// Windows implementation using Win32 API
bool MXLNativeDialog::ShowSettingsDialog_Windows(Settings& settings) {
    // Auto-populate flow IDs if empty
    if (settings.video_flow_id.empty()) {
        settings.video_flow_id = GenerateUUID();
    }
    if (settings.audio_flow_id.empty()) {
        settings.audio_flow_id = GenerateUUID();
    }
    
    // For now, use a simple message box approach
    // In a full implementation, you'd create a custom dialog resource
    std::string message = "Current Settings:\n";
    message += "MXL Domain Path: " + settings.domain_path + "\n";
    message += "Output Enabled: " + std::string(settings.output_enabled ? "Yes" : "No") + "\n";
    message += "Video Enabled: " + std::string(settings.video_enabled ? "Yes" : "No") + "\n";
    message += "Audio Enabled: " + std::string(settings.audio_enabled ? "Yes" : "No") + "\n";
    
    if (settings.video_enabled) {
        message += "Video Flow ID: " + settings.video_flow_id + "\n";
    }
    if (settings.audio_enabled) {
        message += "Audio Flow ID: " + settings.audio_flow_id + "\n";
    }
    
    message += "\nEdit config file to change settings.";
    
    int result = MessageBoxA(NULL, message.c_str(), "MXL Output Settings", MB_OKCANCEL | MB_ICONINFORMATION);
    return result == IDOK;
}

void MXLNativeDialog::ShowMessage_Windows(const std::string& title, const std::string& message) {
    MessageBoxA(NULL, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

#elif defined(__linux__)
// Linux implementation using GTK
bool MXLNativeDialog::ShowSettingsDialog_Linux(Settings& settings) {
    // Auto-populate flow IDs if empty
    if (settings.video_flow_id.empty()) {
        settings.video_flow_id = GenerateUUID();
    }
    if (settings.audio_flow_id.empty()) {
        settings.audio_flow_id = GenerateUUID();
    }
    
    // Initialize GTK if not already done
    if (!gtk_init_check(0, NULL)) {
        return false;
    }
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons("MXL Output Settings",
                                                   NULL,
                                                   GTK_DIALOG_MODAL,
                                                   "OK", GTK_RESPONSE_OK,
                                                   "Cancel", GTK_RESPONSE_CANCEL,
                                                   NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);
    
    // MXL Domain Path (changed from "Domain Path")
    GtkWidget *domain_label = gtk_label_new("MXL Domain Path:");
    GtkWidget *domain_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(domain_entry), settings.domain_path.c_str());
    gtk_grid_attach(GTK_GRID(grid), domain_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), domain_entry, 1, 0, 1, 1);
    
    // Checkboxes
    GtkWidget *output_check = gtk_check_button_new_with_label("Enable MXL Output");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(output_check), settings.output_enabled);
    gtk_grid_attach(GTK_GRID(grid), output_check, 0, 1, 2, 1);
    
    GtkWidget *video_check = gtk_check_button_new_with_label("Enable Video Stream");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(video_check), settings.video_enabled);
    gtk_grid_attach(GTK_GRID(grid), video_check, 0, 2, 2, 1);
    
    GtkWidget *audio_check = gtk_check_button_new_with_label("Enable Audio Stream");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(audio_check), settings.audio_enabled);
    gtk_grid_attach(GTK_GRID(grid), audio_check, 0, 3, 2, 1);
    
    // Flow IDs (conditionally visible)
    GtkWidget *video_label = gtk_label_new("Video Flow ID:");
    GtkWidget *video_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(video_entry), settings.video_flow_id.c_str());
    gtk_widget_set_visible(video_label, settings.video_enabled);
    gtk_widget_set_visible(video_entry, settings.video_enabled);
    gtk_grid_attach(GTK_GRID(grid), video_label, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), video_entry, 1, 4, 1, 1);
    
    GtkWidget *audio_label = gtk_label_new("Audio Flow ID:");
    GtkWidget *audio_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(audio_entry), settings.audio_flow_id.c_str());
    gtk_widget_set_visible(audio_label, settings.audio_enabled);
    gtk_widget_set_visible(audio_entry, settings.audio_enabled);
    gtk_grid_attach(GTK_GRID(grid), audio_label, 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), audio_entry, 1, 5, 1, 1);
    
    // Callback function for toggling widget visibility
    auto toggle_callback = [](GtkToggleButton *button, gpointer user_data) {
        gboolean active = gtk_toggle_button_get_active(button);
        GtkWidget **widgets = (GtkWidget**)user_data;
        gtk_widget_set_visible(widgets[0], active); // label
        gtk_widget_set_visible(widgets[1], active); // entry
    };
    
    // Convert lambda to function pointer
    void (*toggle_callback_ptr)(GtkToggleButton*, gpointer) = +toggle_callback;
    
    // Create widget arrays for signal handlers
    GtkWidget **video_widgets = new GtkWidget*[2];
    video_widgets[0] = video_label;
    video_widgets[1] = video_entry;
    
    GtkWidget **audio_widgets = new GtkWidget*[2];
    audio_widgets[0] = audio_label;
    audio_widgets[1] = audio_entry;
    
    // Add signal handlers to show/hide flow ID fields when checkboxes change
    g_signal_connect(video_check, "toggled", G_CALLBACK(toggle_callback_ptr), (gpointer)video_widgets);
    g_signal_connect(audio_check, "toggled", G_CALLBACK(toggle_callback_ptr), (gpointer)audio_widgets);
    
    gtk_widget_show_all(dialog);
    
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_OK) {
        settings.domain_path = gtk_entry_get_text(GTK_ENTRY(domain_entry));
        settings.output_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(output_check));
        settings.video_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(video_check));
        settings.audio_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(audio_check));
        
        // Only get flow IDs if streams are enabled
        if (settings.video_enabled) {
            settings.video_flow_id = gtk_entry_get_text(GTK_ENTRY(video_entry));
            if (settings.video_flow_id.empty()) {
                settings.video_flow_id = GenerateUUID();
            }
        }
        
        if (settings.audio_enabled) {
            settings.audio_flow_id = gtk_entry_get_text(GTK_ENTRY(audio_entry));
            if (settings.audio_flow_id.empty()) {
                settings.audio_flow_id = GenerateUUID();
            }
        }
        
        gtk_widget_destroy(dialog);
        return true;
    }
    
    gtk_widget_destroy(dialog);
    return false;
}

void MXLNativeDialog::ShowMessage_Linux(const std::string& title, const std::string& message) {
    if (!gtk_init_check(0, NULL)) {
        return;
    }
    
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                              GTK_DIALOG_MODAL,
                                              GTK_MESSAGE_INFO,
                                              GTK_BUTTONS_OK,
                                              "%s", message.c_str());
    gtk_window_set_title(GTK_WINDOW(dialog), title.c_str());
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

#endif
