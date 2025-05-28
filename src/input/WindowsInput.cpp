#include "input/WindowsInput.h"

#ifdef _WIN32
#include <algorithm>  
#include <cmath>      
#include <vector>
#include <chrono>

 
HHOOK LocalTether::Input::WindowsInput::m_hKeyboardHook = nullptr;
HHOOK LocalTether::Input::WindowsInput::m_hMouseHook = nullptr;
LocalTether::Input::WindowsInput* LocalTether::Input::WindowsInput::s_instance_ptr = nullptr;

namespace LocalTether::Input {

WindowsInput::WindowsInput(uint16_t clientScreenWidth, uint16_t clientScreenHeight, bool is_host_mode)
    : m_clientScreenWidth(clientScreenWidth),
      m_clientScreenHeight(clientScreenHeight),
      m_is_host_mode(is_host_mode) {
    
    resetSimulationState();  

    if (m_is_host_mode) {
        s_instance_ptr = this;  
        LocalTether::Utils::Logger::GetInstance().Info("WindowsInput created in Host (Hook) mode.");
    } else {
         
        m_firstPoll = true;
        m_lastPolledMousePos = {0, 0};  
        m_keyStatesBitmask.fill(0);     
        m_previous_combo_held_polling = false;
        LocalTether::Utils::Logger::GetInstance().Info("WindowsInput created in Client (Polling) mode.");
    }
}

WindowsInput::~WindowsInput() {
    stop();
    if (m_is_host_mode && s_instance_ptr == this) {
        s_instance_ptr = nullptr;
    }
}

void WindowsInput::resetSimulationState() {
    InputManager::resetSimulationState();  
    m_simulatedMouseButtonsState = 0;      
    if (m_is_host_mode) {
         
    } else {
         
        m_lastSentRelativeX_polling = -1.0f;
        m_lastSentRelativeY_polling = -1.0f;
        m_lastSentMouseButtons_polling = 0;
        m_accumulatedScrollDeltaX = 0;
        m_accumulatedScrollDeltaY = 0;
    }
    LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: Simulation state reset.");
}

bool WindowsInput::start() {
    if (m_running.load(std::memory_order_relaxed)) {
        LocalTether::Utils::Logger::GetInstance().Warning("WindowsInput::start() called but already running.");
        return true;
    }
    
    m_running.store(true, std::memory_order_relaxed);
    resetSimulationState();  

    if (m_is_host_mode) {
        m_hook_pressed_keys.clear();
        m_hook_combo_was_active_last_check = false;
        m_virtualRelativeX = 0.5f;
        m_virtualRelativeY = 0.5f;
        m_lastHookAbsCoords = {-1, -1};
        {
            std::lock_guard<std::mutex> lock(m_payload_queue_mutex);
            m_received_payloads_queue.clear();
        }
        m_hook_thread_running.store(true, std::memory_order_relaxed);
        try {
            m_hook_thread = std::thread(&WindowsInput::HookThreadMain, this);
        } catch (const std::system_error& e) {
            LocalTether::Utils::Logger::GetInstance().Error("WindowsInput: Failed to create hook thread: " + std::string(e.what()));
            m_running.store(false, std::memory_order_relaxed);
            m_hook_thread_running.store(false, std::memory_order_relaxed);
            return false;
        }
        LocalTether::Utils::Logger::GetInstance().Info("WindowsInput (Host Mode): Start initiated, hook thread launched.");
    } else {
         
        m_keyStatesBitmask.fill(0);
        m_pastKeys.clear();
        m_currentKeys.clear();
        m_keyPressTimes.clear();
        m_firstPoll = true;
        m_previous_combo_held_polling = false;
        LocalTether::Utils::Logger::GetInstance().Info("WindowsInput (Client Mode): Started.");
    }
    return true;
}

void WindowsInput::stop() {
    if (!m_running.load(std::memory_order_relaxed) && (!m_is_host_mode || !m_hook_thread_running.load(std::memory_order_relaxed))) {
        LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: Stop called but not effectively running or already stopping.");
        return;
    }
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput: stop() called.");
    m_running.store(false, std::memory_order_relaxed);

    if (m_is_host_mode) {
        m_hook_thread_running.store(false, std::memory_order_relaxed);
        if (m_hook_thread.joinable()) {
            m_hook_thread.join();  
            LocalTether::Utils::Logger::GetInstance().Info("WindowsInput (Host Mode): Hook thread joined.");
        }
    }
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput stopped.");
}

std::vector<uint8_t> WindowsInput::getPauseKeyCombo() const {
    if (m_is_host_mode) {
        std::lock_guard<std::mutex> lock(m_pause_key_combo_mutex_);
        return InputManager::pause_key_combo_;
    } else {
         
        return InputManager::pause_key_combo_;
    }
}

void WindowsInput::setPauseKeyCombo(const std::vector<uint8_t>& combo) {
    bool was_globally_paused = InputManager::input_globally_paused_.load(std::memory_order_relaxed);
    
    if (m_is_host_mode) {
        std::lock_guard<std::mutex> lock(m_pause_key_combo_mutex_);
        InputManager::pause_key_combo_ = combo;
    } else {
        InputManager::pause_key_combo_ = combo;
    }

    if (combo.empty() && was_globally_paused) {
        InputManager::input_globally_paused_.store(false, std::memory_order_relaxed);
        LocalTether::Utils::Logger::GetInstance().Info("WindowsInput: Pause combo cleared, input RESUMED.");
        if (m_is_host_mode) m_hook_combo_was_active_last_check = false;
        else m_previous_combo_held_polling = false;
    } else if (!combo.empty()) {
        LocalTether::Utils::Logger::GetInstance().Info("WindowsInput: Pause key combo set.");
    }
    if (m_is_host_mode) m_hook_combo_was_active_last_check = false;  
    else m_previous_combo_held_polling = false;  
}

 
std::vector<LocalTether::Network::KeyEvent> WindowsInput::findKeyChanges_polling() {
    m_currentKeys.clear();
    for (BYTE vkCode = 1; vkCode < 255; ++vkCode) {
         
         
        if (GetAsyncKeyState(static_cast<int>(vkCode)) & 0x8000) {
            m_currentKeys.push_back(vkCode);
        }
    }

    std::vector<LocalTether::Network::KeyEvent> changes;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    for (BYTE key : m_pastKeys) {
        if (std::find(m_currentKeys.begin(), m_currentKeys.end(), key) == m_currentKeys.end()) {
            if (isBitSet_polling(key)) {
                auto it = m_keyPressTimes.find(key);
                if (it != m_keyPressTimes.end() &&
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count() >= POLLING_DEBOUNCE_DURATION_MS) {
                    changes.push_back({key, false});  
                    updateKeyState_polling(key, false);
                    m_keyPressTimes.erase(it);
                } else if (it == m_keyPressTimes.end()) {  
                    changes.push_back({key, false});
                    updateKeyState_polling(key, false);
                }
            }
        }
    }

    for (BYTE key : m_currentKeys) {
        if (!isBitSet_polling(key)) {
            auto it = m_keyPressTimes.find(key);
            if (it == m_keyPressTimes.end() ||
                std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count() >= POLLING_DEBOUNCE_DURATION_MS) {
                changes.push_back({key, true});  
                updateKeyState_polling(key, true);
                m_keyPressTimes[key] = now;
            }
        }
    }
    m_pastKeys = m_currentKeys;
    return changes;
}

void WindowsInput::updateKeyState_polling(uint8_t vkCode, bool pressed) {
    if (vkCode == 0) return;
    size_t byteIndex = vkCode / 8;
    size_t bitIndex = vkCode % 8;
    if (byteIndex < KEY_STATE_ARRAY_SIZE_CONST) {
        if (pressed) {
            m_keyStatesBitmask[byteIndex] |= (1 << bitIndex);
        } else {
            m_keyStatesBitmask[byteIndex] &= ~(1 << bitIndex);
        }
    }
}

bool WindowsInput::isBitSet_polling(uint8_t vkCode) const {
    if (vkCode == 0) return false;
    size_t byteIndex = vkCode / 8;
    size_t bitIndex = vkCode % 8;
    if (byteIndex < KEY_STATE_ARRAY_SIZE_CONST) {
        return (m_keyStatesBitmask[byteIndex] & (1 << bitIndex)) != 0;
    }
    return false;
}

 
std::vector<LocalTether::Network::InputPayload> WindowsInput::pollEvents() {
    if (!m_running.load(std::memory_order_relaxed)) {
        return {};
    }

    if (m_is_host_mode) {
         
        std::vector<LocalTether::Network::InputPayload> payloads;
        {
            std::lock_guard<std::mutex> lock(m_payload_queue_mutex);
            if (!m_received_payloads_queue.empty()) {
                payloads.swap(m_received_payloads_queue);
            }
        }
        return payloads;
    } else {
         
        LocalTether::Network::InputPayload current_payload;
        bool events_found = false;
        std::vector<LocalTether::Network::InputPayload> payloads;

         
        if (!InputManager::pause_key_combo_.empty()) {
            bool combo_currently_held_now = true;
            for (uint8_t key_vk_code : InputManager::pause_key_combo_) {
                 
                bool key_satisfied = false;
                if (key_vk_code == VK_CONTROL) {
                    if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) || (GetAsyncKeyState(VK_RCONTROL) & 0x8000) || (GetAsyncKeyState(VK_CONTROL) & 0x8000))
                        key_satisfied = true;
                } else if (key_vk_code == VK_SHIFT) {
                    if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) || (GetAsyncKeyState(VK_RSHIFT) & 0x8000) || (GetAsyncKeyState(VK_SHIFT) & 0x8000))
                        key_satisfied = true;
                } else if (key_vk_code == VK_MENU) {  
                    if ((GetAsyncKeyState(VK_LMENU) & 0x8000) || (GetAsyncKeyState(VK_RMENU) & 0x8000) || (GetAsyncKeyState(VK_MENU) & 0x8000))
                        key_satisfied = true;
                } else {
                    if (GetAsyncKeyState(static_cast<int>(key_vk_code)) & 0x8000)
                        key_satisfied = true;
                }
                if (!key_satisfied) {
                    combo_currently_held_now = false;
                    break;
                }
            }

            if (combo_currently_held_now && !m_previous_combo_held_polling) {
                bool new_pause_state = !InputManager::input_globally_paused_.load(std::memory_order_relaxed);
                InputManager::input_globally_paused_.store(new_pause_state, std::memory_order_relaxed);
                LocalTether::Utils::Logger::GetInstance().Info(
                    std::string("WindowsInput (Polling): Input ") + (new_pause_state ? "PAUSED" : "RESUMED") + " by combo toggle."
                );
            }
            m_previous_combo_held_polling = combo_currently_held_now;
        }

        if (InputManager::input_globally_paused_.load(std::memory_order_relaxed)) {
            return {};
        }

        current_payload.keyEvents = findKeyChanges_polling();
        if (!current_payload.keyEvents.empty()) {
            events_found = true;
        }

        POINT cursorPos;
        if (GetCursorPos(&cursorPos)) {
            int screen_width = GetSystemMetrics(SM_CXSCREEN);
            int screen_height = GetSystemMetrics(SM_CYSCREEN);
            float rel_x = -1.0f, rel_y = -1.0f;

            if (screen_width > 0 && screen_height > 0) {
                rel_x = static_cast<float>(cursorPos.x) / screen_width;
                rel_y = static_cast<float>(cursorPos.y) / screen_height;
                rel_x = std::max(0.0f, std::min(1.0f, rel_x));
                rel_y = std::max(0.0f, std::min(1.0f, rel_y));
            }

            bool significant_move = false;
            if (rel_x != -1.0f) {
                if (m_lastSentRelativeX_polling == -1.0f || m_lastSentRelativeY_polling == -1.0f || m_firstPoll) {
                    significant_move = true;
                } else {
                    constexpr float RELATIVE_DEADZONE = 0.002f;
                    if (std::abs(rel_x - m_lastSentRelativeX_polling) > RELATIVE_DEADZONE ||
                        std::abs(rel_y - m_lastSentRelativeY_polling) > RELATIVE_DEADZONE) {
                        significant_move = true;
                    }
                }
            }

            uint8_t current_buttons = 0;
            if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) current_buttons |= 0x01;
            if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) current_buttons |= 0x02;
            if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) current_buttons |= 0x04;
             
             
             


            bool buttons_changed = (current_buttons != m_lastSentMouseButtons_polling);

            if (significant_move || buttons_changed) {
                current_payload.isMouseEvent = true;
                if (rel_x != -1.0f) current_payload.relativeX = rel_x;
                if (rel_y != -1.0f) current_payload.relativeY = rel_y;
                current_payload.mouseButtons = current_buttons;
                current_payload.sourceDeviceType = LocalTether::Network::InputSourceDeviceType::MOUSE_ABSOLUTE;
                events_found = true;
                if (rel_x != -1.0f) m_lastSentRelativeX_polling = rel_x;
                if (rel_y != -1.0f) m_lastSentRelativeY_polling = rel_y;
                m_lastSentMouseButtons_polling = current_buttons;
            }
        }
        m_firstPoll = false;  

         
         
        int16_t scrollX = m_accumulatedScrollDeltaX.exchange(0);
        int16_t scrollY = m_accumulatedScrollDeltaY.exchange(0);
        if (scrollX != 0 || scrollY != 0) {
            current_payload.scrollDeltaX = scrollX;
            current_payload.scrollDeltaY = scrollY;
            current_payload.isMouseEvent = true;
            if (current_payload.sourceDeviceType == LocalTether::Network::InputSourceDeviceType::UNKNOWN) {
                 current_payload.sourceDeviceType = LocalTether::Network::InputSourceDeviceType::MOUSE_ABSOLUTE;
            }
            events_found = true;
        }

        if (events_found) {
            payloads.push_back(current_payload);
        }
        return payloads;
    }
}


 
void WindowsInput::HookThreadMain() {
     
     
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput (Host Mode): Hook thread started.");
     
    
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    if (!hInstance) {
        LocalTether::Utils::Logger::GetInstance().Error("WindowsInput (Host Mode): GetModuleHandle(nullptr) failed. Error: " + std::to_string(GetLastError()));
        m_hook_thread_running.store(false, std::memory_order_relaxed);
        m_running.store(false, std::memory_order_relaxed);  
        return;
    }

    m_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    if (!m_hKeyboardHook) {
        LocalTether::Utils::Logger::GetInstance().Error("WindowsInput (Host Mode): Failed to install keyboard hook. Error: " + std::to_string(GetLastError()));
        m_hook_thread_running.store(false, std::memory_order_relaxed);
        m_running.store(false, std::memory_order_relaxed);
        return;
    }
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput (Host Mode): Keyboard hook installed.");

    m_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, hInstance, 0);
    if (!m_hMouseHook) {
        LocalTether::Utils::Logger::GetInstance().Error("WindowsInput (Host Mode): Failed to install mouse hook. Error: " + std::to_string(GetLastError()));
        if (m_hKeyboardHook) {
            UnhookWindowsHookEx(m_hKeyboardHook);
            m_hKeyboardHook = nullptr;
        }
        m_hook_thread_running.store(false, std::memory_order_relaxed);
        m_running.store(false, std::memory_order_relaxed);
        return;
    }
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput (Host Mode): Mouse hook installed.");

    MSG msg;
    while (m_hook_thread_running.load(std::memory_order_relaxed)) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_hook_thread_running.store(false, std::memory_order_relaxed);
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!m_hook_thread_running.load(std::memory_order_relaxed)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (m_hKeyboardHook) {
        UnhookWindowsHookEx(m_hKeyboardHook);
        m_hKeyboardHook = nullptr;
        LocalTether::Utils::Logger::GetInstance().Info("WindowsInput (Host Mode): Keyboard hook uninstalled.");
    }
    if (m_hMouseHook) {
        UnhookWindowsHookEx(m_hMouseHook);
        m_hMouseHook = nullptr;
        LocalTether::Utils::Logger::GetInstance().Info("WindowsInput (Host Mode): Mouse hook uninstalled.");
    }
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput (Host Mode): Hook thread finished.");
}

LRESULT CALLBACK WindowsInput::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
     
     
    if (nCode == HC_ACTION && s_instance_ptr) {
        KBDLLHOOKSTRUCT *kbdStruct = (KBDLLHOOKSTRUCT *)lParam;
        if (kbdStruct) {
            BYTE vkCode = static_cast<BYTE>(kbdStruct->vkCode);
            bool isPressed = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

            s_instance_ptr->ProcessKeyFromHook(vkCode, isPressed, kbdStruct->scanCode, (kbdStruct->flags & LLKHF_EXTENDED) != 0);
            s_instance_ptr->CheckPauseComboFromHook();  

            if (InputManager::input_globally_paused_.load(std::memory_order_relaxed)) {
                 
                 
                 
                bool is_key_part_of_combo = false;
                {
                    std::lock_guard<std::mutex> lock(s_instance_ptr->m_pause_key_combo_mutex_);
                    for (uint8_t pKey : s_instance_ptr->pause_key_combo_) {
                         
                        if (pKey == VK_CONTROL && (vkCode == VK_LCONTROL || vkCode == VK_RCONTROL || vkCode == VK_CONTROL)) { is_key_part_of_combo = true; break;}
                        if (pKey == VK_SHIFT && (vkCode == VK_LSHIFT || vkCode == VK_RSHIFT || vkCode == VK_SHIFT)) { is_key_part_of_combo = true; break;}
                        if (pKey == VK_MENU && (vkCode == VK_LMENU || vkCode == VK_RMENU || vkCode == VK_MENU)) { is_key_part_of_combo = true; break;}
                        if (pKey == vkCode) { is_key_part_of_combo = true; break; }
                    }
                }
                
            } else {
                 
                return 1;
            }
        }
    }
    return CallNextHookEx(m_hKeyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK WindowsInput::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
     
     
     if (nCode == HC_ACTION && s_instance_ptr) {
        MSLLHOOKSTRUCT *mouseStruct = (MSLLHOOKSTRUCT *)lParam;
        if (mouseStruct) {
            s_instance_ptr->ProcessMouseFromHook(wParam, mouseStruct); 

            if (InputManager::input_globally_paused_.load(std::memory_order_relaxed)) {
                bool allow_mouse_for_combo = false;
                if (wParam == WM_LBUTTONDOWN || wParam == WM_RBUTTONDOWN || wParam == WM_MBUTTONDOWN || wParam == WM_XBUTTONDOWN) {
                    BYTE vkCode = 0;
                    if (wParam == WM_LBUTTONDOWN) vkCode = VK_LBUTTON;
                    else if (wParam == WM_RBUTTONDOWN) vkCode = VK_RBUTTON;
                    else if (wParam == WM_MBUTTONDOWN) vkCode = VK_MBUTTON;
                    else if (wParam == WM_XBUTTONDOWN) {
                        if(HIWORD(mouseStruct->mouseData) == XBUTTON1) vkCode = VK_XBUTTON1;
                        else if(HIWORD(mouseStruct->mouseData) == XBUTTON2) vkCode = VK_XBUTTON2;
                    }
                    
                    if (vkCode != 0) {
                        std::lock_guard<std::mutex> lock(s_instance_ptr->m_pause_key_combo_mutex_);
                        for (uint8_t pKey : s_instance_ptr->pause_key_combo_) {
                            if (pKey == vkCode) {
                                allow_mouse_for_combo = true;
                                break;
                            }
                        }
                    }
                }
                
            } else {
                 
                return 1;
            }
        }
    }
    return CallNextHookEx(m_hMouseHook, nCode, wParam, lParam);
}

void WindowsInput::ProcessKeyFromHook(BYTE vkCode, bool isPressed, DWORD scanCode, bool isExtended) {
     
     
    if (vkCode == 0) return;
    bool stateChanged = false;

    if (isPressed) {
        if (m_hook_pressed_keys.find(vkCode) == m_hook_pressed_keys.end()) {
            m_hook_pressed_keys.insert(vkCode);
            stateChanged = true;
        }
    } else {
        if (m_hook_pressed_keys.count(vkCode)) {
            m_hook_pressed_keys.erase(vkCode);
            stateChanged = true;
        }
    }

    if(InputManager::input_globally_paused_.load(std::memory_order_relaxed)){
        return;
    }
    if (stateChanged) {
        LocalTether::Network::KeyEvent keyEvent;
        keyEvent.keyCode = vkCode;
        keyEvent.isPressed = isPressed;
         
         

        LocalTether::Network::InputPayload payload;
        payload.keyEvents.push_back(keyEvent);
        payload.isMouseEvent = false;
        {
            std::lock_guard<std::mutex> lock(m_payload_queue_mutex);
            m_received_payloads_queue.push_back(payload);
        }
    }
}

void WindowsInput::ProcessMouseFromHook(WPARAM wParam, MSLLHOOKSTRUCT* mouseInfo) {
     
     
     

    bool should_queue_payload = !InputManager::input_globally_paused_.load(std::memory_order_relaxed);
    bool event_action_occurred_for_payload = false; 

    std::vector<LocalTether::Network::KeyEvent> generated_key_events;
    
    float previous_virtual_x = m_virtualRelativeX;
    float previous_virtual_y = m_virtualRelativeY;

    POINT cursorPos;
    GetCursorPos(&cursorPos);
    if (wParam == WM_MOUSEMOVE) {
       
        long deltaX_abs = mouseInfo->pt.x - cursorPos.x;
        long deltaY_abs = mouseInfo->pt.y - cursorPos.y;

        if (deltaX_abs != 0 || deltaY_abs != 0) {
            int screen_width = GetSystemMetrics(SM_CXSCREEN);
            int screen_height = GetSystemMetrics(SM_CYSCREEN);
            if (screen_width > 0 && screen_height > 0) {
                float normalizedDeltaX = static_cast<float>(deltaX_abs) / screen_width;
                float normalizedDeltaY = static_cast<float>(deltaY_abs) / screen_height;
                
                m_virtualRelativeX += normalizedDeltaX;
                m_virtualRelativeY += normalizedDeltaY;
                m_virtualRelativeX = std::max(0.0f, std::min(1.0f, m_virtualRelativeX));
                m_virtualRelativeY = std::max(0.0f, std::min(1.0f, m_virtualRelativeY));
                if (should_queue_payload) event_action_occurred_for_payload = true;
            }
        }
      
  
    }

    uint8_t buttons_state_before_event = m_lastSentMouseButtons_polling;

    auto handle_mouse_button_event_as_key = 
        [&](BYTE vk_code, bool is_pressed_message, uint8_t button_mask) {
        bool state_truly_changed = false;
        if (is_pressed_message) {  
            if (!(buttons_state_before_event & button_mask)) {  
                m_lastSentMouseButtons_polling |= button_mask;  
                state_truly_changed = true;
            }
        } else {  
            if (buttons_state_before_event & button_mask) {  
                m_lastSentMouseButtons_polling &= ~button_mask;  
                state_truly_changed = true;
            }
        }

        if (state_truly_changed && should_queue_payload) {
            generated_key_events.push_back({vk_code, is_pressed_message});
            event_action_occurred_for_payload = true;
        }
    };

    if (wParam == WM_LBUTTONDOWN) handle_mouse_button_event_as_key(VK_LBUTTON, true, 0x01);
    else if (wParam == WM_LBUTTONUP) handle_mouse_button_event_as_key(VK_LBUTTON, false, 0x01);
    else if (wParam == WM_RBUTTONDOWN) handle_mouse_button_event_as_key(VK_RBUTTON, true, 0x02);
    else if (wParam == WM_RBUTTONUP) handle_mouse_button_event_as_key(VK_RBUTTON, false, 0x02);
    else if (wParam == WM_MBUTTONDOWN) handle_mouse_button_event_as_key(VK_MBUTTON, true, 0x04);
    else if (wParam == WM_MBUTTONUP) handle_mouse_button_event_as_key(VK_MBUTTON, false, 0x04);
    else if (wParam == WM_XBUTTONDOWN) {
        if (HIWORD(mouseInfo->mouseData) == XBUTTON1) handle_mouse_button_event_as_key(VK_XBUTTON1, true, 0x08);
        else if (HIWORD(mouseInfo->mouseData) == XBUTTON2) handle_mouse_button_event_as_key(VK_XBUTTON2, true, 0x10);
    } else if (wParam == WM_XBUTTONUP) {
        if (HIWORD(mouseInfo->mouseData) == XBUTTON1) handle_mouse_button_event_as_key(VK_XBUTTON1, false, 0x08);
        else if (HIWORD(mouseInfo->mouseData) == XBUTTON2) handle_mouse_button_event_as_key(VK_XBUTTON2, false, 0x10);
    }
    
    int16_t scrollDeltaX_hook = 0;
    int16_t scrollDeltaY_hook = 0;
    if (wParam == WM_MOUSEWHEEL) {
        scrollDeltaY_hook = static_cast<int16_t>(GET_WHEEL_DELTA_WPARAM(mouseInfo->mouseData));
        if (should_queue_payload && scrollDeltaY_hook != 0) {
            event_action_occurred_for_payload = true;
        }
    } else if (wParam == WM_MOUSEHWHEEL) {
        scrollDeltaX_hook = static_cast<int16_t>(GET_WHEEL_DELTA_WPARAM(mouseInfo->mouseData));
        if (should_queue_payload && scrollDeltaX_hook != 0) {
            event_action_occurred_for_payload = true;
        }
    }

    if (event_action_occurred_for_payload && should_queue_payload) {
        LocalTether::Network::InputPayload payload_to_send;
        
        bool actual_move_happened = (m_virtualRelativeX != previous_virtual_x || m_virtualRelativeY != previous_virtual_y);
        bool actual_scroll_happened = (scrollDeltaX_hook != 0 || scrollDeltaY_hook != 0);

        if (actual_move_happened || actual_scroll_happened) {
            payload_to_send.isMouseEvent = true;
            payload_to_send.sourceDeviceType = LocalTether::Network::InputSourceDeviceType::MOUSE_ABSOLUTE;
            payload_to_send.relativeX = m_virtualRelativeX;
            payload_to_send.relativeY = m_virtualRelativeY;
            payload_to_send.scrollDeltaX = scrollDeltaX_hook;
            payload_to_send.scrollDeltaY = scrollDeltaY_hook;
        } else {
            payload_to_send.isMouseEvent = false;
            payload_to_send.relativeX = -1.0f;  
            payload_to_send.relativeY = -1.0f;
            payload_to_send.scrollDeltaX = 0;
            payload_to_send.scrollDeltaY = 0;
        }
        
        payload_to_send.keyEvents = generated_key_events;
        payload_to_send.mouseButtons = 0;  

        if (payload_to_send.isMouseEvent || !payload_to_send.keyEvents.empty()) {
            std::lock_guard<std::mutex> lock(m_payload_queue_mutex);
            m_received_payloads_queue.push_back(payload_to_send);
        }
    }   
}

void WindowsInput::CheckPauseComboFromHook() {
    bool combo_currently_active = false;
    std::vector<uint8_t> current_pause_keys_definition;
    {
        std::lock_guard<std::mutex> lock(m_pause_key_combo_mutex_);
        if (InputManager::pause_key_combo_.empty()) {
            if (m_hook_combo_was_active_last_check) {
                 m_hook_combo_was_active_last_check = false;
            }
            return;
        }
        current_pause_keys_definition = InputManager::pause_key_combo_;
    }

    if (!current_pause_keys_definition.empty()) {
        combo_currently_active = true;
        for (uint8_t defined_key : current_pause_keys_definition) {
            bool defined_key_satisfied = false;
            if (defined_key == VK_CONTROL) {
                if (m_hook_pressed_keys.count(VK_LCONTROL) || m_hook_pressed_keys.count(VK_RCONTROL) || m_hook_pressed_keys.count(VK_CONTROL)) {
                    defined_key_satisfied = true;
                }
            } else if (defined_key == VK_SHIFT) {
                if (m_hook_pressed_keys.count(VK_LSHIFT) || m_hook_pressed_keys.count(VK_RSHIFT) || m_hook_pressed_keys.count(VK_SHIFT)) {
                    defined_key_satisfied = true;
                }
            } else if (defined_key == VK_MENU) {
                if (m_hook_pressed_keys.count(VK_LMENU) || m_hook_pressed_keys.count(VK_RMENU) || m_hook_pressed_keys.count(VK_MENU)) {
                    defined_key_satisfied = true;
                }
            } else {
                if (m_hook_pressed_keys.count(defined_key)) {
                    defined_key_satisfied = true;
                }
            }
            if (!defined_key_satisfied) {
                combo_currently_active = false;
                break;
            }
        }
    }
    
    bool about_to_pause_due_to_combo_hook = false;
    if (combo_currently_active && !m_hook_combo_was_active_last_check) {
        if (!InputManager::input_globally_paused_.load(std::memory_order_relaxed)) {
            about_to_pause_due_to_combo_hook = true;
        }
        bool new_pause_state = !InputManager::input_globally_paused_.load(std::memory_order_relaxed);
        InputManager::input_globally_paused_.store(new_pause_state, std::memory_order_relaxed);
        LocalTether::Utils::Logger::GetInstance().Info(
            std::string("WindowsInput (Hook Mode): Input ") + (new_pause_state ? "PAUSED" : "RESUMED") + " by hook combo toggle."
        );
    }
    m_hook_combo_was_active_last_check = combo_currently_active;

    if (about_to_pause_due_to_combo_hook) {
        LocalTether::Network::InputPayload explicit_release_payload;
        explicit_release_payload.isMouseEvent = false;
        LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput (Hook Mode): Pause triggered by combo. Sending explicit release for combo keys.");
        
        std::vector<uint8_t> current_combo_def_copy;
        {
            std::lock_guard<std::mutex> lock(m_pause_key_combo_mutex_);
            current_combo_def_copy = InputManager::pause_key_combo_;
        }

        for (uint8_t key_vk_code : current_combo_def_copy) {
            if (key_vk_code == VK_CONTROL) {
                explicit_release_payload.keyEvents.push_back({VK_LCONTROL, false});
                explicit_release_payload.keyEvents.push_back({VK_RCONTROL, false});
            } else if (key_vk_code == VK_SHIFT) {
                explicit_release_payload.keyEvents.push_back({VK_LSHIFT, false});
                explicit_release_payload.keyEvents.push_back({VK_RSHIFT, false});
            } else if (key_vk_code == VK_MENU) {
                explicit_release_payload.keyEvents.push_back({VK_LMENU, false});
                explicit_release_payload.keyEvents.push_back({VK_RMENU, false});
            } else {
                explicit_release_payload.keyEvents.push_back({key_vk_code, false});
            }
        }
        if (!explicit_release_payload.keyEvents.empty()) {
            std::lock_guard<std::mutex> lock(m_payload_queue_mutex);
            m_received_payloads_queue.push_back(explicit_release_payload);
        }
    }
}


 
void WindowsInput::simulateInput(LocalTether::Network::InputPayload payload, uint16_t hostScreenWidth, uint16_t hostScreenHeight) {
     
     
    if (!m_running.load(std::memory_order_relaxed) && !m_is_host_mode) {  
         LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput (Polling): simulateInput called but not running. Exiting.");
        return;
    }
     if (m_is_host_mode && !m_hook_thread_running.load(std::memory_order_relaxed)) {  
        LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput (Hook): simulateInput called but not running. Exiting.");
        return;
    }


     
    if (payload.keyEvents.empty() && !payload.isMouseEvent) {
         
        return;
    }

    std::vector<INPUT> inputs;

    for (const auto& keyEvent : payload.keyEvents) {
        if (keyEvent.keyCode == 0) continue;
        INPUT input = {0};
        BYTE vkCode = keyEvent.keyCode;
        bool isPressed = keyEvent.isPressed;

        if (vkCode == VK_LBUTTON || vkCode == VK_RBUTTON || vkCode == VK_MBUTTON ||
            vkCode == VK_XBUTTON1 || vkCode == VK_XBUTTON2) {
            input.type = INPUT_MOUSE;
            input.mi.dx = 0; input.mi.dy = 0; input.mi.mouseData = 0;
            if (isPressed) {
                if (vkCode == VK_LBUTTON) input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                else if (vkCode == VK_RBUTTON) input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                else if (vkCode == VK_MBUTTON) input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
                else if (vkCode == VK_XBUTTON1) { input.mi.dwFlags = MOUSEEVENTF_XDOWN; input.mi.mouseData = XBUTTON1; }
                else if (vkCode == VK_XBUTTON2) { input.mi.dwFlags = MOUSEEVENTF_XDOWN; input.mi.mouseData = XBUTTON2; }
            } else {
                if (vkCode == VK_LBUTTON) input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                else if (vkCode == VK_RBUTTON) input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
                else if (vkCode == VK_MBUTTON) input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
                else if (vkCode == VK_XBUTTON1) { input.mi.dwFlags = MOUSEEVENTF_XUP; input.mi.mouseData = XBUTTON1; }
                else if (vkCode == VK_XBUTTON2) { input.mi.dwFlags = MOUSEEVENTF_XUP; input.mi.mouseData = XBUTTON2; }
            }
        } else {
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = vkCode;
            input.ki.dwFlags = isPressed ? 0 : KEYEVENTF_KEYUP;
            UINT scanCode = MapVirtualKeyEx(vkCode, MAPVK_VK_TO_VSC_EX, GetKeyboardLayout(0));
            if (scanCode == 0) scanCode = MapVirtualKeyEx(vkCode, MAPVK_VK_TO_VSC, GetKeyboardLayout(0));
            input.ki.wScan = static_cast<WORD>(scanCode);

             
            switch (vkCode) {
                case VK_RCONTROL: case VK_RMENU: case VK_INSERT: case VK_DELETE:
                case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
                case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT:
                case VK_APPS: case VK_LWIN: case VK_RWIN: case VK_SNAPSHOT:
                case VK_NUMLOCK:  
                case VK_DIVIDE:  
                    input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
                    break;
                default:
                    if (scanCode != 0 && (MapVirtualKeyEx(vkCode, MAPVK_VK_TO_CHAR, GetKeyboardLayout(0)) == 0)) {  
                         
                         
                        if ((input.ki.wScan & 0xFF00) != 0 && vkCode != VK_PAUSE) {  
                              
                        }
                    }
                    break;
            }
             if (scanCode != 0 && !(input.ki.dwFlags & KEYEVENTF_EXTENDEDKEY) && (vkCode == VK_RETURN) && ((input.ki.wScan & 0xFF00) !=0) ) {
                  
                  
             }


            if (scanCode != 0) {  
                 input.ki.dwFlags |= KEYEVENTF_SCANCODE;
            }
        }
        inputs.push_back(input);
    }

    if (payload.isMouseEvent) {
        INPUT mouseEventSim = {0};  
        mouseEventSim.type = INPUT_MOUSE;
        bool mouseEventGenerated = false;

        if (payload.relativeX != -1.0f && payload.relativeY != -1.0f) {
            float processedSimX, processedSimY;
            InputManager::processSimulatedMouseCoordinates(payload.relativeX, payload.relativeY, payload.sourceDeviceType, processedSimX, processedSimY);
             
            mouseEventSim.mi.dx = static_cast<LONG>(processedSimX * 65535.0f);
            mouseEventSim.mi.dy = static_cast<LONG>(processedSimY * 65535.0f);
            mouseEventSim.mi.dwFlags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
            mouseEventGenerated = true;
        }

        if ((payload.mouseButtons & 0x01) != (m_simulatedMouseButtonsState & 0x01)) {
            mouseEventSim.mi.dwFlags |= ((payload.mouseButtons & 0x01) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP); mouseEventGenerated = true;
        }
        if ((payload.mouseButtons & 0x02) != (m_simulatedMouseButtonsState & 0x02)) {
            mouseEventSim.mi.dwFlags |= ((payload.mouseButtons & 0x02) ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP); mouseEventGenerated = true;
        }
        if ((payload.mouseButtons & 0x04) != (m_simulatedMouseButtonsState & 0x04)) {
            mouseEventSim.mi.dwFlags |= ((payload.mouseButtons & 0x04) ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP); mouseEventGenerated = true;
        }
        if ((payload.mouseButtons & 0x08) != (m_simulatedMouseButtonsState & 0x08)) {  
            mouseEventSim.mi.dwFlags |= ((payload.mouseButtons & 0x08) ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP);
            mouseEventSim.mi.mouseData = XBUTTON1; mouseEventGenerated = true;
        }
        if ((payload.mouseButtons & 0x10) != (m_simulatedMouseButtonsState & 0x10)) {  
            mouseEventSim.mi.dwFlags |= ((payload.mouseButtons & 0x10) ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP);
            mouseEventSim.mi.mouseData = XBUTTON2; mouseEventGenerated = true;
        }
        m_simulatedMouseButtonsState = payload.mouseButtons;

         
        bool scrollEventGenerated = false;
        INPUT scrollInput = {0};  
        scrollInput.type = INPUT_MOUSE;

        if (payload.scrollDeltaY != 0) {
            scrollInput.mi.mouseData = static_cast<DWORD>(payload.scrollDeltaY);
            scrollInput.mi.dwFlags |= MOUSEEVENTF_WHEEL;
            scrollEventGenerated = true;
        }
        if (payload.scrollDeltaX != 0) {
            scrollInput.mi.mouseData = static_cast<DWORD>(payload.scrollDeltaX);  
            scrollInput.mi.dwFlags |= MOUSEEVENTF_HWHEEL;
            scrollEventGenerated = true;
        }
        
         
        if (mouseEventGenerated && scrollEventGenerated) {
             
             
            if (mouseEventSim.mi.dwFlags != 0) inputs.push_back(mouseEventSim);
            if (scrollInput.mi.dwFlags != 0) inputs.push_back(scrollInput);
        } else if (mouseEventGenerated) {
            if (mouseEventSim.mi.dwFlags != 0) inputs.push_back(mouseEventSim);
        } else if (scrollEventGenerated) {
            if (scrollInput.mi.dwFlags != 0) inputs.push_back(scrollInput);
        }
    }

    if (!inputs.empty()) {
         
        UINT uSent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
        if (uSent != inputs.size()) {
            LocalTether::Utils::Logger::GetInstance().Error("WindowsInput: SendInput failed to send all events. Error: " + std::to_string(GetLastError()));
        } else {
             
        }
    } else {
         
    }
}


}  
#endif  
