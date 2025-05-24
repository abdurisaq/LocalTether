#pragma once

#include "Message.h"
#include <asio.hpp>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <algorithm> 

namespace LocalTether::Network {

class Session;

enum class ServerState {
    Stopped,
    Starting,
    Running,
    Error
};

class Server {
public:
    using ConnectionHandler = std::function<void(std::shared_ptr<Session>)>;
    using ErrorHandler = std::function<void(const std::error_code&)>;

    Server(asio::io_context& io_context, uint16_t port = 8080);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    
    void start();
    void stop();

    ServerState getState() const;
    std::string getErrorMessage() const;
    uint16_t getPort() const { return port_; }
    

    void setConnectionHandler(ConnectionHandler handler);
    void setErrorHandler(ErrorHandler handler);

    void broadcast(const Message& message);
    void broadcastToReceivers(const Message& message);
    void broadcastExcept(const Message& message, std::shared_ptr<Session> exceptSession);

    size_t getConnectionCount() const;

    
    std::string password;
    bool localNetworkOnly;
    uint32_t hostClientId = 0; 

private:
    
    void doAccept();
    
    void handleMessage(std::shared_ptr<Session> session, const Message& message);
    void handleDisconnect(std::shared_ptr<Session> session);
    
    // Processing methods
    void processHandshake(std::shared_ptr<Session> session, const Message& message);
    void processCommand(std::shared_ptr<Session> session, const Message& message);
    void processLimitedCommand(std::shared_ptr<Session> session, const Message& message);
    void processFileRequest(std::shared_ptr<Session> session, const Message& message);
    
    // Client notifications
    void notifyClientJoined(std::shared_ptr<Session> session);
    void notifyClientLeft(std::shared_ptr<Session> session);
    
    // Member variables
    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    uint16_t port_;
    
    // Connection management
    std::vector<std::shared_ptr<Session>> sessions_;
    std::mutex sessions_mutex_;
    uint32_t nextClientId_ = 1; 
    

    std::atomic<ServerState> state_{ServerState::Stopped};
    std::string lastError_;
    ConnectionHandler connectionHandler_;
    ErrorHandler errorHandler_;
};

} 