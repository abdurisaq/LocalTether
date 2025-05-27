#include "network/Message.h"
#include "utils/Logger.h"
#include "utils/Serialization.h"
#include <cstring>
#include <stdexcept>

namespace LocalTether::Network {

Message::Message(MessageType type, uint32_t clientId, const std::vector<uint8_t>& payload) 
    : payload(payload) {
    header.type = type;
    header.size = static_cast<uint32_t>(payload.size());
    header.clientId = clientId;
}

Message Message::createInput(const InputPayload& inputPayload, uint32_t clientId) {
    std::vector<uint8_t> data = LocalTether::Utils::serializeInputPayload(inputPayload);
    return Message(MessageType::Input, clientId, data);
}

HandshakePayload Message::getHandshakePayload() const {
    if (header.type != MessageType::Handshake) {
        throw std::runtime_error("Message is not a handshake message");
    }
    
    HandshakePayload result;
    size_t offset = 0;

    if (payload.size() < offset + 1) throw std::runtime_error("Handshake payload too small for role");
    result.role = static_cast<ClientRole>(payload[offset]);
    offset += 1;

    if (payload.size() < offset + sizeof(uint32_t)) throw std::runtime_error("Handshake payload too small for name length");
    uint32_t nameLength;
    std::memcpy(&nameLength, payload.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (payload.size() < offset + nameLength) throw std::runtime_error("Handshake payload too small for name");
    result.clientName.assign(reinterpret_cast<const char*>(payload.data() + offset), nameLength);
    offset += nameLength;

    if (payload.size() < offset + sizeof(uint32_t)) throw std::runtime_error("Handshake payload too small for password length");
    uint32_t passwordLength;
    std::memcpy(&passwordLength, payload.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (payload.size() < offset + passwordLength) throw std::runtime_error("Handshake payload too small for password");
    result.password.assign(reinterpret_cast<const char*>(payload.data() + offset), passwordLength);
    offset += passwordLength;

    
    if (payload.size() < offset + sizeof(uint16_t)) { 
        result.hostScreenWidth = 0;
    } else {
        std::memcpy(&result.hostScreenWidth, payload.data() + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);
    }

    if (payload.size() < offset + sizeof(uint16_t)) { // hostScreenHeight
        result.hostScreenHeight = 0;
    } else {
        std::memcpy(&result.hostScreenHeight, payload.data() + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);
    }

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

    auto result = LocalTether::Utils::deserializeInputPayload(payload.data(), payload.size());
    if (!result) {
        throw std::runtime_error("Failed to deserialize input payload");
    }
    
    return *result;
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

Message Message::createHandshake(const HandshakePayload& payload, uint32_t clientId) {
    std::vector<uint8_t> data;
    data.push_back(static_cast<uint8_t>(payload.role));

    uint32_t nameLength = static_cast<uint32_t>(payload.clientName.length());
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&nameLength), reinterpret_cast<const uint8_t*>(&nameLength) + sizeof(nameLength));
    data.insert(data.end(), payload.clientName.begin(), payload.clientName.end());

    uint32_t passwordLength = static_cast<uint32_t>(payload.password.length());
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&passwordLength), reinterpret_cast<const uint8_t*>(&passwordLength) + sizeof(passwordLength));
    data.insert(data.end(), payload.password.begin(), payload.password.end());

    // Serialize screen dimensions
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&payload.hostScreenWidth), reinterpret_cast<const uint8_t*>(&payload.hostScreenWidth) + sizeof(payload.hostScreenWidth));
    data.insert(data.end(), reinterpret_cast<const uint8_t*>(&payload.hostScreenHeight), reinterpret_cast<const uint8_t*>(&payload.hostScreenHeight) + sizeof(payload.hostScreenHeight));

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