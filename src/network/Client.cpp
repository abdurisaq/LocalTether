#include "network/Client.h"
#include "utils/Logger.h"
#include "utils/Serialization.h"  
#include "input/InputManager.h"   
#include <SDL.h>
#ifdef _WIN32
#include <winsock2.h>  
#else
#include <arpa/inet.h>  
#endif


namespace LocalTether::Network {

Client::Client(asio::io_context& io_context)
    : io_context_(io_context),
      resolver_(io_context)
       
{
    LocalTether::Utils::Logger::GetInstance().Debug("Client constructor: Entered.");

    try {
        LocalTether::Utils::Logger::GetInstance().Debug("Client constructor: Attempting to create SSL context (ssl_context_opt_.emplace).");
        ssl_context_opt_.emplace(asio::ssl::context::tls_client);  
        LocalTether::Utils::Logger::GetInstance().Info("Client constructor: SSL context created successfully.");

        if (!ssl_context_opt_->native_handle()) {
            LocalTether::Utils::Logger::GetInstance().Critical("Client constructor: SSL_CTX native_handle is NULL after SSL context construction!");
            throw std::runtime_error("SSL_CTX native_handle is NULL after SSL context construction");
        } else {
            LocalTether::Utils::Logger::GetInstance().Debug("Client constructor: SSL_CTX native_handle seems valid after SSL context construction.");
        }

        LocalTether::Utils::Logger::GetInstance().Debug("Client constructor: Attempting to create SSL socket (socket_opt_.emplace).");
        if (!ssl_context_opt_) {
             LocalTether::Utils::Logger::GetInstance().Critical("Client constructor: ssl_context_opt_ is null before socket creation!");
             throw std::runtime_error("ssl_context_opt_ is null before socket creation");
        }
        socket_opt_.emplace(io_context_, *ssl_context_opt_);
        LocalTether::Utils::Logger::GetInstance().Info("Client constructor: SSL socket created successfully.");

    } catch (const asio::system_error& e) {
        LocalTether::Utils::Logger::GetInstance().Error("Client constructor: SSL context/socket creation failed (asio::system_error): " + std::string(e.what()) + ", Code: " + std::to_string(e.code().value()));
        ssl_context_opt_.reset();
        socket_opt_.reset();
        throw;
    } catch (const std::exception& e) {
        LocalTether::Utils::Logger::GetInstance().Error("Client constructor: SSL context/socket creation failed (std::exception): " + std::string(e.what()));
        ssl_context_opt_.reset();
        socket_opt_.reset();
        throw;
    }

    LocalTether::Utils::Logger::GetInstance().Info("Client created (after SSL init).");

    try {
        LocalTether::Utils::Logger::GetInstance().Debug("Client constructor: Attempting to set SSL verify mode.");
        if (ssl_context_opt_) {
            ssl_context_opt_->set_verify_mode(asio::ssl::verify_none);  
            LocalTether::Utils::Logger::GetInstance().Info("Client SSL context configured (verify_mode set).");
        } else {
            LocalTether::Utils::Logger::GetInstance().Error("Client constructor: Cannot set SSL verify mode, SSL context not initialized.");
            throw std::runtime_error("Cannot set SSL verify mode, SSL context not initialized.");
        }
    } catch (const asio::system_error& e) {
        LocalTether::Utils::Logger::GetInstance().Error("Client SSL context (set_verify_mode) failed (asio::system_error): " + std::string(e.what()) + ", Code: " + std::to_string(e.code().value()));
        throw;
    } catch (const std::exception& e) {
        LocalTether::Utils::Logger::GetInstance().Error("Client SSL context (set_verify_mode) failed (std::exception): " + std::string(e.what()));
        throw;
    }

    LocalTether::Utils::Logger::GetInstance().Debug("Client constructor: Attempting to initialize local screen dimensions.");
    initializeLocalScreenDimensions();
    LocalTether::Utils::Logger::GetInstance().Debug("Client constructor: Finished initializing local screen dimensions.");
}

void Client::initializeLocalScreenDimensions() {
    SDL_DisplayMode dm;
     
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
         LocalTether::Utils::Logger::GetInstance().Warning("Client: SDL_InitSubSystem(SDL_INIT_VIDEO) failed: " + std::string(SDL_GetError()) + ". Screen dimensions might be 0.");
    }
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
        localScreenWidth_ = static_cast<uint16_t>(dm.w);
        localScreenHeight_ = static_cast<uint16_t>(dm.h);
        LocalTether::Utils::Logger::GetInstance().Info("Client local screen dimensions: " + std::to_string(localScreenWidth_) + "x" + std::to_string(localScreenHeight_));
    } else {
        LocalTether::Utils::Logger::GetInstance().Error("Client: Failed to get local screen dimensions using SDL: " + std::string(SDL_GetError()));
        localScreenWidth_ = 1920;
        localScreenHeight_ = 1080;
    }
}

Client::~Client() {
    LocalTether::Utils::Logger::GetInstance().Debug("Client destructor called.");
    disconnect("client destroyed");
}

void Client::setState(ClientState newState, const std::optional<std::error_code>& ec) {
    ClientState oldState = state_.exchange(newState);
    if (ec && ec.value()) {
        lastError_ = ec.value().message();
        if (newState == ClientState::Error && oldState != ClientState::Error) {
             LocalTether::Utils::Logger::GetInstance().Error("Client state changed to Error: " + lastError_);
        }
    } else if (newState == ClientState::Error && lastError_.empty()) {
        lastError_ = "Unknown client error";
        if (oldState != ClientState::Error) {
             LocalTether::Utils::Logger::GetInstance().Error("Client state changed to Error: " + lastError_);
        }
    }
}

void Client::connect(const std::string& host, uint16_t port,
                     ClientRole role, const std::string& name,
                     const std::string& password_param ) {
     
    if (!socket_opt_ || !ssl_context_opt_) {
        LocalTether::Utils::Logger::GetInstance().Error("Client::connect: Client not properly initialized (socket or SSL context missing).");
        if (connectHandler_) connectHandler_(false, "Client not initialized", 0);
        return;
    }

     
    ClientState current_state = state_.load();
    if (current_state == ClientState::Connecting || current_state == ClientState::Connected) {
        LocalTether::Utils::Logger::GetInstance().Warning(
            "Client::connect called while already " +
            std::string(current_state == ClientState::Connecting ? "connecting" : "connected") +
            ". Ignoring new connect request. Current state: " + std::to_string(static_cast<int>(current_state)));
         
         
         
        return;
    }

     
     
     
     
     
     

    LocalTether::Utils::Logger::GetInstance().Info(
        "Client::connect: Initiating new connection. Previous state: " + std::to_string(static_cast<int>(current_state)));

     
    currentHost_ = host;
    currentPort_ = port;
    role_ = role;
    clientName_ = name;
    password_ = password_param;

     
     
     
     

    setState(ClientState::Connecting);  

    LocalTether::Utils::Logger::GetInstance().Info(
        "Client connecting to " + currentHost_ + ":" + std::to_string(currentPort_) +
        " as " + clientName_ + " with role " + Message::messageTypeToString(static_cast<MessageType>(static_cast<int>(role_))) +
        " local screen: " + std::to_string(localScreenWidth_) + "x" + std::to_string(localScreenHeight_));

    doResolve();  
}

void Client::doResolve() {
    if (!socket_opt_) {  
        LocalTether::Utils::Logger::GetInstance().Error("Client::doResolve: Socket not initialized.");
        setState(ClientState::Error); lastError_ = "Socket not initialized for resolve";
        if (connectHandler_) connectHandler_(false, lastError_, 0);
        return;
    }
    resolver_.async_resolve(currentHost_, std::to_string(currentPort_),
        [this](const std::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints) {
            handleResolve(ec, endpoints);
        });
}

void Client::handleResolve(const std::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints) {
    if (state_.load() != ClientState::Connecting) return;

    if (!socket_opt_) {  
        LocalTether::Utils::Logger::GetInstance().Error("Client::handleResolve: Socket not initialized.");
        setState(ClientState::Error); lastError_ = "Socket not initialized in handleResolve";
        if (connectHandler_) connectHandler_(false, lastError_, 0);
        return;
    }

    if (!ec) {
        LocalTether::Utils::Logger::GetInstance().Info("Host resolved: " + currentHost_);
        asio::async_connect(socket_opt_->lowest_layer(), endpoints,
            [this](const std::error_code& error, const asio::ip::tcp::endpoint& endpoint) {
                handleTcpConnect(error, endpoint);
            });
    } else {
        LocalTether::Utils::Logger::GetInstance().Error("Resolve error: " + ec.message());
        setState(ClientState::Error, ec);
        if (connectHandler_) connectHandler_(false, "Resolve error: " + ec.message(), 0);
        if (errorHandler_) errorHandler_(ec);
    }
}

void Client::handleTcpConnect(const std::error_code& error, const asio::ip::tcp::endpoint& endpoint) {
    if (state_.load() != ClientState::Connecting) return;

    if (!socket_opt_) {  
        LocalTether::Utils::Logger::GetInstance().Error("Client::handleTcpConnect: Socket not initialized.");
        setState(ClientState::Error); lastError_ = "Socket not initialized in handleTcpConnect";
        if (connectHandler_) connectHandler_(false, lastError_, 0);
        return;
    }

    if (!error) {
        std::string ep_str = "unknown";
        try {
             ep_str = endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
        } catch(const std::exception&) { /* ignore */ }
        LocalTether::Utils::Logger::GetInstance().Info("TCP connected to " + ep_str);
        doSslHandshake();
    } else {
        LocalTether::Utils::Logger::GetInstance().Error("TCP connect error: " + error.message());
        setState(ClientState::Error, error);
        if (connectHandler_) connectHandler_(false, "TCP connect error: " + error.message(), 0);
        if (errorHandler_) errorHandler_(error);
    }
}

void Client::doSslHandshake() {
    if (!socket_opt_) {
        LocalTether::Utils::Logger::GetInstance().Error("Client::doSslHandshake: Socket not initialized.");
        setState(ClientState::Error); lastError_ = "Socket not initialized for SSL handshake";
        if (connectHandler_) connectHandler_(false, lastError_, 0);
        return;
    }
    socket_opt_->async_handshake(asio::ssl::stream_base::client,
        [this](const std::error_code& error) {
            handleSslHandshake(error);
        });
}

void Client::handleSslHandshake(const std::error_code& error) {
    if (state_.load() != ClientState::Connecting) return;
    if (!socket_opt_) { return; }

    if (!error) {
        LocalTether::Utils::Logger::GetInstance().Info("SSL handshake successful with server.");
        performApplicationHandshake();
    } else {
        LocalTether::Utils::Logger::GetInstance().Error("SSL handshake error: " + error.message());
        setState(ClientState::Error, error);
        if (connectHandler_) connectHandler_(false, "SSL handshake error: " + error.message(), 0);
        if (errorHandler_) errorHandler_(error);
        doClose("SSL handshake failed", false);
    }
}

void Client::performApplicationHandshake() {
    if (!socket_opt_) {
        LocalTether::Utils::Logger::GetInstance().Error("Client::performApplicationHandshake: Socket not initialized.");
        setState(ClientState::Error); lastError_ = "Socket not initialized for app handshake";
        if (connectHandler_) connectHandler_(false, lastError_, 0);
        return;
    }
    LocalTether::Utils::Logger::GetInstance().Debug("Client performing application handshake.");
    HandshakePayload clientHandshake;
    clientHandshake.clientName = clientName_;
    clientHandshake.role = role_;
    clientHandshake.password = password_;
    clientHandshake.hostScreenWidth = localScreenWidth_;  
    clientHandshake.hostScreenHeight = localScreenHeight_;

    auto handshakeMsg = Message::createHandshake(clientHandshake, 0);
    send(handshakeMsg);
    doRead();
}

void Client::disconnect(const std::string& reason) {
    LocalTether::Utils::Logger::GetInstance().Info("Client::disconnect called. Reason: " + reason);
    if (!socket_opt_ && state_.load() == ClientState::Disconnected) {
        return;
    }
    asio::post(io_context_, [this, reason]() {
        doClose(reason, true);
    });
}

void Client::doClose(const std::string& reason, bool notifyDisconnectHandler) {
    stopInputLogging();

    ClientState expected_state = state_.load();
    if (expected_state == ClientState::Disconnected && reason != "reconnecting") {
        return;
    }

    if (state_.exchange(ClientState::Disconnected) == ClientState::Disconnected && reason != "reconnecting") {
         return;
    }

    LocalTether::Utils::Logger::GetInstance().Info("Closing client connection. Reason: " + reason);

    if (socket_opt_ && socket_opt_->lowest_layer().is_open()) {
        socket_opt_->async_shutdown(
            [this, notifyDisconnectHandler, reason_copy = reason](const std::error_code& shutdown_ec) {
                if (shutdown_ec && shutdown_ec != asio::error::eof && shutdown_ec != asio::ssl::error::stream_truncated) {
                    LocalTether::Utils::Logger::GetInstance().Warning("Client SSL shutdown error: " + shutdown_ec.message());
                }
                asio::error_code close_ec;
                if (socket_opt_ && socket_opt_->lowest_layer().is_open()) {  
                    socket_opt_->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, close_ec);
                    socket_opt_->lowest_layer().close(close_ec);
                }

                if (notifyDisconnectHandler && disconnectHandler_) {
                    disconnectHandler_(reason_copy);
                }
            });
    } else {
         if (notifyDisconnectHandler && disconnectHandler_) {
             disconnectHandler_(reason);
         }
    }

    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        std::queue<std::vector<uint8_t>> emptyQueue;
        std::swap(writeQueue_, emptyQueue);
        writing_ = false;
    }
}

LocalTether::Input::InputManager* Client::getInputManager() const {
    return inputManager_.get();
}

void Client::send(const Message& msg) {
    if (!socket_opt_) {
        LocalTether::Utils::Logger::GetInstance().Warning("Client::send: Socket not initialized. Cannot send message.");
        return;
    }
    if (state_.load() != ClientState::Connected && msg.getType() != MessageType::Handshake) {
        LocalTether::Utils::Logger::GetInstance().Warning(
            "Client trying to send " + Message::messageTypeToString(msg.getType()) +
            " while not fully connected. State: " + std::to_string(static_cast<int>(state_.load())));
        return;
    }
    if (state_.load() == ClientState::Disconnected || state_.load() == ClientState::Error) {
         LocalTether::Utils::Logger::GetInstance().Warning(
            "Client trying to send " + Message::messageTypeToString(msg.getType()) +
            " while disconnected or in error state.");
         return;
    }

    auto serialized_data = msg.serialize();

    asio::post(io_context_, [this, data = std::move(serialized_data)]() {
        if (!socket_opt_ || (state_.load() == ClientState::Disconnected || state_.load() == ClientState::Error)) return;

        bool should_start_write = false;
        {
            std::lock_guard<std::mutex> lock(writeMutex_);
            should_start_write = writeQueue_.empty() && !writing_.load(std::memory_order_relaxed);
            writeQueue_.push(std::move(data));
        }

        if (should_start_write) {
            doWrite();
        }
    });
}

void Client::doWrite() {
    if (!socket_opt_ || (state_.load() == ClientState::Disconnected || state_.load() == ClientState::Error)) {
        writing_ = false;
        return;
    }

    std::vector<uint8_t> data_to_send;
    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        if (writeQueue_.empty()) {
            writing_ = false;
            return;
        }
        writing_ = true;
        data_to_send = std::move(writeQueue_.front());
    }

    asio::async_write(*socket_opt_, asio::buffer(data_to_send),  
        [this](const std::error_code& error, size_t bytes_transferred) {
            handleWrite(error, bytes_transferred);
        });
}

void Client::handleWrite(const std::error_code& error, size_t /*bytes_transferred*/) {
    if (!socket_opt_) { writing_ = false; return; }

    bool should_continue_writing = false;
    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        if (!writeQueue_.empty()) {
             writeQueue_.pop();
        }

        if (!error) {
            if (!writeQueue_.empty()) {
                should_continue_writing = true;
            } else {
                writing_ = false;
            }
        } else {
            writing_ = false;
            LocalTether::Utils::Logger::GetInstance().Error("Client write error: " + error.message());
        }
    }

    if (!error && should_continue_writing) {
        if (state_.load() != ClientState::Disconnected && state_.load() != ClientState::Error) {
             doWrite();
        } else {
             writing_ = false;
        }
    } else if (error) {
        setState(ClientState::Error, error);
        if (errorHandler_) errorHandler_(error);
        doClose("write error: " + error.message(), true);
    }
}

void Client::doRead() {
    if (!socket_opt_ || (state_.load() == ClientState::Disconnected || state_.load() == ClientState::Error)) {
        return;
    }
     
     
    readBuffer_.resize(4096); 

    socket_opt_->async_read_some(asio::buffer(readBuffer_),
        [this](const std::error_code& error, size_t bytes_transferred) {
            handleRead(error, bytes_transferred);
        });
}

void Client::handleRead(const std::error_code& error, size_t bytes_transferred) {
    if (!socket_opt_) {  
        LocalTether::Utils::Logger::GetInstance().Error("Client::handleRead: socket_opt_ is not engaged.");
        return;
    }

    if (!error) {
        try {
            LocalTether::Utils::Logger::GetInstance().Trace("Client::handleRead: Received " + std::to_string(bytes_transferred) + " bytes.");
            partialMessage_.insert(partialMessage_.end(),
                                 readBuffer_.data(),
                                 readBuffer_.data() + bytes_transferred);

            size_t processed_offset = 0;
            while (state_.load() != ClientState::Disconnected && state_.load() != ClientState::Error) {  
                if (partialMessage_.size() - processed_offset < Message::HEADER_LENGTH) {
                    LocalTether::Utils::Logger::GetInstance().Trace("Client::handleRead: Not enough data for header. Have " + std::to_string(partialMessage_.size() - processed_offset) + ", need " + std::to_string(Message::HEADER_LENGTH));
                    break;  
                }

                 
                 
                if (!currentReadMessage_.decodeHeader(partialMessage_.data() + processed_offset, Message::HEADER_LENGTH)) {
                    LocalTether::Utils::Logger::GetInstance().Error("Client: Failed to decode message header from partial buffer. Clearing buffer and disconnecting.");
                    partialMessage_.clear();
                    doClose("Header decode failed in stream", true);
                    return;
                }
                
                LocalTether::Utils::Logger::GetInstance().Trace(
                    "Client::handleRead: Decoded header. Type: " + Message::messageTypeToString(currentReadMessage_.getType()) +
                    ", Body Size: " + std::to_string(currentReadMessage_.getBodySize()));


                size_t totalMessageSize = Message::HEADER_LENGTH + currentReadMessage_.getBodySize();

                if (partialMessage_.size() - processed_offset < totalMessageSize) {
                    LocalTether::Utils::Logger::GetInstance().Trace("Client::handleRead: Not enough data for full message. Have " + std::to_string(partialMessage_.size() - processed_offset) + ", need " + std::to_string(totalMessageSize));
                    break;  
                }
                
                if (currentReadMessage_.getBodySize() > Message::MAX_BODY_LENGTH) {
                     LocalTether::Utils::Logger::GetInstance().Error("Client::handleRead: Message body too large: " + std::to_string(currentReadMessage_.getBodySize()) + ". Disconnecting.");
                     partialMessage_.clear();
                     doClose("Message body too large in stream", true);
                     return;
                }

                if (currentReadMessage_.getBodySize() > 0) {
                    if (!currentReadMessage_.decodeBody(partialMessage_.data() + processed_offset + Message::HEADER_LENGTH, currentReadMessage_.getBodySize())) {
                        LocalTether::Utils::Logger::GetInstance().Error("Client: Failed to decode message body from partial buffer. Clearing buffer and disconnecting.");
                        partialMessage_.clear();
                        doClose("Body decode failed in stream", true);
                        return;
                    }
                }
                
                 
                handleMessage(currentReadMessage_);

                processed_offset += totalMessageSize;
                LocalTether::Utils::Logger::GetInstance().Trace("Client::handleRead: Processed one message. Total processed offset: " + std::to_string(processed_offset));
            }

            if (processed_offset > 0) {
                partialMessage_.erase(partialMessage_.begin(),
                                    partialMessage_.begin() + processed_offset);
                LocalTether::Utils::Logger::GetInstance().Trace("Client::handleRead: Erased " + std::to_string(processed_offset) + " bytes from partialMessage_. Remaining: " + std::to_string(partialMessage_.size()));
            }

             
            if (state_.load() == ClientState::Connected || 
                (state_.load() == ClientState::Connecting && !partialMessage_.empty())) {  
                 doRead();
            } else if (state_.load() == ClientState::Connecting && partialMessage_.empty()) {
                 doRead();  
            }


        } catch (const std::exception& e) {
            LocalTether::Utils::Logger::GetInstance().Error(
                "Client: Error processing received data in handleRead: " + std::string(e.what()));
            setState(ClientState::Error);
            lastError_ = e.what();
            if (errorHandler_) {
                errorHandler_(asio::error::invalid_argument); 
            }
            doClose("Exception in handleRead: " + std::string(e.what()), true);
        }
    } else {
        if (error == asio::error::eof || error == asio::error::connection_reset || error == asio::ssl::error::stream_truncated) {
            LocalTether::Utils::Logger::GetInstance().Info("Client disconnected: " + error.message() + ". Current state: " + std::to_string(static_cast<int>(state_.load())));
             
            if (state_.load() != ClientState::Disconnected && state_.load() != ClientState::Error) {
                 doClose("Connection closed by peer: " + error.message(), true);
            }
        } else if (error != asio::error::operation_aborted) {
            LocalTether::Utils::Logger::GetInstance().Error("Client read error: " + error.message());
            if (state_.load() != ClientState::Disconnected && state_.load() != ClientState::Error) {
                setState(ClientState::Error, error);
                if (errorHandler_) {
                    errorHandler_(error);
                }
                doClose("Read error: " + error.message(), true);
            }
        }
         
    }
}


void Client::handleMessage(const Message& message) {
    LocalTether::Utils::Logger::GetInstance().Trace("Client::handleMessage: Received message type: " + Message::messageTypeToString(message.getType()));

    if (message.getType() == MessageType::Handshake) {
        if (state_.load() == ClientState::Connecting) {
            try {
                HandshakePayload serverResponsePayload = message.getHandshakePayload();
                hostScreenWidth_ = serverResponsePayload.hostScreenWidth;
                hostScreenHeight_ = serverResponsePayload.hostScreenHeight;
                 
                 
                 
                clientId_ = serverResponsePayload.clientId; 

                LocalTether::Utils::Logger::GetInstance().Info(
                    "Handshake successful. Client ID: " + std::to_string(clientId_) +
                    ". Host screen: " + std::to_string(hostScreenWidth_) + "x" + std::to_string(hostScreenHeight_));

                setState(ClientState::Connected);

                if (role_ == ClientRole::Host || role_ == ClientRole::Receiver || role_ == ClientRole::Broadcaster) {
                    if (localScreenWidth_ > 0 && localScreenHeight_ > 0) {
                        if (!inputManager_) {
                             LocalTether::Utils::Logger::GetInstance().Info("Creating InputManager. Role: " + std::to_string((int)role_) + ", Host Mode: " + ((role_ == ClientRole::Host) ? "true" : "false"));
                            inputManager_ = LocalTether::Input::createInputManager(localScreenWidth_, localScreenHeight_, (role_ == ClientRole::Host));
                        }
                        if (inputManager_) {
                            if (inputManager_->start()) {
                                LocalTether::Utils::Logger::GetInstance().Info("InputManager started successfully for client.");
                                
                                    startInputLogging();
                             
                            } else {
                                LocalTether::Utils::Logger::GetInstance().Error("Failed to start InputManager for client");
                            }
                        } else {
                            LocalTether::Utils::Logger::GetInstance().Error("Failed to create InputManager for client " );
                        }
                    } else {
                        LocalTether::Utils::Logger::GetInstance().Warning("Local screen dimensions not set, cannot initialize InputManager.");
                    }
                }
                if (connectHandler_) {
                     
                    connectHandler_(true, "Handshake successful", clientId_);
                }
            } catch (const std::exception& e) {
                 LocalTether::Utils::Logger::GetInstance().Error("Error processing handshake payload: " + std::string(e.what()));
                 setState(ClientState::Error); lastError_ = "Handshake processing error";
                 doClose("handshake processing error", true);
            }
        } else {
             LocalTether::Utils::Logger::GetInstance().Warning("Received Handshake message in unexpected state: " + std::to_string(static_cast<int>(state_.load())));
        }
        return;  
    }
    
     
    if (state_.load() != ClientState::Connected) {
        LocalTether::Utils::Logger::GetInstance().Warning("Client::handleMessage: Received non-handshake message while not connected. Type: " + Message::messageTypeToString(message.getType()));
        return;
    }

    if (message.getType() == MessageType::Input && role_ != ClientRole::Host) {
        if (inputManager_ && inputManager_->isRunning()) {
            try {
                if (LocalTether::Input::InputManager::isInputGloballyPaused()) {
                    return;
                }
                InputPayload receivedPayload = message.getInputPayload();
                std::string keyLog = "Client received input for simulation:";
                
                for(const auto & event : receivedPayload.keyEvents){
                    keyLog += " Key: " + std::to_string(event.keyCode) +
                              (event.isPressed ? " Pressed" : " Released") +
                              " (" + LocalTether::Utils::Logger::getKeyName(event.keyCode) + ")";
                }
                if (receivedPayload.isMouseEvent) {
                    keyLog += " Mouse Event: ";
                    keyLog += "RelX: " + std::to_string(receivedPayload.relativeX) +
                              " RelY: " + std::to_string(receivedPayload.relativeY) +
                              " Buttons: " + std::to_string(receivedPayload.mouseButtons) +
                              " ScrollX: " + std::to_string(receivedPayload.scrollDeltaX) +
                              " ScrollY: " + std::to_string(receivedPayload.scrollDeltaY);
                }
                LocalTether::Utils::Logger::GetInstance().Trace(keyLog);
                
                inputManager_->simulateInput(receivedPayload, hostScreenWidth_, hostScreenHeight_);
            } catch (const std::exception& e) {
                LocalTether::Utils::Logger::GetInstance().Error(
                    "Failed to process received input for simulation: " + std::string(e.what()));
            }
        } else {
            LocalTether::Utils::Logger::GetInstance().Warning(
                "Received input message on receiver/broadcaster client, but no InputManager available/running to simulate.");
        }
        return;  
    }
    if (message.getType() == MessageType::Command) {
        std::string commandText = message.getTextPayload();
        LocalTether::Utils::Logger::GetInstance().Debug("Client received command: " + commandText);
        if (commandText.rfind("you_were_renamed:", 0) == 0) {
            std::string newName = commandText.substr(17);
            LocalTether::Utils::Logger::GetInstance().Info("Server renamed this client to: " + newName);
            clientName_ = newName;  
        } else if (commandText.rfind("client_renamed:", 0) == 0) {
             
             
             
            LocalTether::Utils::Logger::GetInstance().Info("Server announced client rename: " + commandText);
        } else if (commandText == "server_shutdown_imminent") {
            LocalTether::Utils::Logger::GetInstance().Info("Server is shutting down. Disconnecting.");
             
             
             
        }
        
    }
     
    if (messageHandler_) {
        messageHandler_(message);
    } else {
        LocalTether::Utils::Logger::GetInstance().Debug("No external message handler set for message type: " + Message::messageTypeToString(message.getType()));
    }
}

void Client::startInputLogging() {
    if (loggingInput_.load(std::memory_order_relaxed) && inputManager_ && inputManager_->isRunning()) {
        LocalTether::Utils::Logger::GetInstance().Info("Input logging already active and InputManager running.");
        return;
    }
    if (loggingInput_) {
         LocalTether::Utils::Logger::GetInstance().Info("Input logging was active, but manager may need restart or was stopped.");
    }

    LocalTether::Utils::Logger::GetInstance().Info("Attempting to start input logging...");

    if (!inputManager_) {
        LocalTether::Utils::Logger::GetInstance().Error("InputManager not available for input logging. Should be created for Host role during handshake.");
        if (localScreenWidth_ > 0 && localScreenHeight_ > 0 && role_ == ClientRole::Host) {
            LocalTether::Utils::Logger::GetInstance().Info("Creating new InputManager for input logging (late).");
            inputManager_ = LocalTether::Input::createInputManager(localScreenWidth_, localScreenHeight_, (role_ == ClientRole::Host));
        } else {
            loggingInput_ = false;
            return;
        }
    } else {
        LocalTether::Utils::Logger::GetInstance().Info("Using existing InputManager for input logging.");
    }

    if (inputManager_ && !inputManager_->isRunning()) {
        if (!inputManager_->start()) {
            LocalTether::Utils::Logger::GetInstance().Error("Failed to start InputManager for input logging.");
            loggingInput_ = false;
            return;
        }
    }

    if (inputManager_ && inputManager_->isRunning()) {
        loggingInput_ = true;
        if (inputThread_.joinable()) {
            inputThread_.join();
        }
        inputThread_ = std::thread(&Client::inputLoop, this);
        LocalTether::Utils::Logger::GetInstance().Info("Input logging thread started.");
    } else {
        LocalTether::Utils::Logger::GetInstance().Error("Failed to start/ensure InputManager is running for input logging.");
        loggingInput_ = false;
    }
}

void Client::inputLoop() {
    LocalTether::Utils::Logger::GetInstance().Info("Input loop running...");
    while (loggingInput_.load(std::memory_order_relaxed)) {
        if (!inputManager_ || !inputManager_->isRunning()) {
            LocalTether::Utils::Logger::GetInstance().Warning("InputManager stopped or not available in inputLoop. Exiting loop.");
            loggingInput_ = false;
            break;
        }

            auto payloads = inputManager_->pollEvents();
        if (role_ == ClientRole::Host && state_.load() == ClientState::Connected) {
            for (const auto& payload : payloads) {
                sendInput(payload);
            }
        }

        if (LocalTether::Input::InputManager::isInputGloballyPaused()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    LocalTether::Utils::Logger::GetInstance().Info("Input loop exited.");
}

void Client::stopInputLogging() {
    if (!loggingInput_.load(std::memory_order_relaxed)) {
        return;
    }
    LocalTether::Utils::Logger::GetInstance().Info("Stopping input logging...");
    loggingInput_ = false;

    if (inputThread_.joinable()) {
        inputThread_.join();
        LocalTether::Utils::Logger::GetInstance().Info("Input logging thread joined.");
    }
    if (inputManager_) {
        inputManager_->stop();
        LocalTether::Utils::Logger::GetInstance().Info("InputManager stopped.");
    }
}

void Client::sendInput(const InputPayload& payload) {
    if (state_.load() != ClientState::Connected) {
        return;
    }
    auto msg = Message::createInput(payload, clientId_);
    send(msg);
}

void Client::sendChatMessage(const std::string& chatMessage) {
    if (state_.load() != ClientState::Connected) return;
    auto msg = Message::createChat(chatMessage, clientId_);
    send(msg);
}

void Client::sendCommand(const std::string& command) {
    if (state_.load() != ClientState::Connected) return;
    auto msg = Message::createCommand(command, clientId_);
    send(msg);
}

void Client::requestFile(const std::string& filename) {
    if (state_.load() != ClientState::Connected) return;
    auto msg = Message::createFileRequest(filename, clientId_);
    send(msg);
}

}  