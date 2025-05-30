 
#include "network/Server.h"
#include <thread>
#include "network/Session.h"
#include "utils/Logger.h"
#include <chrono>
#include "utils/SslCertificateGenerator.h"
#include <iostream>

namespace LocalTether::Network {

Server::Server(asio::io_context& io_context, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context),  
      port_(port),
      state_(ServerState::Stopped),
      localNetworkOnly(true),
      ssl_context_(asio::ssl::context::tls_server) {

    LocalTether::Utils::Logger::GetInstance().Info("Server created on port: " + std::to_string(port_));

     
    asio::error_code ec_acceptor;
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port_);
    acceptor_.open(endpoint.protocol(), ec_acceptor);
    if (ec_acceptor) {
        lastError_ = "Failed to open acceptor: " + ec_acceptor.message();
        LocalTether::Utils::Logger::GetInstance().Error(lastError_);
        setState(ServerState::Error, ec_acceptor);
         
        return;  
    }
    acceptor_.set_option(asio::socket_base::reuse_address(true), ec_acceptor);  
    acceptor_.bind(endpoint, ec_acceptor);
    if (ec_acceptor) {
        lastError_ = "Failed to bind acceptor: " + ec_acceptor.message();
        LocalTether::Utils::Logger::GetInstance().Error(lastError_);
        setState(ServerState::Error, ec_acceptor);
        acceptor_.close();  
        return;  
    }
    acceptor_.listen(asio::socket_base::max_listen_connections, ec_acceptor);
    if (ec_acceptor) {
        lastError_ = "Failed to listen on acceptor: " + ec_acceptor.message();
        LocalTether::Utils::Logger::GetInstance().Error(lastError_);
        setState(ServerState::Error, ec_acceptor);
        acceptor_.close();  
        return;  
    }


    std::string key_file = "server.key";
    std::string cert_file = "server.crt";
    std::string dh_file = "dh.pem";

    if (!LocalTether::Utils::SslCertificateGenerator::EnsureSslFiles(key_file, cert_file, dh_file)) {
        lastError_ = "Failed to ensure SSL files. Server might not start correctly with SSL.";
        LocalTether::Utils::Logger::GetInstance().Error(lastError_);
        setState(ServerState::Error, std::make_error_code(std::errc::io_error));  
         
         
        return;  
    }

    try {
        ssl_context_.set_options(
            asio::ssl::context::default_workarounds |
            asio::ssl::context::no_sslv2 |
            asio::ssl::context::no_sslv3 |
            asio::ssl::context::single_dh_use);

        ssl_context_.use_certificate_chain_file(cert_file);
        ssl_context_.use_private_key_file(key_file, asio::ssl::context::pem);
        ssl_context_.use_tmp_dh_file(dh_file);
        LocalTether::Utils::Logger::GetInstance().Info("Server SSL context configured with generated/existing files.");
    } catch (const asio::system_error& e) {
        lastError_ = "SSL context setup failed (asio::system_error): " + std::string(e.what()) +
                     ", Code: " + std::to_string(e.code().value()) + " (" + e.code().message() + ")";
        LocalTether::Utils::Logger::GetInstance().Error(lastError_);
        setState(ServerState::Error, e.code());
         
    } catch (const std::exception& e) {
        lastError_ = "SSL context setup failed (std::exception): " + std::string(e.what());
        LocalTether::Utils::Logger::GetInstance().Error(lastError_);
        setState(ServerState::Error, std::make_error_code(std::errc::protocol_error));
         
    }
}

Server::~Server() {
    if (acceptor_.is_open()) {
        stop();
    }
}

void Server::start() {
    if (state_ == ServerState::Running || state_ == ServerState::Starting) {
        LocalTether::Utils::Logger::GetInstance().Warning("Server::start called but already running or starting.");
        return;
    }

     
    if (state_ == ServerState::Error) {
        LocalTether::Utils::Logger::GetInstance().Error("Server cannot start due to previous error: " + lastError_);
        if (errorHandler_) {
             
             
             
            errorHandler_(std::make_error_code(std::errc::protocol_error));
        }
         
         
        return;
    }
    
     
    if (!acceptor_.is_open()) {
        lastError_ = "Acceptor not open. Server cannot start.";
        LocalTether::Utils::Logger::GetInstance().Error(lastError_);
        setState(ServerState::Error, std::make_error_code(std::errc::bad_file_descriptor));  
        if (errorHandler_) {
            errorHandler_(std::make_error_code(std::errc::bad_file_descriptor));
        }
        return;
    }

    setState(ServerState::Starting);
    LocalTether::Utils::Logger::GetInstance().Info("Server starting... Attempting to accept connections.");
    doAccept();
}

void Server::stop() {
    if (state_ == ServerState::Stopped) {
        LocalTether::Utils::Logger::GetInstance().Debug("Server::stop called but already stopped.");
        return;
    }
    LocalTether::Utils::Logger::GetInstance().Info("Server stopping...");
    setState(ServerState::Stopped);  
    
    asio::error_code ec;
    acceptor_.close(ec);
    if (ec) {
         LocalTether::Utils::Logger::GetInstance().Warning("Error closing acceptor: " + ec.message());
    }

    std::vector<std::shared_ptr<Session>> sessions_to_close;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_to_close = sessions_;  
        sessions_.clear();
    }
    
    for (auto& session : sessions_to_close) {
        session->close();
    }
    LocalTether::Utils::Logger::GetInstance().Info("Server stopped and all sessions closed.");
}

void Server::doAccept() {
    if (state_ != ServerState::Running && state_ != ServerState::Starting) {
        LocalTether::Utils::Logger::GetInstance().Info("Server not in running/starting state, stopping accept loop.");
        return;
    }
     
    if (state_ == ServerState::Starting) {
        setState(ServerState::Running); 
        LocalTether::Utils::Logger::GetInstance().Info("Server is now running and accepting connections.");
    }

    acceptor_.async_accept(
        [this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                uint32_t newClientId = 0;  
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    newClientId = nextClientId_++;  
                }

                LocalTether::Utils::Logger::GetInstance().Info("Accepted new connection. Assigning Client ID: " + std::to_string(newClientId));
                
                 
                std::shared_ptr<Session> new_session = 
                    std::make_shared<Session>(std::move(socket), this, newClientId, ssl_context_);
                
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    sessions_.push_back(new_session);
                }
                
                new_session->start(
                    [this](std::shared_ptr<Session> s, const Message& m) { this->handleMessage(s, m); },
                    [this](std::shared_ptr<Session> s) { this->handleDisconnect(s); }
                );

                if (connectionHandler_) {
                    connectionHandler_(new_session);
                }

            } else {
                if (ec == asio::error::operation_aborted) {
                    LocalTether::Utils::Logger::GetInstance().Debug("Accept operation aborted (server likely stopping).");
                } else {
                    LocalTether::Utils::Logger::GetInstance().Error("Accept error: " + ec.message());
                    if (errorHandler_) {
                        errorHandler_(ec);
                    }
                     
                     
                     
                     
                }
            }
             
            if (state_ == ServerState::Running) {  
                 doAccept();
            } else if (state_ == ServerState::Starting) {
                 
                 
                LocalTether::Utils::Logger::GetInstance().Debug("Server still starting, continuing accept loop.");
                doAccept();
            }
        });
}

void Server::handleMessage(std::shared_ptr<Session> session, const Message& message) {
    if (!session) return;
    LocalTether::Utils::Logger::GetInstance().Debug(
        "Server handling message from: " + session->getClientAddress() + 
        " (ID: " + std::to_string(session->getClientId()) + 
        ", Type: " + Message::messageTypeToString(message.getType()) + ")");  
    switch (message.getType()) {
        case MessageType::Handshake: {
            processHandshake(session, message);
            break;
        }
        case MessageType::Input: {
            try {
                auto payload = message.getInputPayload();  
                if (!payload.keyEvents.empty()) {
                    std::string keyLog = "Input from " + session->getClientName() + " (" + std::to_string(session->getClientId()) + "): ";
                    for (const auto& keyEvent : payload.keyEvents) {
                        keyLog += std::string(keyEvent.isPressed ? "PRESS " : "RELEASE ") +
                                  "VK:" + std::to_string(keyEvent.keyCode) + " (" +
                                  LocalTether::Utils::Logger::getKeyName(keyEvent.keyCode) + ") ";
                    }
                    LocalTether::Utils::Logger::GetInstance().Info(keyLog);
                }
                if (payload.isMouseEvent) {
                    std::string mouseLog = "Mouse from " + session->getClientName() + " (" + std::to_string(session->getClientId()) + "): ";
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
                    if (mouseLog != "Mouse from " + session->getClientName() + " (" + std::to_string(session->getClientId()) + "): ") {
                         LocalTether::Utils::Logger::GetInstance().Info(mouseLog);
                    }
                }
                 
                if (session->getClientId() == hostClientId_ && hostClientId_ != 0) {
                    broadcastToReceivers(message);
                } else if (hostClientId_ == 0) {
                    LocalTether::Utils::Logger::GetInstance().Warning(
                        "Received input message, but no host is designated yet.");
                } else {
                     
                     
                     
                }
            } catch (const std::exception& e) {
                LocalTether::Utils::Logger::GetInstance().Error("Failed to parse input payload: " + std::string(e.what()));
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
                "Received unknown/unhandled message type " + Message::messageTypeToString(message.getType()) + 
                " from: " + session->getClientAddress());
    }
}

void Server::processLimitedCommand(std::shared_ptr<Session> session, const Message& message) {
    if (!session) return;
    std::string commandText = message.getTextPayload();
    LocalTether::Utils::Logger::GetInstance().Info("Client " + session->getClientName() + " sent limited command: " + commandText);
     
    if (commandText == "request_host_info") {
        HandshakePayload hostInfoPayload;
        hostInfoPayload.role = ClientRole::Host;  
        hostInfoPayload.clientName = (hostClientId_ != 0 && !sessions_.empty()) ? 
                                     (std::find_if(sessions_.begin(), sessions_.end(), 
                                         [this](const auto& s){ return s->getClientId() == hostClientId_; })
                                         != sessions_.end() ? 
                                         (*std::find_if(sessions_.begin(), sessions_.end(), 
                                         [this](const auto& s){ return s->getClientId() == hostClientId_; }))->getClientName() 
                                         : "Host")
                                     : "No Host";
        hostInfoPayload.clientId = hostClientId_;
        hostInfoPayload.hostScreenWidth = hostScreenWidth_;
        hostInfoPayload.hostScreenHeight = hostScreenHeight_;
        auto response = Message::createHandshake(hostInfoPayload, 0);
        LocalTether::Utils::Logger::GetInstance().Info(
            "Responding to limited command 'request_host_info' from client " + session->getClientName() + 
            " (ID: " + std::to_string(session->getClientId()) + 
            ") with host info: " + hostInfoPayload.clientName);
        session->send(response);
    } else {
        LocalTether::Utils::Logger::GetInstance().Warning("Unknown limited command from client: " + commandText);
        auto reply = Message::createCommand("unknown_limited_command: " + commandText, 0);
        LocalTether::Utils::Logger::GetInstance().Warning(
            "Client " + session->getClientName() + " (ID: " + std::to_string(session->getClientId()) + 
            ") sent unknown limited command: " + commandText);
        session->send(reply);
    }
}

void Server::processHandshake(std::shared_ptr<Session> session, const Message& message) {
    if (!session) return;
    try {
        auto handshakeData = message.getHandshakePayload();
        bool isAuthenticated = password.empty() || (handshakeData.password == password);

        if (isAuthenticated) {
            session->setClientName(handshakeData.clientName);
            session->setRole(handshakeData.role);  

             
            if (handshakeData.role == ClientRole::Host) {
                if (hostClientId_ == 0) {  
                    hostClientId_ = session->getClientId();
                    hostScreenWidth_ = handshakeData.hostScreenWidth;
                    hostScreenHeight_ = handshakeData.hostScreenHeight;
                    session->setRole(ClientRole::Host);  
                    LocalTether::Utils::Logger::GetInstance().Info(
                        "Client " + handshakeData.clientName + " (ID: " + std::to_string(session->getClientId()) + ") designated as Host.");
                } else if (hostClientId_ == session->getClientId()) {  
                    session->setRole(ClientRole::Host);  
                    hostScreenWidth_ = handshakeData.hostScreenWidth;  
                    hostScreenHeight_ = handshakeData.hostScreenHeight;
                    LocalTether::Utils::Logger::GetInstance().Info(
                        "Host " + handshakeData.clientName + " (ID: " + std::to_string(session->getClientId()) + ") re-confirmed.");
                } else {  
                    LocalTether::Utils::Logger::GetInstance().Warning(
                        "Client " + handshakeData.clientName + " (ID: " + std::to_string(session->getClientId()) + 
                        ") tried to connect as Host, but Host (ID: " + std::to_string(hostClientId_) + 
                        ") already exists. Assigning Receiver role.");
                    session->setRole(ClientRole::Receiver);  
                }
            } else {  
                 
                if (hostClientId_ == 0 && handshakeData.role == ClientRole::Broadcaster) {
                     LocalTether::Utils::Logger::GetInstance().Warning(
                        "Client " + handshakeData.clientName + " (ID: " + std::to_string(session->getClientId()) + 
                        ") wants to be Broadcaster, but no Host is active. Assigning Receiver role for now.");
                     session->setRole(ClientRole::Receiver);
                } else {
                    session->setRole(handshakeData.role);  
                }
            }

            HandshakePayload responsePayload;
            responsePayload.role = session->getRole();  
            responsePayload.clientName = "Server";  
            responsePayload.clientId = session->getClientId();  
            responsePayload.hostScreenWidth = (hostClientId_ != 0) ? hostScreenWidth_ : 0;
            responsePayload.hostScreenHeight = (hostClientId_ != 0) ? hostScreenHeight_ : 0;

            auto responseMsg = Message::createHandshake(responsePayload, 0);  
            LocalTether::Utils::Logger::GetInstance().Info(
                "Sending handshake response to " + session->getClientName() + 
                " (ID: " + std::to_string(session->getClientId()) + 
                "), Role: " + session->getRoleString() +
                ", Host ID: " + std::to_string(hostClientId_));
            session->send(responseMsg);

            session->setAppHandshakeComplete(true);
            LocalTether::Utils::Logger::GetInstance().Info(
                "Client " + session->getClientName() + " (ID: " + std::to_string(session->getClientId()) +
                ") application handshake complete. Role: " + session->getRoleString());

            notifyClientJoined(session);
        } else {
            LocalTether::Utils::Logger::GetInstance().Warning(
                "Authentication failed for: " + session->getClientAddress() + " with name " + handshakeData.clientName);
            auto response = Message::createCommand("auth_failed", 0);
            session->send(response);
            session->close();
        }
    } catch (const std::exception& e) {
        LocalTether::Utils::Logger::GetInstance().Error(
            "Handshake processing error for " + session->getClientAddress() + ": " + std::string(e.what()));
        session->close();
    }
}

void Server::handleDisconnect(std::shared_ptr<Session> session) {
    if (!session) return;

    std::string clientAddr = session->getClientAddress();  
    uint32_t clientId = session->getClientId();
    std::string clientName = session->getClientName();

    LocalTether::Utils::Logger::GetInstance().Info(
        "Client disconnected: " + clientAddr +
        " (ID: " + std::to_string(clientId) +
        ", Name: " + clientName + ")");

     
     
     

    if (clientId == hostClientId_ && hostClientId_ != 0) {
        LocalTether::Utils::Logger::GetInstance().Info(
            "Host (ID: " + std::to_string(hostClientId_) + ", Name: " + clientName + ") has disconnected.");
        hostClientId_ = 0;
        hostScreenWidth_ = 0;
        hostScreenHeight_ = 0;
         
        auto hostLeftMsg = Message::createCommand("host_left", 0);
        broadcast(hostLeftMsg);
    }

    notifyClientLeft(session);

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                                    [&](const std::shared_ptr<Session>& s) { return s == session || s->getClientId() == clientId; }),
                        sessions_.end());
    }
}

void Server::broadcast(const Message& message) {
    std::vector<std::shared_ptr<Session>> currentSessions;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        currentSessions.reserve(sessions_.size());
        for(const auto& s : sessions_) {
            if(s && s->isAppHandshakeComplete()) {  
                currentSessions.push_back(s);
            }
        }
    }
    
    for (const auto& session_ptr : currentSessions) {
        session_ptr->send(message);
    }
}

void Server::broadcastToReceivers(const Message& message) {
    std::vector<std::shared_ptr<Session>> currentSessions;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for(const auto& s : sessions_) {
             
            if(s && s->isAppHandshakeComplete() &&  s->getCanReceiveInput() &&
               (s->getRole() == LocalTether::Network::ClientRole::Receiver || s->getRole() == LocalTether::Network::ClientRole::Broadcaster) ) { 
                currentSessions.push_back(s);
            }
        }
    }
     
    LocalTether::Utils::Logger::GetInstance().Debug(
        "Broadcasting message type " + Message::messageTypeToString(message.getType()) +
        " to " + std::to_string(currentSessions.size()) + " connected Receiver/Broadcaster clients.");  
    
    for (const auto& session_ptr : currentSessions) {  
         
         
        if (message.getType() == MessageType::Input && session_ptr->getClientId() == hostClientId_) {
            continue;
        }

        LocalTether::Utils::Logger::GetInstance().Debug(
            "Broadcasting message type " + Message::messageTypeToString(message.getType()) +
            " to " + session_ptr->getRoleString() + ": " + session_ptr->getClientName() +
            " (ID: " + std::to_string(session_ptr->getClientId()) + ")");
        session_ptr->send(message);  
    }
}

void Server::broadcastExcept(const Message& message, std::shared_ptr<Session> exceptSession) {
    std::vector<std::shared_ptr<Session>> currentSessions;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        currentSessions.reserve(sessions_.size());
         for(const auto& s : sessions_) {
             if(s && s->isAppHandshakeComplete() && s != exceptSession) {
                 currentSessions.push_back(s);
             }
        }
    }
    
    for (const auto& session_ptr : currentSessions) {
        session_ptr->send(message);
    }
}

void Server::processCommand(std::shared_ptr<Session> session, const Message& message) {
    if (!session) return;
    std::string commandText = message.getTextPayload();
     
    if (session->getClientId() != hostClientId_) {
        LocalTether::Utils::Logger::GetInstance().Warning(
            "Client " + session->getClientName() + " (ID: " + std::to_string(session->getClientId()) +
            ") attempted to send host-only command: " + commandText);
         
        return;
    }

    LocalTether::Utils::Logger::GetInstance().Info("Host " + session->getClientName() + " sent command: " + commandText);

    if (commandText == "shutdown_server") {  
        LocalTether::Utils::Logger::GetInstance().Info("Shutdown command received from host. Shutting down server.");
        auto shutdownMsg = Message::createCommand("server_shutdown_imminent", 0);
        broadcast(shutdownMsg);  
        
        asio::post(io_context_, [this]() {  
            std::this_thread::sleep_for(std::chrono::milliseconds(500));  
            stop();
        });
    } else if (commandText.rfind("kick_client:", 0) == 0) {
        try {
            uint32_t clientIdToKick = std::stoul(commandText.substr(12));
            std::shared_ptr<Session> sessionToKick = nullptr;
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                for (const auto& s_ptr : sessions_) {
                    if (s_ptr->getClientId() == clientIdToKick) {
                        sessionToKick = s_ptr;
                        break;
                    }
                }
            }
            if (sessionToKick && clientIdToKick != hostClientId_) {  
                LocalTether::Utils::Logger::GetInstance().Info("Host commanded kick for client ID: " + std::to_string(clientIdToKick));
                sessionToKick->close();  
            } else if (clientIdToKick == hostClientId_) {
                LocalTether::Utils::Logger::GetInstance().Warning("Host attempted to kick itself. Action denied.");
            } else {
                LocalTether::Utils::Logger::GetInstance().Warning("Kick command: Client ID " + std::to_string(clientIdToKick) + " not found.");
            }
        } catch (const std::exception& e) {
            LocalTether::Utils::Logger::GetInstance().Error("Error processing kick_client command: " + std::string(e.what()));
        }
    } else if (commandText.rfind("rename_client:", 0) == 0) {
         
        try {
            size_t first_colon = commandText.find(':');
            size_t second_colon = commandText.find(':', first_colon + 1);
            if (first_colon != std::string::npos && second_colon != std::string::npos) {
                uint32_t clientIdToRename = std::stoul(commandText.substr(first_colon + 1, second_colon - (first_colon + 1)));
                std::string newName = commandText.substr(second_colon + 1);
                
                if (newName.length() > 0 && newName.length() < 64) {  
                    std::shared_ptr<Session> sessionToRename = nullptr;
                    {
                        std::lock_guard<std::mutex> lock(sessions_mutex_);
                        for (const auto& s_ptr : sessions_) {
                            if (s_ptr->getClientId() == clientIdToRename) {
                                sessionToRename = s_ptr;
                                break;
                            }
                        }
                    }
                    if (sessionToRename) {
                        std::string oldName = sessionToRename->getClientName();
                        sessionToRename->setClientName(newName);
                        LocalTether::Utils::Logger::GetInstance().Info("Client ID " + std::to_string(clientIdToRename) + " renamed from '" + oldName + "' to '" + newName + "' by host.");
                         
                        sessionToRename->send(Message::createCommand("you_were_renamed:" + newName, 0));
                         
                        broadcast(Message::createCommand("client_renamed:" + std::to_string(clientIdToRename) + ":" + newName, 0));
                    } else {
                         LocalTether::Utils::Logger::GetInstance().Warning("Rename command: Client ID " + std::to_string(clientIdToRename) + " not found.");
                    }
                } else {
                     LocalTether::Utils::Logger::GetInstance().Warning("Rename command: Invalid new name provided.");
                }
            }
        } catch (const std::exception& e) {
            LocalTether::Utils::Logger::GetInstance().Error("Error processing rename_client command: " + std::string(e.what()));
        }
    } else if (commandText.rfind("toggle_input_client:", 0) == 0) {
         
        try {
            size_t first_colon = commandText.find(':');
            size_t second_colon = commandText.find(':', first_colon + 1);
            if (first_colon != std::string::npos && second_colon != std::string::npos) {
                uint32_t clientIdToToggle = std::stoul(commandText.substr(first_colon + 1, second_colon - (first_colon + 1)));
                std::string stateStr = commandText.substr(second_colon + 1);
                bool newState = (stateStr == "true");
                std::shared_ptr<Session> sessionToToggle = nullptr;
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    for (const auto& s_ptr : sessions_) {
                        if (s_ptr->getClientId() == clientIdToToggle) {
                            sessionToToggle = s_ptr;
                            break;
                        }
                    }
                }
                if (sessionToToggle && sessionToToggle->getRole() == ClientRole::Receiver) {
                    sessionToToggle->setCanReceiveInput(newState);
                    LocalTether::Utils::Logger::GetInstance().Info("Input for client ID " + std::to_string(clientIdToToggle) + " set to " + (newState ? "ENABLED" : "DISABLED") + " by host.");
                } else if (sessionToToggle) {
                     LocalTether::Utils::Logger::GetInstance().Warning("Toggle input: Client ID " + std::to_string(clientIdToToggle) + " is not a Receiver. Action denied.");
                } else {
                     LocalTether::Utils::Logger::GetInstance().Warning("Toggle input command: Client ID " + std::to_string(clientIdToToggle) + " not found.");
                }
            }
        } catch (const std::exception& e) {
            LocalTether::Utils::Logger::GetInstance().Error("Error processing toggle_input_client command: " + std::string(e.what()));
        }
    } else {
        LocalTether::Utils::Logger::GetInstance().Warning("Unknown command from host: " + commandText);
        auto reply = Message::createCommand("unknown_command:" + commandText, 0);
        session->send(reply);
    }
}

void Server::processFileRequest(std::shared_ptr<Session> session, const Message& message) {
    if (!session) return;
     
    std::string requestedFile = message.getTextPayload();  
    LocalTether::Utils::Logger::GetInstance().Info("Client " + session->getClientName() + " requested file: " + requestedFile);
     
    auto response = Message::createCommand("file_request_unsupported: " + requestedFile, 0);
    LocalTether::Utils::Logger::GetInstance().Warning("File request from " + session->getClientName() + 
        " (ID: " + std::to_string(session->getClientId()) + ") is unsupported.");
    session->send(response);
}

void Server::notifyClientJoined(std::shared_ptr<Session> session) {
    if (!session) return;
    auto joinMsg = Message::createCommand(
        "client_joined:" + std::to_string(session->getClientId()) + ":" +
        session->getClientName() + ":" + session->getRoleString(),
        0);
    broadcastExcept(joinMsg, session);  

     
    std::string client_list_str = "current_clients:";
    std::vector<std::shared_ptr<Session>> currentSessions;
     {
         std::lock_guard<std::mutex> lock(sessions_mutex_);
         currentSessions = sessions_;
     }
    for(const auto& s_ptr : currentSessions) {
        if (s_ptr && s_ptr->isAppHandshakeComplete()) {
            client_list_str += std::to_string(s_ptr->getClientId()) + "," + s_ptr->getClientName() + "," + s_ptr->getRoleString() + ";";
        }
    }
    if (client_list_str.back() == ';') client_list_str.pop_back();  
    auto clientListMsg = Message::createCommand(client_list_str, 0);
    LocalTether::Utils::Logger::GetInstance().Info(
        "Notifying new client " + session->getClientName() + " (ID: " + std::to_string(session->getClientId()) + 
        ") of current clients: " + client_list_str);
    session->send(clientListMsg);
}

void Server::notifyClientLeft(std::shared_ptr<Session> session) {
    if (!session) return;
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
 void Server::setState(ServerState newState, const std::optional<std::error_code>& ec) {
     state_ = newState;
     if (ec && ec.value()) {
         lastError_ = ec.value().message();
         LocalTether::Utils::Logger::GetInstance().Error("Server state changed to Error: " + lastError_);
     } else if (newState == ServerState::Error && lastError_.empty()) {
         lastError_ = "Unknown server error";
          LocalTether::Utils::Logger::GetInstance().Error("Server state changed to Error: " + lastError_);
     }
      
 }

std::string Server::getErrorMessage() const {
    return lastError_;
}

size_t Server::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

uint16_t Server::getPort() const {
    return port_;
}

std::vector<std::shared_ptr<LocalTether::Network::Session>> Server::getSessions() const {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_;  
}
uint32_t Server::getHostClientId() const {
    return hostClientId_;
}

}  