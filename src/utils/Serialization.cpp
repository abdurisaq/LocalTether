#include "utils/Serialization.h"
#include <cstring>

namespace LocalTether::Utils {

std::vector<uint8_t> serializeInputPayload(const Network::InputPayload& payload) {
    std::vector<uint8_t> buffer;

  
    size_t fixedSize = sizeof(payload.isMouseEvent) +
                       sizeof(payload.relativeX) +
                       sizeof(payload.relativeY) +
                       sizeof(payload.mouseButtons) +
                       sizeof(payload.scrollDeltaX) +
                       sizeof(payload.scrollDeltaY);

    size_t keyEventsCountSize = sizeof(uint32_t);
    size_t keyEventsDataSize = payload.keyEvents.size() * sizeof(Network::KeyEvent);


    buffer.resize(fixedSize + keyEventsCountSize + keyEventsDataSize);

    size_t offset = 0;


    memcpy(buffer.data() + offset, &payload.isMouseEvent, sizeof(payload.isMouseEvent));
    offset += sizeof(payload.isMouseEvent);

    memcpy(buffer.data() + offset, &payload.relativeX, sizeof(payload.relativeX));
    offset += sizeof(payload.relativeX);

    memcpy(buffer.data() + offset, &payload.relativeY, sizeof(payload.relativeY));
    offset += sizeof(payload.relativeY);

    memcpy(buffer.data() + offset, &payload.mouseButtons, sizeof(payload.mouseButtons));
    offset += sizeof(payload.mouseButtons);

    memcpy(buffer.data() + offset, &payload.scrollDeltaX, sizeof(payload.scrollDeltaX));
    offset += sizeof(payload.scrollDeltaX);

    memcpy(buffer.data() + offset, &payload.scrollDeltaY, sizeof(payload.scrollDeltaY));
    offset += sizeof(payload.scrollDeltaY);


    uint32_t keyEventsCount = static_cast<uint32_t>(payload.keyEvents.size());
    memcpy(buffer.data() + offset, &keyEventsCount, sizeof(keyEventsCount));
    offset += sizeof(keyEventsCount);

    if (keyEventsCount > 0) {
        memcpy(buffer.data() + offset, payload.keyEvents.data(), keyEventsDataSize);
    }

    return buffer;
}

std::optional<Network::InputPayload> deserializeInputPayload(const uint8_t* data, size_t length) {
    Network::InputPayload payload;


    size_t minSize = sizeof(payload.isMouseEvent) +
                     sizeof(payload.relativeX) +
                     sizeof(payload.relativeY) +
                     sizeof(payload.mouseButtons) +
                     sizeof(payload.scrollDeltaX) +
                     sizeof(payload.scrollDeltaY) +
                     sizeof(uint32_t); 

    if (length < minSize) {
        return std::nullopt;
    }

    size_t offset = 0;


    memcpy(&payload.isMouseEvent, data + offset, sizeof(payload.isMouseEvent));
    offset += sizeof(payload.isMouseEvent);

    memcpy(&payload.relativeX, data + offset, sizeof(payload.relativeX));
    offset += sizeof(payload.relativeX);

    memcpy(&payload.relativeY, data + offset, sizeof(payload.relativeY));
    offset += sizeof(payload.relativeY);

    memcpy(&payload.mouseButtons, data + offset, sizeof(payload.mouseButtons));
    offset += sizeof(payload.mouseButtons);

    memcpy(&payload.scrollDeltaX, data + offset, sizeof(payload.scrollDeltaX));
    offset += sizeof(payload.scrollDeltaX);

    memcpy(&payload.scrollDeltaY, data + offset, sizeof(payload.scrollDeltaY));
    offset += sizeof(payload.scrollDeltaY);

    uint32_t keyEventsCount = 0;
    memcpy(&keyEventsCount, data + offset, sizeof(keyEventsCount));
    offset += sizeof(keyEventsCount);


    size_t requiredSize = offset + keyEventsCount * sizeof(Network::KeyEvent);
    if (length < requiredSize) {
        return std::nullopt;
    }

    if (keyEventsCount > 0) {
        payload.keyEvents.resize(keyEventsCount);
        memcpy(payload.keyEvents.data(), data + offset, keyEventsCount * sizeof(Network::KeyEvent));
    }

    return payload;
}

} 