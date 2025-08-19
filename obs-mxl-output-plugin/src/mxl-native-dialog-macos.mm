#include "mxl-native-dialog.h"

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#include <obs.h>

// Custom view class to handle checkbox events
@interface MXLDialogView : NSView {
    NSTextField *domainField;
    NSButton *outputCheck;
    NSTextField *videoLabel;
    NSTextField *videoField;
    NSTextField *audioLabel;
    NSTextField *audioField;
    NSButton *videoCheck;
    NSButton *audioCheck;
}
- (void)setupControls:(MXLNativeDialog::Settings&)settings;
- (void)videoCheckChanged:(id)sender;
- (void)audioCheckChanged:(id)sender;
- (void)getSettings:(MXLNativeDialog::Settings&)settings;
@end

@implementation MXLDialogView

- (void)setupControls:(MXLNativeDialog::Settings&)settings {
    // Status information at the top
    NSTextField *statusLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 240, 500, 20)];
    [statusLabel setStringValue:@"Current Status:"];
    [statusLabel setBezeled:NO];
    [statusLabel setDrawsBackground:NO];
    [statusLabel setEditable:NO];
    [statusLabel setSelectable:NO];
    [statusLabel setFont:[NSFont boldSystemFontOfSize:13]];
    [self addSubview:statusLabel];
    
    // Get current status
    extern obs_output_t *global_mxl_output;
    NSString *statusText;
    if (global_mxl_output && obs_output_active(global_mxl_output)) {
        statusText = @"✅ MXL Output is RUNNING";
    } else if (global_mxl_output) {
        statusText = @"⏸️ MXL Output is STOPPED";
    } else {
        statusText = @"⚪ MXL Output is NOT CREATED";
    }
    
    NSTextField *statusValue = [[NSTextField alloc] initWithFrame:NSMakeRect(120, 240, 380, 20)];
    [statusValue setStringValue:statusText];
    [statusValue setBezeled:NO];
    [statusValue setDrawsBackground:NO];
    [statusValue setEditable:NO];
    [statusValue setSelectable:NO];
    [statusValue setTextColor:[NSColor systemBlueColor]];
    [self addSubview:statusValue];
    
    // MXL Domain Path (changed from "Domain Path")
    NSTextField *domainLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 210, 140, 20)];
    [domainLabel setStringValue:@"MXL Domain Path:"];
    [domainLabel setBezeled:NO];
    [domainLabel setDrawsBackground:NO];
    [domainLabel setEditable:NO];
    [domainLabel setSelectable:NO];
    [self addSubview:domainLabel];
    
    domainField = [[NSTextField alloc] initWithFrame:NSMakeRect(160, 210, 340, 20)];
    [domainField setStringValue:[NSString stringWithUTF8String:settings.domain_path.c_str()]];
    [self addSubview:domainField];
    
    // Output Enabled
    outputCheck = [[NSButton alloc] initWithFrame:NSMakeRect(10, 180, 200, 20)];
    [outputCheck setButtonType:NSButtonTypeSwitch];
    [outputCheck setTitle:@"Enable MXL Output"];
    [outputCheck setState:settings.output_enabled ? NSControlStateValueOn : NSControlStateValueOff];
    [self addSubview:outputCheck];
    
    // Video Enabled
    videoCheck = [[NSButton alloc] initWithFrame:NSMakeRect(10, 150, 200, 20)];
    [videoCheck setButtonType:NSButtonTypeSwitch];
    [videoCheck setTitle:@"Enable Video Stream"];
    [videoCheck setState:settings.video_enabled ? NSControlStateValueOn : NSControlStateValueOff];
    [videoCheck setTarget:self];
    [videoCheck setAction:@selector(videoCheckChanged:)];
    [self addSubview:videoCheck];
    
    // Audio Enabled
    audioCheck = [[NSButton alloc] initWithFrame:NSMakeRect(10, 120, 200, 20)];
    [audioCheck setButtonType:NSButtonTypeSwitch];
    [audioCheck setTitle:@"Enable Audio Stream"];
    [audioCheck setState:settings.audio_enabled ? NSControlStateValueOn : NSControlStateValueOff];
    [audioCheck setTarget:self];
    [audioCheck setAction:@selector(audioCheckChanged:)];
    [self addSubview:audioCheck];
    
    // Video Flow ID (conditionally visible, wider field)
    videoLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 90, 140, 20)];
    [videoLabel setStringValue:@"Video Flow ID:"];
    [videoLabel setBezeled:NO];
    [videoLabel setDrawsBackground:NO];
    [videoLabel setEditable:NO];
    [videoLabel setSelectable:NO];
    [videoLabel setHidden:!settings.video_enabled];
    [self addSubview:videoLabel];
    
    videoField = [[NSTextField alloc] initWithFrame:NSMakeRect(160, 90, 340, 20)];
    [videoField setStringValue:[NSString stringWithUTF8String:settings.video_flow_id.c_str()]];
    [videoField setHidden:!settings.video_enabled];
    [self addSubview:videoField];
    
    // Audio Flow ID (conditionally visible, wider field)
    audioLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(10, 60, 140, 20)];
    [audioLabel setStringValue:@"Audio Flow ID:"];
    [audioLabel setBezeled:NO];
    [audioLabel setDrawsBackground:NO];
    [audioLabel setEditable:NO];
    [audioLabel setSelectable:NO];
    [audioLabel setHidden:!settings.audio_enabled];
    [self addSubview:audioLabel];
    
    audioField = [[NSTextField alloc] initWithFrame:NSMakeRect(160, 60, 340, 20)];
    [audioField setStringValue:[NSString stringWithUTF8String:settings.audio_flow_id.c_str()]];
    [audioField setHidden:!settings.audio_enabled];
    [self addSubview:audioField];
}

- (void)videoCheckChanged:(id)sender {
    BOOL enabled = [videoCheck state] == NSControlStateValueOn;
    [videoLabel setHidden:!enabled];
    [videoField setHidden:!enabled];
}

- (void)audioCheckChanged:(id)sender {
    BOOL enabled = [audioCheck state] == NSControlStateValueOn;
    [audioLabel setHidden:!enabled];
    [audioField setHidden:!enabled];
}

- (void)getSettings:(MXLNativeDialog::Settings&)settings {
    settings.domain_path = [[domainField stringValue] UTF8String];
    settings.output_enabled = [outputCheck state] == NSControlStateValueOn;
    settings.video_enabled = [videoCheck state] == NSControlStateValueOn;
    settings.audio_enabled = [audioCheck state] == NSControlStateValueOn;
    
    // Only get flow IDs if streams are enabled
    if (settings.video_enabled) {
        settings.video_flow_id = [[videoField stringValue] UTF8String];
        if (settings.video_flow_id.empty()) {
            settings.video_flow_id = MXLNativeDialog::GenerateUUID();
        }
    }
    
    if (settings.audio_enabled) {
        settings.audio_flow_id = [[audioField stringValue] UTF8String];
        if (settings.audio_flow_id.empty()) {
            settings.audio_flow_id = MXLNativeDialog::GenerateUUID();
        }
    }
}

@end

// macOS implementation using Cocoa
bool MXLNativeDialog::ShowSettingsDialog_macOS(Settings& settings) {
    @autoreleasepool {
        // Auto-populate flow IDs if empty
        if (settings.video_flow_id.empty()) {
            settings.video_flow_id = GenerateUUID();
        }
        if (settings.audio_flow_id.empty()) {
            settings.audio_flow_id = GenerateUUID();
        }
        
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:@"MXL Output Settings"];
        [alert setInformativeText:@"Configure your MXL output settings"];
        [alert addButtonWithTitle:@"OK"];
        [alert addButtonWithTitle:@"Cancel"];
        
        // Create wider custom view with form controls (increased height for status)
        MXLDialogView *accessoryView = [[MXLDialogView alloc] initWithFrame:NSMakeRect(0, 0, 520, 270)];
        [accessoryView setupControls:settings];
        
        [alert setAccessoryView:accessoryView];
        
        // Show dialog and handle response
        NSModalResponse response = [alert runModal];
        
        if (response == NSAlertFirstButtonReturn) { // OK
            // Get values from the custom view
            [accessoryView getSettings:settings];
            return true;
        } else {
            return false; // Cancel
        }
    }
}

void MXLNativeDialog::ShowMessage_macOS(const std::string& title, const std::string& message) {
    @autoreleasepool {
        NSAlert *alert = [[NSAlert alloc] init];
        [alert setMessageText:[NSString stringWithUTF8String:title.c_str()]];
        [alert setInformativeText:[NSString stringWithUTF8String:message.c_str()]];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
    }
}

#endif
