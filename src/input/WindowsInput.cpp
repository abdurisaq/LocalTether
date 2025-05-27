#include "input/WindowsInput.h"

#ifdef _WIN32
#include "utils/Logger.h" 
#include <algorithm> 

namespace LocalTether::Input {


WindowsInput::WindowsInput(uint16_t clientScreenWidth, uint16_t clientScreenHeight)
    : clientScreenWidth_(clientScreenWidth), clientScreenHeight_(clientScreenHeight) {
    firstPoll_ = true;
    lastPolledMousePos_ = {0, 0};
    resetSimulationState();
}

WindowsInput::~WindowsInput() {
    stop();
}

bool WindowsInput::start() {
    running_ = true;
    resetSimulationState();
    keyStatesBitmask_.fill(0);
    pastKeys_.clear();
    currentKeys_.clear();
    keyPressTimes_.clear();
    lastSentMousePos_ = {-1, -1};
    lastMouseButtons_ = 0;
    firstPoll_ = true;
    lastPolledMousePos_ = {0, 0};
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput started.");
    return true;
}

void WindowsInput::stop() {
    running_ = false;
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput stopped.");
}

std::vector<uint8_t> WindowsInput::getPauseKeyCombo() const {
    return InputManager::pause_key_combo_;
}

void WindowsInput::setPauseKeyCombo(const std::vector<uint8_t>& combo) {
    InputManager::pause_key_combo_ = combo;  
    if (combo.empty() && InputManager::input_globally_paused_.load(std::memory_order_relaxed)) {  
        InputManager::input_globally_paused_.store(false, std::memory_order_relaxed);
        LocalTether::Utils::Logger::GetInstance().Info("WindowsInput: Pause combo cleared, input resumed.");
    } else if (!combo.empty()) {
        LocalTether::Utils::Logger::GetInstance().Info("WindowsInput: Pause key combo set.");
    }
}
std::vector<LocalTether::Network::InputPayload> WindowsInput::pollEvents() {
    if (!running_.load(std::memory_order_relaxed)) {
        return {};
    }

     
    LocalTether::Network::InputPayload current_payload;  
    bool events_found = false;
    std::vector<LocalTether::Network::InputPayload> payloads;


     
    if (!InputManager::pause_key_combo_.empty()) {
        bool combo_currently_held_now = true;  
        for (uint8_t key_vk_code : InputManager::pause_key_combo_) {
            if (!(GetAsyncKeyState(static_cast<int>(key_vk_code)) & 0x8000)) {
                combo_currently_held_now = false;
                break;
            }
        }
        
         
        if (combo_currently_held_now && !previous_combo_held) {  
            bool new_pause_state = !InputManager::input_globally_paused_.load(std::memory_order_relaxed);
            InputManager::input_globally_paused_.store(new_pause_state, std::memory_order_relaxed);
            
            if (new_pause_state) {
                LocalTether::Utils::Logger::GetInstance().Info("WindowsInput: Input PAUSED by combo toggle.");
            } else {
                LocalTether::Utils::Logger::GetInstance().Info("WindowsInput: Input RESUMED by combo toggle.");
            }
        }
        previous_combo_held = combo_currently_held_now;  
    }

    if (InputManager::input_globally_paused_.load(std::memory_order_relaxed)) {
        return {}; 
    }

     
     
     

     
    current_payload.keyEvents = findKeyChanges();  
    if (!current_payload.keyEvents.empty()) {
        events_found = true;
    }

     
     
     
    /*
    if (!InputManager::pause_key_combo_.empty() && !current_payload.keyEvents.empty()) {  
        std::vector<BYTE> currentlyPressedVkCodes;
        for(const auto& ke : current_payload.keyEvents) {  
            if(ke.isPressed) currentlyPressedVkCodes.push_back(ke.keyCode);
        }

        bool pauseComboActive = true;
        if (currentlyPressedVkCodes.size() < InputManager::pause_key_combo_.size()) { 
            pauseComboActive = false;
        } else {
            for (uint8_t pKey : InputManager::pause_key_combo_) {  
                bool found = false;
                for (BYTE cKey : currentlyPressedVkCodes) {
                    if (cKey == pKey) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    pauseComboActive = false;
                    break;
                }
            }
        }

        if (pauseComboActive) {  
            InputManager::input_globally_paused_.store(true, std::memory_order_relaxed);
            LocalTether::Utils::Logger::GetInstance().Info("Input sending paused by combo (from key events).");
            return {}; 
        }
    }
    */
    
     
     
     
     

     
     
     
     

    POINT cursorPos;
    if (GetCursorPos(&cursorPos)) {
        int screen_width = GetSystemMetrics(SM_CXSCREEN);
        int screen_height = GetSystemMetrics(SM_CYSCREEN);

        float rel_x = -1.0f;
        float rel_y = -1.0f;

        if (screen_width > 0 && screen_height > 0) {
            rel_x = static_cast<float>(cursorPos.x) / screen_width;
            rel_y = static_cast<float>(cursorPos.y) / screen_height;
            
            rel_x = std::max(0.0f, std::min(1.0f, rel_x));
            rel_y = std::max(0.0f, std::min(1.0f, rel_y));
        }

        bool significant_move = false;
        if (rel_x != -1.0f) { 
             if (m_lastSentRelativeX == -1.0f || m_lastSentRelativeY == -1.0f) { 
                significant_move = true;
            } else {
                constexpr float RELATIVE_DEADZONE = 0.002f; 
                if (std::abs(rel_x - m_lastSentRelativeX) > RELATIVE_DEADZONE ||
                    std::abs(rel_y - m_lastSentRelativeY) > RELATIVE_DEADZONE) {
                    significant_move = true;
                }
            }
        }
        
        uint8_t current_buttons = 0;
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) current_buttons |= 0x01; 
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) current_buttons |= 0x02; 
        if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) current_buttons |= 0x04; 

        bool buttons_changed = (current_buttons != m_lastSentMouseButtons); 

        if (significant_move || buttons_changed) {
            current_payload.isMouseEvent = true; 
            if (rel_x != -1.0f) current_payload.relativeX = rel_x;
            if (rel_y != -1.0f) current_payload.relativeY = rel_y;
            current_payload.mouseButtons = current_buttons;
            current_payload.sourceDeviceType = LocalTether::Network::InputSourceDeviceType::MOUSE_ABSOLUTE;
            events_found = true; 
            if (rel_x != -1.0f) m_lastSentRelativeX = rel_x; 
            if (rel_y != -1.0f) m_lastSentRelativeY = rel_y;
            m_lastSentMouseButtons = current_buttons;
        }
    }

    if (m_mouseWheelDeltaX != 0 || m_mouseWheelDeltaY != 0) { 
        current_payload.scrollDeltaX = m_mouseWheelDeltaX;  
        current_payload.scrollDeltaY = m_mouseWheelDeltaY;
        current_payload.isMouseEvent = true; 
        if (current_payload.sourceDeviceType == LocalTether::Network::InputSourceDeviceType::UNKNOWN) {
            current_payload.sourceDeviceType = LocalTether::Network::InputSourceDeviceType::MOUSE_ABSOLUTE;
        }
        events_found = true; 
        m_mouseWheelDeltaX = 0; 
        m_mouseWheelDeltaY = 0;
    }

    if (events_found) { 
        payloads.push_back(current_payload);
    }

    return payloads;
}


std::vector<LocalTether::Network::KeyEvent> WindowsInput::findKeyChanges() {
    currentKeys_.clear();
    for (BYTE vkCode = 1; vkCode < 255; ++vkCode) { 
        
        if (vkCode == VK_SHIFT || vkCode == VK_CONTROL || vkCode == VK_MENU) {
             
            if (GetAsyncKeyState(vkCode) & 0x8000) {
                currentKeys_.push_back(vkCode);
            }
        } else {
          
             if (GetAsyncKeyState(vkCode) & 0x8000) {
                currentKeys_.push_back(vkCode);
            }
        }
    }

    std::vector<LocalTether::Network::KeyEvent> changes;
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

    for (BYTE key : pastKeys_) {
        if (std::find(currentKeys_.begin(), currentKeys_.end(), key) == currentKeys_.end()) {
        
            if (isBitSet(key)) { 
                 auto it = keyPressTimes_.find(key);
                if (it != keyPressTimes_.end() &&
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count() >= DEBOUNCE_DURATION) {
                    changes.push_back({key, false});
                    updateKeyState(key, false);
                    keyPressTimes_.erase(it);
                } else if (it == keyPressTimes_.end()) { 
                     changes.push_back({key, false});
                     updateKeyState(key, false);
                }
            }
        }
    }

    for (BYTE key : currentKeys_) {
        if (!isBitSet(key)) { 
            auto it = keyPressTimes_.find(key);
            if (it == keyPressTimes_.end() || 
                std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count() >= DEBOUNCE_DURATION) {
                changes.push_back({key, true});
                updateKeyState(key, true);
                keyPressTimes_[key] = now;
            }
        }
    }

    pastKeys_ = currentKeys_;
    return changes;
}


void WindowsInput::updateKeyState(uint8_t vkCode, bool pressed) {
    if (vkCode == 0) return; 
    size_t byteIndex = vkCode / 8;
    size_t bitIndex = vkCode % 8;
    if (byteIndex < ARRAY_SIZE) {
        if (pressed) {
            keyStatesBitmask_[byteIndex] |= (1 << bitIndex);
        } else {
            keyStatesBitmask_[byteIndex] &= ~(1 << bitIndex);
        }
    }
}

bool WindowsInput::isBitSet(uint8_t vkCode) const {
    if (vkCode == 0) return false;
    size_t byteIndex = vkCode / 8;
    size_t bitIndex = vkCode % 8;
    if (byteIndex < ARRAY_SIZE) {
        return (keyStatesBitmask_[byteIndex] & (1 << bitIndex)) != 0;
    }
    return false;
}

double WindowsInput::calculateDistance(POINT a, POINT b) {
    return std::sqrt(std::pow(static_cast<double>(a.x) - b.x, 2) + std::pow(static_cast<double>(a.y) - b.y, 2));
}


void WindowsInput::simulateInput( LocalTether::Network::InputPayload payload, uint16_t hostScreenWidth, uint16_t hostScreenHeight) {
    #ifdef _WIN32
    LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: Simulating input events.");

    if (payload.keyEvents.empty() && !payload.isMouseEvent) {
        LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: No input events to simulate.");
        return;
    }else {
        LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: Simulating " + std::to_string(payload.keyEvents.size()) + " key events.");
    }
    if (!running_.load(std::memory_order_relaxed)) {
        LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: simulateInput detected running_ as false. Exiting.");
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
            input.mi.dx = 0; 
            input.mi.dy = 0;
            input.mi.mouseData = 0; 
            if (isPressed) { 
                if (vkCode == VK_LBUTTON) input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                else if (vkCode == VK_RBUTTON) input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
                else if (vkCode == VK_MBUTTON) input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
                else if (vkCode == VK_XBUTTON1) {
                    input.mi.dwFlags = MOUSEEVENTF_XDOWN;
                    input.mi.mouseData = XBUTTON1;
                } else if (vkCode == VK_XBUTTON2) {
                    input.mi.dwFlags = MOUSEEVENTF_XDOWN;
                    input.mi.mouseData = XBUTTON2;
                }
            } else { 
                if (vkCode == VK_LBUTTON) input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                else if (vkCode == VK_RBUTTON) input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
                else if (vkCode == VK_MBUTTON) input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
                else if (vkCode == VK_XBUTTON1) {
                    input.mi.dwFlags = MOUSEEVENTF_XUP;
                    input.mi.mouseData = XBUTTON1;
                } else if (vkCode == VK_XBUTTON2) {
                    input.mi.dwFlags = MOUSEEVENTF_XUP;
                    input.mi.mouseData = XBUTTON2;
                }
            }
        } else { 
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = vkCode;
            input.ki.dwFlags = isPressed ? 0 : KEYEVENTF_KEYUP;

          
            switch (vkCode) {
                case VK_RCONTROL:
                case VK_RMENU:
                case VK_INSERT:
                case VK_DELETE:
                case VK_HOME:
                case VK_END:
                case VK_PRIOR: 
                case VK_NEXT:  
                case VK_UP:
                case VK_DOWN:
                case VK_LEFT:
                case VK_RIGHT:
                case VK_APPS:
                case VK_LWIN:
                case VK_RWIN:
                case VK_SNAPSHOT:
                    input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
                    break;
                default:
                    
                    UINT scanCode = MapVirtualKeyEx(vkCode, MAPVK_VK_TO_VSC_EX, GetKeyboardLayout(0));
                    if (scanCode & 0xFF00) { 
                        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
                    }
                    break;
            }
        }
        inputs.push_back(input);
    }

    if (payload.isMouseEvent) {
        INPUT mouseEvent = {0};
        mouseEvent.type = INPUT_MOUSE;
        bool needsSynReport = false;  

         
        if (payload.relativeX != -1.0f && payload.relativeY != -1.0f) {
            float processedSimX, processedSimY;
            processSimulatedMouseCoordinates(payload.relativeX, payload.relativeY, payload.sourceDeviceType, processedSimX, processedSimY);
            LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: original mouse coordinates processed to (" + std::to_string(payload.relativeX) + ", " + std::to_string(payload.relativeY) + ")");
            LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: Simulated mouse coordinates processed to (" + std::to_string(processedSimX) + ", " + std::to_string(processedSimY) + ")");
            payload.relativeX = processedSimX;
            payload.relativeY = processedSimY;
            mouseEvent.mi.dx = static_cast<LONG>(payload.relativeX * 65535.0f);  
            mouseEvent.mi.dy = static_cast<LONG>(payload.relativeY * 65535.0f);
            mouseEvent.mi.dwFlags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
             
        }

        if ((payload.mouseButtons & 0x01) != (m_simulatedMouseButtonsState & 0x01) ) {  
            mouseEvent.mi.dwFlags |= ((payload.mouseButtons & 0x01) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
        }
        if ((payload.mouseButtons & 0x02) != (m_simulatedMouseButtonsState & 0x02) ) {  
            mouseEvent.mi.dwFlags |= ((payload.mouseButtons & 0x02) ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
        }
        if ((payload.mouseButtons & 0x04) != (m_simulatedMouseButtonsState & 0x04) ) {  
            mouseEvent.mi.dwFlags |= ((payload.mouseButtons & 0x04) ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
        }
        
        m_simulatedMouseButtonsState = payload.mouseButtons;

        if (payload.scrollDeltaX != 0) {
            mouseEvent.mi.mouseData = static_cast<DWORD>(payload.scrollDeltaX);
            mouseEvent.mi.dwFlags |= MOUSEEVENTF_HWHEEL;
        }
        if (payload.scrollDeltaY != 0) {
            mouseEvent.mi.mouseData = static_cast<DWORD>(payload.scrollDeltaY); 
            mouseEvent.mi.dwFlags |= MOUSEEVENTF_WHEEL;
        }
        
        if (mouseEvent.mi.dwFlags != 0) { 
            inputs.push_back(mouseEvent);
        }
    }

    LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: Processed " + std::to_string(inputs.size()) + " input events for simulation.");
    if (!inputs.empty()) {
        LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: Sending " + std::to_string(inputs.size()) + " input events.");

        for( const auto& input : inputs) {
            if (input.type == INPUT_MOUSE) {
                LocalTether::Utils::Logger::GetInstance().Debug("Mouse Event: " + std::to_string(input.mi.dwFlags));
            } else if (input.type == INPUT_KEYBOARD) {
                LocalTether::Utils::Logger::GetInstance().Debug("Keyboard Event: " + std::to_string(input.ki.wVk) + 
                    " Flags: " + std::to_string(input.ki.dwFlags));
            }
        }

        UINT uSent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
        if (uSent != inputs.size()) {
            LocalTether::Utils::Logger::GetInstance().Error("WindowsInput: SendInput failed to send all events. Error: " + std::to_string(GetLastError()));
        }else{
            LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: SendInput successfully sent all events.");
            LocalTether::Utils::Logger::GetInstance().Debug("number of events sent: " + std::to_string(uSent));
        }
    }else{
        LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: No input events processed so cant simulate anything.");
    }
#else
      
    LocalTether::Utils::Logger::GetInstance().Warning("InputManager::simulateInput not implemented for this platform.");
#endif
}

} 
#endif  