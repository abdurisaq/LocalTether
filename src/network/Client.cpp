#include "network/Client.h"
#include "network/Message.h"
#include "utils/Logger.h"

namespace LocalTether::Network {

Client::Client(asio::io_context& io_context)
    : io_context_(io_context), socket_(io_context_), state_(ClientState::Disconnected) {
}

Client::~Client() {
    disconnect();
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

void Client::sendInput(const std::vector<uint8_t>& inputData) {
    if (state_ != ClientState::Connected) {
        return;
    }
    
    auto msg = Message::createInput(inputData, clientId_);
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
    
    auto handshake = Message::createHandshake(role_, clientName_, password_, 0);
    auto data = handshake.serialize();
    
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
            
            setState(ClientState::Connected);
            
    
            clientId_ = message.getClientId();
            
            if (connectHandler_) {
                connectHandler_();
            }
        }
        return;
    }
    
    if (messageHandler_ && state_ == ClientState::Connected) {
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