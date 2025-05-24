#include "ui/panels/FileExplorerPanel.h"


//all these are empty, not implemented
namespace LocalTether::UI::Panels {
    FileExplorerPanel::FileExplorerPanel() {
        
    }
    
    void FileExplorerPanel::Show(bool* p_open) {
        ImGui::Begin("File Explorer", p_open);
        
        
        ImGui::Text("Filter:");
        ImGui::SameLine();
        ImGui::InputText("##Filter", search_filter, IM_ARRAYSIZE(search_filter));
        
        ImGui::Separator();
        
 
        DrawFolderTree();
        
        ImGui::Separator();
        
        DrawFileList();
        
        ImGui::End();
    }
    
    void FileExplorerPanel::SetRootDirectory(const std::string& path) {
        
    }
    
    std::string FileExplorerPanel::GetSelectedFilePath() const {
        
        return "";
    }
    
    void FileExplorerPanel::DrawFolderTree() {
  
        if (ImGui::CollapsingHeader("Folders", &open_folders, ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::TreeNode("Documents"))
            {
                if (ImGui::TreeNode("Projects"))
                {
                    if (ImGui::TreeNode("LocalTether"))
                    {
                        if (ImGui::TreeNode("src"))
                        {
                            ImGui::Selectable("main.cpp", true);
                            ImGui::TreePop();
                        }
                        if (ImGui::TreeNode("include"))
                        {
                            ImGui::Selectable("imgui_include.h", false);
                            ImGui::TreePop();
                        }
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }
            
            if (ImGui::TreeNode("Downloads"))
            {
                ImGui::TreePop();
            }
            
            if (ImGui::TreeNode("Pictures"))
            {
                ImGui::TreePop();
            }
        }
    }
    
    void FileExplorerPanel::DrawFileList() {
    
        ImGui::Text("Files:");
        
        ImGui::BeginChild("Files List", ImVec2(0, 0), true);
        
        if (ImGui::Selectable("main.cpp", selected_file == 0))
            selected_file = 0;
        if (ImGui::Selectable("imgui_include.h", selected_file == 1))
            selected_file = 1;
        if (ImGui::Selectable("CMakeLists.txt", selected_file == 2))
            selected_file = 2;
        if (ImGui::Selectable("README.md", selected_file == 3))
            selected_file = 3;
        if (ImGui::Selectable("build.ps1", selected_file == 4))
            selected_file = 4;
        if (ImGui::Selectable("build.sh", selected_file == 5))
            selected_file = 5;
        
        ImGui::EndChild();
    }
}