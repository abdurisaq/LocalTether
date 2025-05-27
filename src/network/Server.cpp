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
    LocalTether::Utils::Logger::GetInstance().Debug(
        "Received message from: " + session->getClientAddress() + 
        " (ID: " + std::to_string(session->getClientId()) + 
        ", Type: " + std::to_string(static_cast<int>(message.getType())) + ")");
    switch (message.getType()) {
        case MessageType::Handshake: {
        
            processHandshake(session, message);
            break;
        }
            
        case MessageType::Input: {

            try {
            auto payload = message.getInputPayload();
            
            // Log key events
            if (!payload.keyEvents.empty()) {
                std::string keyLog = "Input from " + session->getClientName() + " (" + std::to_string(session->getClientId()) + "): ";
                
                for (const auto& keyEvent : payload.keyEvents) {
                    keyLog += std::string(keyEvent.isPressed ? "PRESS " : "RELEASE ") + 
                              "VK:" + std::to_string(keyEvent.keyCode) + " ";
                    
                    // Optionally convert VK code to human-readable name for common keys
                    keyLog +=  LocalTether::Utils::Logger::getKeyName(keyEvent.keyCode) + " ";
                }
                
                LocalTether::Utils::Logger::GetInstance().Info(keyLog);
            }
            
            // Log mouse events
            if (payload.isMouseEvent) {
                std::string mouseLog = "Mouse from " + session->getClientName() + ": ";
                
                if (payload.relativeX != 0 || payload.relativeY != 0) {
                    mouseLog += "Move(" + std::to_string(payload.relativeX) + "," + 
                               std::to_string(payload.relativeY) + ") ";
                }
                
                if (payload.scrollDeltaX != 0 || payload.scrollDeltaY != 0) {
                    mouseLog += "Scroll(" + std::to_string(payload.scrollDeltaX) + "," + 
                               std::to_string(payload.scrollDeltaY) + ") ";
                }
                
                if (payload.mouseButtons != 0) {
                    mouseLog += "Buttons: ";
                    if (payload.mouseButtons & 0x01) mouseLog += "Left ";
                    if (payload.mouseButtons & 0x02) mouseLog += "Right ";
                    if (payload.mouseButtons & 0x04) mouseLog += "Middle ";
                    if (payload.mouseButtons & 0x08) mouseLog += "X1 ";
                    if (payload.mouseButtons & 0x10) mouseLog += "X2 ";
                }
                
                LocalTether::Utils::Logger::GetInstance().Info(mouseLog);
            }
        } catch (const std::exception& e) {
            LocalTether::Utils::Logger::GetInstance().Error("Failed to parse input payload: " + std::string(e.what()));
        }
            
            if (session->getClientId() == hostClientId_ && hostClientId_ != 0) {
                
                broadcastToReceivers(message); 
            } else if (hostClientId_ == 0) {
                 LocalTether::Utils::Logger::GetInstance().Warning(
                    "Received input message, but no host is designated yet.");
            } else {
                 LocalTether::Utils::Logger::GetInstance().Warning(
                    "Received input message from non-host client: " + std::to_string(session->getClientId()));
            }
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
        auto handshakeData = message.getHandshakePayload(); 
        
        bool isAuthenticated = false;
        if (!password.empty()) { 
            isAuthenticated = (handshakeData.password == password);
        } else {
            isAuthenticated = true; 
        }
        
        if (isAuthenticated) {
            
            if(hostClientId_ != 0 && hostClientId_ != session->getClientId()) {
                session->setRole(ClientRole::Receiver);
            }

            session->setClientName(handshakeData.clientName);
            
            if (handshakeData.role == ClientRole::Host) {
                if (hostClientId_ == 0) {
                    hostClientId_ = session->getClientId();
                    hostScreenWidth_ = handshakeData.hostScreenWidth; 
                    hostScreenHeight_ = handshakeData.hostScreenHeight;
                    LocalTether::Utils::Logger::GetInstance().Info(
                        "Client " + handshakeData.clientName + " (ID: " + std::to_string(session->getClientId()) + ") designated as Host.");
                } else if (hostClientId_ != session->getClientId()) {
                    LocalTether::Utils::Logger::GetInstance().Warning(
                        "Client " + handshakeData.clientName + " tried to connect as Host, but Host (ID: " + std::to_string(hostClientId_) + ") already exists. Rejecting role.");
                    
                }
            }

            HandshakePayload responsePayload;
            responsePayload.role = session->getRole(); 
            responsePayload.clientName = "Server"; 

            responsePayload.hostScreenWidth = (hostClientId_ != 0) ? hostScreenWidth_ : 0;
            responsePayload.hostScreenHeight = (hostClientId_ != 0) ? hostScreenHeight_ : 0;

            auto responseMsg = Message::createHandshake(responsePayload, session->getClientId());
            session->send(responseMsg);
            
            LocalTether::Utils::Logger::GetInstance().Info(
                handshakeData.clientName + " (ID: " + std::to_string(session->getClientId()) + 
                ") authenticated. Role: " + 
                (session->getRole() == ClientRole::Host ? "Host" :
                 session->getRole() == ClientRole::Broadcaster ? "Broadcaster" : "Receiver"));
                 
            notifyClientJoined(session);
        } else {
            LocalTether::Utils::Logger::GetInstance().Warning(
                "Authentication failed for: " + session->getClientAddress() + " with name " + handshakeData.clientName);
            auto response = Message::createCommand("auth_failed", 0); // Server (ID 0) sends command
            session->send(response);
            session->close(); 
        }
    } catch (const std::exception& e) {
        LocalTether::Utils::Logger::GetInstance().Error(
            "Handshake processing error: " + std::string(e.what()));
        session->close(); 
    }
}

void Server::handleDisconnect(std::shared_ptr<Session> session) {
    if (!session) return;
    
    LocalTether::Utils::Logger::GetInstance().Info(
        "Client disconnected: " + session->getClientAddress() + 
        " (ID: " + std::to_string(session->getClientId()) + 
        ", Name: " + session->getClientName() + ")");

    
        session->close();

    if (session->getClientId() == hostClientId_) {
        LocalTether::Utils::Logger::GetInstance().Info(
            "Host (ID: " + std::to_string(hostClientId_) + ") has disconnected.");
        hostClientId_ = 0;
    }

    notifyClientLeft(session); 

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(std::remove(sessions_.begin(), sessions_.end(), session), sessions_.end());
    }
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
    LocalTether::Utils::Logger::GetInstance().Debug(
        "Broadcasting message to all receivers. Message type: " + 
        std::to_string(static_cast<int>(message.getType())));
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