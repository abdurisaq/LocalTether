#include "input/WindowsInput.h"

#ifdef _WIN32
#include "utils/Logger.h" 
#include <algorithm> 

namespace LocalTether::Input {


WindowsInput::WindowsInput() {

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
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput started.");
    return true;
}

void WindowsInput::stop() {
    running_ = false;
    LocalTether::Utils::Logger::GetInstance().Info("WindowsInput stopped.");
}

std::vector<LocalTether::Network::InputPayload> WindowsInput::pollEvents() {
    if (!running_) {
        return {};
    }

    std::vector<LocalTether::Network::InputPayload> payloads;

    auto keyEvents = findKeyChanges();
    if (!keyEvents.empty()) {
        LocalTether::Network::InputPayload kbdPayload;
        kbdPayload.isMouseEvent = false;
        kbdPayload.keyEvents = std::move(keyEvents);
        payloads.push_back(kbdPayload);
    }

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

LocalTether::Network::InputPayload WindowsInput::pollMouseEvents() {
    LocalTether::Network::InputPayload payload;
    payload.isMouseEvent = false; 

    POINT currentPos;
    if (GetCursorPos(&currentPos)) {
        BYTE currentButtons = 0;
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) currentButtons |= 0x01;
        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) currentButtons |= 0x02;
        if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) currentButtons |= 0x04;
        
        if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) currentButtons |= 0x08;
        if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) currentButtons |= 0x10;


        bool posChanged = (lastSentMousePos_.x != currentPos.x || lastSentMousePos_.y != currentPos.y);
        bool buttonsChanged = (lastMouseButtons_ != currentButtons);

        if (posChanged || buttonsChanged) {
            if (lastSentMousePos_.x == -1 && lastSentMousePos_.y == -1 && !buttonsChanged) {
                
            } else {
                 payload.isMouseEvent = true;
                 payload.mouseX = static_cast<int16_t>(currentPos.x);
                 payload.mouseY = static_cast<int16_t>(currentPos.y);
                 payload.mouseButtons = currentButtons;
            }
            lastSentMousePos_ = currentPos;
            lastMouseButtons_ = currentButtons;
        }
    }
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
    std::vector<INPUT> inputs;

    // Simulate Key Events
    for (const auto& keyEvent : payload.keyEvents) {
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = keyEvent.keyCode;
        input.ki.dwFlags = keyEvent.isPressed ? 0 : KEYEVENTF_KEYUP;
        // Consider adding KEYEVENTF_SCANCODE and input.ki.wScan = MapVirtualKey(keyEvent.keyCode, MAPVK_VK_TO_VSC);
        // For extended keys, set KEYEVENTF_EXTENDEDKEY if necessary
        // Example: if (keyEvent.keyCode == VK_RIGHT || ... ) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        inputs.push_back(input);
    }

    // Simulate Mouse Event
    if (payload.isMouseEvent) {
        // Mouse Movement: SetCursorPos is simpler for absolute screen coordinates.
        // SendInput with MOUSEEVENTF_MOVE is typically for relative moves or when combining with button presses.
        SetCursorPos(payload.mouseX, payload.mouseY);

        // Mouse Buttons
        // This logic sends discrete down/up events based on state changes.
        static BYTE lastSimulatedMouseButtons = 0; // Static to remember state across calls within this instance

        // Left Button
        if ((payload.mouseButtons & 0x01) && !(lastSimulatedMouseButtons & 0x01)) { // Press
            INPUT btnInput = {0}; btnInput.type = INPUT_MOUSE; btnInput.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; inputs.push_back(btnInput);
        } else if (!(payload.mouseButtons & 0x01) && (lastSimulatedMouseButtons & 0x01)) { // Release
            INPUT btnInput = {0}; btnInput.type = INPUT_MOUSE; btnInput.mi.dwFlags = MOUSEEVENTF_LEFTUP; inputs.push_back(btnInput);
        }
        // Right Button
        if ((payload.mouseButtons & 0x02) && !(lastSimulatedMouseButtons & 0x02)) {
            INPUT btnInput = {0}; btnInput.type = INPUT_MOUSE; btnInput.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN; inputs.push_back(btnInput);
        } else if (!(payload.mouseButtons & 0x02) && (lastSimulatedMouseButtons & 0x02)) {
            INPUT btnInput = {0}; btnInput.type = INPUT_MOUSE; btnInput.mi.dwFlags = MOUSEEVENTF_RIGHTUP; inputs.push_back(btnInput);
        }
        // Middle Button
        if ((payload.mouseButtons & 0x04) && !(lastSimulatedMouseButtons & 0x04)) {
            INPUT btnInput = {0}; btnInput.type = INPUT_MOUSE; btnInput.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN; inputs.push_back(btnInput);
        } else if (!(payload.mouseButtons & 0x04) && (lastSimulatedMouseButtons & 0x04)) {
            INPUT btnInput = {0}; btnInput.type = INPUT_MOUSE; btnInput.mi.dwFlags = MOUSEEVENTF_MIDDLEUP; inputs.push_back(btnInput);
        }
        // XBUTTON1
        if ((payload.mouseButtons & 0x08) && !(lastSimulatedMouseButtons & 0x08)) {
            INPUT btnInput = {0}; btnInput.type = INPUT_MOUSE; btnInput.mi.dwFlags = MOUSEEVENTF_XDOWN; btnInput.mi.mouseData = XBUTTON1; inputs.push_back(btnInput);
        } else if (!(payload.mouseButtons & 0x08) && (lastSimulatedMouseButtons & 0x08)) {
            INPUT btnInput = {0}; btnInput.type = INPUT_MOUSE; btnInput.mi.dwFlags = MOUSEEVENTF_XUP; btnInput.mi.mouseData = XBUTTON1; inputs.push_back(btnInput);
        }
        // XBUTTON2
        if ((payload.mouseButtons & 0x10) && !(lastSimulatedMouseButtons & 0x10)) {
            INPUT btnInput = {0}; btnInput.type = INPUT_MOUSE; btnInput.mi.dwFlags = MOUSEEVENTF_XDOWN; btnInput.mi.mouseData = XBUTTON2; inputs.push_back(btnInput);
        } else if (!(payload.mouseButtons & 0x10) && (lastSimulatedMouseButtons & 0x10)) {
            INPUT btnInput = {0}; btnInput.type = INPUT_MOUSE; btnInput.mi.dwFlags = MOUSEEVENTF_XUP; btnInput.mi.mouseData = XBUTTON2; inputs.push_back(btnInput);
        }
        lastSimulatedMouseButtons = payload.mouseButtons;
    }

    if (!inputs.empty()) {
        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }
#else
    // Log that input simulation is not implemented for non-Windows platforms
    LocalTether::Utils::Logger::GetInstance().Warning("InputManager::simulateInput not implemented for this platform.");
#endif
}

} 
#endif // _WIN32