#pragma once

#include "network/Message.h" 
#include <vector>
#include <memory>
#include "utils/Logger.h"

namespace LocalTether::Input {

class InputManager {
public:
    virtual ~InputManager() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;


    virtual std::vector<LocalTether::Network::InputPayload> pollEvents() = 0;
    virtual void simulateInput(const LocalTether::Network::InputPayload& payload) = 0;
};


std::unique_ptr<InputManager> createInputManager();

}