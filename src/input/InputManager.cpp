#include "input/InputManager.h"

#ifdef _WIN32
#include "input/WindowsInput.h"
#else
#include "input/LinuxInput.h"
#endif

namespace LocalTether::Input {


std::unique_ptr<InputManager> createInputManager(uint16_t clientScreenWidth, uint16_t clientScreenHeight) {
#ifdef _WIN32
    LocalTether::Utils::Logger::GetInstance().Info("Creating WindowsInput manager for client screen: " + std::to_string(clientScreenWidth) + "x" + std::to_string(clientScreenHeight));
    return std::make_unique<WindowsInput>(clientScreenWidth, clientScreenHeight);
#else
    LocalTether::Utils::Logger::GetInstance().Info("Creating LinuxInput manager for client screen: " + std::to_string(clientScreenWidth) + "x" + std::to_string(clientScreenHeight));
    return std::make_unique<LinuxInput>(clientScreenWidth, clientScreenHeight);
#endif
}

}