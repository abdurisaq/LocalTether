#pragma once
#include "imgui_include.h"
#include <vector>
#include <string>
#include <ctime>

namespace LocalTether::UI::Panels {
    class ConsolePanel {
    public:
        ConsolePanel();
        

        void Show(bool* p_open = nullptr);
        

        void AddLogMessage(const std::string& message);

        const std::vector<std::string>& GetLogs() const { return log_items; }
        

        void Clear();
        
    private:

        void ProcessCommand(const std::string& command);
        

        std::vector<std::string> log_items;
        

        char input[256] = "";
    };
}