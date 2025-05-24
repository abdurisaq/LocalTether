#pragma once

#ifndef _WIN32 // linux

#include "input/InputManager.h"
#include "utils/Logger.h" 
#include <vector>

namespace LocalTether::Input {

class LinuxInput : public InputManager {
public:
    LinuxInput();
    ~LinuxInput() override;

    bool start() override;
    void stop() override;

    std::vector<LocalTether::Network::InputPayload> pollEvents() override;
    void simulateInput(const LocalTether::Network::InputPayload& payload) override;
};

} 

#endif 