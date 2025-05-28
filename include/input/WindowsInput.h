#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <winuser.h>

#include "input/InputManager.h"
#include "network/Message.h"
#include "utils/Logger.h"

#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_set>
#include <queue>
#include <cmath>
#include <array> 
#include <chrono> 


static constexpr size_t KEY_STATE_ARRAY_SIZE_CONST = 256 / 8; 
static constexpr int POLLING_DEBOUNCE_DURATION_MS = 5;


namespace LocalTether::Input {

class WindowsInput : public InputManager {
public:
    
    WindowsInput(uint16_t clientScreenWidth, uint16_t clientScreenHeight, bool is_host_mode);
    ~WindowsInput() override;

    bool start() override;
    void stop() override;

    std::vector<LocalTether::Network::InputPayload> pollEvents() override;
    void simulateInput(LocalTether::Network::InputPayload payload, uint16_t hostScreenWidth, uint16_t hostScreenHeight) override;

    void setPauseKeyCombo(const std::vector<uint8_t>& combo) override;
    std::vector<uint8_t> getPauseKeyCombo() const override;

    bool isRunning() const override {
        return m_running.load(std::memory_order_relaxed);
    }

    void resetSimulationState() override;

private:
    bool m_is_host_mode; 

    std::atomic<bool> m_running{false};
    uint16_t m_clientScreenWidth;
    uint16_t m_clientScreenHeight;
    uint8_t m_simulatedMouseButtonsState = 0; 

    std::thread m_hook_thread;
    std::atomic<bool> m_hook_thread_running{false};
    
    static HHOOK m_hKeyboardHook;
    static HHOOK m_hMouseHook;
    static WindowsInput* s_instance_ptr;

    std::vector<LocalTether::Network::InputPayload> m_received_payloads_queue; 
    std::mutex m_payload_queue_mutex; 

    std::unordered_set<BYTE> m_hook_pressed_keys; 
    bool m_hook_combo_was_active_last_check = false;
    mutable std::mutex m_pause_key_combo_mutex_;

    float m_virtualRelativeX = 0.5f; 
    float m_virtualRelativeY = 0.5f;
    POINT m_lastHookAbsCoords = {-1, -1}; 

    std::array<uint8_t, KEY_STATE_ARRAY_SIZE_CONST> m_keyStatesBitmask;
    std::vector<BYTE> m_pastKeys;
    std::vector<BYTE> m_currentKeys;
    std::unordered_map<BYTE, std::chrono::steady_clock::time_point> m_keyPressTimes;
    
    POINT m_lastPolledMousePos = {0,0};
    float m_lastSentRelativeX_polling = -1.0f; 
    float m_lastSentRelativeY_polling = -1.0f;
    uint8_t m_lastSentMouseButtons_polling = 0; 
    bool m_firstPoll = true; 
    bool m_previous_combo_held_polling = false; 

    std::atomic<int16_t> m_accumulatedScrollDeltaX{0}; 
    std::atomic<int16_t> m_accumulatedScrollDeltaY{0};


    void HookThreadMain();
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    void ProcessKeyFromHook(BYTE vkCode, bool isPressed, DWORD scanCode, bool isExtended);
    void ProcessMouseFromHook(WPARAM wParam, MSLLHOOKSTRUCT* mouseInfo);
    void CheckPauseComboFromHook();

    std::vector<LocalTether::Network::KeyEvent> findKeyChanges_polling();
    void updateKeyState_polling(uint8_t vkCode, bool pressed);
    bool isBitSet_polling(uint8_t vkCode) const;


};

} 
#endif 