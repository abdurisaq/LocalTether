#include "ui/panels/NetworkSettingsPanel.h"
#include "utils/Logger.h"
#include <cstring>

//unimplemented
namespace LocalTether::UI::Panels {
    NetworkSettingsPanel::NetworkSettingsPanel() {
       
    }
    
    void NetworkSettingsPanel::Show(bool* p_open) {
        ImGui::Begin("Network Settings", p_open);
        ImGui::Text("Configure network settings here");
        
        ImGui::InputText("IP Address", ip_address, IM_ARRAYSIZE(ip_address));
        
        ImGui::InputInt("Port", &port);
        
        ImGui::Checkbox("Use SSL", &use_ssl);
        
        ImGui::Separator();
        

        ImGui::Combo("Protocol", &protocol, protocols, IM_ARRAYSIZE(protocols));
        
        ImGui::InputText("Username", username, IM_ARRAYSIZE(username));
        
        ImGui::InputText("Password", password, IM_ARRAYSIZE(password), ImGuiInputTextFlags_Password);
        
        ImGui::SliderInt("Timeout (s)", &timeout, 5, 120);
        
        ImGui::Separator();
        
        if (ImGui::Button("Connect")) {
            Utils::Logger::GetInstance().Info("Connecting to " + 
                std::string(ip_address) + ":" + std::to_string(port));
            Connect();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Reset")) {
            ResetToDefaults();
            Utils::Logger::GetInstance().Info("Network settings reset to defaults");
        }
        
        ImGui::End();
    }
    
    bool NetworkSettingsPanel::Connect() {
     
        return true;
    }
    
    void NetworkSettingsPanel::ResetToDefaults() {
        strcpy(ip_address, "192.168.1.1");
        port = 8080;
        use_ssl = true;
        protocol = 0;
        strcpy(username, "admin");
        strcpy(password, "password");
        timeout = 30;
    }
}