#pragma once

#include "network/Message.h" 
#include <vector>
#include <memory>
#include <atomic>
#include "utils/Logger.h"
#include "utils/Config.h"
#include "utils/KeycodeConverter.h"
#ifdef _WIN32
#ifndef NOMINMAX 
    #define NOMINMAX
#endif
#endif
namespace LocalTether::Input {

class InputManager {
public:
    InputManager() { 
        loadAndSetPauseCombo();
    }
    virtual ~InputManager() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    virtual void resetSimulationState() {
        m_lastSimulatedRelativeX.store(-1.0f);
        m_lastSimulatedRelativeY.store(-1.0f);
        m_anchorDeviceRelativeX.store(-1.0f);
        m_anchorDeviceRelativeY.store(-1.0f);
        LocalTether::Utils::Logger::GetInstance().Debug("InputManager: Base simulation state reset.");
    }

    virtual std::vector<LocalTether::Network::InputPayload> pollEvents() = 0;
    virtual void simulateInput( LocalTether::Network::InputPayload payload,uint16_t hostScreenWidth, uint16_t hostScreenHeight) = 0;
    virtual void setPauseKeyCombo(const std::vector<uint8_t>& combo) = 0;
    virtual std::vector<uint8_t> getPauseKeyCombo() const = 0;

    static bool isInputGloballyPaused() { 
        return input_globally_paused_.load(std::memory_order_relaxed);
    }

    virtual bool isRunning() const = 0;

    protected:
    
    std::vector<uint8_t> pause_key_combo_;
    static std::atomic<bool> input_globally_paused_; 

    std::atomic<float> m_lastSimulatedRelativeX{-1.0f};
    std::atomic<float> m_lastSimulatedRelativeY{-1.0f};
    std::atomic<float> m_anchorDeviceRelativeX{-1.0f};
    std::atomic<float> m_anchorDeviceRelativeY{-1.0f};

    static constexpr float SIMULATION_JUMP_THRESHOLD = 0.02f;
    

    void processSimulatedMouseCoordinates(float payloadX, float payloadY, Network::InputSourceDeviceType sourceDeviceType, float& outSimX, float& outSimY) ;

    void loadAndSetPauseCombo() {
        auto& config = LocalTether::Utils::Config::GetInstance();
        std::vector<uint8_t> loaded_combo = config.Get<std::vector<uint8_t>>(LocalTether::Utils::Config::GetPauseComboKey(), {});

        if (loaded_combo.empty()) {
            LocalTether::Utils::Logger::GetInstance().Info("InputManager: No pause combo in config or config was empty. Setting default: CTRL + 0");
            pause_key_combo_ = {VK_CONTROL, static_cast<uint8_t>('0')};
        } else {
            pause_key_combo_ = loaded_combo; 
        }
    }

};


std::unique_ptr<InputManager> createInputManager(uint16_t clientScreenWidth, uint16_t clientScreenHeight,bool is_host_mode);

}