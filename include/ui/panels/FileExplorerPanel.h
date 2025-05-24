#pragma once
#include "imgui_include.h"
#include <string>

namespace LocalTether::UI::Panels {
    class FileExplorerPanel {
    public:
        FileExplorerPanel();
        
        void Show(bool* p_open = nullptr);
        
       
        void SetRootDirectory(const std::string& path);
        
        
        std::string GetSelectedFilePath() const;
        
    private:
 
        int selected_file = 0;
        char search_filter[128] = "";
        

        bool open_folders = true;
  
        void DrawFolderTree();
        
       
        void DrawFileList();
    };
}