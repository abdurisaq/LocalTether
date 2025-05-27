#include "ui/FlowPanels.h"
#include "ui/UIState.h"
#include "utils/Logger.h"
#include "ui/panels/ConsolePanel.h"
#include "ui/panels/FileExplorerPanel.h"
#include "ui/panels/NetworkSettingsPanel.h"
#include "ui/panels/PropertiesPanel.h"
#include "ui/panels/PauseSettingsPanel.h"
#include "utils/ScanNetwork.h"
#include "network/Message.h"
#include "network/Server.h" 
#include "network/Session.h"
#include "network/Client.h" 

#include <imgui.h>
#include <thread>
#include <atomic>

namespace LocalTether::UI::Flow {


    using Utils::Logger;
    using Network::ClientRole;
    using Network::MessageType;
    using Network::Session;
    void ShowHomePanel() {

    if (ImGui::Begin("Welcome", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Start a session:");

        if (ImGui::Button("Host Session")) {
            LocalTether::UI::app_mode = LocalTether::UI::AppMode::HostSetup;  
        }

        if (ImGui::Button("Join Session")) {
            LocalTether::UI::app_mode = LocalTether::UI::AppMode::JoinSetup;  
        }
    }
    ImGui::End();

    }

    void ShowHostSetupPanel() {
    static bool allowInternet = false;
    static char password[64] = "";

    ImGui::Begin("Host Setup", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Checkbox("Allow Internet Connections", &allowInternet);
    ImGui::InputText("Join Password", password, sizeof(password));

    if (ImGui::Button("Start Hosting")) {
        try {
            auto& server = getServer();  
            
            
            server.localNetworkOnly = !allowInternet;
            server.password = password;
            
           
            server.setErrorHandler([](const std::error_code& error) {
                Logger::GetInstance().Error(
                    "Server error: " + error.message());
            });
            
            server.setConnectionHandler([](std::shared_ptr<Network::Session> session) {
                
                Logger::GetInstance().Info(
                    "Client connected: " + session->getClientAddress());
                    
            });
            
            
            server.start();
            
     
            auto& client = getClient();
            
            client.setConnectHandler([]() {
                Logger::GetInstance().Info("Connected as host");
            });
            
            client.setErrorHandler([](const std::error_code& error) {
                Logger::GetInstance().Error(
                    "Host client error: " + error.message());
            });
            
            
            client.connect(
                "127.0.0.1", 
                server.getPort(),
                ClientRole::Host,  
                "Host",
                password);
                
            
            app_mode = AppMode::ConnectedAsHost;
            Logger::GetInstance().Info("Hosting session started");
        }
        catch (const std::exception& e) {
            Logger::GetInstance().Error(
                "Failed to start hosting: " + std::string(e.what()));
        }
    }

    if (ImGui::Button("Back")) {
        app_mode = AppMode::None;
    }

    ImGui::End();
    }

void ShowJoinSetupPanel() {
    static char ip[64] = "127.0.0.1";
    static char password[64] = "";
    static char name[64] = "Guest";
    
   
    static bool scanning = false;
    static std::vector<std::string> discovered_servers;
    static std::thread scan_thread;
    static int selected_server_idx = -1;
    
    ImGui::Begin("Join Setup", nullptr, ImGuiWindowFlags_NoCollapse);
    
    if (discovered_servers.empty()) {
        ImGui::InputText("Server IP", ip, sizeof(ip));
    } else {
       
        std::vector<const char*> server_names;
        for (const auto& server : discovered_servers) {
            server_names.push_back(server.c_str());
        }
        
        if (ImGui::Combo("Available Servers", &selected_server_idx, 
                       server_names.data(), server_names.size())) {
            
            if (selected_server_idx >= 0 && selected_server_idx < discovered_servers.size()) {
                strncpy(ip, discovered_servers[selected_server_idx].c_str(), sizeof(ip) - 1);
                ip[sizeof(ip) - 1] = '\0'; 
            }
        }
    }
    
    ImGui::InputText("Password", password, sizeof(password), ImGuiInputTextFlags_Password);
    
    if (!scanning) {
        if (ImGui::Button("Scan For Servers")) {
            
            discovered_servers.clear();
            selected_server_idx = -1;
            scanning = true;
            
            if (scan_thread.joinable()) {
                scan_thread.join(); 
            }
            
            scan_thread = std::thread([&]() {
                
                std::vector<std::string> temp_results;
                std::atomic<bool> running = true;
                
                
                temp_results = scanForServer(std::ref(running));
                
               
                std::lock_guard<std::mutex> lock(UI::g_mutex);
                discovered_servers = std::move(temp_results);
                scanning = false;
                LocalTether::Utils::Logger::GetInstance().Info(
                    "Scan complete, found " + std::to_string(discovered_servers.size()) + " servers");
            });
            scan_thread.detach();  
        }
    } else {
        // Animated spinner
        static float angle = 0.0f;
        angle = fmodf(angle + 0.05f, IM_PI * 2.0f);
        
      
        float avail_width = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX((avail_width - 20.0f) * 0.5f);
        
      
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        const float radius = 10.0f;
        ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);
        
        
        draw_list->AddCircle(center, radius, ImGui::GetColorU32(ImGuiCol_Text), 12, 1.0f);
        
        const float line_length = radius * 0.8f;
        ImVec2 line_end = ImVec2(
            center.x + cosf(angle) * line_length,
            center.y + sinf(angle) * line_length
        );
        draw_list->AddLine(center, line_end, ImGui::GetColorU32(ImGuiCol_Text), 2.0f);
        
        ImGui::Dummy(ImVec2(20.0f, 20.0f)); 

        const char* scan_text = "Scanning for servers...";
        ImGui::SetCursorPosX((avail_width - ImGui::CalcTextSize(scan_text).x) * 0.5f);
        ImGui::Text("%s", scan_text);
        
        ImGui::Spacing();
        ImGui::Spacing();
        
       
        float button_width = 120;
        ImGui::SetCursorPosX((avail_width - button_width) * 0.5f);
        if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
            scanning = false;
            LocalTether::Utils::Logger::GetInstance().Info("Scan cancelled");
        }
    }
    
    ImGui::Separator();
 
    if (ImGui::Button("Connect")) {
        try {
            auto& client = LocalTether::UI::getClient();
            
       
            client.setConnectHandler([]() {
                LocalTether::UI::app_mode = LocalTether::UI::AppMode::ConnectedAsClient;
                LocalTether::Utils::Logger::GetInstance().Info("Connected to server");
            });
            
            client.setMessageHandler([](const LocalTether::Network::Message& msg) {
                
                switch(msg.getType()) {
                    case LocalTether::Network::MessageType::Input:
                        
                        break;
                    
                    case LocalTether::Network::MessageType::ChatMessage:
                        
                        break;
                        
                    case LocalTether::Network::MessageType::Command:
                        
                        break;
                        
                   
                }
            });
            
            client.setDisconnectHandler([]() {
                LocalTether::UI::app_mode = LocalTether::UI::AppMode::None;
                LocalTether::Utils::Logger::GetInstance().Info("Disconnected from server");
            });
            
           
            client.connect(
                ip, 
                8080, 
                LocalTether::Network::ClientRole::Broadcaster,
                name,
                password);
                
          
            LocalTether::UI::app_mode = LocalTether::UI::AppMode::Connecting;
            LocalTether::Utils::Logger::GetInstance().Info(std::string("Connecting to ") + ip);
        }
        catch (const std::exception& e) {
            LocalTether::Utils::Logger::GetInstance().Error(
                "Connection error: " + std::string(e.what()));
        }
    }

    ImGui::SameLine();
    
    if (ImGui::Button("Back")) {
        discovered_servers.clear();
        ip[0] = '\0';
        LocalTether::UI::app_mode = LocalTether::UI::AppMode::None;
    }
    
    ImGui::End();
}




  LocalTether::UI::Panels::ConsolePanel consolePanel;
  LocalTether::UI::Panels::FileExplorerPanel fileExplorer;
  LocalTether::UI::Panels::NetworkSettingsPanel networkSettings;
  LocalTether::UI::Panels::PropertiesPanel properties;
  LocalTether::UI::Panels::PauseSettingsPanel pauseSettings;

void ShowHostDashboard() {
    consolePanel.Show();
    fileExplorer.Show();
    networkSettings.Show();
    properties.Show();
    auto& client = LocalTether::UI::getClient();
    if (client.getInputManager()) {
        pauseSettings.Show(&LocalTether::UI::show_pause_settings, client.getInputManager());
    }
}

void ShowClientDashboard() {
    consolePanel.Show();
    networkSettings.Show();
    auto& client = LocalTether::UI::getClient();
    if (client.getInputManager()) {
        pauseSettings.Show(&LocalTether::UI::show_pause_settings, client.getInputManager());
    }
}
}
