#include "network/Session.h"
#include "utils/Logger.h"
#include <iostream>

namespace LocalTether::Network {

Session::Session(asio::ip::tcp::socket socket)
    : socket_(std::move(socket)), active_(true) {
}

Session::~Session() {
    if (active_.exchange(false)) {
        if (socket_.is_open()) {
            asio::error_code ec;
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec); 
            socket_.close(ec);                                       
        }
    }
}

void Session::start(MessageHandler msgHandler, DisconnectHandler disconnectHandler) {
    messageHandler_ = std::move(msgHandler);
    disconnectHandler_ = std::move(disconnectHandler);
    
     
    doRead();
}

void Session::send(const Message& message) {
    if (!active_.load(std::memory_order_relaxed)) { 
        return;
    }
    auto serialized = message.serialize();
     
     
     

    auto self = shared_from_this(); 
    asio::post(socket_.get_executor(), [self, serialized_data = std::move(serialized)]() {
        if (!self->active_.load(std::memory_order_relaxed)) {
            return;
        }
        bool shouldStartWrite = false;
        {
            std::lock_guard<std::mutex> lock(self->writeMutex_);
            shouldStartWrite = self->writeQueue_.empty() && !self->writing_.load(std::memory_order_relaxed);
            self->writeQueue_.push(std::move(serialized_data));
        }

        if (shouldStartWrite) {
            self->doWrite();
        }
    });
}

void Session::close() {
    if (!active_.exchange(false)) {
        return; 
    }

    
    auto self = shared_from_this();
    asio::post(socket_.get_executor(), [self]() {
        
        if (self->socket_.is_open()) {
            asio::error_code ec_shutdown;
            self->socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec_shutdown);
            asio::error_code ec_close;
            self->socket_.close(ec_close);
        }
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
    if (!active_.load(std::memory_order_relaxed)) {
        
        std::lock_guard<std::mutex> lock(writeMutex_);
        if (!writeQueue_.empty()) {
            std::queue<std::vector<uint8_t>> emptyQueue;
            std::swap(writeQueue_, emptyQueue);
        }
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
        writeQueue_.pop();
    }

    auto self = shared_from_this(); 
    asio::async_write(socket_, asio::buffer(data_to_send),
        [self](const std::error_code& error, size_t bytes_transferred) {
            self->handleWrite(error, bytes_transferred);
        });
}
void Session::handleWrite(const std::error_code& error, size_t /*bytes_transferred*/) {
    if (!error) {
       
        std::lock_guard<std::mutex> lock(writeMutex_); 
        if (!writeQueue_.empty()) {
            
            asio::post(socket_.get_executor(), [self = shared_from_this()]() { self->doWrite(); });
        } else {
            writing_ = false; 
        }
    } else {
        writing_ = false; 
        if (active_.exchange(false)) { 
             LocalTether::Utils::Logger::GetInstance().Warning(
                "Session write error for client ID " + std::to_string(clientId_) + ": " + error.message());
            if (disconnectHandler_) {
                disconnectHandler_(shared_from_this()); 
            }
        }
    }
}

} 