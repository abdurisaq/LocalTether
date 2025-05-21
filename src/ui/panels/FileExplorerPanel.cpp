#include "ui/panels/FileExplorerPanel.h"

namespace LocalTether::UI::Panels {
    FileExplorerPanel::FileExplorerPanel() {
        // Initialize with default values
    }
    
    void FileExplorerPanel::Show(bool* p_open) {
        ImGui::Begin("File Explorer", p_open);
        
        // Filter/search bar
        ImGui::Text("Filter:");
        ImGui::SameLine();
        ImGui::InputText("##Filter", search_filter, IM_ARRAYSIZE(search_filter));
        
        ImGui::Separator();
        
        // Draw tree and file list
        DrawFolderTree();
        
        ImGui::Separator();
        
        DrawFileList();
        
        ImGui::End();
    }
    
    void FileExplorerPanel::SetRootDirectory(const std::string& path) {
        // TODO: Set the root directory for file browsing
    }
    
    std::string FileExplorerPanel::GetSelectedFilePath() const {
        // TODO: Return the full path of the selected file
        return "";
    }
    
    void FileExplorerPanel::DrawFolderTree() {
        // Tree view for folders
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
        // List view for files
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