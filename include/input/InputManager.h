#pragma once

#include "network/Message.h" 
#include <vector>
#include <memory>
#include <atomic>
#include "utils/Logger.h"

namespace LocalTether::Input {

class InputManager {
public:
    virtual ~InputManager() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;


    virtual std::vector<LocalTether::Network::InputPayload> pollEvents() = 0;
    virtual void simulateInput(const LocalTether::Network::InputPayload& payload,uint16_t hostScreenWidth, uint16_t hostScreenHeight) = 0;
    virtual void setPauseKeyCombo(const std::vector<uint8_t>& combo) = 0;
    virtual std::vector<uint8_t> getPauseKeyCombo() const = 0;

    protected:
    std::vector<uint8_t> pause_key_combo_;
    std::atomic<bool> input_globally_paused_{false}; 
};


std::unique_ptr<InputManager> createInputManager(uint16_t clientScreenWidth, uint16_t clientScreenHeight);

}