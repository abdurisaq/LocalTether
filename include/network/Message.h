#pragma once
#include <vector>
#include <string>
#include <cstdint>

#include <cereal/cereal.hpp>  
#include <cereal/types/vector.hpp>  
#include <cereal/types/string.hpp>

namespace LocalTether::Network {

enum class MessageType : uint8_t {
    Unknown = 0,
    Handshake,
    Input,
    ChatMessage,
    Command,
    FileRequest,  
    FileData,     
    FileAck,      
    Heartbeat,
    Error,        
    ClientListUpdate,  
    HostInfoUpdate     
     
};

enum class ClientRole : uint8_t {
    Broadcaster,    
    Receiver,       
    Host            
};

 
struct MessageHeader {
    MessageType type;
    uint32_t size;  
    uint32_t clientId;
};


struct KeyEvent {
    uint8_t keyCode;    
    bool isPressed;   
    template <class Archive>
    void serialize(Archive & ar) {
        ar(CEREAL_NVP(keyCode), CEREAL_NVP(isPressed));
    }  
};

struct HandshakePayload {
    ClientRole role;
    std::string clientName;
    std::string password;
    uint32_t clientId = 0;
    uint16_t hostScreenWidth = 0; 
    uint16_t hostScreenHeight = 0;

    template <class Archive>
    void serialize(Archive & ar) {
        ar(CEREAL_NVP(role), 
           CEREAL_NVP(clientName), 
           CEREAL_NVP(password), 
           CEREAL_NVP(clientId), 
           CEREAL_NVP(hostScreenWidth), 
           CEREAL_NVP(hostScreenHeight)
           );
    }
};

enum class InputSourceDeviceType : uint8_t {
    UNKNOWN = 0,
    MOUSE_ABSOLUTE = 2,     
    TRACKPAD_ABSOLUTE = 3 
};

struct InputPayload {
    std::vector<KeyEvent> keyEvents;  
    InputSourceDeviceType sourceDeviceType = InputSourceDeviceType::UNKNOWN;
    bool isMouseEvent = false;      
    float relativeX = -1.0f;  
    float relativeY = -1.0f;      
    uint8_t mouseButtons = 0;         
    int16_t scrollDeltaX = 0;
    int16_t scrollDeltaY = 0;

    template <class Archive>
    void serialize(Archive & ar) {
        ar(CEREAL_NVP(keyEvents), 
           CEREAL_NVP(sourceDeviceType),
           CEREAL_NVP(isMouseEvent), 
           CEREAL_NVP(relativeX), 
           CEREAL_NVP(relativeY), 
           CEREAL_NVP(mouseButtons), 
           CEREAL_NVP(scrollDeltaX), 
           CEREAL_NVP(scrollDeltaY));
    }
};

struct ChatPayload {
    std::string text;
};

struct FileRequestPayload {
    std::string filename;
};

struct FileDataPayload {
    std::string filename;
    std::vector<uint8_t> chunkData;
    uint32_t chunkId;
    uint32_t totalChunks;
};

struct CommandPayload {
    std::string command;
    uint32_t clientId;
};
class Message {
public:

     static constexpr size_t MIN_MESSAGE_LENGTH = 5;  
    static constexpr size_t HEADER_LENGTH = 9;       
    static constexpr uint32_t MAX_BODY_LENGTH = 1024 * 1024 * 4;  

     
    Message();  
    Message(MessageType type, uint32_t clientId, const std::vector<uint8_t>& body = {});
    Message(MessageType type, uint32_t clientId, const std::string& textPayload);

     
    MessageType getType() const;
    uint32_t getClientId() const;
    const std::vector<uint8_t>& getBody() const;
    uint32_t getBodySize() const;  

     
    void setType(MessageType type);
    void setClientId(uint32_t clientId);
    void setBody(const std::vector<uint8_t>& body);
    void setBody(const uint8_t* data, size_t length);  

     
    std::vector<uint8_t> serialize() const;
     
    bool decodeHeader(const uint8_t* buffer, size_t bufferSize);  
     
    bool decodeBody(const uint8_t* buffer, size_t bufferSize);  

     
    std::string getTextPayload() const;
    InputPayload getInputPayload() const;  
    HandshakePayload getHandshakePayload() const;  

     
    static Message createHandshake(const HandshakePayload& payload, uint32_t clientId);
    static Message createInput(const InputPayload& payload, uint32_t clientId);
    static Message createChat(const std::string& message, uint32_t clientId);
    static Message createCommand(const std::string& command, uint32_t clientId);
    static Message createFileRequest(const std::string& filename, uint32_t clientId);  
    static Message createHeartbeat(uint32_t clientId);

     
    static std::string messageTypeToString(MessageType type);


private:
    MessageType type_;
    uint32_t clientId_;  
    uint32_t bodySize_;  
    std::vector<uint8_t> body_;
};

} 