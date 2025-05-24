#include "network/Session.h"
#include "utils/Logger.h"
#include <iostream>

namespace LocalTether::Network {

Session::Session(asio::ip::tcp::socket socket)
    : socket_(std::move(socket)), active_(true) {
}

Session::~Session() {
    close();
}

void Session::start(MessageHandler msgHandler, DisconnectHandler disconnectHandler) {
    messageHandler_ = std::move(msgHandler);
    disconnectHandler_ = std::move(disconnectHandler);
    
    // Start
    doRead();
}

void Session::send(const Message& message) {
    auto serialized = message.serialize();
    
    asio::post(socket_.get_executor(), [this, serialized = std::move(serialized)]() {
        bool shouldStartWrite = false;
        
        {
            std::lock_guard<std::mutex> lock(writeMutex_);
            shouldStartWrite = writeQueue_.empty() && !writing_;
            writeQueue_.push(std::move(serialized));
        }
        
        if (shouldStartWrite) {
            doWrite();
        }
    });
}

void Session::close() {
    if (!active_) return;
    
    active_ = false;
    
    
    asio::post(socket_.get_executor(), [this]() {
        asio::error_code ec;
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    });
}

std::string Session::getClientAddress() const {
    try {
        return socket_.remote_endpoint().address().to_string() + ":" + 
               std::to_string(socket_.remote_endpoint().port());
    } catch (const std::exception&) {
        return "unknown";
    }
}

void Session::doRead() {
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(readBuffer_),
        [this, self](const std::error_code& error, size_t bytes_transferred) {
            handleRead(error, bytes_transferred);
        });
}

void Session::handleRead(const std::error_code& error, size_t bytes_transferred) {
    if (!error) {
        try {
            
            partialMessage_.insert(partialMessage_.end(), 
                                 readBuffer_.data(), 
                                 readBuffer_.data() + bytes_transferred);
            
            
            size_t processed = 0;
            //get full msg
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
                
                
                if (messageHandler_) {
                    messageHandler_(shared_from_this(), message);
                }
                
                
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
                "Session error processing data: " + std::string(e.what()));
            
            
            if (disconnectHandler_) {
                disconnectHandler_(shared_from_this());
            }
        }
    }
    else {
        
        active_ = false;
        
        if (error != asio::error::eof && error != asio::error::operation_aborted) {
            LocalTether::Utils::Logger::GetInstance().Warning(
                "Session read error: " + error.message());
        }
        
        if (disconnectHandler_) {
            disconnectHandler_(shared_from_this());
        }
    }
}

void Session::doWrite() {
    auto self = shared_from_this();
    
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
    
    asio::async_write(
        socket_, 
        asio::buffer(data),
        [this, self](const std::error_code& error, size_t ) {
            if (!error) {
                
                doWrite();
            }
            else {
                
                active_ = false;
                
                LocalTether::Utils::Logger::GetInstance().Warning(
                    "Session write error: " + error.message());
                
                if (disconnectHandler_) {
                    disconnectHandler_(self);
                }
            }
        });
}

} 