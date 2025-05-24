#include "input/WindowsInput.h"

#ifdef _WIN32
#include "utils/Logger.h" 
#include <algorithm> 

namespace LocalTether::Input {


WindowsInput::WindowsInput() {
    firstPoll_ = true;
    lastPolledMousePos_ = {0, 0};
    inputSendingPaused_ = false;
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
    inputSendingPaused_ = false;
    firstPoll_ = true;
    lastPolledMousePos_ = {0, 0};
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput started.");
    return true;
}

void WindowsInput::stop() {
    running_ = false;
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput stopped.");
}

std::vector<LocalTether::Network::InputPayload> WindowsInput::pollEvents() {
    if (!running_.load(std::memory_order_relaxed)) {
        return {};
    }
    if(inputSendingPaused_.load(std::memory_order_relaxed)) {
        
        bool pauseComboStillHeld = true;
        if (pauseKeyBinds_.empty()) { 
             pauseComboStillHeld = false;
        } else {
            for (const auto& key : pauseKeyBinds_) {
                if (!(GetAsyncKeyState(key) & 0x8000)) {
                    pauseComboStillHeld = false;
                    break;
                }
            }
        }
        if(!pauseComboStillHeld && !pauseKeyBinds_.empty()) { 
            LocalTether::Utils::Logger::GetInstance().Info("Input sending resumed (pause combo released).");
            inputSendingPaused_ = false;
        } else {
            return {}; 
        }
    }

    std::vector<LocalTether::Network::InputPayload> payloads;


    auto keyEvents = findKeyChanges();
    if (!keyEvents.empty()) {
        
        if (!pauseKeyBinds_.empty()) {
            std::vector<BYTE> currentlyPressedVkCodes;
            for(const auto& ke : keyEvents) {
                if(ke.isPressed) currentlyPressedVkCodes.push_back(ke.keyCode);
            }

            bool pauseComboActive = true;
            if (currentlyPressedVkCodes.size() < pauseKeyBinds_.size()) { 
                pauseComboActive = false;
            } else {
                for (int pKey : pauseKeyBinds_) {
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

            if (pauseComboActive && currentlyPressedVkCodes.size() == pauseKeyBinds_.size()) {
                 inputSendingPaused_ = true;
                 LocalTether::Utils::Logger::GetInstance().Info("Input sending paused by combo.");
                 
                 return {}; 
            }
        }
       
        LocalTether::Network::InputPayload kbdPayload;
        kbdPayload.isMouseEvent = false;
        kbdPayload.keyEvents = std::move(keyEvents);
        payloads.push_back(kbdPayload);
    }

    // Poll Mouse Events
    LocalTether::Network::InputPayload mousePayload = pollMouseEvents(); 
    if (mousePayload.isMouseEvent) { 
        payloads.push_back(mousePayload);
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


void WindowsInput::setPauseKeyBinds(const std::vector<int>& pauseKeyBinds) {
        pauseKeyBinds_ = pauseKeyBinds;
    }
LocalTether::Network::InputPayload WindowsInput::pollMouseEvents() {
    LocalTether::Network::InputPayload payload; 

    if (!running_.load(std::memory_order_relaxed) || inputSendingPaused_.load(std::memory_order_relaxed)) {
        return payload; 
    }

    POINT currentPos;
    if (GetCursorPos(&currentPos)) {
        if (firstPoll_) {
            lastPolledMousePos_ = currentPos;
            firstPoll_ = false;
            payload.deltaX = 0;
            payload.deltaY = 0;
        } else {
            payload.deltaX = static_cast<int16_t>(currentPos.x - lastPolledMousePos_.x);
            payload.deltaY = static_cast<int16_t>(currentPos.y - lastPolledMousePos_.y);
            lastPolledMousePos_ = currentPos;
        }

        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) payload.mouseButtons |= 0x01;
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) payload.mouseButtons |= 0x02;
        if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) payload.mouseButtons |= 0x04;
        if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) payload.mouseButtons |= 0x08;
        if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) payload.mouseButtons |= 0x10;


        if (payload.deltaX != 0 || payload.deltaY != 0 || payload.mouseButtons != 0) {
            payload.isMouseEvent = true;
        }
    } else {
        // Failed to get cursor position, log or handle
        LocalTether::Utils::Logger::GetInstance().Error("WindowsInput: GetCursorPos failed. Error: " + std::to_string(GetLastError()));
    }

    //haven't figured out how to poll scroll wheel yet, left at 0 for now
    payload.scrollDeltaX = 0;
    payload.scrollDeltaY = 0;

    return payload;
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


void WindowsInput::simulateInput(const LocalTether::Network::InputPayload& payload) {
#ifdef _WIN32
    if (!running_.load(std::memory_order_relaxed)) {
        //probably log here, maybe laters
        return;
    }

    std::vector<INPUT> inputs;

    for (const auto& keyEvent : payload.keyEvents) {
        if (keyEvent.keyCode == 0) continue; 

        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = keyEvent.keyCode;
        input.ki.dwFlags = keyEvent.isPressed ? 0 : KEYEVENTF_KEYUP;

        switch (keyEvent.keyCode) {
            case VK_RCONTROL:
            case VK_RMENU:
            case VK_INSERT:
            case VK_DELETE:
            case VK_HOME:
            case VK_END:
            case VK_PRIOR: // Page Up
            case VK_NEXT:  // Page Down
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
              
                UINT scanCode = MapVirtualKeyEx(keyEvent.keyCode, MAPVK_VK_TO_VSC_EX, GetKeyboardLayout(0));
                if (scanCode & 0xFF00) { 
                    input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
                }
                break;
        }
        inputs.push_back(input);
    }

    // changed to deltas instead
    if (payload.isMouseEvent) {
        
        if (payload.deltaX != 0 || payload.deltaY != 0) {
            INPUT moveInput = {0};
            moveInput.type = INPUT_MOUSE;
            moveInput.mi.dx = payload.deltaX;
            moveInput.mi.dy = payload.deltaY;
            moveInput.mi.dwFlags = MOUSEEVENTF_MOVE;
            inputs.push_back(moveInput);
        }

       //still defaulting to 0 , not implemented yet
        if (payload.scrollDeltaY != 0) {
            INPUT scrollInput = {0};
            scrollInput.type = INPUT_MOUSE;
            scrollInput.mi.dwFlags = MOUSEEVENTF_WHEEL;
            scrollInput.mi.mouseData = static_cast<DWORD>(payload.scrollDeltaY); 
            inputs.push_back(scrollInput);
        }
        if (payload.scrollDeltaX != 0) {
            INPUT hScrollInput = {0};
            hScrollInput.type = INPUT_MOUSE;
            hScrollInput.mi.dwFlags = MOUSEEVENTF_HWHEEL; 
            hScrollInput.mi.mouseData = static_cast<DWORD>(payload.scrollDeltaX);
            inputs.push_back(hScrollInput);
        }
        
    }

    if (!inputs.empty()) {
        UINT uSent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
        if (uSent != inputs.size()) {
            LocalTether::Utils::Logger::GetInstance().Error("WindowsInput: SendInput failed to send all events. Error: " + std::to_string(GetLastError()));
        }
    }
#else
     //should never get here, but just in case
    LocalTether::Utils::Logger::GetInstance().Warning("InputManager::simulateInput not implemented for this platform.");
#endif
}

} 
#endif // _WIN32