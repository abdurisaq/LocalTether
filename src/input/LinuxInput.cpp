#include "input/LinuxInput.h"

#ifndef _WIN32 //non windows, so linux for now



namespace LocalTether::Input {

LinuxInput::LinuxInput() {
    LocalTether::Utils::Logger::GetInstance().Info("LinuxInput constructor called (stub).");
}

LinuxInput::~LinuxInput() {
    LocalTether::Utils::Logger::GetInstance().Info("LinuxInput destructor called (stub).");
}

bool LinuxInput::start() {
    LocalTether::Utils::Logger::GetInstance().Info("LinuxInput::start() called (unimplemented stub).");
    
    return true; 
}

void LinuxInput::stop() {
    LocalTether::Utils::Logger::GetInstance().Info("LinuxInput::stop() called (unimplemented stub).");
    
}

std::vector<LocalTether::Network::InputPayload> LinuxInput::pollEvents() {
    LocalTether::Utils::Logger::GetInstance().Debug("LinuxInput::pollEvents() called (unimplemented stub).");
    //not implemented for linux side yet, probably gonna have to determine between X11 and Wayland
    return {}; // Return an empty vector for now
}

void LinuxInput::simulateInput(const LocalTether::Network::InputPayload& payload) {
    LocalTether::Utils::Logger::GetInstance().Info("LinuxInput::simulateInput() called (unimplemented stub). Received an input to simulate.");
    
    if (!payload.keyEvents.empty()) {
        LocalTether::Utils::Logger::GetInstance().Info("  SimulateInput: Contains " + std::to_string(payload.keyEvents.size()) + " key events.");
    }
    if (payload.isMouseEvent) {
        LocalTether::Utils::Logger::GetInstance().Info("  SimulateInput: Contains mouse event data (X: " + std::to_string(payload.mouseX) +
                                                     ", Y: " + std::to_string(payload.mouseY) +
                                                     ", Buttons: " + std::to_string(payload.mouseButtons) + ").");
    }
    
   
}

}

#endif 