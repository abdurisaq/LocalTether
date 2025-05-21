#include "ui/panels/ConsolePanel.h"
#include <ctime>
#include <cstring>

namespace LocalTether::UI::Panels {
    ConsolePanel::ConsolePanel() {
        // Initialize with empty log
    }
    
    void ConsolePanel::Show(bool* p_open) {
        ImGui::Begin("Console", p_open);
        
        // Console controls
        if (ImGui::Button("Clear")) {
            Clear();
            AddLogMessage("Console cleared");
        }
        
        ImGui::SameLine();
        
        ImGui::PushItemWidth(-70); // Make input text take most of the width
        bool reclaim_focus = false;
        
        // Fixed issue with CallbackCompletion by removing it
        if (ImGui::InputText("Command", input, IM_ARRAYSIZE(input), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (input[0]) {
                AddLogMessage(std::string("> ") + input);
                
                // Process command
                ProcessCommand(input);
                
                memset(input, 0, sizeof(input));
                reclaim_focus = true;
            }
        }
        
        ImGui::PopItemWidth();
        
        // Auto-focus on the input box
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
        
        // Console output - use a child window for scrolling
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true, 
                         ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
        
        // Display each logged item
        for (const auto& item : log_items) {
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            
            // Apply different colors for different message types
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
        
        // Auto-scroll when at the bottom
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
            // The actual exit will be handled in the next frame
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