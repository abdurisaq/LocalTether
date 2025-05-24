#pragma once

#include "Message.h"
#include <asio.hpp>
#include <memory>
#include <functional>
#include <queue>
#include <mutex>
#include <atomic>

namespace LocalTether::Network {

class Session : public std::enable_shared_from_this<Session> {
public:
    using MessageHandler = std::function<void(std::shared_ptr<Session>, const Message&)>;
    using DisconnectHandler = std::function<void(std::shared_ptr<Session>)>;
    
    explicit Session(asio::ip::tcp::socket socket);
    ~Session();
    

    void start(MessageHandler msgHandler, DisconnectHandler disconnectHandler);
    

    void send(const Message& message);

    void close();
    

    uint32_t getClientId() const { return clientId_; }
    void setClientId(uint32_t id) { clientId_ = id; }
    
    ClientRole getRole() const { return role_; }
    void setRole(ClientRole role) { role_ = role; }
    
    std::string getClientName() const { return clientName_; }
    void setClientName(const std::string& name) { clientName_ = name; }
    
    std::string getClientAddress() const;
    bool isActive() const { return active_; }
    
private:
 
    void doRead();
    void handleRead(const std::error_code& error, size_t bytes_transferred);
    

    void doWrite();
    void handleWrite(const std::error_code& error, size_t bytes_transferred);
    

    asio::ip::tcp::socket socket_;
    std::array<char, 8192> readBuffer_;
    std::queue<std::vector<uint8_t>> writeQueue_;
    std::mutex writeMutex_;
    std::atomic<bool> writing_{false};
    std::atomic<bool> active_{false};
    

    uint32_t clientId_ = 0;
    ClientRole role_{ClientRole::Receiver};
    std::string clientName_;
    
   
    std::vector<uint8_t> partialMessage_;
    
   
    MessageHandler messageHandler_;
    DisconnectHandler disconnectHandler_;
};

} 