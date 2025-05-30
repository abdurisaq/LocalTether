#include "network/Message.h"
#include "utils/Logger.h"  
#include <cstring>  
#include <stdexcept>  
#include <sstream>    

#ifdef _WIN32
#include <winsock2.h>  
#else
#include <arpa/inet.h>  
#endif


#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/chrono.hpp>  

#include "ui/panels/FileExplorerPanel.h" 

namespace LocalTether::Network {

 
Message::Message() : type_(MessageType::Unknown), clientId_(0), bodySize_(0) {}

Message::Message(MessageType type, uint32_t clientId, const std::vector<uint8_t>& body)
    : type_(type), clientId_(clientId), bodySize_(static_cast<uint32_t>(body.size())), body_(body) {}

Message::Message(MessageType type, uint32_t clientId, const std::string& textPayload)
    : type_(type), clientId_(clientId) {
    body_.assign(textPayload.begin(), textPayload.end());
    bodySize_ = static_cast<uint32_t>(body_.size());
}

MessageType Message::getType() const {
    return type_;
}

uint32_t Message::getClientId() const {
    return clientId_;
}

const std::vector<uint8_t>& Message::getBody() const {
    return body_;
}

uint32_t Message::getBodySize() const {
    return bodySize_;  
}

void Message::setType(MessageType type) {
    type_ = type;
}

void Message::setClientId(uint32_t clientId) {
    clientId_ = clientId;
}

void Message::setBody(const std::vector<uint8_t>& body) {
    body_ = body;
    bodySize_ = static_cast<uint32_t>(body_.size());
}

void Message::setBody(const uint8_t* data, size_t length) {
    body_.assign(data, data + length);
    bodySize_ = static_cast<uint32_t>(length);
}

std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> buffer;
    buffer.resize(HEADER_LENGTH + body_.size());  

    size_t offset = 0;
    buffer[offset] = static_cast<uint8_t>(type_);
    offset += 1;

    uint32_t netClientId = htonl(clientId_);  
    std::memcpy(buffer.data() + offset, &netClientId, sizeof(netClientId));
    offset += sizeof(netClientId);

    uint32_t netBodySize = htonl(static_cast<uint32_t>(body_.size()));  
    std::memcpy(buffer.data() + offset, &netBodySize, sizeof(netBodySize));
    offset += sizeof(netBodySize);

    if (!body_.empty()) {
        std::memcpy(buffer.data() + offset, body_.data(), body_.size());
    }

    return buffer;
}

bool Message::decodeHeader(const uint8_t* buffer, size_t bufferSize) {
    if (bufferSize < HEADER_LENGTH) {
         
        return false;
    }

    size_t offset = 0;
    type_ = static_cast<MessageType>(buffer[offset]);
    offset += 1;

    uint32_t netClientId;
    std::memcpy(&netClientId, buffer + offset, sizeof(netClientId));
    clientId_ = ntohl(netClientId);  
    offset += sizeof(netClientId);

    uint32_t netBodySize;
    std::memcpy(&netBodySize, buffer + offset, sizeof(netBodySize));
    bodySize_ = ntohl(netBodySize);  

    if (bodySize_ > MAX_BODY_LENGTH) {
        LocalTether::Utils::Logger::GetInstance().Error(
            "Message decodeHeader: Declared body size (" + std::to_string(bodySize_) +
            ") exceeds MAX_BODY_LENGTH (" + std::to_string(MAX_BODY_LENGTH) + ").");
        bodySize_ = 0;  
        return false;  
    }
    return true;
}

bool Message::decodeBody(const uint8_t* buffer, size_t bufferSize) {
    if (bufferSize < bodySize_) {  
        LocalTether::Utils::Logger::GetInstance().Error(
            "Message decodeBody: Insufficient data for body. Expected " +
            std::to_string(bodySize_) + ", got " + std::to_string(bufferSize));
        return false;
    }
    body_.assign(buffer, buffer + bodySize_);
    return true;
}


std::string Message::getTextPayload() const {
    return std::string(body_.begin(), body_.end());
}

InputPayload Message::getInputPayload() const {
    if (type_ != MessageType::Input) {
        throw std::runtime_error("Message is not of type Input.");
    }
    std::stringstream ss(std::ios::binary | std::ios::in | std::ios::out);
    ss.write(reinterpret_cast<const char*>(body_.data()), body_.size());
    ss.seekg(0);  

    InputPayload payload;
    try {
        cereal::BinaryInputArchive archive(ss);
        archive(payload);
    } catch (const cereal::Exception& e) {
        throw std::runtime_error("Failed to deserialize InputPayload: " + std::string(e.what()));
    }
    return payload;
}

HandshakePayload Message::getHandshakePayload() const {
    if (type_ != MessageType::Handshake) {
        throw std::runtime_error("Message is not of type Handshake.");
    }
    std::stringstream ss(std::ios::binary | std::ios::in | std::ios::out);
    ss.write(reinterpret_cast<const char*>(body_.data()), body_.size());
    ss.seekg(0);

    HandshakePayload payload;
    try {
        cereal::BinaryInputArchive archive(ss);
        archive(payload);
    } catch (const cereal::Exception& e) {
        throw std::runtime_error("Failed to deserialize HandshakePayload: " + std::string(e.what()));
    }
    return payload;
}


 
Message Message::createHandshake(const HandshakePayload& payload, uint32_t clientId) {
    std::stringstream ss(std::ios::binary | std::ios::in | std::ios::out);
    {
        cereal::BinaryOutputArchive archive(ss);
        archive(payload);
    }
    std::string serialized_payload = ss.str();
    std::vector<uint8_t> body(serialized_payload.begin(), serialized_payload.end());
    return Message(MessageType::Handshake, clientId, body);
}

Message Message::createInput(const InputPayload& payload, uint32_t clientId) {
    std::stringstream ss(std::ios::binary | std::ios::in | std::ios::out);
    {
        cereal::BinaryOutputArchive archive(ss);
        archive(payload);
    }
    std::string serialized_payload = ss.str();
    std::vector<uint8_t> body(serialized_payload.begin(), serialized_payload.end());
    return Message(MessageType::Input, clientId, body);
}

Message Message::createChat(const std::string& message, uint32_t clientId) {
    return Message(MessageType::ChatMessage, clientId, message);
}

Message Message::createCommand(const std::string& command, uint32_t clientId) {
    return Message(MessageType::Command, clientId, command);
}

Message Message::createFileRequest(const std::string& filename, uint32_t clientId) {
    return Message(MessageType::FileRequest, clientId, filename);
}


Message Message::createFileSystemUpdate(const LocalTether::UI::Panels::FileMetadata& rootNode, uint32_t senderClientId) {
    std::ostringstream os(std::ios::binary);
    {
        cereal::BinaryOutputArchive archive(os);
        archive(rootNode);
    }
    std::string serialized_str = os.str();
    std::vector<uint8_t> body(serialized_str.begin(), serialized_str.end());
    return Message(MessageType::FileSystemUpdate, senderClientId, body);
}

LocalTether::UI::Panels::FileMetadata Message::getFileSystemMetadataPayload() const {
    if (type_ != MessageType::FileSystemUpdate) {
        throw std::runtime_error("Message is not of type FileSystemUpdate");
    }
    LocalTether::UI::Panels::FileMetadata rootNode;
    std::string body_str(body_.begin(), body_.end());
    std::istringstream is(body_str, std::ios::binary);
    {
        cereal::BinaryInputArchive archive(is);
        archive(rootNode);
    }
    return rootNode;
}

Message Message::createFileUpload(const std::string& serverRelativePath, const std::string& fileNameOnServer, const std::vector<char>& fileContent, uint32_t senderId) {
    std::vector<uint8_t> body;
     
    body.insert(body.end(), serverRelativePath.begin(), serverRelativePath.end());
    body.push_back('\0');  
     
    body.insert(body.end(), fileNameOnServer.begin(), fileNameOnServer.end());
    body.push_back('\0');  
     
    body.insert(body.end(), fileContent.begin(), fileContent.end());
    return Message(MessageType::FileUpload, senderId, body);
}
Message Message::createFileResponse(const std::string& relativePath, const std::vector<char>& fileContent, uint32_t senderId) {
    std::vector<uint8_t> body;
     
    body.insert(body.end(), relativePath.begin(), relativePath.end());
    body.push_back('\0');  
     
    body.insert(body.end(), fileContent.begin(), fileContent.end());
    return Message(MessageType::FileResponse, senderId, body);
}
Message Message::createFileError(const std::string& errorMessage, const std::string& relatedPath, uint32_t senderId) {
    std::vector<uint8_t> body;
     
    body.insert(body.end(), errorMessage.begin(), errorMessage.end());
    body.push_back('\0');  
     
    body.insert(body.end(), relatedPath.begin(), relatedPath.end());
     
     
    return Message(MessageType::FileError, senderId, body);
}

std::string Message::getServerRelativePathFromUpload() const {
    if (type_ != MessageType::FileUpload || body_.empty()) {
        return "";
    }
    auto it_first_null = std::find(body_.begin(), body_.end(), '\0');
    if (it_first_null == body_.end()) {
        return "";  
    }
    return std::string(body_.begin(), it_first_null);
}

std::string Message::getFileNameFromUpload() const {
    if (type_ != MessageType::FileUpload || body_.empty()) {
        return "";
    }
    auto it_first_null = std::find(body_.begin(), body_.end(), '\0');
    if (it_first_null == body_.end() || (it_first_null + 1) == body_.end()) {
        return "";  
    }
    auto it_second_null = std::find(it_first_null + 1, body_.end(), '\0');
     
    return std::string(it_first_null + 1, it_second_null);
}

std::vector<char> Message::getFileContentFromUploadOrResponse() const {
    if (body_.empty()) {
        return {};
    }
    if (type_ == MessageType::FileUpload) {
        auto it_first_null = std::find(body_.begin(), body_.end(), '\0');
        if (it_first_null == body_.end() || (it_first_null + 1) == body_.end()) return {};  
        auto it_second_null = std::find(it_first_null + 1, body_.end(), '\0');
        if (it_second_null == body_.end() || (it_second_null + 1) == body_.end()) return {};  
        return std::vector<char>(it_second_null + 1, body_.end());
    } else if (type_ == MessageType::FileResponse) {
        auto it_first_null = std::find(body_.begin(), body_.end(), '\0');
        if (it_first_null == body_.end() || (it_first_null + 1) == body_.end()) return {};  
        return std::vector<char>(it_first_null + 1, body_.end());
    }
    return {};
}

std::string Message::getRelativePathFromFileResponse() const {
    if (type_ != MessageType::FileResponse || body_.empty()) {
        return "";
    }
    auto it_first_null = std::find(body_.begin(), body_.end(), '\0');
    if (it_first_null == body_.end()) {
        return "";  
    }
    return std::string(body_.begin(), it_first_null);
}

std::string Message::getErrorMessageFromFileError() const {
    if (type_ != MessageType::FileError || body_.empty()) {
        return "";
    }
    auto it_first_null = std::find(body_.begin(), body_.end(), '\0');
    if (it_first_null == body_.end()) {
         
         
        return std::string(body_.begin(), body_.end());  
    }
    return std::string(body_.begin(), it_first_null);
}
std::string Message::getRelatedPathFromFileError() const {
    if (type_ != MessageType::FileError || body_.empty()) {
        return "";
    }
    auto it_first_null = std::find(body_.begin(), body_.end(), '\0');
    if (it_first_null == body_.end() || (it_first_null + 1) == body_.end()) {
        return "";  
    }
     
    return std::string(it_first_null + 1, body_.end());
}



std::string Message::messageTypeToString(MessageType type){
    switch (type) {
        case MessageType::Invalid: return "Invalid";
        case MessageType::Handshake: return "Handshake";
        case MessageType::HandshakeResponse: return "HandshakeResponse";
        case MessageType::Input: return "Input";
        case MessageType::ChatMessage: return "ChatMessage";
        case MessageType::Command: return "Command";
        case MessageType::KeepAlive: return "KeepAlive";
        case MessageType::Disconnect: return "Disconnect";
        case MessageType::FileSystemUpdate: return "FileSystemUpdate";
        case MessageType::FileRequest: return "FileRequest";
        case MessageType::FileUpload: return "FileUpload";
        case MessageType::FileData: return "FileData";
        case MessageType::FileResponse: return "FileResponse";
        case MessageType::FileError: return "FileError";
        default: return "Unknown";
    }
}


}  