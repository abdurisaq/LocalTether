 
#pragma once

#include "Message.h"
#include "utils/Logger.h"
#define ASIO_ENABLE_SSL  
#include <asio.hpp>
#include <asio/ssl.hpp>  
#include <memory>
#include <queue>
#include <mutex>
#include <vector>
#include <array>
#include <functional>
#include <atomic>

namespace LocalTether::Network {

class Server;  

class Session : public std::enable_shared_from_this<Session> {
public:
    using MessageHandler = std::function<void(std::shared_ptr<Session>, const Message&)>;
    using DisconnectHandler = std::function<void(std::shared_ptr<Session>)>;

    Session(asio::ip::tcp::socket tcp_socket, Server* server, uint32_t clientId, asio::ssl::context& ssl_context);
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void start(MessageHandler msgHandler, DisconnectHandler discHandler);
    void send(const Message& msg);
    void close();

    uint32_t getClientId() const { return clientId_; }
    std::string getClientAddress() const;
    ClientRole getRole() const { return role_; }
    std::string getRoleString() const;
    void setRole(ClientRole role) { role_ = role; }
    std::string getClientName() const { return clientName_; }
    void setClientName(const std::string& name) { clientName_ = name; }
    bool isAppHandshakeComplete() const { return appHandshakeComplete_.load(); }
    void setAppHandshakeComplete(bool status) { appHandshakeComplete_.store(status); }
private:
    void doSslHandshake();
    void handleSslHandshake(const std::error_code& error);

    void performApplicationHandshake(); 
    void handleAppHandshakeResponseHeader(const std::error_code& error, size_t bytes_transferred);
    void handleAppHandshakeResponseBody(const std::error_code& error, size_t bytes_transferred);

    void doRead();
    void handleReadHeader(const std::error_code& error, size_t bytes_transferred);
    void handleReadBody(const std::error_code& error, size_t bytes_transferred);

    void doWrite();
    void handleWrite(const std::error_code& error, size_t bytes_transferred);

    void doClose(const std::string& reason = "normal closure");

    asio::ssl::stream<asio::ip::tcp::socket> socket_; 
    Server* server_; 
    uint32_t clientId_;
    ClientRole role_{ClientRole::Receiver};
    std::string clientName_{"UnknownClient"};

    std::vector<char> readBuffer_;  
    Message currentReadMessage_;        
     

    std::queue<std::vector<uint8_t>> writeQueue_;
    std::mutex writeMutex_;
    std::atomic<bool> writing_{false};
    std::atomic<bool> active_{false}; 
    std::atomic<bool> sslHandshakeComplete_{false};
    std::atomic<bool> appHandshakeComplete_{false};

    MessageHandler messageHandler_;
    DisconnectHandler disconnectHandler_;

    std::string remoteAddressString_;
};

}  