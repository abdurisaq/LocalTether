#include "network/Client.h"
#include "network/Message.h"
#include "utils/Logger.h"
#include "input/InputManager.h"

#ifdef _WIN32
#include "input/WindowsInput.h"
#include <windows.h>
#else
#include "input/LinuxInput.h"
#endif


namespace LocalTether::Network {

Client::Client(asio::io_context& io_context)
    : io_context_(io_context), socket_(io_context_), state_(ClientState::Disconnected) {
}

Client::~Client() {
    disconnect();
    initializeLocalScreenDimensions();
}
void Client::initializeLocalScreenDimensions() {
    SDL_DisplayMode dm;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { 
         LocalTether::Utils::Logger::GetInstance().Warning("Client: SDL_Init(SDL_INIT_VIDEO) failed: " + std::string(SDL_GetError()) + ". Screen dimensions might be 0.");
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


void Client::connect(const std::string& host, uint16_t port, 
                    ClientRole role, const std::string& name, 
                    const std::string& password) {
    if (state_ != ClientState::Disconnected) {
        
        disconnect();
    }
    
    currentHost_ = host;
    currentPort_ = port;
    role_ = role;
    clientName_ = name;
    password_ = password;
    
    try {
        setState(ClientState::Connecting);
    
        asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        
        asio::async_connect(socket_, endpoints,
            [this](const std::error_code& error, const asio::ip::tcp::endpoint&) {
                handleConnect(error);
            });
    }
    catch (const std::exception& e) {
        setState(ClientState::Error);
        lastError_ = e.what();
        LocalTether::Utils::Logger::GetInstance().Error("Connection error: " + lastError_);
        
        if (errorHandler_) {
            errorHandler_(asio::error::connection_refused);
        }
    }
}

void Client::disconnect() {
    if (state_ == ClientState::Disconnected) {
        return;
    }
    stopInputLogging();
    try {
        if (socket_.is_open()) {
            
            asio::error_code ec;
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        }
    }
    catch (const std::exception& e) {
        LocalTether::Utils::Logger::GetInstance().Warning(
            "Error during disconnect: " + std::string(e.what()));
    }
    //clera
    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        std::queue<std::vector<uint8_t>> empty;
        std::swap(writeQueue_, empty);
        writing_ = false;
    }
    
    setState(ClientState::Disconnected);
    
    if (disconnectHandler_) {
        disconnectHandler_();
    }
}


void Client::sendInput(const InputPayload& payload) {
    if (state_ != ClientState::Connected) {
        return;
    }
    auto msg = Message::createInput(payload, clientId_);
    auto data = msg.serialize();
    
    asio::post(io_context_, [this, data = std::move(data)]() {
        bool shouldStartWrite = writeQueue_.empty() && !writing_;
        {
            std::lock_guard<std::mutex> lock(writeMutex_);
            writeQueue_.push(std::move(data));
        }
        if (shouldStartWrite) {
            doWrite();
        }
    });
}

void Client::startInputLogging() {
    if (loggingInput_) {
        return;
    }
    LocalTether::Utils::Logger::GetInstance().Info("Attempting to start input logging...");
    
    inputManager_ = LocalTether::Input::createInputManager(localScreenWidth_, localScreenHeight_);

    if (inputManager_ && inputManager_->start()) {
        loggingInput_ = true;
        inputThread_ = std::thread(&Client::inputLoop, this);
        LocalTether::Utils::Logger::GetInstance().Info("Input logging thread started.");
    } else {
        LocalTether::Utils::Logger::GetInstance().Error("Failed to start InputManager.");
    }
}


void Client::inputLoop() {
    LocalTether::Utils::Logger::GetInstance().Info("Input loop running...");
    while (loggingInput_ && inputManager_) {
        auto payloads = inputManager_->pollEvents();
        for (const auto& payload : payloads) {
            if (state_ == ClientState::Connected) {
                
                 sendInput(payload);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); //hardcoded 10 for now
    }
    LocalTether::Utils::Logger::GetInstance().Info("Input loop exited.");
}
void Client::stopInputLogging() {
    if (!loggingInput_) {
        return;
    }
    loggingInput_ = false;
    if (inputManager_) {
        inputManager_->stop();
    }
    if (inputThread_.joinable()) {
        inputThread_.join();
        LocalTether::Utils::Logger::GetInstance().Info("Input logging thread stopped.");
    }
    inputManager_.reset();
}




void Client::sendChat(const std::string& text) {
    if (state_ != ClientState::Connected) {
        return;
    }
    
    auto msg = Message::createChat(text, clientId_);
    auto data = msg.serialize();
    
    asio::post(io_context_, [this, data = std::move(data)]() {
        bool shouldStartWrite = false;
        
        {
            std::lock_guard<std::mutex> lock(writeMutex_);
            shouldStartWrite = writeQueue_.empty() && !writing_;
            writeQueue_.push(std::move(data));
        }
        
        if (shouldStartWrite) {
            doWrite();
        }
    });
}

void Client::sendCommand(const std::string& command) {
    if (state_ != ClientState::Connected) {
        return;
    }
    
    auto msg = Message::createCommand(command, clientId_);
    auto data = msg.serialize();
    
    asio::post(io_context_, [this, data = std::move(data)]() {
        bool shouldStartWrite = false;
        
        {
            std::lock_guard<std::mutex> lock(writeMutex_);
            shouldStartWrite = writeQueue_.empty() && !writing_;
            writeQueue_.push(std::move(data));
        }
        
        if (shouldStartWrite) {
            doWrite();
        }
    });
}

void Client::requestFile(const std::string& filename) {
    if (state_ != ClientState::Connected) {
        return;
    }
    
    auto msg = Message::createFileRequest(filename, clientId_);
    auto data = msg.serialize();
    
    asio::post(io_context_, [this, data = std::move(data)]() {
        bool shouldStartWrite = false;
        
        {
            std::lock_guard<std::mutex> lock(writeMutex_);
            shouldStartWrite = writeQueue_.empty() && !writing_;
            writeQueue_.push(std::move(data));
        }
        
        if (shouldStartWrite) {
            doWrite();
        }
    });
}

void Client::handleConnect(const std::error_code& error) {
    if (!error) {
       
        performHandshake();
        
        doRead();
    }
    else {
        setState(ClientState::Error, error);
        
        if (errorHandler_) {
            errorHandler_(error);
        }
    }
}

void Client::performHandshake() {
    
    uint16_t screenWidth = 0;
    uint16_t screenHeight = 0;

    if (role_ == ClientRole::Host) {
        screenWidth = localScreenWidth_; 
        screenHeight = localScreenHeight_;
    }

    auto handshake_payload = Network::HandshakePayload{role_, clientName_, password_, screenWidth, screenHeight};
    auto msg = Message::createHandshake(handshake_payload, 0); 
    auto data = msg.serialize();

    asio::async_write(socket_, asio::buffer(data),
        [this](const std::error_code& error, size_t) {
            if (error) {
                setState(ClientState::Error, error);
                if (errorHandler_) {
                    errorHandler_(error);
                }
            }
        });
}

void Client::doRead() {
    socket_.async_read_some(asio::buffer(readBuffer_),
        [this](const std::error_code& error, size_t bytes_transferred) {
            handleRead(error, bytes_transferred);
        });
}

void Client::handleRead(const std::error_code& error, size_t bytes_transferred) {
    if (!error) {
        try {
            
            LocalTether::Utils::Logger::GetInstance().Debug(
                "Received " + std::to_string(bytes_transferred) + " bytes from server.");
                
            partialMessage_.insert(partialMessage_.end(), 
                                 readBuffer_.data(), 
                                 readBuffer_.data() + bytes_transferred);
            
            // Process complete messages
            size_t processed = 0;
            while (processed < partialMessage_.size()) {
               
                if (partialMessage_.size() - processed < sizeof(MessageHeader)) {
                    break;
                }
               
                MessageHeader* header = reinterpret_cast<MessageHeader*>(partialMessage_.data() + processed);
                size_t totalSize = sizeof(MessageHeader) + header->size;
                
             
                if (partialMessage_.size() - processed < totalSize) {
                    break;
                }
                
                auto message = Message::parse(
                    reinterpret_cast<const char*>(partialMessage_.data() + processed), 
                    totalSize);
                
                
                handleMessage(message);
                
            
                processed += totalSize;
            }
            
           
            if (processed > 0) {
                partialMessage_.erase(partialMessage_.begin(), 
                                    partialMessage_.begin() + processed);
            }
            
            // Continue reading
            doRead();
        }
        catch (const std::exception& e) {
            LocalTether::Utils::Logger::GetInstance().Error(
                "Error processing received data: " + std::string(e.what()));
            
            setState(ClientState::Error);
            lastError_ = e.what();
            
            if (errorHandler_) {
                errorHandler_(asio::error::invalid_argument);
            }
        }
    }
    else {
       
        if (error == asio::error::eof || error == asio::error::connection_reset) {
           
            disconnect();
        }
        else {
            setState(ClientState::Error, error);
            
            if (errorHandler_) {
                errorHandler_(error);
            }
        }
    }
}

void Client::handleMessage(const Message& message) {
    
    

    if (message.getType() == MessageType::Handshake) {
        if (state_ == ClientState::Connecting) {
            
            HandshakePayload serverResponsePayload = message.getHandshakePayload(); 
            hostScreenWidth_ = serverResponsePayload.hostScreenWidth;
            hostScreenHeight_ = serverResponsePayload.hostScreenHeight;
            clientId_ = message.getClientId(); 

            LocalTether::Utils::Logger::GetInstance().Info(
                "Handshake successful. Client ID: " + std::to_string(clientId_) +
                ". Host screen: " + std::to_string(hostScreenWidth_) + "x" + std::to_string(hostScreenHeight_));

            setState(ClientState::Connected);

            if (role_ == ClientRole::Host && connectHandler_) {
                LocalTether::Utils::Logger::GetInstance().Info("Client is Host, starting input logging.");
                startInputLogging();
            } else if (role_ != ClientRole::Host) { 
                if (!inputManager_) {
                    inputManager_ = LocalTether::Input::createInputManager(localScreenWidth_, localScreenHeight_);
                }
                if (inputManager_) {
                    inputManager_->start(); // Ensure it's started
                    LocalTether::Utils::Logger::GetInstance().Info("InputManager started for receiver client for simulation.");
                } else {
                    LocalTether::Utils::Logger::GetInstance().Error("Failed to create/start InputManager for simulation on receiver client.");
                }
            }
            if (connectHandler_) {
                connectHandler_();
            }
        }
        return;
    }
    
    if (message.getType() == MessageType::Input && role_ != ClientRole::Host) {
        if (inputManager_) { 
            try {
                std::string keyLog = "Client received input";
                
                const auto& keyEvents = message.getInputPayload().keyEvents;
                for(const auto & event : keyEvents){
                    keyLog += " Key: " + std::to_string(event.keyCode) + 
                              (event.isPressed ? " Pressed" : " Released");

                    keyLog += LocalTether::Utils::Logger::getKeyName(event.keyCode) + " ";
                }
                if (message.getInputPayload().isMouseEvent) {
                    keyLog += " Mouse Event: ";
                    keyLog += "RelativeX: " + std::to_string(message.getInputPayload().relativeX) + 
                              " RelativeY: " + std::to_string(message.getInputPayload().relativeY);
                }
            
                
                LocalTether::Utils::Logger::GetInstance().Info(keyLog);
                
                InputPayload receivedPayload = message.getInputPayload();
                inputManager_->simulateInput(receivedPayload,hostScreenWidth_, hostScreenHeight_); 
            } catch (const std::exception& e) {
                LocalTether::Utils::Logger::GetInstance().Error(
                    "Failed to process received input: " + std::string(e.what()));
            }
        } else {
            LocalTether::Utils::Logger::GetInstance().Warning(
                "Received input message on receiver client, but no InputManager available to simulate.");
        }
    } else if (messageHandler_) {
        messageHandler_(message);
    }
}

void Client::doWrite() {
    std::vector<uint8_t> data;
    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        if (writeQueue_.empty()) {
            writing_ = false;
            return;
        }
        
        writing_ = true;
        data = std::move(writeQueue_.front());
        writeQueue_.pop();
    }
    
    asio::async_write(socket_, asio::buffer(data),
        [this](const std::error_code& error, size_t ) {
            if (!error) {
                doWrite(); 
            }
            else {
              
                setState(ClientState::Error, error);
                
                if (errorHandler_) {
                    errorHandler_(error);
                }
            }
        });
}

void Client::setState(ClientState newState, const std::error_code& error) {
    ClientState oldState = state_;
    state_ = newState;
    
    if (error) {
        lastError_ = error.message();
    }
    
   
    if (oldState != newState) {
        std::string stateStr;
        switch (newState) {
            case ClientState::Disconnected: stateStr = "Disconnected"; break;
            case ClientState::Connecting: stateStr = "Connecting"; break;
            case ClientState::Connected: stateStr = "Connected"; break;
            case ClientState::Error: stateStr = "Error: " + lastError_; break;
        }
        
        LocalTether::Utils::Logger::GetInstance().Info("Client state: " + stateStr);
    }
}

void Client::setConnectHandler(ConnectHandler handler) {
    connectHandler_ = std::move(handler);
}

void Client::setMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}

void Client::setDisconnectHandler(DisconnectHandler handler) {
    disconnectHandler_ = std::move(handler);
}

void Client::setErrorHandler(ErrorHandler handler) {
    errorHandler_ = std::move(handler);
}


} 



