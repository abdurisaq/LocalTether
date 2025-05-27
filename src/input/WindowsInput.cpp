#include "input/WindowsInput.h"

#ifdef _WIN32
#include "utils/Logger.h" 
#include <algorithm> 

namespace LocalTether::Input {


WindowsInput::WindowsInput(uint16_t clientScreenWidth, uint16_t clientScreenHeight)
    : clientScreenWidth_(clientScreenWidth), clientScreenHeight_(clientScreenHeight) {
    firstPoll_ = true;
    lastPolledMousePos_ = {0, 0};
}

WindowsInput::~WindowsInput() {
    stop();
}

bool WindowsInput::start() {
    running_ = true;
   
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
    InputManager::pause_key_combo_ = combo; // Use base class member
    if (combo.empty() && InputManager::input_globally_paused_.load(std::memory_order_relaxed)) { // Use base class member
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

    // Declare current_payload and events_found here
    LocalTether::Network::InputPayload current_payload; // relativeX/Y default to -1.0f
    bool events_found = false;
    std::vector<LocalTether::Network::InputPayload> payloads;


    // Pause Combo Check Part 1 (before checking global pause state)
    if (!InputManager::pause_key_combo_.empty()) {
        bool combo_currently_held_now = true; // Use a different name to avoid conflict with static
        for (uint8_t key_vk_code : InputManager::pause_key_combo_) {
            if (!(GetAsyncKeyState(static_cast<int>(key_vk_code)) & 0x8000)) {
                combo_currently_held_now = false;
                break;
            }
        }
        
        // Toggle pause state on combo press (not release)
        if (combo_currently_held_now && !previous_combo_held) { // previous_combo_held is now a member
            bool new_pause_state = !InputManager::input_globally_paused_.load(std::memory_order_relaxed);
            InputManager::input_globally_paused_.store(new_pause_state, std::memory_order_relaxed);
            
            if (new_pause_state) {
                LocalTether::Utils::Logger::GetInstance().Info("WindowsInput: Input PAUSED by combo toggle.");
            } else {
                LocalTether::Utils::Logger::GetInstance().Info("WindowsInput: Input RESUMED by combo toggle.");
            }
        }
        previous_combo_held = combo_currently_held_now; // Update member variable
    }

    if (InputManager::input_globally_paused_.load(std::memory_order_relaxed)) {
        return {}; 
    }

    // auto keyEvents = findKeyChanges(); // This was the old structure
    // if (!keyEvents.empty()) { ... }
    // The new structure integrates key polling more directly or assumes findKeyChanges populates current_payload.keyEvents

    // 1. Poll Key Changes
    current_payload.keyEvents = findKeyChanges(); // Assuming findKeyChanges returns std::vector<KeyEvent>
    if (!current_payload.keyEvents.empty()) {
        events_found = true;
    }

    // This pause combo logic seems redundant or misplaced if the one above is active.
    // The original code had two pause combo checks. I'll keep the first one and remove this one
    // to avoid confusion and potential double toggling.
    /*
    if (!InputManager::pause_key_combo_.empty() && !current_payload.keyEvents.empty()) { // Check only if there are key events
        std::vector<BYTE> currentlyPressedVkCodes;
        for(const auto& ke : current_payload.keyEvents) { // Use current_payload.keyEvents
            if(ke.isPressed) currentlyPressedVkCodes.push_back(ke.keyCode);
        }

        bool pauseComboActive = true;
        if (currentlyPressedVkCodes.size() < InputManager::pause_key_combo_.size()) { 
            pauseComboActive = false;
        } else {
            for (uint8_t pKey : InputManager::pause_key_combo_) { // Use uint8_t to match combo type
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

        if (pauseComboActive) { // Simplified: if combo is active based on current key events
            InputManager::input_globally_paused_.store(true, std::memory_order_relaxed);
            LocalTether::Utils::Logger::GetInstance().Info("Input sending paused by combo (from key events).");
            return {}; 
        }
    }
    */
    
    // If key events were found and they were not a pause combo, add them.
    // This logic needs to be re-evaluated. If findKeyChanges() populates current_payload.keyEvents,
    // and events_found is true, we will later add current_payload to payloads if any mouse activity also occurs,
    // or if only key events occurred.

    // The original code structure was:
    // 1. Poll keys -> if key events, make a kbdPayload and add to payloads.
    // 2. Poll mouse -> if mouse events, make a mousePayload (or augment existing if logic allows).
    // Let's try to stick to one current_payload that accumulates events.

    POINT cursorPos;
    if (GetCursorPos(&cursorPos)) {
        int screen_width = GetSystemMetrics(SM_CXSCREEN);
        int screen_height = GetSystemMetrics(SM_CYSCREEN);

        float rel_x = -1.0f;
        float rel_y = -1.0f;

        if (screen_width > 0 && screen_height > 0) {
            rel_x = static_cast<float>(cursorPos.x) / screen_width;
            rel_y = static_cast<float>(cursorPos.y) / screen_height;
            
            rel_x = max(0.0f, min(1.0f, rel_x));
            rel_y = max(0.0f, min(1.0f, rel_y));
        }

        bool significant_move = false;
        if (rel_x != -1.0f) { 
             if (m_lastSentRelativeX == -1.0f || m_lastSentRelativeY == -1.0f) { // Uses member variable
                significant_move = true;
            } else {
                constexpr float RELATIVE_DEADZONE = 0.005f; 
                if (std::abs(rel_x - m_lastSentRelativeX) > RELATIVE_DEADZONE || // Uses member variable
                    std::abs(rel_y - m_lastSentRelativeY) > RELATIVE_DEADZONE) { // Uses member variable
                    significant_move = true;
                }
            }
        }
        
        uint8_t current_buttons = 0;
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) current_buttons |= 0x01; 
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) current_buttons |= 0x02; 
        if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) current_buttons |= 0x04; 

        bool buttons_changed = (current_buttons != m_lastSentMouseButtons); // Uses member variable

        if (significant_move || buttons_changed) {
            current_payload.isMouseEvent = true; // Modify the single current_payload
            if (rel_x != -1.0f) current_payload.relativeX = rel_x;
            if (rel_y != -1.0f) current_payload.relativeY = rel_y;
            current_payload.mouseButtons = current_buttons;
            events_found = true; // Mark that some event (mouse) has been found
            if (rel_x != -1.0f) m_lastSentRelativeX = rel_x; 
            if (rel_y != -1.0f) m_lastSentRelativeY = rel_y;
            m_lastSentMouseButtons = current_buttons;
        }
    }

    if (m_mouseWheelDeltaX != 0 || m_mouseWheelDeltaY != 0) { // Uses member variable
        current_payload.scrollDeltaX = m_mouseWheelDeltaX;    // Modify the single current_payload
        current_payload.scrollDeltaY = m_mouseWheelDeltaY;
        current_payload.isMouseEvent = true; 
        events_found = true; // Mark that some event (scroll) has been found
        m_mouseWheelDeltaX = 0; 
        m_mouseWheelDeltaY = 0;
    }

    if (events_found) { // If any key, mouse, or scroll event was found
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


void WindowsInput::simulateInput(const LocalTether::Network::InputPayload& payload, uint16_t hostScreenWidth, uint16_t hostScreenHeight) {
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
        bool needsSynReport = false; // Windows SendInput batches implicitly

        // Position
        if (payload.relativeX != -1.0f && payload.relativeY != -1.0f) {
            mouseEvent.mi.dx = static_cast<LONG>(payload.relativeX * 65535.0f); // Range 0-65535
            mouseEvent.mi.dy = static_cast<LONG>(payload.relativeY * 65535.0f);
            mouseEvent.mi.dwFlags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
            // LocalTether::Utils::Logger::GetInstance().Debug("WindowsInput: Simulating relative move (" + std::to_string(payload.relativeX) + ", " + std::to_string(payload.relativeY) + ")");
        }

        // Buttons (from payload.mouseButtons bitmask)
        // This requires careful mapping if payload.mouseButtons is different from how keyEvents handles mouse buttons.
        // If mouse buttons are *only* in keyEvents, this section might not be needed or needs adjustment.
        // Assuming payload.mouseButtons is a state:
        if ((payload.mouseButtons & 0x01) != (m_simulatedMouseButtonsState & 0x01) ) { // Left button change
            mouseEvent.mi.dwFlags |= ((payload.mouseButtons & 0x01) ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
        }
        if ((payload.mouseButtons & 0x02) != (m_simulatedMouseButtonsState & 0x02) ) { // Right button change
            mouseEvent.mi.dwFlags |= ((payload.mouseButtons & 0x02) ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP);
        }
        if ((payload.mouseButtons & 0x04) != (m_simulatedMouseButtonsState & 0x04) ) { // Middle button change
            mouseEvent.mi.dwFlags |= ((payload.mouseButtons & 0x04) ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP);
        }
        // Add XBUTTON1, XBUTTON2 if your payload.mouseButtons supports them
        m_simulatedMouseButtonsState = payload.mouseButtons;


        // Scroll
        if (payload.scrollDeltaX != 0) {
            mouseEvent.mi.mouseData = static_cast<DWORD>(payload.scrollDeltaX);
            mouseEvent.mi.dwFlags |= MOUSEEVENTF_HWHEEL;
        }
        if (payload.scrollDeltaY != 0) {
            mouseEvent.mi.mouseData = static_cast<DWORD>(payload.scrollDeltaY); // Windows uses positive for scroll down, negative for up. Check payload convention.
            mouseEvent.mi.dwFlags |= MOUSEEVENTF_WHEEL;
        }
        
        if (mouseEvent.mi.dwFlags != 0) { // Only add if there are actual mouse flags set
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
     //should never get here, but just in case
    LocalTether::Utils::Logger::GetInstance().Warning("InputManager::simulateInput not implemented for this platform.");
#endif
}

} 
#endif // _WIN32