#pragma once
#include "imgui_include.h"
#include <string>

namespace LocalTether::UI::Panels {
    class FileExplorerPanel {
    public:
        FileExplorerPanel();
        
        // Show the file explorer panel
        void Show(bool* p_open = nullptr);
        
        // Set the root directory for browsing
        void SetRootDirectory(const std::string& path);
        
        // Get the currently selected file path
        std::string GetSelectedFilePath() const;
        
    private:
        // File selection state
        int selected_file = 0;
        
        // Search filter
        char search_filter[128] = "";
        
        // Folder state
        bool open_folders = true;
        
        // Draw the folder tree structure
        void DrawFolderTree();
        
        // Draw the file list
        void DrawFileList();
    };
}