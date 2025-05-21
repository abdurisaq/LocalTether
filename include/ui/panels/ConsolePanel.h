#pragma once
#include "imgui_include.h"
#include <vector>
#include <string>
#include <ctime>

namespace LocalTether::UI::Panels {
    class ConsolePanel {
    public:
        ConsolePanel();
        
        // Show the console panel
        void Show(bool* p_open = nullptr);
        
        // Add a message to the console log
        void AddLogMessage(const std::string& message);
        
        // Get the log messages for inspection or saving
        const std::vector<std::string>& GetLogs() const { return log_items; }
        
        // Clear all console messages
        void Clear();
        
    private:
        // Command processing
        void ProcessCommand(const std::string& command);
        
        // Log storage
        std::vector<std::string> log_items;
        
        // Input buffer
        char input[256] = "";
    };
}