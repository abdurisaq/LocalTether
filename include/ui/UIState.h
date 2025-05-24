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

    extern std::mutex g_mutex;

    enum class AppMode {
        None,
        HostSetup,
        JoinSetup,
        Connecting,
        ConnectedAsHost,
        ConnectedAsClient
    };
    extern AppMode app_mode;

    LocalTether::Network::Client& getClient();
    LocalTether::Network::Server& getServer();
}