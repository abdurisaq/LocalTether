#include "ui/FlowPanels.h"
#include "ui/UIState.h"
#include "utils/Logger.h"
#include "ui/panels/ConsolePanel.h"
#include "ui/panels/FileExplorerPanel.h"
#include "ui/panels/NetworkSettingsPanel.h"
#include "ui/panels/PropertiesPanel.h"

#include <imgui.h>

namespace LocalTether::UI::Flow {

  void ShowHomePanel() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 work_pos = viewport->WorkPos;
    
   
    ImGui::SetNextWindowPos(ImVec2(work_pos.x + work_size.x * 0.5f, work_pos.y + work_size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(work_size.x * 0.8f, work_size.y * 0.8f), ImGuiCond_Always);
    
    ImGui::Begin("Welcome", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);

    ImGui::Text("Start a session:");

    if (ImGui::Button("Host Session")) {
        LocalTether::UI::app_mode = LocalTether::UI::AppMode::HostSetup;  
    }

    if (ImGui::Button("Join Session")) {
        LocalTether::UI::app_mode = LocalTether::UI::AppMode::JoinSetup;  
    }

    ImGui::End();
  
  }

  void ShowHostSetupPanel() {
    static bool allowInternet = false;
    static char password[64] = "";

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 work_pos = viewport->WorkPos;
    
    ImGui::SetNextWindowPos(ImVec2(work_pos.x + work_size.x * 0.5f, work_pos.y + work_size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(work_size.x * 0.8f, work_size.y * 0.8f), ImGuiCond_Always);
    
    ImGui::Begin("Host Setup", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
    

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

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_size = viewport->WorkSize;
    ImVec2 work_pos = viewport->WorkPos;
    
    
    ImGui::SetNextWindowPos(ImVec2(work_pos.x + work_size.x * 0.5f, work_pos.y + work_size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(work_size.x * 0.8f, work_size.y * 0.8f), ImGuiCond_Always);
    
    ImGui::Begin("Join Setup", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking);
    

    ImGui::InputText("Server IP", ip, sizeof(ip));
    ImGui::InputText("Password", password, sizeof(password));

    if (ImGui::Button("Connect")) {
        LocalTether::UI::app_mode = LocalTether::UI::AppMode::ConnectedAsClient;
        LocalTether::Utils::Logger::GetInstance().Info(std::string("Connecting to ") + ip);
    }

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