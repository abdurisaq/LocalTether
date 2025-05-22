#include "ui/FlowPanels.h"
#include "ui/UIState.h"
#include "utils/Logger.h"
#include "ui/panels/ConsolePanel.h"
#include "ui/panels/FileExplorerPanel.h"
#include "ui/panels/NetworkSettingsPanel.h"
#include "ui/panels/PropertiesPanel.h"
#include "utils/ScanNetwork.h"
#include <imgui.h>
#include <atomic>

namespace LocalTether::UI::Flow {

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

    
    ImGui::Begin("Host Setup", nullptr, ImGuiWindowFlags_NoCollapse );
    

    ImGui::Checkbox("Allow Internet Connections", &allowInternet);
    ImGui::InputText("Join Password", password, sizeof(password));

    if (ImGui::Button("Start Hosting")) {
        LocalTether::UI::app_mode = LocalTether::UI::AppMode::ConnectedAsHost;
        LocalTether::Utils::Logger::GetInstance().Info("Hosting session started");
    }

    if (ImGui::Button("Back")) {
        LocalTether::UI::app_mode = LocalTether::UI::AppMode::None;
    }

    ImGui::End();
  }


  void ShowJoinSetupPanel() {
    static char ip[64] = "127.0.0.1";
    static char password[64] = "";
    
    // State for scanning
    static bool scanning = false;
    static std::vector<std::string> discovered_servers;
    static std::thread scan_thread;
    static int selected_server_idx = -1;
    
    ImGui::Begin("Join Setup", nullptr, ImGuiWindowFlags_NoCollapse);
    
    // Show either IP input or server selection dropdown
    if (discovered_servers.empty()) {
        ImGui::InputText("Server IP", ip, sizeof(ip));
    } else {
        // Convert vector to const char* array for ImGui::Combo
        std::vector<const char*> server_names;
        for (const auto& server : discovered_servers) {
            server_names.push_back(server.c_str());
        }
        
        if (ImGui::Combo("Available Servers", &selected_server_idx, 
                       server_names.data(), server_names.size())) {
            // Copy selected server IP to the ip buffer
            if (selected_server_idx >= 0 && selected_server_idx < discovered_servers.size()) {
                strncpy(ip, discovered_servers[selected_server_idx].c_str(), sizeof(ip) - 1);
                ip[sizeof(ip) - 1] = '\0';  // Ensure null termination
            }
        }
    }
    
    ImGui::InputText("Password", password, sizeof(password), ImGuiInputTextFlags_Password);
    
    if (!scanning) {
    if (ImGui::Button("Scan For Servers")) {
        // Reset state for new scan
        discovered_servers.clear();
        selected_server_idx = -1;
        scanning = true;
        
        // Start scan thread
        if (scan_thread.joinable()) {
            scan_thread.join();  // Clean up any previous thread
        }
        
        scan_thread = std::thread([&]() {
            // Create temporary vector to hold results
            std::vector<std::string> temp_results;
            std::atomic<bool> running = true;
            
            // Call your scan function
            temp_results = scanForServer(std::ref(running));
            
            // Thread-safe update of UI state
            std::lock_guard<std::mutex> lock(UI::g_mutex);
            discovered_servers = std::move(temp_results);
            scanning = false;
            LocalTether::Utils::Logger::GetInstance().Info(
                "Scan complete, found " + std::to_string(discovered_servers.size()) + " servers");
        });
        scan_thread.detach();  // Let it run independently
    }
} else {
    // Animated spinner
    static float angle = 0.0f;
    angle = fmodf(angle + 0.05f, IM_PI * 2.0f);
    
    // Center the spinner
    float avail_width = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((avail_width - 20.0f) * 0.5f);
    
    // Draw the spinner
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    const float radius = 10.0f;
    ImVec2 center = ImVec2(pos.x + radius, pos.y + radius);
    
    // Draw spinner circle
    draw_list->AddCircle(center, radius, ImGui::GetColorU32(ImGuiCol_Text), 12, 1.0f);
    
    // Draw rotating line
    const float line_length = radius * 0.8f;
    ImVec2 line_end = ImVec2(
        center.x + cosf(angle) * line_length,
        center.y + sinf(angle) * line_length
    );
    draw_list->AddLine(center, line_end, ImGui::GetColorU32(ImGuiCol_Text), 2.0f);
    
    ImGui::Dummy(ImVec2(20.0f, 20.0f)); // Space for the spinner
    
    // Add some text under the spinner
    const char* scan_text = "Scanning for servers...";
    ImGui::SetCursorPosX((avail_width - ImGui::CalcTextSize(scan_text).x) * 0.5f);
    ImGui::Text("%s", scan_text);
    
    ImGui::Spacing();
    ImGui::Spacing();
    
    // Center the button
    float button_width = 120;
    ImGui::SetCursorPosX((avail_width - button_width) * 0.5f);
    if (ImGui::Button("Cancel", ImVec2(button_width, 0))) {
        scanning = false;
        LocalTether::Utils::Logger::GetInstance().Info("Scan cancelled");
    }
}
    
    ImGui::Separator();
    
    if (ImGui::Button("Connect")) {
        LocalTether::UI::app_mode = LocalTether::UI::AppMode::ConnectedAsClient;
        LocalTether::Utils::Logger::GetInstance().Info(std::string("Connecting to ") + ip);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Back")) {
        LocalTether::UI::app_mode = LocalTether::UI::AppMode::None;
    }
    
    ImGui::End();
}


  LocalTether::UI::Panels::ConsolePanel consolePanel;
  LocalTether::UI::Panels::FileExplorerPanel fileExplorer;
  LocalTether::UI::Panels::NetworkSettingsPanel networkSettings;
  LocalTether::UI::Panels::PropertiesPanel properties;

  void ShowHostDashboard() {
      consolePanel.Show();
      fileExplorer.Show();
      networkSettings.Show();
      properties.Show();
  }

  void ShowClientDashboard() {
      consolePanel.Show();
      networkSettings.Show();
  }
}
