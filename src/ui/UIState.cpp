#include "ui/UIState.h"
#include "network/Client.h" 
#include "network/Server.h" 
#include "utils/Logger.h"
#include <thread>
 
 
#include "core/SDLApp.h"  

namespace LocalTether::UI {
    
     
    bool show_example_panel = true;
    bool show_network_settings = true;
    bool show_file_explorer = true;
    bool show_console = true;
    bool show_properties = true;
    bool show_controls_panel = true;

    std::atomic<bool> server_setup_in_progress{false};
    std::atomic<bool> server_setup_success{false};
    std::string server_setup_error_message;
    std::mutex server_setup_mutex;
    std::thread server_setup_thread;

    std::mutex g_mutex;
    AppMode app_mode = AppMode::None;
    
     
     
     
     
     
    static asio::io_context io_context_instance;  
    static std::unique_ptr<Network::Client> client_instance;
    static std::unique_ptr<Network::Server> server_instance;
    static std::thread io_thread_instance;
    static asio::executor_work_guard<asio::io_context::executor_type> work_guard_instance
        = asio::make_work_guard(io_context_instance);
    static bool network_initialized = false;
    
    
    void initializeNetwork() {
        Utils::Logger::GetInstance().Info("Initializing network resources...");
        if (!network_initialized) {
            io_thread_instance = std::thread([]() {
                try {
                    Utils::Logger::GetInstance().Info("Network io_context thread started.");
                    io_context_instance.run();
                    Utils::Logger::GetInstance().Info("Network io_context thread finished.");
                }
                catch (const std::exception& e) {
                    Utils::Logger::GetInstance().Error(
                        "Network thread error: " + std::string(e.what()));
                }
            });
             
             
             
             
            network_initialized = true;
        }
    }
    
    Network::Client& getClient() {
        if (!network_initialized) {
            initializeNetwork();
        }
        
        if (!client_instance) {
            client_instance = std::make_unique<Network::Client>(io_context_instance);
        }
        return *client_instance;
    }
    
     
    LocalTether::Network::Server* getServerPtr() {
        return server_instance.get();
    }

     
    void resetServerInstance() {
        if (server_instance) {
            Utils::Logger::GetInstance().Info("Resetting server instance.");
            if (server_instance->getState() != Network::ServerState::Stopped) {
                try {
                    server_instance->stop(); 
                } catch (const std::exception& e) {
                    Utils::Logger::GetInstance().Error("Exception while stopping server during reset: " + std::string(e.what()));
                }
            }
            server_instance.reset(); 
        }
    }
    void resetClientInstance() {
    if (client_instance) {
        LocalTether::Utils::Logger::GetInstance().Info("Resetting client instance.");
        client_instance.reset();
    }
}

    LocalTether::Network::Server& getServer() {
        if (!network_initialized) {
            initializeNetwork();
        }
        if (!server_instance) {
            Utils::Logger::GetInstance().Info("Creating new Server instance.");
          
            Core::SDLApp& appContext = Core::SDLApp::GetInstance();  
             

            if (false) {  
                 Utils::Logger::GetInstance().Critical("Application context is null, cannot create server.");
                  
                 throw std::runtime_error("Application context is null, cannot create server.");
            }
             
            server_instance = std::make_unique<Network::Server>(io_context_instance, 8080);
        }
        return *server_instance;
    }
    
    void cleanupNetwork() {
        Utils::Logger::GetInstance().Info("Cleaning up network resources...");
        client_instance.reset();
        server_instance.reset();
        
        work_guard_instance.reset();  
        if (io_context_instance.stopped()) {
             Utils::Logger::GetInstance().Info("io_context already stopped.");
        } else {
            io_context_instance.stop();
             Utils::Logger::GetInstance().Info("io_context stopped.");
        }

        if (io_thread_instance.joinable()) {
            Utils::Logger::GetInstance().Info("Joining network thread...");
            io_thread_instance.join();
            Utils::Logger::GetInstance().Info("Network thread joined.");
        }
        network_initialized = false;  
         
         
         
    }
}