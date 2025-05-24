#pragma once
#include "imgui_include.h"

namespace LocalTether::UI::Panels {
    class NetworkSettingsPanel {
    public:
        NetworkSettingsPanel();
        

        void Show(bool* p_open = nullptr);
        
    
        bool Connect();
        
        
        void ResetToDefaults();
        
    private:
        
        char ip_address[64] = "192.168.1.1";
        int port = 8080;
        bool use_ssl = true;
        int protocol = 0;
        char username[64] = "admin";
        char password[64] = "password";
        int timeout = 30;
        
        static constexpr const char* protocols[] = { "HTTP", "HTTPS", "FTP", "SSH" };
    };
}