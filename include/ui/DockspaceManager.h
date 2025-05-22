#pragma once
#include "imgui_include.h"

namespace LocalTether::UI {
    class DockspaceManager {
    public:
        
        void CreateDockspace(bool* p_open = nullptr);
        
        
        static void SetupDefaultLayout(ImGuiID dockspace_id);
    
    private:
        
        ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
        
        
        bool opt_fullscreen = true;
    };
}