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
        

        

        void Clear();
        
    private:

        void ProcessCommand(const std::string& command);
        

        char input[256] = "";
    };
}