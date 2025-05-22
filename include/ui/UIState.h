#pragma once

namespace LocalTether::UI {
    
    extern bool show_example_panel;
    extern bool show_network_settings;
    extern bool show_file_explorer;
    extern bool show_console;
    extern bool show_properties;

    enum class AppMode {
    None,
    HostSetup,
    JoinSetup,
    Connecting,
    ConnectedAsHost,
    ConnectedAsClient
};
    extern AppMode app_mode;
}