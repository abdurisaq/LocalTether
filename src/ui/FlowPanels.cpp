#include "ui/FlowPanels.h"
#include "ui/UIState.h"
#include "utils/Logger.h"
#include "ui/panels/ConsolePanel.h"
#include "ui/panels/FileExplorerPanel.h"
#include "ui/panels/ControlsPanel.h"
#include "utils/ScanNetwork.h"
#include "network/Message.h"
#include "network/Server.h" 
#include "network/Session.h"
#include "network/Client.h" 
#include "ui/Icons.h" 

#include <imgui.h>
#include <thread>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>

namespace LocalTether::UI::Flow {

    using Utils::Logger;
    using Network::ClientRole;
    using Network::MessageType;
    using Network::Session;

     
    static void CenterNextItem(float item_width) {
        float avail_width = ImGui::GetContentRegionAvail().x;
        float cursor_x = (avail_width - item_width) * 0.5f;
        if (cursor_x > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMin().x + cursor_x);
        }
    }
    
     
    static bool StyledButton(const char* label, const ImVec2& size = ImVec2(0,0)) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5)); 
        bool pressed = ImGui::Button(label, size);
        ImGui::PopStyleVar();
        return pressed;
    }

     
    static void CenterText(const char* text) {
        float text_width = ImGui::CalcTextSize(text).x;
        CenterNextItem(text_width);
        ImGui::TextUnformatted(text);
    }

     
    static bool CenteredButton(const char* label, const ImVec2& size = ImVec2(0,0)) {
        float button_width = size.x;
        if (button_width == 0) { 
            button_width = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().FramePadding.x * 2.0f;  
        }
        CenterNextItem(button_width);
        return StyledButton(label, size);
    }

    void DrawSeparator() {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    void ShowHomePanel() {


        if (ImGui::Begin("Welcome to LocalTether", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::Spacing();
            ImGui::Spacing();
            CenterText("Choose an option to get started:");
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            float button_width = 250;
            if (CenteredButton(ICON_FA_SERVER " Host New Session", ImVec2(button_width, 40))) {
                LocalTether::UI::app_mode = LocalTether::UI::AppMode::HostSetup;  
            }
            ImGui::Spacing();
            ImGui::Spacing();
            if (CenteredButton(ICON_FA_WIFI " Join Existing Session", ImVec2(button_width, 40))) {
                LocalTether::UI::app_mode = LocalTether::UI::AppMode::JoinSetup;  
            }
        }
        ImGui::End();
    }

    void ShowGeneratingServerAssetsPanel() {

        ImGui::Begin("Initializing Server", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
        
        ImGui::Spacing();
        CenterText("Initializing server and generating SSL assets...");
        ImGui::Spacing();
        ImGui::Spacing();

        static float angle = 0.0f;
        angle = fmodf(angle + 0.05f, IM_PI * 2.0f);
        
        float avail_width = ImGui::GetContentRegionAvail().x;
        float spinner_size = 20.0f; 
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMin().x + (avail_width - spinner_size) * 0.5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + spinner_size * 0.5f + 10.0f);  
        ImVec2 pos = ImGui::GetCursorScreenPos(); 

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
        ImGui::Dummy(ImVec2(0.0f, spinner_size + 40.0f));  

        if (!UI::server_setup_in_progress.load()) {  
            if (UI::server_setup_thread.joinable()) {
                UI::server_setup_thread.join();
            }

            if (UI::server_setup_success.load()) {
                try {
                    auto& server = UI::getServer(); 
                    auto& client = UI::getClient();
                    
                    client.setConnectHandler([](bool success, const std::string& message, uint32_t assignedId) {
                        if (success) {
                            UI::app_mode = UI::AppMode::ConnectedAsHost;
                        } else {
                            UI::resetServerInstance(); 
                            UI::app_mode = UI::AppMode::HostSetup;  
                        }
                    });
                    client.setErrorHandler([](const std::error_code& error) {
                        UI::resetServerInstance(); 
                        UI::app_mode = UI::AppMode::HostSetup;  
                    });
                    client.connect(
                        "127.0.0.1", 
                        server.getPort(),
                        Network::ClientRole::Host,  
                        "HostInternalClient",
                        server.password  
                    );
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
                DrawSeparator();
                CenterText("Error during server setup");
                ImGui::TextWrapped("%s", error_msg.c_str());
                ImGui::Spacing();
                if (CenteredButton("Back to Host Setup")) {
                    UI::app_mode = UI::AppMode::HostSetup;
                }
            }
        }
        ImGui::End();
    }

    void ShowHostSetupPanel() {
        static bool allowInternet = false;
        static char password_buffer[64] = "";  


        ImGui::Begin("Host New Session", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::Spacing();
        CenterText("Configure your hosting settings below.");
        ImGui::Separator();
        ImGui::Spacing();

        CenterText("Network Configuration");
        ImGui::Spacing();
        float checkbox_label_width = ImGui::CalcTextSize("Allow Internet Connections (WAN)").x;
        float checkbox_icon_width = ImGui::CalcTextSize("(?)").x;
        float checkbox_total_width = checkbox_label_width + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemInnerSpacing.x + checkbox_icon_width + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x;
        CenterNextItem(checkbox_total_width);
        ImGui::BeginGroup();
        ImGui::Checkbox("Allow Internet Connections (WAN)", &allowInternet);
        ImGui::SameLine(); 
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("If unchecked, the server will only be accessible on your local network (LAN).\nIf checked, it may be accessible over the internet (requires port forwarding).");
            ImGui::EndTooltip();
        }
        ImGui::EndGroup();
        ImGui::Spacing();

        CenterText("Security");
        ImGui::Spacing();
        float input_width_host = 250; 
        float password_icon_width = ImGui::CalcTextSize("(?)").x;
        float password_total_width = input_width_host + ImGui::GetStyle().ItemInnerSpacing.x + password_icon_width + ImGui::GetStyle().FramePadding.x * 2;
        CenterNextItem(password_total_width);
        ImGui::BeginGroup();
        ImGui::PushItemWidth(input_width_host);
        ImGui::InputTextWithHint("##Password", "Session Password (optional)", password_buffer, sizeof(password_buffer), ImGuiInputTextFlags_Password);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Set a password for clients to join this session.\nLeave blank for an open session.");
            ImGui::EndTooltip();
        }
        ImGui::EndGroup();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float button_width_start = ImGui::CalcTextSize(ICON_FA_PLAY " Start Hosting").x + ImGui::GetStyle().FramePadding.x * 4.0f + 40;  
        float button_width_back = ImGui::CalcTextSize(ICON_FA_ARROW_LEFT " Back").x + ImGui::GetStyle().FramePadding.x * 4.0f + 20;  
        float total_button_width = button_width_start + button_width_back + ImGui::GetStyle().ItemSpacing.x;
        
        CenterNextItem(total_button_width);
        ImGui::BeginGroup();
        if (StyledButton(ICON_FA_PLAY " Start Hosting", ImVec2(button_width_start, 35))) {
            LocalTether::UI::app_mode = LocalTether::UI::AppMode::GeneratingServerAssets;
            LocalTether::UI::server_setup_in_progress = true;
            LocalTether::UI::server_setup_success = false;
            {
                std::lock_guard<std::mutex> lock(LocalTether::UI::server_setup_mutex);
                LocalTether::UI::server_setup_error_message.clear();
            }
            if (LocalTether::UI::server_setup_thread.joinable()) {
                LocalTether::UI::server_setup_thread.join();
            }
            bool allowInternet_copy = allowInternet;
            std::string password_copy = password_buffer;  
            LocalTether::UI::server_setup_thread = std::thread([allowInternet_copy, password_copy]() {
                bool current_success_flag = false;
                std::string current_error_message_thread;
                try {
                    auto& server = LocalTether::UI::getServer(); 
                    server.localNetworkOnly = !allowInternet_copy;
                    server.password = password_copy;  
                    server.setErrorHandler([](const std::error_code& error) {
                        LocalTether::Utils::Logger::GetInstance().Error("Server runtime error (async): " + error.message());
                    });
                    server.setConnectionHandler([](std::shared_ptr<LocalTether::Network::Session> session) {
                        LocalTether::Utils::Logger::GetInstance().Info("Client connected (async): " + session->getClientAddress());
                    });
                    server.start(); 
                    if (server.getState() == LocalTether::Network::ServerState::Error) {
                        throw std::runtime_error("Server entered error state during start: " + server.getErrorMessage());
                    }
                    current_success_flag = true;
                    LocalTether::Utils::Logger::GetInstance().Info("Server setup thread: Server started successfully.");
                } catch (const std::exception& e) {
                    current_error_message_thread = e.what();
                    LocalTether::Utils::Logger::GetInstance().Error("Exception during server setup thread: " + current_error_message_thread);
                    LocalTether::UI::resetServerInstance(); 
                }
                {
                    std::lock_guard<std::mutex> lock(LocalTether::UI::server_setup_mutex);
                    LocalTether::UI::server_setup_error_message = current_error_message_thread;
                }
                LocalTether::UI::server_setup_success = current_success_flag;
                LocalTether::UI::server_setup_in_progress = false; 
            });
        }
        ImGui::SameLine();
        if (StyledButton(ICON_FA_ARROW_LEFT " Back", ImVec2(button_width_back, 35))) {
            LocalTether::UI::app_mode = LocalTether::UI::AppMode::None;
        }
        ImGui::EndGroup();
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
        

        ImGui::Begin("Join Existing Session", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::Spacing();
        CenterText("Discover or manually enter server details to connect.");
        ImGui::Separator();
        ImGui::Spacing();

        CenterText("Server Discovery");
        ImGui::Spacing();

        float discovery_content_width = ImGui::GetContentRegionAvail().x * 0.8f; 
        
        if (!scanning) {
            float scan_button_width = ImGui::CalcTextSize(ICON_FA_SEARCH " Scan For Servers").x + ImGui::GetStyle().FramePadding.x * 4.0f + 20;  
            CenterNextItem(scan_button_width);
            if (StyledButton(ICON_FA_SEARCH " Scan For Servers", ImVec2(scan_button_width, 0))) {
                discovered_servers.clear();
                selected_server_idx = -1;
                scanning = true;
                if (scan_thread.joinable()) {
                    scan_thread.join(); 
                }
                scan_thread = std::thread([&]() { 
                    std::vector<std::string> temp_results;
                    std::atomic<bool> running_flag = true; 
                    temp_results = scanForServer(std::ref(running_flag)); 
                    std::lock_guard<std::mutex> lock(LocalTether::UI::g_mutex); 
                    discovered_servers = std::move(temp_results);
                    scanning = false;
                    LocalTether::Utils::Logger::GetInstance().Info("Scan complete, found " + std::to_string(discovered_servers.size()) + " servers");
                });
                scan_thread.detach();  
            }
        } else {
            float spinner_text_width = ImGui::CalcTextSize("Scanning for servers...").x;
            float cancel_button_width = ImGui::CalcTextSize("Cancel Scan").x + ImGui::GetStyle().FramePadding.x * 2.0f; 
            float spinner_radius = 8.0f;
            float scanning_group_width = (spinner_radius * 2 + 5) + spinner_text_width + ImGui::GetStyle().ItemInnerSpacing.x + cancel_button_width;
            CenterNextItem(scanning_group_width);
            ImGui::BeginGroup();
            static float angle = 0.0f; 
            angle = fmodf(angle + 0.05f, IM_PI * 2.0f);
            ImVec2 pos = ImGui::GetCursorScreenPos(); 
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 center_spinner = ImVec2(pos.x + spinner_radius, pos.y + ImGui::GetTextLineHeight() * 0.5f);
            draw_list->AddCircle(center_spinner, spinner_radius, ImGui::GetColorU32(ImGuiCol_Text), 12, 1.0f);
            const float line_length = spinner_radius * 0.8f;
            ImVec2 line_end = ImVec2(center_spinner.x + cosf(angle) * line_length, center_spinner.y + sinf(angle) * line_length);
            draw_list->AddLine(center_spinner, line_end, ImGui::GetColorU32(ImGuiCol_Text), 2.0f);
            ImGui::SameLine(spinner_radius * 2 + 5); 
            ImGui::Text("Scanning for servers...");
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel Scan")) {
                scanning = false; 
                LocalTether::Utils::Logger::GetInstance().Info("Scan cancelled by user");
            }
            ImGui::EndGroup();
        }
        ImGui::Spacing();

        if (!discovered_servers.empty()) {
            CenterText("Available Servers:");
            CenterNextItem(discovery_content_width);
            ImGui::PushItemWidth(discovery_content_width); 
            if (ImGui::BeginListBox("##DiscoveredServers", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 4))) {
                for (int n = 0; n < discovered_servers.size(); n++) {
                    const bool is_selected = (selected_server_idx == n);
                    if (ImGui::Selectable(discovered_servers[n].c_str(), is_selected)) {
                        selected_server_idx = n;
                        strncpy(ip, discovered_servers[selected_server_idx].c_str(), sizeof(ip) - 1);
                        ip[sizeof(ip) - 1] = '\0'; 
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndListBox();
            }
            ImGui::PopItemWidth();
        } else if (!scanning) {
            CenterText("No servers found yet. Try scanning.");
        }
        ImGui::Spacing();

        CenterText("Manual Connection");
        ImGui::Spacing();

        float input_width_join = 300; 
        CenterNextItem(input_width_join); 
        ImGui::PushItemWidth(input_width_join);
        ImGui::InputTextWithHint("##ServerIP", "Server IP Address", ip, sizeof(ip));
        ImGui::PopItemWidth(); 
        ImGui::Spacing();

        CenterNextItem(input_width_join);
        ImGui::PushItemWidth(input_width_join);
        ImGui::InputTextWithHint("##Password", "Session Password (if any)", password, sizeof(password), ImGuiInputTextFlags_Password);
        ImGui::PopItemWidth();
        ImGui::Spacing();

        CenterNextItem(input_width_join);
        ImGui::PushItemWidth(input_width_join);
        ImGui::InputTextWithHint("##Name", "Your Name", name, sizeof(name));
        ImGui::PopItemWidth();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float button_width_connect = ImGui::CalcTextSize(ICON_FA_PLUG " Connect").x + ImGui::GetStyle().FramePadding.x * 4.0f + 40;  
        float button_width_back_join = ImGui::CalcTextSize(ICON_FA_ARROW_LEFT " Back").x + ImGui::GetStyle().FramePadding.x * 4.0f + 20;  
        float total_button_width_join = button_width_connect + button_width_back_join + ImGui::GetStyle().ItemSpacing.x;

        CenterNextItem(total_button_width_join);
        ImGui::BeginGroup();
        bool can_connect = (ip[0] != '\0');
        ImGui::BeginDisabled(!can_connect);
        if (StyledButton(ICON_FA_PLUG " Connect", ImVec2(button_width_connect, 35))) {
            try {
                auto& client_ref = LocalTether::UI::getClient();  
                client_ref.setConnectHandler([](bool success, const std::string& message, uint32_t assignedId) {  
                    if (success) {
                        LocalTether::UI::app_mode = LocalTether::UI::AppMode::ConnectedAsClient;
                        LocalTether::Utils::Logger::GetInstance().Info("Connected to server. Assigned ID: " + std::to_string(assignedId) + ". Msg: " + message);
                    } else {
                        UI::resetServerInstance(); 
                        UI::app_mode = UI::AppMode::None;  
                        LocalTether::Utils::Logger::GetInstance().Error("Failed to connect to server: " + message);
                    }
                });
                client_ref.setMessageHandler([](const LocalTether::Network::Message& msg) { });
                client_ref.setDisconnectHandler([](const std::string& reason) {  
                    LocalTether::UI::app_mode = LocalTether::UI::AppMode::None;
                    LocalTether::Utils::Logger::GetInstance().Info("Disconnected from server: " + reason);
                    LocalTether::UI::resetClientInstance(); 
                });
                client_ref.connect(ip, 8080, LocalTether::Network::ClientRole::Broadcaster, name, password); 
                LocalTether::UI::app_mode = LocalTether::UI::AppMode::Connecting;
                LocalTether::Utils::Logger::GetInstance().Info(std::string("Attempting to connect to ") + ip);
            }
            catch (const std::exception& e) {
                LocalTether::Utils::Logger::GetInstance().Error("Connection error: " + std::string(e.what()));
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (StyledButton(ICON_FA_ARROW_LEFT " Back", ImVec2(button_width_back_join, 35))) {
            if (scanning) scanning = false; 
            if (scan_thread.joinable()) scan_thread.join();
            discovered_servers.clear();
            ip[0] = '\0'; 
            selected_server_idx = -1;
            LocalTether::UI::app_mode = LocalTether::UI::AppMode::None;
        }
        ImGui::EndGroup();
        ImGui::End();
    }

    LocalTether::UI::Panels::ConsolePanel& GetConsolePanelInstance() {
        static LocalTether::UI::Panels::ConsolePanel instance;
        return instance;
    }

    LocalTether::UI::Panels::FileExplorerPanel& GetFileExplorerPanelInstance() {
        static LocalTether::UI::Panels::FileExplorerPanel instance;  
        return instance;
    }

    LocalTether::UI::Panels::ControlsPanel& GetControlsPanelInstance() {
        static LocalTether::UI::Panels::ControlsPanel instance;
        return instance;
    }
 
    void ShowHostDashboard() {
        GetConsolePanelInstance().Show(&LocalTether::UI::show_console);  
        GetFileExplorerPanelInstance().Show(&LocalTether::UI::show_file_explorer);  
        GetControlsPanelInstance().Show(&LocalTether::UI::show_controls_panel);
    }

    void ShowClientDashboard() {
        GetConsolePanelInstance().Show(&LocalTether::UI::show_console);  
        GetFileExplorerPanelInstance().Show(&LocalTether::UI::show_file_explorer);  
        GetControlsPanelInstance().Show(&LocalTether::UI::show_controls_panel);
    }
}