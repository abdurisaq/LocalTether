#include "network/Message.h"
#include "utils/Logger.h"
#include <cstring>
#include <stdexcept>

namespace LocalTether::Network {

Message::Message(MessageType type, uint32_t clientId, const std::vector<uint8_t>& payload) 
    : payload(payload) {
    header.type = type;
    header.size = static_cast<uint32_t>(payload.size());
    header.clientId = clientId;
}

Message Message::createInput(const std::vector<uint8_t>& inputData, uint32_t clientId) {
    return Message(MessageType::Input, clientId, inputData);
}

Message Message::createInput(const InputPayload& inputPayload, uint32_t clientId) {
    std::vector<uint8_t> data;


    uint8_t flags = 0;
    if (inputPayload.isMouseEvent) {
        flags |= 0x01;
    }
    data.push_back(flags);

    uint8_t numKeyEvents = static_cast<uint8_t>(inputPayload.keyEvents.size());
    if (inputPayload.keyEvents.size() > 255) {
        numKeyEvents = 255;
    }
    data.push_back(numKeyEvents);

    for (size_t i = 0; i < numKeyEvents; ++i) {
        const auto& event = inputPayload.keyEvents[i];
        data.push_back(event.keyCode);
        data.push_back(event.isPressed ? 1 : 0);
    }

    if (inputPayload.isMouseEvent) {
        data.push_back(static_cast<uint8_t>((inputPayload.mouseX >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(inputPayload.mouseX & 0xFF));
        data.push_back(static_cast<uint8_t>((inputPayload.mouseY >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>(inputPayload.mouseY & 0xFF));
        data.push_back(inputPayload.mouseButtons);
    }

    return Message(MessageType::Input, clientId, data);
}

HandshakePayload Message::getHandshakePayload() const {
    if (header.type != MessageType::Handshake) {
        throw std::runtime_error("Message is not a handshake message");
    }
    
    HandshakePayload result;

    // [role:1][nameLength:4][name][passwordLength:4][password]
    
    if (payload.size() < 1) {
        throw std::runtime_error("Handshake payload too small");
    }
    
    result.role = static_cast<ClientRole>(payload[0]);
    
    size_t offset = 1;
    
    if (payload.size() < offset + 4) {
        throw std::runtime_error("Handshake payload too small for name length");
    }
    
    uint32_t nameLength;
    std::memcpy(&nameLength, payload.data() + offset, 4);
    offset += 4;
    

    if (payload.size() < offset + nameLength) {
        throw std::runtime_error("Handshake payload too small for name");
    }
    
    result.clientName = std::string(
        reinterpret_cast<const char*>(payload.data() + offset), 
        nameLength
    );
    offset += nameLength;
    
    if (payload.size() < offset + 4) {
        throw std::runtime_error("Handshake payload too small for password length");
    }
    
    uint32_t passwordLength;
    std::memcpy(&passwordLength, payload.data() + offset, 4);
    offset += 4;
    
    if (payload.size() < offset + passwordLength) {
        throw std::runtime_error("Handshake payload too small for password");
    }
    
    result.password = std::string(
        reinterpret_cast<const char*>(payload.data() + offset), 
        passwordLength
    );
    
    return result;
}

InputPayload Message::getInputPayload() const {
    if (header.type != MessageType::Input) {
        throw std::runtime_error("Message is not an Input message.");
    }

    InputPayload p;
    if (payload.empty()) {
        Utils::Logger::GetInstance().Warning("Received empty Input payload.");
        return p;
    }

    size_t offset = 0;

    if (offset >= payload.size()) throw std::runtime_error("Input payload too short for flags.");
    uint8_t flags = payload[offset++];
    p.isMouseEvent = (flags & 0x01) != 0;

    // NumKeyEvents
    if (offset >= payload.size()) throw std::runtime_error("Input payload too short for numKeyEvents.");
    uint8_t numKeyEvents = payload[offset++];

    p.keyEvents.reserve(numKeyEvents);
    for (uint8_t i = 0; i < numKeyEvents; ++i) {
        if (offset + 1 >= payload.size()) throw std::runtime_error("Input payload too short for KeyEvent data.");
        KeyEvent event;
        event.keyCode = payload[offset++];
        event.isPressed = (payload[offset++] != 0);
        p.keyEvents.push_back(event);
    }

    if (p.isMouseEvent) {
        if (offset + 4 >= payload.size()) throw std::runtime_error("Input payload too short for mouse data.");
        p.mouseX = static_cast<int16_t>((static_cast<uint16_t>(payload[offset]) << 8) | payload[offset+1]);
        offset += 2;
        p.mouseY = static_cast<int16_t>((static_cast<uint16_t>(payload[offset]) << 8) | payload[offset+1]);
        offset += 2;
        p.mouseButtons = payload[offset++];
    }
    return p;
}

Message Message::createChat(const std::string& text, uint32_t clientId) {
    std::vector<uint8_t> data(text.begin(), text.end());
    return Message(MessageType::ChatMessage, clientId, data);
}

Message Message::createFileRequest(const std::string& filename, uint32_t clientId) {
    std::vector<uint8_t> data(filename.begin(), filename.end());
    return Message(MessageType::FileRequest, clientId, data);
}

Message Message::createFileData(const std::string& filename, 
                             const std::vector<uint8_t>& chunk,
                             uint32_t chunkId, 
                             uint32_t totalChunks, 
                             uint32_t clientId) {
    // Format: [filenameLength:4][filename][chunkId:4][totalChunks:4][chunkData]
    std::vector<uint8_t> data;
    
    
    uint32_t filenameLength = static_cast<uint32_t>(filename.size());
    data.resize(4);
    std::memcpy(data.data(), &filenameLength, 4);

    data.insert(data.end(), filename.begin(), filename.end());
    

    size_t chunkIdPos = data.size();
    data.resize(data.size() + 4);
    std::memcpy(data.data() + chunkIdPos, &chunkId, 4);

    size_t totalChunksPos = data.size();
    data.resize(data.size() + 4);
    std::memcpy(data.data() + totalChunksPos, &totalChunks, 4);

    data.insert(data.end(), chunk.begin(), chunk.end());
    
    return Message(MessageType::FileData, clientId, data);
}

Message Message::createCommand(const std::string& command, uint32_t clientId) {
    std::vector<uint8_t> data(command.begin(), command.end());
    return Message(MessageType::Command, clientId, data);
}

Message Message::createHandshake(ClientRole role, 
                               const std::string& clientName,
                               const std::string& password, 
                               uint32_t clientId) {
    // Format: [role:1][nameLength:4][name][passwordLength:4][password]
    std::vector<uint8_t> data;
    

    data.push_back(static_cast<uint8_t>(role));
    

    uint32_t nameLength = static_cast<uint32_t>(clientName.size());
    size_t nameLengthPos = data.size();
    data.resize(data.size() + 4);
    std::memcpy(data.data() + nameLengthPos, &nameLength, 4);
    
    // Add client name
    data.insert(data.end(), clientName.begin(), clientName.end());
    

    uint32_t passwordLength = static_cast<uint32_t>(password.size());
    size_t passwordLengthPos = data.size();
    data.resize(data.size() + 4);
    std::memcpy(data.data() + passwordLengthPos, &passwordLength, 4);
    

    data.insert(data.end(), password.begin(), password.end());
    
    return Message(MessageType::Handshake, clientId, data);
}

std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> result;
    result.resize(sizeof(MessageHeader));
    
  
    std::memcpy(result.data(), &header, sizeof(MessageHeader));
    

    result.insert(result.end(), payload.begin(), payload.end());
    
    return result;
}

Message Message::parse(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(MessageHeader)) {
        throw std::runtime_error("Message data too small for header");
    }
    

    MessageHeader header;
    std::memcpy(&header, data.data(), sizeof(MessageHeader));
    
 
    if (data.size() != sizeof(MessageHeader) + header.size) {
        throw std::runtime_error("Message size mismatch");
    }
    

    std::vector<uint8_t> payload(data.begin() + sizeof(MessageHeader), data.end());
    
    return Message(header.type, header.clientId, payload);
}

Message Message::parse(const char* data, size_t length) {
    if (length < sizeof(MessageHeader)) {
        throw std::runtime_error("Message data too small for header");
    }
    
    
    MessageHeader header;
    std::memcpy(&header, data, sizeof(MessageHeader));
    
    
    if (length != sizeof(MessageHeader) + header.size) {
        throw std::runtime_error("Message size mismatch");
    }
    
    const uint8_t* payloadStart = reinterpret_cast<const uint8_t*>(data) + sizeof(MessageHeader);
    std::vector<uint8_t> payload(payloadStart, payloadStart + header.size);
    
    return Message(header.type, header.clientId, payload);
}

std::string Message::getTextPayload() const {
    if (payload.empty()) {
        return "";
    }
    if (payload.back() == 0) {
        return std::string(reinterpret_cast<const char*>(payload.data()), payload.size() - 1);
    } else {
        return std::string(reinterpret_cast<const char*>(payload.data()), payload.size());
    }
}

} 