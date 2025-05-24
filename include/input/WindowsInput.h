#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winuser.h>
#include "input/InputManager.h"
#include <vector>
#include <array>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <cmath> 

#define ARRAY_SIZE 32
#define DEBOUNCE_DURATION 5 //milliseconds
#define KEYEVENT_SIZE 9 

namespace LocalTether::Input {

class WindowsInput : public InputManager {
public:
    WindowsInput();
    ~WindowsInput() override;

    bool start() override;
    void stop() override;
    std::vector<LocalTether::Network::InputPayload> pollEvents() override;
    void simulateInput(const LocalTether::Network::InputPayload& payload) override;
    void setPauseKeyBinds(const std::vector<int>& pauseKeyBinds);
private:

    std::vector<BYTE> currentKeys_;
    std::vector<BYTE> pastKeys_;
    std::array<uint8_t, ARRAY_SIZE> keyStatesBitmask_ = {0};
    std::unordered_map<BYTE, std::chrono::steady_clock::time_point> keyPressTimes_;

    POINT lastSentMousePos_ = {-1, -1};
    BYTE lastMouseButtons_ = 0;

    std::atomic<bool> running_{false};
    std::atomic<bool> inputSendingPaused_{false};
    std::vector<int> pauseKeyBinds_;
  
    std::vector<LocalTether::Network::KeyEvent> findKeyChanges();
    LocalTether::Network::InputPayload pollMouseEvents();

    void updateKeyState(uint8_t vkCode, bool pressed);
    bool isBitSet(uint8_t vkCode) const;
    static double calculateDistance(POINT a, POINT b);

     POINT lastPolledMousePos_ = {0, 0};
     bool firstPoll_ = true;

};

} 
#endif 