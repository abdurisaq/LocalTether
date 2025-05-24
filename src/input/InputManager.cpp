#include "input/InputManager.h"

#ifdef _WIN32
#include "input/WindowsInput.h"
#else
#include "input/LinuxInput.h"
#endif

namespace LocalTether::Input {


std::unique_ptr<InputManager> createInputManager() {
#ifdef _WIN32
    LocalTether::Utils::Logger::GetInstance().Info("Creating WindowsInput manager.");
    return std::make_unique<WindowsInput>();
#else
    LocalTether::Utils::Logger::GetInstance().Info("Creating LinuxInput manager (stub).");
    return std::make_unique<LinuxInput>();
#endif
}

}