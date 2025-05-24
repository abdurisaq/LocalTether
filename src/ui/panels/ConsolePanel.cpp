#include "ui/panels/ConsolePanel.h"
#include <ctime>
#include <cstring>
#include "utils/Logger.h"
namespace LocalTether::UI::Panels {

    
    ConsolePanel::ConsolePanel() {
       
    }
    
    void ConsolePanel::Show(bool* p_open) {
        ImGui::Begin("Console", p_open);
        
        if (ImGui::Button("Clear")) {
            Clear(); // This will now call Logger::GetInstance().Clear()
        }
        
        ImGui::SameLine();
        
        ImGui::PushItemWidth(-70); 
        bool reclaim_focus = false;
        
        if (ImGui::InputText("Command", input, IM_ARRAYSIZE(input), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (input[0]) {
                // Log the command itself using the global logger
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
                reclaim_focus = true; // Also reclaim focus on send button
            }
        }
        
        ImGui::Separator();
    
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true, 
                         ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); 
        
        // Get logs directly from the Logger instance
        const auto& logs_from_logger = LocalTether::Utils::Logger::GetInstance().GetLogs();
   
        for (const auto& item : logs_from_logger) { // Iterate over logger's logs
            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default color
          
            // Your existing color coding logic
            if (item.find("[ERROR]") != std::string::npos || item.find("[CRITICAL]") != std::string::npos)
                color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            else if (item.find("[WARNING]") != std::string::npos)
                color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            else if (item.find("[DEBUG]") != std::string::npos)
                color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // Example color for Debug
            // Check for command echo "> " after the timestamp and level
            // This might need adjustment if your Logger::FormatMessage changes significantly
            size_t prompt_pos = item.find("> ");
            if (prompt_pos != std::string::npos) {
                 // Check if "> " appears after typical log prefixes like "[TIME] [LEVEL] "
                 size_t level_end_pos = item.find("] ");
                 if (level_end_pos != std::string::npos) {
                     level_end_pos = item.find("] ", level_end_pos + 1); // Find the second ']'
                     if (level_end_pos != std::string::npos && prompt_pos > level_end_pos) {
                        color = ImVec4(0.4f, 0.8f, 1.0f, 1.0f); // Command echo color
                     }
                 }
            }
            
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(item.c_str());
            ImGui::PopStyleColor();
        }
        
        // Auto-scroll 
        static size_t last_log_count = 0;
        if (logs_from_logger.size() > last_log_count || ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10.0f) {
             if (logs_from_logger.size() != last_log_count && ImGui::GetScrollY() < ImGui::GetScrollMaxY() -10.0f) {
                // Don't auto-scroll if user has scrolled up, unless new message arrived and they were already at bottom
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
        LocalTether::Utils::Logger::GetInstance().Clear(); // Clear the logger's in-memory logs
        // Optionally, log that the console view was cleared
        LocalTether::Utils::Logger::GetInstance().Info("Console view cleared by user.");
    }
    
    void ConsolePanel::ProcessCommand(const std::string& command) {
        // Use Logger::GetInstance().Info() or other levels for command feedback
        if (command == "help") {
            LocalTether::Utils::Logger::GetInstance().Info("Available commands: help, clear, exit, version, info");
        }
        else if (command == "clear") {
            // Clear() is already called by the button, but if typed, it will clear again.
            // The "Console cleared" message will be logged by Clear() itself.
        }
        else if (command == "exit") {
            LocalTether::Utils::Logger::GetInstance().Info("Exiting application (command not implemented yet)...");
            // Actual exit logic would be elsewhere, e.g., setting a flag in your main app loop.
        }
        else if (command == "version") {
            LocalTether::Utils::Logger::GetInstance().Info("LocalTether v0.1.0 (Example Version)");
        }
        else if (command == "info") {
            LocalTether::Utils::Logger::GetInstance().Info("Running on ImGui " IMGUI_VERSION);
            // Add more info if needed
        }
        else {
            LocalTether::Utils::Logger::GetInstance().Warning("Unknown command: " + command);
        }
    }
}