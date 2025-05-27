#include "ui/DockspaceManager.h"
#include "utils/Logger.h"
#include "ui/UIState.h"

namespace LocalTether::UI {
    void DockspaceManager::CreateDockspace(bool* p_open) 
    {
   
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        
       
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        
        
        ImVec2 work_pos = viewport->WorkPos;   
        ImVec2 work_size = viewport->WorkSize;  
        
        ImGui::SetNextWindowPos(work_pos);
        ImGui::SetNextWindowSize(work_size);
        ImGui::SetNextWindowViewport(viewport->ID);
        

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        

        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse 
                    | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

     
        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
            window_flags |= ImGuiWindowFlags_NoBackground;

        ImGui::Begin("MainDockspace", p_open, window_flags);
        ImGui::PopStyleVar(3); 

         
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New", "Ctrl+N")) 
                {
                    Utils::Logger::GetInstance().Info("New file created");
                }
                
                if (ImGui::MenuItem("Open", "Ctrl+O")) 
                {
                    Utils::Logger::GetInstance().Info("Open file dialog requested");
                }
                
                if (ImGui::MenuItem("Save", "Ctrl+S")) 
                {
                    Utils::Logger::GetInstance().Info("Save requested");
                }
                
                ImGui::Separator();
                
                if (ImGui::MenuItem("Exit", "Alt+F4")) 
                {
                    if (p_open) *p_open = false;
                }
                
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
                if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
                if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
                if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("File Explorer", NULL, &LocalTether::UI::show_file_explorer);
                ImGui::MenuItem("Example Panel", NULL, &LocalTether::UI::show_example_panel);
                ImGui::MenuItem("Network Settings", NULL, &LocalTether::UI::show_network_settings);
                ImGui::MenuItem("Console", NULL, &LocalTether::UI::show_console);
                ImGui::MenuItem("Properties", NULL, &LocalTether::UI::show_properties);
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Tools"))
            {
                if (ImGui::MenuItem("Settings")) 
                {
                    Utils::Logger::GetInstance().Info("Opening settings");
                }
                ImGui::EndMenu();
            }
            
            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("Documentation")) 
                {
                    Utils::Logger::GetInstance().Info("Opening documentation");
                }
                if (ImGui::MenuItem("About")) 
                {
                    Utils::Logger::GetInstance().Info("Opening about dialog");
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMenuBar();
        }
    
        float menu_bar_height = ImGui::GetFrameHeight(); 
        ImGui::SetCursorPos(ImVec2(0, menu_bar_height));
        ImVec2 dockspace_size = ImVec2(work_size.x, work_size.y - menu_bar_height);
        
        ImGuiID dockspace_id = ImGui::GetID("MainDockspaceID");
        ImGui::DockSpace(dockspace_id, dockspace_size, dockspace_flags);
        
      
        SetupDefaultLayout(dockspace_id);
        
        ImGui::End();
    }

    void DockspaceManager::SetupDefaultLayout(ImGuiID dockspace_id)
    {
        static bool first_time = true;
        if (first_time)
        {
            first_time = false;

             
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

            
            ImGuiID dock_main_id = dockspace_id;
            
            
            ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, nullptr, &dock_main_id);
            
           
            ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
            
            
            ImGuiID dock_bottom_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);

            ImGuiID dock_right_bottom_id = ImGui::DockBuilderSplitNode(dock_right_id, ImGuiDir_Down, 0.4f, nullptr, &dock_right_id);

            ImGui::DockBuilderDockWindow("File Explorer", dock_left_id);
            ImGui::DockBuilderDockWindow("Network Settings", dock_left_id);
            ImGui::DockBuilderDockWindow("Example Panel", dock_main_id);
            ImGui::DockBuilderDockWindow("Console", dock_bottom_id);
            ImGui::DockBuilderDockWindow("Properties", dock_right_id);
            ImGui::DockBuilderDockWindow("Welcome", dock_main_id);
            ImGui::DockBuilderDockWindow("Host Setup", dock_main_id);
            ImGui::DockBuilderDockWindow("Join Setup", dock_main_id);

            ImGui::DockBuilderDockWindow("Pause Key Combo Settings", dock_right_bottom_id);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }
}
