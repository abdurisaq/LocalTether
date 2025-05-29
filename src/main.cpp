#include "core/SDLApp.h"
#include "ui/DockspaceManager.h"
#include "ui/UIState.h"
#include "ui/FlowPanels.h"  
#include "ui/panels/ConsolePanel.h"
#include "ui/panels/FileExplorerPanel.h"
#include "ui/panels/NetworkSettingsPanel.h"
#include "ui/panels/PropertiesPanel.h"
#include "utils/Logger.h"
#include "input/InputManager.h"
#include "input/LinuxInputHelper.h"

#define ASIO_ENABLE_SSL
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef _WIN32
// Windows-specific 
#define WIN32_LEAN_AND_MEAN  
#define _WIN32_WINNT 0x0601  
#include <windows.h>        
#else
// Linux-specific includes
#include <polkit/polkit.h>
#endif



namespace LT = LocalTether;

int main(int argc, char** argv) {
    #ifndef _WIN32
    if (argc > 1 && std::string(argv[1]) == "--input-helper-mode") {
        
        LocalTether::Utils::Logger::GetInstance().Log("Input helper mode starting", LocalTether::Utils::LogLevel::Info);
        return LocalTether::Input::runInputHelperMode(argc, argv); 
    }
    if (setenv("OPENSSL_CONF", "/dev/null", 1) != 0) {
        LT::Utils::Logger::GetInstance().Warning("Failed to set OPENSSL_CONF environment variable to /dev/null.");
    } else {
        LT::Utils::Logger::GetInstance().Debug("Set OPENSSL_CONF to /dev/null to prevent loading system OpenSSL config.");
    }
    #endif

    LT::Core::SDLApp app("LocalTether");
    if (!app.Initialize()) {
        return -1;
    }
    
    asio::io_context io_context;
    if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL) == 0) {
        LT::Utils::Logger::GetInstance().Critical("Failed to initialize OpenSSL library.");
        unsigned long err_code;
        char err_buf[256];
        while ((err_code = ERR_get_error()) != 0) {
            ERR_error_string_n(err_code, err_buf, sizeof(err_buf));
            LT::Utils::Logger::GetInstance().Error("OpenSSL Init Error: " + std::string(err_buf));
        }
        return -1; 
    } else {
        LT::Utils::Logger::GetInstance().Info("OpenSSL library initialized successfully.");
    }

#ifndef _WIN32
    LT::Utils::Logger::GetInstance().Info("PolKit ready");
#endif

    
    LT::UI::DockspaceManager dockspaceManager;
    LT::UI::Panels::ConsolePanel consolePanel;
    LT::UI::Panels::FileExplorerPanel fileExplorerPanel;
    LT::UI::Panels::NetworkSettingsPanel networkSettingsPanel;
    LT::UI::Panels::PropertiesPanel propertiesPanel;

    
    LocalTether::Utils::Logger::GetInstance().Info("--- Application Main Started ---");

    LT::UI::app_mode = LT::UI::AppMode::None;
    app.SetRenderCallback([&]() {
        
        bool running = app.IsRunning();
        dockspaceManager.CreateDockspace(&running);

        switch (LT::UI::app_mode) {

            case LT::UI::AppMode::None:
                LT::UI::Flow::ShowHomePanel();
                break;
            
            case LT::UI::AppMode::Connecting:
                
                break;
            case LT::UI::AppMode::HostSetup:
                LT::UI::Flow::ShowHostSetupPanel();
                break;
            case LT::UI::AppMode::JoinSetup:
                LT::UI::Flow::ShowJoinSetupPanel();
                break;
            case LT::UI::AppMode::ConnectedAsHost:
                LT::UI::Flow::ShowHostDashboard();
                break;
            case LT::UI::AppMode::ConnectedAsClient:
                LT::UI::Flow::ShowClientDashboard();
                break;
            case LT::UI::AppMode::GeneratingServerAssets:
                LT::UI::Flow::ShowGeneratingServerAssetsPanel();
                break;
        }
    });

    
    app.Run();

    return 0;
}
