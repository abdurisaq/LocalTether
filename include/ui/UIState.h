#pragma once
#include <asio.hpp>
#include <mutex>
#include <memory>  

// Forward declarations
namespace LocalTether::Network {
    class Client;
    class Server;
}

namespace LocalTether::UI {
    
    extern bool show_example_panel;
    extern bool show_network_settings;
    extern bool show_file_explorer;
    extern bool show_console;
    extern bool show_properties;
    extern bool show_pause_settings;

    extern std::mutex g_mutex;

    enum class AppMode {
        None,
        HostSetup,
        GeneratingServerAssets,
        JoinSetup,
        Connecting,
        ConnectedAsHost,
        ConnectedAsClient
    };
    extern AppMode app_mode;

    LocalTether::Network::Client& getClient();
    LocalTether::Network::Server& getServer();

    void resetServerInstance();
    void resetClientInstance();
    LocalTether::Network::Server* getServerPtr();

    void initializeNetwork();
    void cleanupNetwork();


    extern std::atomic<bool> server_setup_in_progress;
    extern std::atomic<bool> server_setup_success;
    extern std::string server_setup_error_message;
    extern std::mutex server_setup_mutex;
    extern std::thread server_setup_thread;
}