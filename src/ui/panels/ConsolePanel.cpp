#include "ui/panels/ConsolePanel.h"
#include <ctime>
#include <cstring>

namespace LocalTether::UI::Panels {
    ConsolePanel::ConsolePanel() {
       
    }
    
    void ConsolePanel::Show(bool* p_open) {
        ImGui::Begin("Console", p_open);
        
       
        if (ImGui::Button("Clear")) {
            Clear();
            AddLogMessage("Console cleared");
        }
        
        ImGui::SameLine();
        
        ImGui::PushItemWidth(-70); 
        bool reclaim_focus = false;
        
        if (ImGui::InputText("Command", input, IM_ARRAYSIZE(input), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (input[0]) {
                AddLogMessage(std::string("> ") + input);
                
             
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
                AddLogMessage(std::string("> ") + input);
                ProcessCommand(input);
                memset(input, 0, sizeof(input));
            }
        }
        
        ImGui::Separator();
    
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true, 
                         ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); 
        
   
        for (const auto& item : log_items) {
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
          
            if (item.find("[ERROR]") != std::string::npos)
                color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            else if (item.find("[WARNING]") != std::string::npos)
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            else if (item.find("> ") == item.find_first_not_of("[0-9:] "))
                color = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
            
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(item.c_str());
            ImGui::PopStyleColor();
        }
        
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        
        ImGui::PopStyleVar();
        ImGui::EndChild();
        
        ImGui::End();
    }
    
    void ConsolePanel::AddLogMessage(const std::string& message) {
        std::time_t currentTime = std::time(nullptr);
        std::tm* localTime = std::localtime(&currentTime);
        
        char timestamp[32];
        std::strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", localTime);
        
        log_items.push_back(std::string(timestamp) + message);
    }
    
    void ConsolePanel::Clear() {
        log_items.clear();
    }
    
    void ConsolePanel::ProcessCommand(const std::string& command) {
        if (command == "help") {
            AddLogMessage("Available commands: help, clear, exit, version, info");
        }
        else if (command == "clear") {
            log_items.clear();
            AddLogMessage("Console cleared");
        }
        else if (command == "exit") {
            AddLogMessage("Exiting application...");
          
        }
        else if (command == "version") {
            AddLogMessage("LocalTether v0.1.0");
        }
        else if (command == "info") {
            AddLogMessage("Running on ImGui " IMGUI_VERSION);
            AddLogMessage("Built with docking branch support");
        }
        else {
            AddLogMessage("Unknown command: " + command);
        }
    }
}