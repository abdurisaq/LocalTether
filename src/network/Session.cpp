 
#include "network/Session.h"
#include "network/Server.h"  
#include "utils/Logger.h"

namespace LocalTether::Network {

Session::Session(asio::ip::tcp::socket tcp_socket, Server* server, uint32_t clientId, asio::ssl::context& ssl_context)
    : socket_(std::move(tcp_socket), ssl_context),  
      server_(server),
      clientId_(clientId) {
    try {
         
        asio::error_code ec;
        asio::ip::tcp::endpoint endpoint = socket_.lowest_layer().remote_endpoint(ec);
        if (!ec) {
            remoteAddressString_ = endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
        } else {
            remoteAddressString_ = "unknown (error: " + ec.message() + ")";
        }
    } catch (const std::system_error& e) {
        remoteAddressString_ = "unknown (exception: " + std::string(e.what()) + ")";
    }
    LocalTether::Utils::Logger::GetInstance().Info(
        "Session created for Client ID " + std::to_string(clientId_) + " at " + remoteAddressString_);
}

Session::~Session() {
    LocalTether::Utils::Logger::GetInstance().Debug(
        "Session destroyed for Client ID " + std::to_string(clientId_) + " (" + remoteAddressString_ + ")");
     
    if (active_.load()) {
        doClose("session destructor");
    }
}

std::string Session::getClientAddress() const {
    return remoteAddressString_;
}

std::string Session::getRoleString() const {
    switch (role_) {
        case ClientRole::Host: return "Host";
        case ClientRole::Broadcaster: return "Broadcaster";
        case ClientRole::Receiver: return "Receiver";
        default: return "UnknownRole";
    }
}

void Session::start(MessageHandler msgHandler, DisconnectHandler discHandler) {
    messageHandler_ = std::move(msgHandler);
    disconnectHandler_ = std::move(discHandler);
    
    active_.store(true);  
    doSslHandshake();
}

void Session::doSslHandshake() {
    auto self = shared_from_this();
    socket_.async_handshake(asio::ssl::stream_base::server,
        [this, self](const std::error_code& error) {
            handleSslHandshake(error);
        });
}

void Session::handleSslHandshake(const std::error_code& error) {
    if (!active_.load()) return;  

    if (!error) {
        sslHandshakeComplete_.store(true);
        LocalTether::Utils::Logger::GetInstance().Info(
            "SSL handshake successful for Client ID " + std::to_string(clientId_) + " (" + remoteAddressString_ + ")");
        performApplicationHandshake();  
    } else {
        LocalTether::Utils::Logger::GetInstance().Error(
            "SSL handshake failed for Client ID " + std::to_string(clientId_) + " (" + remoteAddressString_ + "): " + error.message());
        doClose("SSL handshake failed");
    }
}

void Session::performApplicationHandshake() {
     
     
     
    LocalTether::Utils::Logger::GetInstance().Debug(
        "Client ID " + std::to_string(clientId_) + " performing application handshake (waiting for client hello).");
    doRead();  
}


void Session::send(const Message& message) {
    if (!active_.load(std::memory_order_relaxed)) {
        LocalTether::Utils::Logger::GetInstance().Warning(
            "Attempted to send message on inactive session for Client ID " + std::to_string(clientId_));
        return;
    }
    if (!appHandshakeComplete_.load(std::memory_order_relaxed) && message.getType() != MessageType::Handshake) {
          
          
          
          
    }

    auto serialized_data = message.serialize();  

    auto self = shared_from_this();
    asio::post(socket_.get_executor(), [self, data = std::move(serialized_data)]() {
        if (!self->active_.load(std::memory_order_relaxed)) return;

        bool should_start_write = false;
        {
            std::lock_guard<std::mutex> lock(self->writeMutex_);
            should_start_write = self->writeQueue_.empty() && !self->writing_.load(std::memory_order_relaxed);
            self->writeQueue_.push(std::move(data));
        }

        if (should_start_write) {
            self->doWrite();
        }
    });
}

void Session::doWrite() {
    if (!active_.load(std::memory_order_relaxed)) {
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

    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(data_to_send),
        [this, self, data_sent_size = data_to_send.size()](const std::error_code& error, size_t bytes_transferred) {
             
            handleWrite(error, bytes_transferred);
        });
}

void Session::handleWrite(const std::error_code& error, size_t /*bytes_transferred*/) {
     
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
            LocalTether::Utils::Logger::GetInstance().Error(
                "Session write error for Client ID " + std::to_string(clientId_) + " (" + remoteAddressString_ + "): " + error.message());
             
        }
    }  

    if (!error && should_continue_writing) {
        if (active_.load(std::memory_order_relaxed)) {  
             doWrite();
        } else {
             writing_ = false;  
        }
    } else if (error) {
          
          
         doClose("write error: " + error.message());
    }
}

void Session::doRead() {
    if (!active_.load()) return;

    auto self = shared_from_this();
     
    readBuffer_.resize(Message::HEADER_LENGTH);  

    asio::async_read(socket_, asio::buffer(readBuffer_.data(), Message::HEADER_LENGTH),
        [this, self](const std::error_code& error, size_t bytes_transferred) {
            handleReadHeader(error, bytes_transferred);
        });
}

void Session::handleReadHeader(const std::error_code& error, size_t bytes_transferred) {
    if (!active_.load()) return;

    if (!error && bytes_transferred == Message::HEADER_LENGTH) {
        currentReadMessage_.decodeHeader(reinterpret_cast<const uint8_t*>(readBuffer_.data()),readBuffer_.size());  
        LocalTether::Utils::Logger::GetInstance().Trace(
            "Client ID " + std::to_string(clientId_) + " received header. Type: " +
            Message::messageTypeToString(currentReadMessage_.getType()) + ", Body Size: " + std::to_string(currentReadMessage_.getBodySize()));

        if (currentReadMessage_.getBodySize() > 0) {
            if (currentReadMessage_.getBodySize() > Message::MAX_BODY_LENGTH) {  
                LocalTether::Utils::Logger::GetInstance().Error(
                    "Client ID " + std::to_string(clientId_) + " message body too large: " + std::to_string(currentReadMessage_.getBodySize()));
                doClose("message body too large");
                return;
            }
            readBuffer_.resize(currentReadMessage_.getBodySize());
            asio::async_read(socket_, asio::buffer(readBuffer_.data(), currentReadMessage_.getBodySize()),
                [this, self = shared_from_this()](const std::error_code& ec, size_t length) {
                    handleReadBody(ec, length);
                });
        } else {  
             
            if (sslHandshakeComplete_.load() && !appHandshakeComplete_.load() && currentReadMessage_.getType() == MessageType::Handshake) {
                appHandshakeComplete_.store(true);
                LocalTether::Utils::Logger::GetInstance().Info(
                    "Client ID " + std::to_string(clientId_) + " application handshake received (no body).");
                 
                if (messageHandler_) {
                    messageHandler_(shared_from_this(), currentReadMessage_);
                }
                 
                 
                if (active_.load()) doRead();  
            } else if (appHandshakeComplete_.load()) {
                if (messageHandler_) {
                    messageHandler_(shared_from_this(), currentReadMessage_);
                }
                if (active_.load()) doRead();  
            } else {
                 LocalTether::Utils::Logger::GetInstance().Warning(
                    "Client ID " + std::to_string(clientId_) + " received non-handshake message or unexpected state.");
                 doClose("unexpected message before app handshake completion");
            }
        }
    } else {
        if (error == asio::error::eof || error == asio::ssl::error::stream_truncated) {
            LocalTether::Utils::Logger::GetInstance().Info(
                "Client ID " + std::to_string(clientId_) + " (" + remoteAddressString_ + ") disconnected (EOF/SSL stream truncated during header read).");
        } else if (error != asio::error::operation_aborted) {  
            LocalTether::Utils::Logger::GetInstance().Error(
                "Session read header error for Client ID " + std::to_string(clientId_) + " (" + remoteAddressString_ + "): " + error.message());
        }
        doClose("read header error: " + error.message());
    }
}

void Session::handleReadBody(const std::error_code& error, size_t bytes_transferred) {
    if (!active_.load()) return;

    if (!error && bytes_transferred == currentReadMessage_.getBodySize()) {
        currentReadMessage_.setBody(reinterpret_cast<const uint8_t*>(readBuffer_.data()), currentReadMessage_.getBodySize());
        LocalTether::Utils::Logger::GetInstance().Trace(
            "Client ID " + std::to_string(clientId_) + " received body for type: " + Message::messageTypeToString(currentReadMessage_.getType()));

         
        if (sslHandshakeComplete_.load() && !appHandshakeComplete_.load() && currentReadMessage_.getType() == MessageType::Handshake) {
            appHandshakeComplete_.store(true);
            LocalTether::Utils::Logger::GetInstance().Info(
                "Client ID " + std::to_string(clientId_) + " application handshake received (with body).");
             
            if (messageHandler_) {
                messageHandler_(shared_from_this(), currentReadMessage_);
            }
             
             
            if (active_.load()) doRead();
        } else if (appHandshakeComplete_.load()) {
             if (messageHandler_) {
                 messageHandler_(shared_from_this(), currentReadMessage_);
             }
             if (active_.load()) doRead();  
        } else {
             LocalTether::Utils::Logger::GetInstance().Warning(
                 "Client ID " + std::to_string(clientId_) + " received non-handshake message or unexpected state (body).");
             doClose("unexpected message before app handshake completion (body)");
        }
    } else {
        if (error == asio::error::eof || error == asio::ssl::error::stream_truncated) {
            LocalTether::Utils::Logger::GetInstance().Info(
                "Client ID " + std::to_string(clientId_) + " (" + remoteAddressString_ + ") disconnected (EOF/SSL stream truncated during body read).");
        } else if (error != asio::error::operation_aborted) {
            LocalTether::Utils::Logger::GetInstance().Error(
                "Session read body error for Client ID " + std::to_string(clientId_) + " (" + remoteAddressString_ + "): " + error.message());
        } else if (!error && bytes_transferred != currentReadMessage_.getBodySize()) {
             LocalTether::Utils::Logger::GetInstance().Error(
                "Session read body error for Client ID " + std::to_string(clientId_) + " (" + remoteAddressString_ + 
                "): Incomplete body read. Expected " + std::to_string(currentReadMessage_.getBodySize()) + 
                ", got " + std::to_string(bytes_transferred));
        }
        doClose("read body error: " + error.message());
    }
}

void Session::close() {  
     
     
    asio::post(socket_.get_executor(), [self = shared_from_this()]() {
        self->doClose("explicit close called");
    });
}

void Session::doClose(const std::string& reason) {
    if (!active_.exchange(false)) {  
         
        return;
    }

    LocalTether::Utils::Logger::GetInstance().Info(
        "Closing session for Client ID " + std::to_string(clientId_) + " (" + remoteAddressString_ + "). Reason: " + reason);

    asio::error_code ec;
     
    if (sslHandshakeComplete_.load()) {
        socket_.async_shutdown(  
            [this, self = shared_from_this()](const std::error_code& shutdown_ec) {
                if (shutdown_ec && shutdown_ec != asio::error::eof && shutdown_ec != asio::ssl::error::stream_truncated) {
                     
                    LocalTether::Utils::Logger::GetInstance().Warning(
                        "SSL shutdown error for Client ID " + std::to_string(clientId_) + ": " + shutdown_ec.message());
                }
                 
                asio::error_code close_ec;
                socket_.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, close_ec);
                socket_.lowest_layer().close(close_ec);
                
                 
                if (disconnectHandler_) {
                    disconnectHandler_(shared_from_this());
                }
            });
    } else {
         
        socket_.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket_.lowest_layer().close(ec);
        if (disconnectHandler_) {
            disconnectHandler_(shared_from_this());
        }
    }
    
     
    {
        std::lock_guard<std::mutex> lock(writeMutex_);
        std::queue<std::vector<uint8_t>> emptyQueue;
        std::swap(writeQueue_, emptyQueue);
        writing_ = false;
    }
    appHandshakeComplete_.store(false);
    sslHandshakeComplete_.store(false);
}

}  