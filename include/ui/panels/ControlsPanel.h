#pragma once
#include "imgui_include.h"
#include <string>
#include <vector>
#include <map>
#include <cstdint>    
#include <algorithm>  
#include "input/InputManager.h"

 
 
 
 
 
 
 
 
 

namespace LocalTether::UI::Panels {

class ControlsPanel {
public:
    ControlsPanel();
    void Show(bool* p_open = nullptr);

private:
     
    void ShowHostControls();
    void ShowClientControls();
    
     
    void ShowPauseKeySettings(LocalTether::Input::InputManager* inputManager);

     
    std::string comboToString(const std::vector<uint8_t>& combo);
    std::string vkToString(uint8_t vkCode);

     
    std::map<uint32_t, char[64]> clientRenameBuffers_;
    uint32_t renamingClientId_ = 0;

     
    bool ctrl_modifier_ = false;
    bool shift_modifier_ = false;
    bool alt_modifier_ = false;
    uint8_t main_key_ = 0; 
    char main_key_buffer_[64];
    std::vector<uint8_t> currently_editing_combo_;
    bool key_capture_active_ = false;
};

}  