#include "ui/panels/PauseSettingsPanel.h"
#include "utils/Logger.h"



namespace LocalTether::UI::Panels {

PauseSettingsPanel::PauseSettingsPanel() {
     
     
     
    auto& config = LocalTether::Utils::Config::GetInstance();
    std::vector<uint8_t> loaded_combo = config.Get(LocalTether::Utils::Config::PAUSE_COMBO_KEY, std::vector<uint8_t>{});
    
    ctrl_modifier_ = false;
    shift_modifier_ = false;
    alt_modifier_ = false;
    main_key_ = 0;

    for(uint8_t vk : loaded_combo) {
        if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) ctrl_modifier_ = true;
        else if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) shift_modifier_ = true;
        else if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) alt_modifier_ = true;
        else main_key_ = vk;  
    }
    if (main_key_ != 0) {
        snprintf(main_key_buffer_, sizeof(main_key_buffer_), "%s", vkToString(main_key_).c_str());
    } else {
        snprintf(main_key_buffer_, sizeof(main_key_buffer_), "Click to set main key");
    }
}

 
std::string PauseSettingsPanel::vkToString(uint8_t vkCode) {
     
    if (vkCode >= 'A' && vkCode <= 'Z' || vkCode >= '0' && vkCode <= '9') {
        return std::string(1, static_cast<char>(vkCode));
    }
    switch (vkCode) {
        case VK_CONTROL: return "Ctrl";
        case VK_LCONTROL: return "LCtrl";
        case VK_RCONTROL: return "RCtrl";
        case VK_SHIFT: return "Shift";
        case VK_LSHIFT: return "LShift";
        case VK_RSHIFT: return "RShift";
        case VK_MENU: return "Alt";
        case VK_LMENU: return "LAlt";
        case VK_RMENU: return "RAlt";
        case VK_ESCAPE: return "Esc";
        case VK_RETURN: return "Enter";
        case VK_TAB: return "Tab";
        case VK_SPACE: return "Space";
         
        default:
            return "VK(" + std::to_string(vkCode) + ")";
    }
}

std::string PauseSettingsPanel::comboToString(const std::vector<uint8_t>& combo) {
    if (combo.empty()) {
        return "None";
    }
    std::string s;
    for (size_t i = 0; i < combo.size(); ++i) {
        s += vkToString(combo[i]);
        if (i < combo.size() - 1) {
            s += " + ";
        }
    }
    return s;
}


void PauseSettingsPanel::Show(bool* p_open, LocalTether::Input::InputManager* inputManager ) {
    if (!p_open || !*p_open) return;
    if( !inputManager) {
        LocalTether::Utils::Logger::GetInstance().Error("InputManager is null in PauseSettingsPanel::Show");
        return;
    }
    auto& config = LocalTether::Utils::Config::GetInstance();  

    if (!inputManager) {
        ImGui::Begin("Pause Key Combo Settings", p_open);
        ImGui::Text("Error: InputManager not available.");
        ImGui::End();
        return;
    }

    ImGui::Begin("Pause Key Combo Settings", p_open);

    std::vector<uint8_t> active_combo = inputManager->getPauseKeyCombo();
    ImGui::Text("Current Pause Combo: %s", comboToString(active_combo).c_str());
    ImGui::Separator();
    ImGui::Text("Set New Combo:");

    ImGui::Checkbox("Ctrl", &ctrl_modifier_); ImGui::SameLine();
    ImGui::Checkbox("Shift", &shift_modifier_); ImGui::SameLine();
    ImGui::Checkbox("Alt", &alt_modifier_);

    if (key_capture_active_) {
        ImGui::Text("Press the main key for the combo...");
        for (ImGuiKey key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; key = (ImGuiKey)(key + 1)) {
            if (ImGui::IsKeyPressed(key, false)) { 
                if (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl || key == ImGuiKey_ModCtrl ||
                    key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift || key == ImGuiKey_ModShift ||
                    key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt || key == ImGuiKey_ModAlt ||
                    key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper || key == ImGuiKey_ModSuper) {
                    continue;
                }
                uint8_t captured_vk = 0;
                if (key >= ImGuiKey_A && key <= ImGuiKey_Z) captured_vk = 'A' + (key - ImGuiKey_A);
                else if (key >= ImGuiKey_0 && key <= ImGuiKey_9) captured_vk = '0' + (key - ImGuiKey_0);
                else if (key == ImGuiKey_Escape) captured_vk = VK_ESCAPE;
                else if (key == ImGuiKey_Enter || key == ImGuiKey_KeypadEnter) captured_vk = VK_RETURN;
                else if (key == ImGuiKey_Tab) captured_vk = VK_TAB;
                else if (key == ImGuiKey_Space) captured_vk = VK_SPACE;
                 

                if (captured_vk != 0) {
                    main_key_ = captured_vk;
                    snprintf(main_key_buffer_, sizeof(main_key_buffer_), "%s", vkToString(main_key_).c_str());
                    key_capture_active_ = false;
                }
                break; 
            }
        }
        if (ImGui::Button("Cancel Key Capture")) {
            key_capture_active_ = false;
            if (main_key_ == 0) {
                 snprintf(main_key_buffer_, sizeof(main_key_buffer_), "Click to set main key");
            } else {
                 snprintf(main_key_buffer_, sizeof(main_key_buffer_), "%s", vkToString(main_key_).c_str());
            }
        }
    } else {
        if (ImGui::Button(main_key_buffer_)) {
            key_capture_active_ = true;
        }
    }

    if (ImGui::Button("Apply This Combo")) {
        currently_editing_combo_.clear();
        if (ctrl_modifier_) currently_editing_combo_.push_back(VK_CONTROL);
        if (shift_modifier_) currently_editing_combo_.push_back(VK_SHIFT);
        if (alt_modifier_) currently_editing_combo_.push_back(VK_MENU);
        if (main_key_ != 0) {
            bool is_main_key_modifier = (main_key_ == VK_CONTROL || main_key_ == VK_LCONTROL || main_key_ == VK_RCONTROL ||
                                         main_key_ == VK_SHIFT   || main_key_ == VK_LSHIFT   || main_key_ == VK_RSHIFT ||
                                         main_key_ == VK_MENU    || main_key_ == VK_LMENU    || main_key_ == VK_RMENU);
            if (!is_main_key_modifier) {
                 if (std::find(currently_editing_combo_.begin(), currently_editing_combo_.end(), main_key_) == currently_editing_combo_.end()){
                    currently_editing_combo_.push_back(main_key_);
                 }
            } else if (currently_editing_combo_.empty()){
                 currently_editing_combo_.push_back(main_key_);
            }
        }

        if (!currently_editing_combo_.empty()) {
            inputManager->setPauseKeyCombo(currently_editing_combo_);
            config.Set(LocalTether::Utils::Config::PAUSE_COMBO_KEY, currently_editing_combo_);
            config.SaveToFile();  
            LocalTether::Utils::Logger::GetInstance().Info("UI: Pause combo applied and saved: " + comboToString(currently_editing_combo_));
        } else {
            LocalTether::Utils::Logger::GetInstance().Warning("UI: Cannot apply an empty combo. Clear instead if intended.");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Active Combo")) {
        std::vector<uint8_t> empty_combo;
        inputManager->setPauseKeyCombo(empty_combo);
        config.Set(LocalTether::Utils::Config::PAUSE_COMBO_KEY, empty_combo);
        config.SaveToFile();  
        
        ctrl_modifier_ = false;
        shift_modifier_ = false;
        alt_modifier_ = false;
        main_key_ = 0;
        snprintf(main_key_buffer_, sizeof(main_key_buffer_), "Click to set main key");
        key_capture_active_ = false;
        LocalTether::Utils::Logger::GetInstance().Info("UI: Active pause combo cleared and saved.");
    }

    ImGui::End();
}

}