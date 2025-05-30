#include "ui/panels/ControlsPanel.h"
#include "ui/UIState.h"           
#include "utils/Logger.h"
#include "network/Server.h"
#include "network/Client.h"
#include "network/Session.h"
#include "network/Message.h"
#include "ui/Icons.h"             
#include "input/InputManager.h"      
#include "utils/Config.h"            
#include "utils/KeycodeConverter.h"  

#include <cstdio>  
#include <cstring>  

 
 
#ifndef VK_CONTROL
#define VK_CONTROL 0x11
#endif
#ifndef VK_SHIFT
#define VK_SHIFT   0x10
#endif
#ifndef VK_MENU  
#define VK_MENU    0x12
#endif
#ifndef VK_LCONTROL
#define VK_LCONTROL 0xA2
#endif
#ifndef VK_RCONTROL
#define VK_RCONTROL 0xA3
#endif
#ifndef VK_LSHIFT
#define VK_LSHIFT 0xA0
#endif
#ifndef VK_RSHIFT
#define VK_RSHIFT 0xA1
#endif
#ifndef VK_LMENU
#define VK_LMENU 0xA4
#endif
#ifndef VK_RMENU
#define VK_RMENU 0xA5
#endif
#ifndef VK_ESCAPE
#define VK_ESCAPE 0x1B
#endif
#ifndef VK_RETURN  
#define VK_RETURN 0x0D
#endif
#ifndef VK_TAB
#define VK_TAB 0x09
#endif
#ifndef VK_SPACE
#define VK_SPACE 0x20
#endif
 
 


namespace LocalTether::UI::Panels {

ControlsPanel::ControlsPanel() {
    renamingClientId_ = 0;
    clientRenameBuffers_.clear();

    auto& config = LocalTether::Utils::Config::GetInstance();
    std::vector<uint8_t> loaded_combo = config.Get(LocalTether::Utils::Config::GetPauseComboKey(), std::vector<uint8_t>{VK_CONTROL, VK_SHIFT, 'P'}); 

    ctrl_modifier_ = false;
    shift_modifier_ = false;
    alt_modifier_ = false;
    main_key_ = 0;
    key_capture_active_ = false;
    currently_editing_combo_.clear();  

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

std::string ControlsPanel::vkToString(uint8_t vkCode) {
     
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
void ControlsPanel::Show(bool* p_open) {
    if (!p_open || !*p_open) {
        return;
    }
    ImGui::Begin("Session Controls", nullptr);

    if (LocalTether::UI::app_mode == LocalTether::UI::AppMode::ConnectedAsHost) {
        ShowHostControls();
    } else if (LocalTether::UI::app_mode == LocalTether::UI::AppMode::ConnectedAsClient) {
        ShowClientControls();
    } else {
        ImGui::Text("No active session or unsupported mode for controls.");
    }

    ImGui::End();
}

void ControlsPanel::ShowHostControls() {
    ImGui::Text("Host Controls");
    ImGui::Separator();

    auto* server_ptr = LocalTether::UI::getServerPtr();
    if (!server_ptr) {
        ImGui::Text("Server instance not available.");
        return;
    }
    auto& server = *server_ptr;
    auto& host_client_for_commands = LocalTether::UI::getClient();

    ImGui::Text("Connected Clients:");
    if (ImGui::BeginTable("ClientsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Role");
        ImGui::TableSetupColumn("Input");
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableHeadersRow();

        std::vector<std::shared_ptr<LocalTether::Network::Session>> sessions = server.getSessions();

        for (const auto& session_ptr : sessions) {
            if (!session_ptr || !session_ptr->isAppHandshakeComplete()) continue;

            uint32_t current_client_id = session_ptr->getClientId();
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(current_client_id));

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", current_client_id);

            ImGui::TableSetColumnIndex(1);
            if (renamingClientId_ == current_client_id) {
                char* renameBuffer = clientRenameBuffers_[current_client_id];  
                ImGui::PushItemWidth(-FLT_MIN);
                if (ImGui::InputText("##Rename", renameBuffer, 64, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                    if (strlen(renameBuffer) > 0) {
                         host_client_for_commands.sendCommand("rename_client:" + std::to_string(current_client_id) + ":" + std::string(renameBuffer));
                    }
                    renamingClientId_ = 0;
                }
                ImGui::PopItemWidth();
            } else {
                ImGui::Text("%s", session_ptr->getClientName().c_str());
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", session_ptr->getRoleString().c_str());

            ImGui::TableSetColumnIndex(3);
            if (session_ptr->getRole() == LocalTether::Network::ClientRole::Receiver) {
                bool canReceive = session_ptr->getCanReceiveInput();
                if (ImGui::Checkbox("##InputToggle", &canReceive)) {
                    host_client_for_commands.sendCommand("toggle_input_client:" + std::to_string(current_client_id) + ":" + (canReceive ? "true" : "false"));
                }
            } else {
                ImGui::Text("-");
            }

            ImGui::TableSetColumnIndex(4);
            if (current_client_id != server.getHostClientId()) {
                if (ImGui::Button(ICON_FA_TIMES " Kick")) {
                    host_client_for_commands.sendCommand("kick_client:" + std::to_string(current_client_id));
                }
                ImGui::SameLine();
            }

            if (renamingClientId_ == current_client_id) {
                if (ImGui::Button(ICON_FA_SAVE " Save")) {
                    char* renameBuffer = clientRenameBuffers_[current_client_id];
                    if (strlen(renameBuffer) > 0) {
                        host_client_for_commands.sendCommand("rename_client:" + std::to_string(current_client_id) + ":" + std::string(renameBuffer));
                    }
                    renamingClientId_ = 0;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    renamingClientId_ = 0;
                }
            } else {
                 if (ImGui::Button(ICON_FA_EDIT " Ren.")) {
                    renamingClientId_ = current_client_id;
                     
                    strncpy(clientRenameBuffers_[current_client_id], session_ptr->getClientName().c_str(), 63);
                    clientRenameBuffers_[current_client_id][63] = '\0';
                }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    if (ImGui::Button(ICON_FA_POWER_OFF " Shutdown Server")) {
        host_client_for_commands.sendCommand("shutdown_server");
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Local Pause Key Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (host_client_for_commands.getInputManager()) {
            ShowPauseKeySettings(host_client_for_commands.getInputManager());
        } else {
            ImGui::Text("Input Manager not available for host client.");
        }
    }
}

void ControlsPanel::ShowClientControls() {
    ImGui::Text("Client Controls");
    ImGui::Separator();
    auto& client = LocalTether::UI::getClient();
    if (ImGui::Button(ICON_FA_SIGN_OUT_ALT " Disconnect from Server")) {
        LocalTether::Utils::Logger::GetInstance().Info("User initiated disconnect via Controls Panel.");
        client.disconnect("User disconnected");
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("Pause Key Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (client.getInputManager()) {
            ShowPauseKeySettings(client.getInputManager());
        } else {
            ImGui::Text("Input Manager not available.");
        }
    }
}


std::string ControlsPanel::comboToString(const std::vector<uint8_t>& combo) {
    if (combo.empty()) {
        return "None";
    }
    std::string s;
    bool first = true;
    for (uint8_t vk : combo) {
        if (!first) {
            s += " + ";
        }
        s += vkToString(vk);  
        first = false;
    }
    return s;
}

void ControlsPanel::ShowPauseKeySettings(LocalTether::Input::InputManager* inputManager) {
    if (!inputManager) {
        ImGui::Text("Error: InputManager not available for pause settings.");
        return;
    }
    auto& config = LocalTether::Utils::Config::GetInstance();

    std::vector<uint8_t> active_combo = inputManager->getPauseKeyCombo();
    ImGui::Text("Current Pause Combo: %s", comboToString(active_combo).c_str());
    ImGui::Separator();
    ImGui::Text("Set New Combo:");

    ImGui::Checkbox("Ctrl", &this->ctrl_modifier_); ImGui::SameLine();
    ImGui::Checkbox("Shift", &this->shift_modifier_); ImGui::SameLine();
    ImGui::Checkbox("Alt", &this->alt_modifier_);

    if (this->key_capture_active_) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press the main key for the combo...");
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
                else if (key == ImGuiKey_Space) captured_vk = VK_SPACE;
                else if (key == ImGuiKey_Enter || key == ImGuiKey_KeypadEnter) captured_vk = VK_RETURN;
                else if (key == ImGuiKey_Escape) captured_vk = VK_ESCAPE;
                else if (key == ImGuiKey_Tab) captured_vk = VK_TAB;
                 
                 


                if (captured_vk != 0) {
                      
                    bool captured_is_ctrl = (captured_vk == VK_CONTROL || captured_vk == VK_LCONTROL || captured_vk == VK_RCONTROL);
                    bool captured_is_shift = (captured_vk == VK_SHIFT || captured_vk == VK_LSHIFT || captured_vk == VK_RSHIFT);
                    bool captured_is_alt = (captured_vk == VK_MENU || captured_vk == VK_LMENU || captured_vk == VK_RMENU);

                    if (!((this->ctrl_modifier_ && captured_is_ctrl) ||
                          (this->shift_modifier_ && captured_is_shift) ||
                          (this->alt_modifier_ && captured_is_alt))) {
                        this->main_key_ = captured_vk;
                        snprintf(this->main_key_buffer_, sizeof(this->main_key_buffer_), "%s", vkToString(this->main_key_).c_str());
                        this->key_capture_active_ = false;
                        break; 
                    }
                }
            }
        }
        if (ImGui::Button("Cancel Key Capture")) {
            this->key_capture_active_ = false;
            if (this->main_key_ == 0) {
                 snprintf(this->main_key_buffer_, sizeof(this->main_key_buffer_), "Click to set main key");
            } else {
                 snprintf(this->main_key_buffer_, sizeof(this->main_key_buffer_), "%s", vkToString(this->main_key_).c_str());
            }
        }
    } else {
        if (ImGui::Button(this->main_key_buffer_)) {
            this->key_capture_active_ = true;
            snprintf(this->main_key_buffer_, sizeof(this->main_key_buffer_), "Press a key...");
        }
    }

    if (ImGui::Button("Apply This Combo")) {
        this->currently_editing_combo_.clear();
        if (this->ctrl_modifier_) this->currently_editing_combo_.push_back(VK_CONTROL);
        if (this->shift_modifier_) this->currently_editing_combo_.push_back(VK_SHIFT);
        if (this->alt_modifier_) this->currently_editing_combo_.push_back(VK_MENU);
        
        if (this->main_key_ != 0) {
            bool main_key_is_ctrl = (this->main_key_ == VK_CONTROL || this->main_key_ == VK_LCONTROL || this->main_key_ == VK_RCONTROL);
            bool main_key_is_shift = (this->main_key_ == VK_SHIFT || this->main_key_ == VK_LSHIFT || this->main_key_ == VK_RSHIFT);
            bool main_key_is_alt = (this->main_key_ == VK_MENU || this->main_key_ == VK_LMENU || this->main_key_ == VK_RMENU);

            if (!((this->ctrl_modifier_ && main_key_is_ctrl) ||
                  (this->shift_modifier_ && main_key_is_shift) ||
                  (this->alt_modifier_ && main_key_is_alt))) {
                 if (std::find(this->currently_editing_combo_.begin(), this->currently_editing_combo_.end(), this->main_key_) == this->currently_editing_combo_.end()){
                    this->currently_editing_combo_.push_back(this->main_key_);
                 }
            } else if (!this->ctrl_modifier_ && !this->shift_modifier_ && !this->alt_modifier_) {  
                 if (std::find(this->currently_editing_combo_.begin(), this->currently_editing_combo_.end(), this->main_key_) == this->currently_editing_combo_.end()){
                    this->currently_editing_combo_.push_back(this->main_key_);
                 }
            }
        }

        if (!this->currently_editing_combo_.empty()) {
            inputManager->setPauseKeyCombo(this->currently_editing_combo_);
            config.Set(LocalTether::Utils::Config::GetPauseComboKey(), this->currently_editing_combo_);
            config.SaveToFile();  
            LocalTether::Utils::Logger::GetInstance().Info("Pause key combo updated to: " + comboToString(this->currently_editing_combo_));
        } else {
            LocalTether::Utils::Logger::GetInstance().Warning("Attempted to apply an empty pause key combo.");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset to Default")) {
        std::vector<uint8_t> default_combo = {VK_CONTROL, VK_SHIFT, 'P'};
        inputManager->setPauseKeyCombo(default_combo);
        config.Set(LocalTether::Utils::Config::GetPauseComboKey(), default_combo);
        config.SaveToFile();  
        
        this->ctrl_modifier_ = false; this->shift_modifier_ = false; this->alt_modifier_ = false; this->main_key_ = 0;
        for(uint8_t vk : default_combo) {
            if (vk == VK_CONTROL) this->ctrl_modifier_ = true;
            else if (vk == VK_SHIFT) this->shift_modifier_ = true;
            else if (vk == VK_MENU) this->alt_modifier_ = true;
            else this->main_key_ = vk;
        }
        if (this->main_key_ != 0) snprintf(this->main_key_buffer_, sizeof(this->main_key_buffer_), "%s", vkToString(this->main_key_).c_str());
        else snprintf(this->main_key_buffer_, sizeof(this->main_key_buffer_), "Click to set main key");

        LocalTether::Utils::Logger::GetInstance().Info("Pause key combo reset to default: " + comboToString(default_combo));
    }
}

}  