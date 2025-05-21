#pragma once
#include "imgui_include.h"

namespace LocalTether::UI {
    class DockspaceManager {
    public:
        // Create the dockspace for the application
        void CreateDockspace(bool* p_open = nullptr);
        
        // Set up the default docking layout
        static void SetupDefaultLayout(ImGuiID dockspace_id);
    
    private:
        // Internal dockspace flags
        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        
        // Fullscreen option
        bool opt_fullscreen = true;
    };
}