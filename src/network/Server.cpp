#include "network/Server.h"
#include "network/Session.h"
#include "utils/Logger.h"
#include <iostream>

namespace LocalTether::Network {

Server::Server(asio::io_context& io_context, uint16_t port)
    : io_context_(io_context), 
      acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
      port_(port),
      state_(ServerState::Stopped),
      localNetworkOnly(true) {
}

Server::~Server() {
    stop();
}

void Server::start() {
    if (state_ != ServerState::Stopped) {
        return;
    }
    
    try {
        state_ = ServerState::Starting;
        
        doAccept();
        
        state_ = ServerState::Running;
        LocalTether::Utils::Logger::GetInstance().Info("Server started on port " + std::to_string(port_));
    }
    catch (const std::exception& e) {
        state_ = ServerState::Error;
        lastError_ = e.what();
        LocalTether::Utils::Logger::GetInstance().Error("Server start error: " + lastError_);
        
        if (errorHandler_) {
            errorHandler_(asio::error::operation_aborted);
        }
    }
}

void Server::stop() {
    if (state_ == ServerState::Stopped) {
        return;
    }
    
    asio::error_code ec;
    acceptor_.close(ec);
    
   
    std::vector<std::shared_ptr<Session>> sessionsToClose;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessionsToClose = sessions_;
        sessions_.clear();
    }
    
    for (auto& session : sessionsToClose) {
        session->close();
    }
    
    state_ = ServerState::Stopped;
    LocalTether::Utils::Logger::GetInstance().Info("Server stopped");
}

void Server::doAccept() {
    
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    
    acceptor_.async_accept(
        *socket,
        [this, socket](const std::error_code& error) {
            if (!error) {
                
                auto session = std::make_shared<Session>(std::move(*socket));

                session->setClientId(nextClientId_++);
             
                session->start(
                    // Message handler
                    [this](std::shared_ptr<Session> s, const Message& msg) {
                        handleMessage(s, msg);
                    },
                    // Disconnect handler
                    [this](std::shared_ptr<Session> s) {
                        handleDisconnect(s);
                    }
                );
                
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    sessions_.push_back(session);
                }
                
                LocalTether::Utils::Logger::GetInstance().Info(
                    "Client connected: " + session->getClientAddress());
                
                if (connectionHandler_) {
                    connectionHandler_(session);
                }
            }
            else if (error != asio::error::operation_aborted) {
                LocalTether::Utils::Logger::GetInstance().Error(
                    "Accept error: " + error.message());
                    
                if (errorHandler_) {
                    errorHandler_(error);
                }
            }
            
            if (state_ == ServerState::Running) {
                doAccept();
            }
        });
}

void Server::handleMessage(std::shared_ptr<Session> session, const Message& message) {
    switch (message.getType()) {
        case MessageType::Handshake: {
        
            processHandshake(session, message);
            break;
        }
            
        case MessageType::Input: {
           
            break;
        }
            
        case MessageType::ChatMessage: {
           
            broadcast(message);
            break;
        }
            
        case MessageType::FileRequest: {
     
            processFileRequest(session, message);
            break;
        }
            
        case MessageType::Command: {
           
            if (session->getRole() == ClientRole::Host) {
                processCommand(session, message);
            } else {
                
                processLimitedCommand(session, message);
            }
            break;
        }
            
        case MessageType::Heartbeat: {
            
            session->send(message); 
            break;
        }
            
        default:
            LocalTether::Utils::Logger::GetInstance().Warning(
                "Received unknown message type from: " + session->getClientAddress());
    }
}

void Server::processHandshake(std::shared_ptr<Session> session, const Message& message) {
    try {
        
        auto textData = message.getTextPayload();
        
        bool isAuthenticated = false;
        ClientRole requestedRole = ClientRole::Receiver;
        std::string clientName = "Unknown";
        
        if (!password.empty()) {
            // needa parse message and check
            isAuthenticated = true; 
        } else {
            isAuthenticated = true;
        }
        
        if (isAuthenticated) {
            
            session->setRole(requestedRole);
            session->setClientName(clientName);
            
            
            auto response = Message::createHandshake(
                requestedRole,
                "Server",
                "",  
                0  
            );
            session->send(response);
            
          
            LocalTether::Utils::Logger::GetInstance().Info(
                clientName + " connected as " + 
                (requestedRole == ClientRole::Host ? "host" : 
                 requestedRole == ClientRole::Broadcaster ? "broadcaster" : "receiver"));
 
            notifyClientJoined(session);
        }
        else {
            
            LocalTether::Utils::Logger::GetInstance().Warning(
                "Authentication failed for: " + session->getClientAddress());

            auto response = Message::createCommand("auth_failed", 0);
            session->send(response);
            session->close();
        }
    }
    catch (const std::exception& e) {
        LocalTether::Utils::Logger::GetInstance().Error(
            "Handshake error: " + std::string(e.what()));
        session->close();
    }
}

void Server::handleDisconnect(std::shared_ptr<Session> session) {
    
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = std::find(sessions_.begin(), sessions_.end(), session);
        if (it != sessions_.end()) {
            sessions_.erase(it);
        }
    }
    
    LocalTether::Utils::Logger::GetInstance().Info(
        "Client disconnected: " + session->getClientAddress() + 
        " (" + session->getClientName() + ")");

    notifyClientLeft(session);
}

void Server::broadcast(const Message& message) {
    std::vector<std::shared_ptr<Session>> currentSessions;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        currentSessions = sessions_; 
    }
    
    for (const auto& session : currentSessions) {
        session->send(message);
    }
}

void Server::broadcastToReceivers(const Message& message) {
    std::vector<std::shared_ptr<Session>> currentSessions;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        currentSessions = sessions_; 
    }
    
    for (const auto& session : currentSessions) {
        if (session->getRole() == ClientRole::Receiver ) {
            session->send(message);
        }
    }
}

void Server::broadcastExcept(const Message& message, std::shared_ptr<Session> exceptSession) {
    std::vector<std::shared_ptr<Session>> currentSessions;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        currentSessions = sessions_; 
    }
    
    for (const auto& session : currentSessions) {
        if (session != exceptSession) {
            session->send(message);
        }
    }
}

void Server::processCommand(std::shared_ptr<Session> session, const Message& message) {

    std::string commandText = message.getTextPayload();
    
    if (commandText == "shutdown") {
      
        auto shutdownMsg = Message::createCommand("server_shutdown", 0);
        broadcast(shutdownMsg);
        
        asio::post(io_context_, [this]() {
            stop();
        });
    }
    else if (commandText.find("set_broadcaster ") == 0) {
        //set broadcaster to someone else not done
    }
    else {
        // Other host commands
    }
}

void Server::processLimitedCommand(std::shared_ptr<Session> session, const Message& message) {

    std::string commandText = message.getTextPayload();
    
    if (commandText == "toggle_input_stream") {
        //non host client stuff
    }
    else {
        // Other commands
    }
}

void Server::processFileRequest(std::shared_ptr<Session> session, const Message& message) {
   
}

void Server::notifyClientJoined(std::shared_ptr<Session> session) {
    
    auto joinMsg = Message::createCommand(
        "client_joined:" + std::to_string(session->getClientId()) + ":" + 
        session->getClientName(),
        0); 
    
    broadcastExcept(joinMsg, session);
}

void Server::notifyClientLeft(std::shared_ptr<Session> session) {
    
    auto leftMsg = Message::createCommand(
        "client_left:" + std::to_string(session->getClientId()) + ":" + 
        session->getClientName(),
        0); 
    
    broadcast(leftMsg);
}

void Server::setConnectionHandler(ConnectionHandler handler) {
    connectionHandler_ = std::move(handler);
}

void Server::setErrorHandler(ErrorHandler handler) {
    errorHandler_ = std::move(handler);
}

ServerState Server::getState() const {
    return state_;
}

std::string Server::getErrorMessage() const {
    return lastError_;
}

size_t Server::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(sessions_mutex_));
    return sessions_.size();
}

} 