#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX 
    #define NOMINMAX
#endif
#include <windows.h>
#include <winuser.h>
#include "input/InputManager.h"
#include <vector>
#include <array>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <cmath> 
#include "core/SDLApp.h"

#define ARRAY_SIZE 32
#define DEBOUNCE_DURATION 5 //milliseconds
#define KEYEVENT_SIZE 9 

namespace LocalTether::Input {

class WindowsInput : public InputManager {
public:
    WindowsInput(uint16_t clientScreenWidth, uint16_t clientScreenHeight);
    ~WindowsInput() override;

    bool start() override;
    void stop() override;
    std::vector<LocalTether::Network::InputPayload> pollEvents() override;
    void simulateInput( LocalTether::Network::InputPayload payload, uint16_t hostScreenWidth, uint16_t hostScreenHeight) override; 


    void setPauseKeyCombo(const std::vector<uint8_t>& combo) override;
    std::vector<uint8_t> getPauseKeyCombo() const override;

    bool isRunning() const override {
        return running_.load(std::memory_order_relaxed);
    }
private:

    std::vector<BYTE> currentKeys_;
    std::vector<BYTE> pastKeys_;
    std::array<uint8_t, ARRAY_SIZE> keyStatesBitmask_ = {0};
    std::unordered_map<BYTE, std::chrono::steady_clock::time_point> keyPressTimes_;

    POINT lastSentMousePos_ = {-1, -1};
    BYTE lastMouseButtons_ = 0;

    std::atomic<bool> running_{false};

  
    uint16_t clientScreenWidth_;
    uint16_t clientScreenHeight_;

    std::vector<LocalTether::Network::KeyEvent> findKeyChanges();
    LocalTether::Network::InputPayload pollMouseEvents();

    void updateKeyState(uint8_t vkCode, bool pressed);
    bool isBitSet(uint8_t vkCode) const;
    static double calculateDistance(POINT a, POINT b);

     POINT lastPolledMousePos_ = {0, 0};
     bool firstPoll_ = true;

     float m_lastSentRelativeX = -1.0f;
    float m_lastSentRelativeY = -1.0f;
    uint8_t m_lastSentMouseButtons = 0;
    uint8_t m_simulatedMouseButtonsState = 0; 
    int16_t m_mouseWheelDeltaX = 0; 
    int16_t m_mouseWheelDeltaY = 0; 
    bool previous_combo_held = false; 

};

} 
#endif 