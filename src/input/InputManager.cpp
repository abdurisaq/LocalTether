#include "input/InputManager.h"

#ifdef _WIN32
#include "input/WindowsInput.h"
#else
#include "input/LinuxInput.h"
#endif

namespace LocalTether::Input {


std::unique_ptr<InputManager> createInputManager(uint16_t clientScreenWidth, uint16_t clientScreenHeight,bool is_host_mode) {
#ifdef _WIN32
    LocalTether::Utils::Logger::GetInstance().Info("Creating WindowsInput manager for client screen: " + std::to_string(clientScreenWidth) + "x" + std::to_string(clientScreenHeight));
    return std::make_unique<WindowsInput>(clientScreenWidth, clientScreenHeight,is_host_mode);
#else
    LocalTether::Utils::Logger::GetInstance().Info("Creating LinuxInput manager for client screen: " + std::to_string(clientScreenWidth) + "x" + std::to_string(clientScreenHeight));
    return std::make_unique<LinuxInput>(clientScreenWidth, clientScreenHeight,is_host_mode);
#endif
}

void InputManager::processSimulatedMouseCoordinates(float payloadX, float payloadY, Network::InputSourceDeviceType sourceDeviceType, float& outSimX, float& outSimY) {
    float lastSimX_val = m_lastSimulatedRelativeX.load(std::memory_order_relaxed);
    float lastSimY_val = m_lastSimulatedRelativeY.load(std::memory_order_relaxed);
    float anchorDevX_val = m_anchorDeviceRelativeX.load(std::memory_order_relaxed);
    float anchorDevY_val = m_anchorDeviceRelativeY.load(std::memory_order_relaxed);

    LocalTether::Utils::Logger::GetInstance().Debug(
        "SimMouseProc START: payload(" + std::to_string(payloadX) + "," + std::to_string(payloadY) +
        "), sourceDevice: " + std::to_string(static_cast<int>(sourceDeviceType)) +
        ", lastSim(" + std::to_string(lastSimX_val) + "," + std::to_string(lastSimY_val) +
        "), anchorDev(" + std::to_string(anchorDevX_val) + "," + std::to_string(anchorDevY_val) + ")"
    );
    
    if (payloadX < 0.0f || payloadY < 0.0f) { return; }

    if (sourceDeviceType == Network::InputSourceDeviceType::TRACKPAD_ABSOLUTE) {
        LocalTether::Utils::Logger::GetInstance().Debug("SimMouseProc: Applying TRACKPAD_ABSOLUTE logic.");
        if (lastSimX_val < 0.0f || anchorDevX_val < 0.0f) { // Initial state or after reset for trackpad
            LocalTether::Utils::Logger::GetInstance().Debug("SimMouseProc: Trackpad initial state or reset.");
            outSimX = payloadX;
            outSimY = payloadY;
            m_anchorDeviceRelativeX.store(payloadX, std::memory_order_relaxed);
            m_anchorDeviceRelativeY.store(payloadY, std::memory_order_relaxed);
            LocalTether::Utils::Logger::GetInstance().Debug("SimMouseProc: Trackpad set outSim to payload, anchor to payload. New Anchor: (" + std::to_string(payloadX) + "," + std::to_string(payloadY) + ")");
        } else {
            float deltaPayloadToAnchorX = payloadX - anchorDevX_val;
            float deltaPayloadToAnchorY = payloadY - anchorDevY_val;
            float distSqPayloadToAnchor = (deltaPayloadToAnchorX * deltaPayloadToAnchorX) +
                                          (deltaPayloadToAnchorY * deltaPayloadToAnchorY);
            float thresholdSq = SIMULATION_JUMP_THRESHOLD * SIMULATION_JUMP_THRESHOLD;

            LocalTether::Utils::Logger::GetInstance().Debug(
                "SimMouseProc: Trackpad comparing payload to anchor. distSqPayloadToAnchor: " + std::to_string(distSqPayloadToAnchor) +
                ", thresholdSq: " + std::to_string(thresholdSq)
            );

            if (distSqPayloadToAnchor > thresholdSq) {
                LocalTether::Utils::Logger::GetInstance().Debug("SimMouseProc: Trackpad FAR from anchor. Cursor stays. New anchor is payload.");
                outSimX = lastSimX_val;
                outSimY = lastSimY_val;
                m_anchorDeviceRelativeX.store(payloadX, std::memory_order_relaxed);
                m_anchorDeviceRelativeY.store(payloadY, std::memory_order_relaxed);
                LocalTether::Utils::Logger::GetInstance().Debug("SimMouseProc: Trackpad New Anchor: (" + std::to_string(payloadX) + "," + std::to_string(payloadY) + ")");
            } else {
                LocalTether::Utils::Logger::GetInstance().Debug("SimMouseProc: Trackpad CLOSE to anchor. Applying delta to lastSim.");
                outSimX = lastSimX_val + deltaPayloadToAnchorX;
                outSimY = lastSimY_val + deltaPayloadToAnchorY;
                m_anchorDeviceRelativeX.store(payloadX, std::memory_order_relaxed); // Update anchor to current payload
                m_anchorDeviceRelativeY.store(payloadY, std::memory_order_relaxed);
                LocalTether::Utils::Logger::GetInstance().Debug("SimMouseProc: Trackpad New Anchor (updated after drag): (" + std::to_string(payloadX) + "," + std::to_string(payloadY) + ")");
            }
        }
    } else { 
        LocalTether::Utils::Logger::GetInstance().Debug("SimMouseProc: Applying DIRECT simulation logic for device type: " + std::to_string(static_cast<int>(sourceDeviceType)));
        outSimX = payloadX;
        outSimY = payloadY;
        if (anchorDevX_val >= 0.0f) { // If anchors were previously set by trackpad logic
             LocalTether::Utils::Logger::GetInstance().Debug("SimMouseProc: Non-trackpad input, resetting device anchors.");
             m_anchorDeviceRelativeX.store(-1.0f, std::memory_order_relaxed);
             m_anchorDeviceRelativeY.store(-1.0f, std::memory_order_relaxed);
        }
    }

    outSimX = std::max(0.0f, std::min(1.0f, outSimX));
    outSimY = std::max(0.0f, std::min(1.0f, outSimY));

    m_lastSimulatedRelativeX.store(outSimX, std::memory_order_relaxed);
    m_lastSimulatedRelativeY.store(outSimY, std::memory_order_relaxed);
    LocalTether::Utils::Logger::GetInstance().Debug("SimMouseProc END: Final outSim(" + std::to_string(outSimX) + "," + std::to_string(outSimY) + "), stored as lastSim.");
}




std::atomic<bool> LocalTether::Input::InputManager::input_globally_paused_{false};
}