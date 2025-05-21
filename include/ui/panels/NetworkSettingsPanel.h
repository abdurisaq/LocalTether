#pragma once
#include "imgui_include.h"

namespace LocalTether::UI::Panels {
    class NetworkSettingsPanel {
    public:
        NetworkSettingsPanel();
        
        // Show the network settings panel
        void Show(bool* p_open = nullptr);
        
        // Connect to a server using the current settings
        bool Connect();
        
        // Reset settings to defaults
        void ResetToDefaults();
        
    private:
        // Connection settings
        char ip_address[64] = "192.168.1.1";
        int port = 8080;
        bool use_ssl = true;
        int protocol = 0;
        char username[64] = "admin";
        char password[64] = "password";
        int timeout = 30;
        
        // Protocol options
        static constexpr const char* protocols[] = { "HTTP", "HTTPS", "FTP", "SSH" };
    };
}