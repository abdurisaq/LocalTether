#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace LocalTether::Network {

enum class MessageType : uint8_t {
    Input,         
    ChatMessage,    
    FileRequest,  
    FileData,       
    Command,        
    Handshake,     
    Heartbeat      
};

enum class ClientRole : uint8_t {
    Broadcaster,    
    Receiver,       
    Host            
};

// Header for all messages
struct MessageHeader {
    MessageType type;
    uint32_t size;  
    uint32_t clientId;
};

class Message {
public:
    
    static Message createInput(const std::vector<uint8_t>& inputData, uint32_t clientId);
    static Message createChat(const std::string& text, uint32_t clientId);
    static Message createFileRequest(const std::string& filename, uint32_t clientId);
    static Message createFileData(const std::string& filename, const std::vector<uint8_t>& chunk, 
                               uint32_t chunkId, uint32_t totalChunks, uint32_t clientId);
    static Message createCommand(const std::string& command, uint32_t clientId);
    static Message createHandshake(ClientRole role, const std::string& clientName, 
                                const std::string& password, uint32_t clientId = 0);
    
    
    std::vector<uint8_t> serialize() const;
    
    
    static Message parse(const std::vector<uint8_t>& data);
    static Message parse(const char* data, size_t length);
    
    
    MessageType getType() const { return header.type; }
    uint32_t getClientId() const { return header.clientId; }
    const std::vector<uint8_t>& getPayload() const { return payload; }
    

    std::string getTextPayload() const;
    
private:
    Message(MessageType type, uint32_t clientId, const std::vector<uint8_t>& payload);
    
    MessageHeader header;
    std::vector<uint8_t> payload;
};

} 