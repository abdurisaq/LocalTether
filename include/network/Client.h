#pragma once

#include "Message.h"
#include <asio.hpp>
#include <memory>
#include <string>
#include <functional>
#include <queue>
#include <mutex>
#include <atomic>
#include <thread>
#include "input/InputManager.h"

namespace LocalTether::Network {

enum class ClientState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

class Client {
public:
    using ConnectHandler = std::function<void()>;
    using MessageHandler = std::function<void(const Message&)>;
    using DisconnectHandler = std::function<void()>;
    using ErrorHandler = std::function<void(const std::error_code&)>;

    Client(asio::io_context& io_context);
    ~Client();

    // Connection management
    void connect(const std::string& host, uint16_t port, 
                ClientRole role = ClientRole::Receiver,
                const std::string& name = "User",
                const std::string& password = "");
    void disconnect();
    
    // Send messages
    void sendInput(const std::vector<uint8_t>& inputData);
    void sendInput(const InputPayload& payload);
    void sendChat(const std::string& text);
    void sendCommand(const std::string& command);
    void requestFile(const std::string& filename);
    
    // Status information
    ClientState getState() const { return state_; }
    std::string getErrorMessage() const { return lastError_; }
    std::string getConnectedHost() const { return currentHost_; }
    uint16_t getConnectedPort() const { return currentPort_; }
    ClientRole getRole() const { return role_; }
    
    // Set event handlers
    void setConnectHandler(ConnectHandler handler);
    void setMessageHandler(MessageHandler handler);
    void setDisconnectHandler(DisconnectHandler handler);
    void setErrorHandler(ErrorHandler handler);

private:
    void handleMessage(const Message& message);

    void doRead();
    void handleRead(const std::error_code& error, size_t bytes_transferred);

    void doWrite();
    void handleWrite(const std::error_code& error, size_t bytes_transferred);
    
    void handleConnect(const std::error_code& error);
    void performHandshake();

    void setState(ClientState newState, const std::error_code& error = std::error_code());


    std::unique_ptr<LocalTether::Input::InputManager> inputManager_;
    std::thread inputThread_;
    std::atomic<bool> loggingInput_{false};

    void inputLoop(); 
    void startInputLogging();
    void stopInputLogging();
    
    // Member variables
    asio::io_context& io_context_;
    asio::ip::tcp::socket socket_;

    std::string currentHost_;
    uint16_t currentPort_{0};
    std::atomic<ClientState> state_{ClientState::Disconnected};
    std::string lastError_;

    uint32_t clientId_{0};
    ClientRole role_{ClientRole::Receiver};
    std::string clientName_{"User"};
    std::string password_;
    
    std::array<char, 8192> readBuffer_;
    std::vector<uint8_t> partialMessage_;
    std::queue<std::vector<uint8_t>> writeQueue_;
    std::mutex writeMutex_;
    std::atomic<bool> writing_{false};

    ConnectHandler connectHandler_;
    MessageHandler messageHandler_;
    DisconnectHandler disconnectHandler_;
    ErrorHandler errorHandler_;
};

} 