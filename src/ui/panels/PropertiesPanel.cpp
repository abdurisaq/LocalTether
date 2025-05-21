#include "ui/panels/PropertiesPanel.h"

namespace LocalTether::UI::Panels {
    PropertiesPanel::PropertiesPanel() {
        // Initialize with default values
    }
    
    void PropertiesPanel::Show(bool* p_open) {
        ImGui::Begin("Properties", p_open);
        
        if (ImGui::BeginTabBar("PropertiesTabs")) {
            if (ImGui::BeginTabItem("General")) {
                ImGui::Text("File: %s", filename.c_str());
                ImGui::Text("Size: %s", filesize.c_str());
                ImGui::Text("Created: %s", created_date.c_str());
                ImGui::Text("Modified: %s", modified_date.c_str());
                
                ImGui::Separator();
                
                ImGui::InputText("Tags", tags, IM_ARRAYSIZE(tags));
                
                ImGui::Checkbox("Read-only", &read_only);
                
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Details")) {
                ImGui::Text("Lines: %s", line_count.c_str());
                ImGui::Text("Character encoding: %s", encoding.c_str());
                ImGui::Text("Line endings: %s", line_endings.c_str());
                
                ImGui::Separator();
                
                ImGui::Combo("Syntax Theme", &syntax_theme, themes, IM_ARRAYSIZE(themes));
                
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Permissions")) {
                ImGui::Text("Owner: %s", owner.c_str());
                ImGui::Text("Group: %s", group.c_str());
                
                ImGui::Checkbox("Read", &perm_read);
                ImGui::Checkbox("Write", &perm_write);
                ImGui::Checkbox("Execute", &perm_execute);
                
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        
        ImGui::End();
    }
    
    void PropertiesPanel::SetFile(const std::string& filepath) {
        // TODO: Load file properties
        filename = filepath.substr(filepath.find_last_of("/\\") + 1);
        
        // In a real implementation, you would get actual file properties here
    }
}