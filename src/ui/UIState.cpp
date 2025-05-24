#include "ui/UIState.h"
#include "network/Client.h" 
#include "network/Server.h" 
#include "utils/Logger.h"
#include <thread>

namespace LocalTether::UI {
    


    bool show_example_panel = true;
    bool show_network_settings = true;
    bool show_file_explorer = true;
    bool show_console = true;
    bool show_properties = true;
    std::mutex g_mutex;
    AppMode app_mode = AppMode::None;
    
    
    namespace {
        static asio::io_context io_context;
        static std::unique_ptr<Network::Client> client;
        static std::unique_ptr<Network::Server> server;
        static std::thread io_thread;
        static asio::executor_work_guard<asio::io_context::executor_type> work_guard 
            = asio::make_work_guard(io_context);
        static bool initialized = false;
    }
    
    
    void initializeNetwork() {
        if (!initialized) {
            
            io_thread = std::thread([]() {
                try {
                    io_context.run();
                }
                catch (const std::exception& e) {
                    
                    Utils::Logger::GetInstance().Error(
                        "Network thread error: " + std::string(e.what()));
                }
            });
            io_thread.detach();
            initialized = true;
        }
    }
    
    // Client and server getters
    Network::Client& getClient() {
        if (!initialized) {
            initializeNetwork();
        }
        
        if (!client) {
            client = std::make_unique<Network::Client>(io_context);
        }
        return *client;
    }
    
    Network::Server& getServer() {
        if (!initialized) {
            initializeNetwork();
        }
        
        if (!server) {
            server = std::make_unique<Network::Server>(io_context, 8080);
        }
        return *server;
    }
    
    // Clean up 
    void cleanupNetwork() {
        client.reset();
        server.reset();
        work_guard.reset();
        io_context.stop();
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }
}