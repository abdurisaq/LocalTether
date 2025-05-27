#pragma once
#include "imgui_include.h"
#include "input/InputManager.h"
#include "utils/Config.h" // Changed from ConfigManager.h
#include "utils/KeycodeConverter.h" 
#include <vector>
#include <string>
#include <map>
#include <algorithm> 

namespace LocalTether::UI::Panels {

class PauseSettingsPanel {
public:
    PauseSettingsPanel();
    // Changed signature to use Config::GetInstance() internally
    void Show(bool* p_open = nullptr, LocalTether::Input::InputManager* inputManager = nullptr); 

private:
    std::string comboToString(const std::vector<uint8_t>& combo);
    std::string vkToString(uint8_t vkCode);
    void captureKeyForCombo();

    bool ctrl_modifier_ = false;
    bool shift_modifier_ = false;
    bool alt_modifier_ = false;
    uint8_t main_key_ = 0; 
    char main_key_buffer_[64] = "Click to set main key";

    std::vector<uint8_t> currently_editing_combo_;
    bool key_capture_active_ = false;
};

} // namespace LocalTether::UI::Panels