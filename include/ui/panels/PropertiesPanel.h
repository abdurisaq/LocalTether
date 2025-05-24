#pragma once
#include "imgui_include.h"
#include <string>
namespace LocalTether::UI::Panels {
    class PropertiesPanel {
    public:
        PropertiesPanel();
        
       
        void Show(bool* p_open = nullptr);
        
        
        void SetFile(const std::string& filepath);
        
    private:
        
        std::string filename = "main.cpp";
        std::string filesize = "8.2 KB";
        std::string created_date = "2024-05-20 15:42:30";
        std::string modified_date = "2024-05-20 16:10:15";
        
     
        char tags[128] = "cpp, main, source";
        bool read_only = false;
        
    
        std::string line_count = "342";
        std::string encoding = "UTF-8";
        std::string line_endings = "LF";
        int syntax_theme = 0;
        
   
        std::string owner = "duri";
        std::string group = "users";
        bool perm_read = true;
        bool perm_write = true;
        bool perm_execute = false;
        
        
        static constexpr const char* themes[] = { "Default", "Dark", "Light", "Monokai", "Solarized" };
    };
}
