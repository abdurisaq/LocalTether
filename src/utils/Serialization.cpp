#include "utils/Serialization.h"
#include <cstring>

namespace LocalTether::Utils {

std::vector<uint8_t> serializeInputPayload(const Network::InputPayload& payload) {
    std::vector<uint8_t> buffer;

  
    size_t initial_capacity = sizeof(payload.isMouseEvent) +
                              sizeof(payload.relativeX) +
                              sizeof(payload.relativeY) +
                              sizeof(payload.mouseButtons) +
                              sizeof(payload.scrollDeltaX) +
                              sizeof(payload.scrollDeltaY) +
                              sizeof(payload.sourceDeviceType) +  
                              sizeof(uint32_t) +  
                              (payload.keyEvents.size() * (sizeof(uint8_t) + sizeof(bool)));
    buffer.reserve(initial_capacity);

    auto append = [&](const void* d, size_t s) {
        buffer.insert(buffer.end(), static_cast<const uint8_t*>(d), static_cast<const uint8_t*>(d) + s);
    };

    append(&payload.isMouseEvent, sizeof(payload.isMouseEvent));
    append(&payload.relativeX, sizeof(payload.relativeX));
    append(&payload.relativeY, sizeof(payload.relativeY));
    append(&payload.mouseButtons, sizeof(payload.mouseButtons));
    append(&payload.scrollDeltaX, sizeof(payload.scrollDeltaX));
    append(&payload.scrollDeltaY, sizeof(payload.scrollDeltaY));
    append(&payload.sourceDeviceType, sizeof(payload.sourceDeviceType));  

    uint32_t numKeyEvents = static_cast<uint32_t>(payload.keyEvents.size());
    append(&numKeyEvents, sizeof(numKeyEvents));
    for (const auto& keyEvent : payload.keyEvents) {
        append(&keyEvent.keyCode, sizeof(keyEvent.keyCode));
        append(&keyEvent.isPressed, sizeof(keyEvent.isPressed));
    }
    return buffer;
}

std::optional<Network::InputPayload> deserializeInputPayload(const uint8_t* data, size_t length) {
    Network::InputPayload payload;
    size_t offset = 0;

    auto read = [&](void* dest, size_t s) {
        if (offset + s > length) return false;
        std::memcpy(dest, data + offset, s);
        offset += s;
        return true;
    };

    if (!read(&payload.isMouseEvent, sizeof(payload.isMouseEvent))) return std::nullopt;
    if (!read(&payload.relativeX, sizeof(payload.relativeX))) return std::nullopt;
    if (!read(&payload.relativeY, sizeof(payload.relativeY))) return std::nullopt;
    if (!read(&payload.mouseButtons, sizeof(payload.mouseButtons))) return std::nullopt;
    if (!read(&payload.scrollDeltaX, sizeof(payload.scrollDeltaX))) return std::nullopt;
    if (!read(&payload.scrollDeltaY, sizeof(payload.scrollDeltaY))) return std::nullopt;
    if (!read(&payload.sourceDeviceType, sizeof(payload.sourceDeviceType))) return std::nullopt;  

    uint32_t numKeyEvents;
    if (!read(&numKeyEvents, sizeof(numKeyEvents))) return std::nullopt;

    payload.keyEvents.resize(numKeyEvents);
    for (uint32_t i = 0; i < numKeyEvents; ++i) {
        if (!read(&payload.keyEvents[i].keyCode, sizeof(payload.keyEvents[i].keyCode))) return std::nullopt;
        if (!read(&payload.keyEvents[i].isPressed, sizeof(payload.keyEvents[i].isPressed))) return std::nullopt;
    }

     
    if (offset > length && numKeyEvents > 0) return std::nullopt;  
    if (offset > length && numKeyEvents == 0 && (offset - (sizeof(payload.isMouseEvent) + sizeof(payload.relativeX) + sizeof(payload.relativeY) + sizeof(payload.mouseButtons) + sizeof(payload.scrollDeltaX) + sizeof(payload.scrollDeltaY) + sizeof(payload.sourceDeviceType) + sizeof(uint32_t))) > 0 ) return std::nullopt;


    return payload;
}
}
