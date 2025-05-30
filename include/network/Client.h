#pragma once

#include "Message.h"
#include "utils/Logger.h"
#include "input/InputManager.h"  
#include <optional>
#include <thread>  
#include <atomic>  
#define ASIO_ENABLE_SSL
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <queue>
#include <utils/KeycodeConverter.h>
#include <vector> 
#include <cstdint>

#include <cereal/archives/binary.hpp> 
#include <sstream>
#include "ui/FlowPanels.h" 
#include "ui/panels/FileExplorerPanel.h"

namespace LocalTether::Network {

 
enum class ClientState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

 
 


class Client {
public:
    using ConnectHandler = std::function<void(bool success, const std::string& message, uint32_t assignedId)>;
    using DisconnectHandler = std::function<void(const std::string& reason)>;
    using MessageHandler = std::function<void(const Message& message)>;
    using ErrorHandler = std::function<void(const std::error_code& ec)>;

    Client(asio::io_context& io_context);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    void connect(const std::string& host, uint16_t port,
                 ClientRole role, const std::string& name,
                 const std::string& password_param); 

    void disconnect(const std::string& reason = "user disconnected");
    void send(const Message& msg);

    ClientState getState() const { return state_.load(); }
    std::string getLastError() const { return lastError_; }
    uint32_t getClientId() const { return clientId_; }
    ClientRole getRole() const { return role_; }
    uint16_t getHostScreenWidth() const { return hostScreenWidth_; }
    uint16_t getHostScreenHeight() const { return hostScreenHeight_; }


    void setConnectHandler(ConnectHandler handler) { connectHandler_ = std::move(handler); }
    void setDisconnectHandler(DisconnectHandler handler) { disconnectHandler_ = std::move(handler); }
    void setMessageHandler(MessageHandler handler) { messageHandler_ = std::move(handler); }
    void setErrorHandler(ErrorHandler handler) { errorHandler_ = std::move(handler); }

     
    void sendInput(const InputPayload& payload);
    void sendChatMessage(const std::string& chatMessage);
    void sendCommand(const std::string& command);
    void requestFile(const std::string& filename);

    LocalTether::Input::InputManager* getInputManager() const;


    void uploadFile(const std::string& localFilePath, const std::string& serverRelativePath, const std::string& fileNameOnServer);
    void handleFileResponse(const Message& msg);

private:

    void initializeLocalScreenDimensions();
    void setState(ClientState newState, const std::optional<std::error_code>& ec = std::nullopt);
    void doResolve();
    void handleResolve(const std::error_code& ec, const asio::ip::tcp::resolver::results_type& endpoints);
    void handleTcpConnect(const std::error_code& error, const asio::ip::tcp::endpoint& endpoint);
    
    void handleFileError(const Message& msg);

    void doSslHandshake();
    void handleSslHandshake(const std::error_code& error);

    void performApplicationHandshake();
     
     

    void doRead();
    void handleRead(const std::error_code& error, size_t bytes_transferred);
    void handleMessage(const Message& message);  

    void doWrite();
    void handleWrite(const std::error_code& error, size_t bytes_transferred);

    void doClose(const std::string& reason, bool notifyDisconnectHandler);

     
    void startInputLogging();
    void stopInputLogging();
    void inputLoop();

    asio::io_context& io_context_;
    asio::ip::tcp::resolver resolver_;
    
    std::optional<asio::ssl::context> ssl_context_opt_;
    std::optional<asio::ssl::stream<asio::ip::tcp::socket>> socket_opt_;

    std::string currentHost_;
    uint16_t currentPort_{0};
    std::atomic<ClientState> state_{ClientState::Disconnected};
    std::string lastError_;

    uint32_t clientId_{0}; 
    ClientRole role_{ClientRole::Receiver};
    std::string clientName_{"User"};
    std::string password_;
    
    std::vector<char> readBuffer_; 
    Message currentReadMessage_;      
    std::vector<uint8_t> partialMessage_;

    std::queue<std::vector<uint8_t>> writeQueue_;
    std::mutex writeMutex_;
    std::atomic<bool> writing_{false};

    ConnectHandler connectHandler_;
    DisconnectHandler disconnectHandler_;
    MessageHandler messageHandler_;  
    ErrorHandler errorHandler_; 

     
    std::unique_ptr<LocalTether::Input::InputManager> inputManager_;
    std::thread inputThread_;
    std::atomic<bool> loggingInput_{false};
    uint16_t localScreenWidth_{0};   
    uint16_t localScreenHeight_{0};  
    uint16_t hostScreenWidth_{0};    
    uint16_t hostScreenHeight_{0};   


      

};

}  