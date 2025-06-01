#include "ui/panels/ConsolePanel.h"
#include <ctime>
#include <cstring>
#include "utils/Logger.h"
namespace LocalTether::UI::Panels {

    
    ConsolePanel::ConsolePanel() {
       
    }
    
    void ConsolePanel::Show(bool* p_open) {
        if (p_open && !*p_open) { 
            return;
        }
        ImGui::Begin("Console", nullptr);
        
        if (ImGui::Button("Clear")) {
            Clear();  
        }
        
        ImGui::SameLine();
        
        ImGui::PushItemWidth(-70); 
        bool reclaim_focus = false;
        
        if (ImGui::InputText("Command", input, IM_ARRAYSIZE(input), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (input[0]) {
                 
                LocalTether::Utils::Logger::GetInstance().Info(std::string("> ") + input);
                
                ProcessCommand(input);
                
                memset(input, 0, sizeof(input));
                reclaim_focus = true;
            }
        }
        
        ImGui::PopItemWidth();
        
        ImGui::SetItemDefaultFocus();
        if (reclaim_focus)
            ImGui::SetKeyboardFocusHere(-1);
        
        ImGui::SameLine();
        if (ImGui::Button("Send")) {
            if (input[0]) {
                LocalTether::Utils::Logger::GetInstance().Info(std::string("> ") + input);
                ProcessCommand(input);
                memset(input, 0, sizeof(input));
                reclaim_focus = true;  
            }
        }
        
        ImGui::Separator();
    
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true, 
                         ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); 
        
         
        std::vector<std::string> logs_from_logger = LocalTether::Utils::Logger::GetInstance().GetLogs();
   
        for (const auto& item : logs_from_logger) {  
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  
          
             
            if (item.find("[ERROR]") != std::string::npos || item.find("[CRITICAL]") != std::string::npos)
                color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            else if (item.find("[WARNING]") != std::string::npos)
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            else if (item.find("[DEBUG]") != std::string::npos)
                color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  
             
             
            size_t prompt_pos = item.find("> ");
            if (prompt_pos != std::string::npos) {
                  
                 size_t level_end_pos = item.find("] ");
                 if (level_end_pos != std::string::npos) {
                     level_end_pos = item.find("] ", level_end_pos + 1);  
                     if (level_end_pos != std::string::npos && prompt_pos > level_end_pos) {
                        color = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);  
                     }
                 }
            }
            
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(item.c_str());
            ImGui::PopStyleColor();
        }
        
         
        static size_t last_log_count = 0;
        if (logs_from_logger.size() > last_log_count || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) {
             if (logs_from_logger.size() != last_log_count && ImGui::GetScrollY() < ImGui::GetScrollMaxY() -10.0f) {
                 
             } else {
                ImGui::SetScrollHereY(1.0f);
             }
        }
        last_log_count = logs_from_logger.size();

        
        ImGui::PopStyleVar();
        ImGui::EndChild();
        
        ImGui::End();
    }
    
   void ConsolePanel::Clear() {
        LocalTether::Utils::Logger::GetInstance().Clear();  
         
        LocalTether::Utils::Logger::GetInstance().Info("Console view cleared by user.");
    }
    
    void ConsolePanel::ProcessCommand(const std::string& command) {
         
        if (command == "help") {
            LocalTether::Utils::Logger::GetInstance().Info("Available commands: help, clear, exit, version, info");
        }
        else if (command == "clear") {
             
             
        }
        else if (command == "exit") {
            LocalTether::Utils::Logger::GetInstance().Info("Exiting application (command not implemented yet)...");
             
        }
        else if (command == "version") {
            LocalTether::Utils::Logger::GetInstance().Info("LocalTether v0.1.0 (Example Version)");
        }
        else if (command == "info") {
            LocalTether::Utils::Logger::GetInstance().Info("Running on ImGui " IMGUI_VERSION);
             
        }
        else {
            LocalTether::Utils::Logger::GetInstance().Warning("Unknown command: " + command);
        }
    }
}