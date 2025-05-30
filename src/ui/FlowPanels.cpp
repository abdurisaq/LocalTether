#include "ui/FlowPanels.h"
#include "ui/UIState.h"
#include "utils/Logger.h"
#include "ui/panels/ConsolePanel.h"
#include "ui/panels/FileExplorerPanel.h"
#include "ui/panels/NetworkSettingsPanel.h"
#include "ui/panels/ControlsPanel.h"
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

    void ShowGeneratingServerAssetsPanel() {
        ImGui::Begin("Generating Server Assets", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        
        ImGui::Text("Initializing server and generating SSL assets...");
        ImGui::Spacing();

         
        static float angle = 0.0f;
        angle = fmodf(angle + 0.05f, IM_PI * 2.0f);
        
        float avail_width = ImGui::GetContentRegionAvail().x;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float spinner_size = 20.0f; 
        ImGui::SetCursorPosX((avail_width - spinner_size) * 0.5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spinner_size * 0.5f + 20.0f);  
        pos = ImGui::GetCursorScreenPos(); 

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const float radius = 10.0f;
        ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);
        draw_list->AddCircle(center, radius, ImGui::GetColorU32(ImGuiCol_Text), 12, 2.0f);
        const float line_length = radius * 0.8f;
        ImVec2 line_end = ImVec2(
            center.x + cosf(angle) * line_length,
            center.y + sinf(angle) * line_length
        );
        draw_list->AddLine(center, line_end, ImGui::GetColorU32(ImGuiCol_Text), 2.0f);
        ImGui::Dummy(ImVec2(0.0f, spinner_size + 30.0f));  

        if (!UI::server_setup_in_progress.load()) {  
            if (UI::server_setup_thread.joinable()) {
                UI::server_setup_thread.join();
            }

            if (UI::server_setup_success.load()) {
                Logger::GetInstance().Info("Server setup successful. Connecting internal host client.");
                try {
                    auto& server = UI::getServer(); 
                    Logger::GetInstance().Info("Server started successfully on port: " + std::to_string(server.getPort()));
                    auto& client = UI::getClient();
                    Logger::GetInstance().Info("Setting up internal host client connection...");
                    
                     
                     
                     
                     
                     
                     
                     
                     
                     
                     

                    client.setConnectHandler([](bool success, const std::string& message, uint32_t assignedId) {
                        if (success) {
                            Logger::GetInstance().Info("Host-Client connected to server successfully. ID: " + std::to_string(assignedId));
                            UI::app_mode = UI::AppMode::ConnectedAsHost;
                        } else {
                            Logger::GetInstance().Error("Host-Client failed to connect: " + message);
                            UI::resetServerInstance(); 
                            UI::app_mode = UI::AppMode::HostSetup;  
                        }
                    });
                    client.setErrorHandler([](const std::error_code& error) {
                        Logger::GetInstance().Error("Host-Client error: " + error.message());
                        UI::resetServerInstance(); 
                        UI::app_mode = UI::AppMode::HostSetup;  
                    });
                    
                     
                     
                    Logger::GetInstance().Info("Attempting to connect internal host client to server...");
                    client.connect(
                        "127.0.0.1", 
                        server.getPort(),
                        Network::ClientRole::Host,  
                        "HostInternalClient",
                        server.password  
                    );
                                
                    Logger::GetInstance().Info("Hosting session started and host-client initiated connection attempt.");

                } catch (const std::exception& e_client) {
                    Logger::GetInstance().Error("Failed to setup or connect internal host client: " + std::string(e_client.what()));
                    UI::resetServerInstance(); 
                    UI::app_mode = UI::AppMode::HostSetup;  
                }
            } else {  
                std::string error_msg;
                {
                    std::lock_guard<std::mutex> lock(UI::server_setup_mutex);
                    error_msg = UI::server_setup_error_message;
                }
                Logger::GetInstance().Error("Server setup failed (from GeneratingPanel): " + error_msg);
                 
                
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error during server setup:");
                ImGui::TextWrapped("%s", error_msg.c_str());
                ImGui::Spacing();
                if (ImGui::Button("Back to Host Setup")) {
                    UI::app_mode = UI::AppMode::HostSetup;
                }
            }
        }
        ImGui::End();
    }


    void ShowHostSetupPanel() {
        static bool allowInternet = false;
        static char password_buffer[64] = "";  

        ImGui::Begin("Host Setup", nullptr, ImGuiWindowFlags_NoCollapse);

        ImGui::Checkbox("Allow Internet Connections", &allowInternet);
        ImGui::InputText("Join Password", password_buffer, sizeof(password_buffer), ImGuiInputTextFlags_Password);

        if (ImGui::Button("Start Hosting")) {
             
            UI::app_mode = UI::AppMode::GeneratingServerAssets;
            
             
            UI::server_setup_in_progress = true;
            UI::server_setup_success = false;
            {
                std::lock_guard<std::mutex> lock(UI::server_setup_mutex);
                UI::server_setup_error_message.clear();
            }

             
            if (UI::server_setup_thread.joinable()) {
                UI::server_setup_thread.join();
            }

             
            bool allowInternet_copy = allowInternet;
            std::string password_copy = password_buffer;  

            UI::server_setup_thread = std::thread([allowInternet_copy, password_copy]() {
                bool current_success_flag = false;
                std::string current_error_message_thread;
                try {
                    auto& server = UI::getServer(); 

                    server.localNetworkOnly = !allowInternet_copy;
                    server.password = password_copy;  
                    
                    server.setErrorHandler([](const std::error_code& error) {
                        Logger::GetInstance().Error("Server runtime error (async): " + error.message());
                    });
                    server.setConnectionHandler([](std::shared_ptr<Network::Session> session) {
                        Logger::GetInstance().Info("Client connected (async): " + session->getClientAddress());
                    });
                    
                    server.start(); 
                    
                    if (server.getState() == Network::ServerState::Error) {
                        throw std::runtime_error("Server entered error state during start: " + server.getErrorMessage());
                    }
                    current_success_flag = true;
                    Logger::GetInstance().Info("Server setup thread: Server started successfully.");

                } catch (const std::exception& e) {
                    current_error_message_thread = e.what();
                    Logger::GetInstance().Error("Exception during server setup thread: " + current_error_message_thread);
                    UI::resetServerInstance(); 
                }

                {
                    std::lock_guard<std::mutex> lock(UI::server_setup_mutex);
                    UI::server_setup_error_message = current_error_message_thread;
                }
                UI::server_setup_success = current_success_flag;
                UI::server_setup_in_progress = false; 
            });
        }

        if (ImGui::Button("Back")) {
            UI::app_mode = UI::AppMode::None;
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
            
       
            client.setConnectHandler([](bool success, const std::string& message, uint32_t assignedId) {  
                if (success) {
                    LocalTether::UI::app_mode = LocalTether::UI::AppMode::ConnectedAsClient;
                    LocalTether::Utils::Logger::GetInstance().Info("Connected to server. Assigned ID: " + std::to_string(assignedId) + ". Msg: " + message);
                } else {
                    LocalTether::Utils::Logger::GetInstance().Error("Failed to connect to server: " + message);
                     
                }
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
            
            client.setDisconnectHandler([](const std::string& reason) {  
                LocalTether::UI::app_mode = LocalTether::UI::AppMode::None;
                LocalTether::Utils::Logger::GetInstance().Info("Disconnected from server: " + reason);
                LocalTether::UI::resetClientInstance(); 
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
  
  LocalTether::UI::Panels::ControlsPanel controls;

  LocalTether::UI::Panels::FileExplorerPanel& GetFileExplorerPanelInstance() {
    return fileExplorer; // fileExplorer is defined around line 405
}


void ShowHostDashboard() {
    consolePanel.Show(&LocalTether::UI::show_console);  
    fileExplorer.Show(&LocalTether::UI::show_file_explorer);  
    networkSettings.Show(&LocalTether::UI::show_network_settings);  
    controls.Show(&LocalTether::UI::show_controls_panel);
    
}

void ShowClientDashboard() {
    consolePanel.Show(&LocalTether::UI::show_console);  
    networkSettings.Show(&LocalTether::UI::show_network_settings);  
    controls.Show(&LocalTether::UI::show_controls_panel);
    
}
}
