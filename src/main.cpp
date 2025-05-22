#include "core/SDLApp.h"
#include "ui/StyleManager.h"
#include "ui/DockspaceManager.h"
#include "ui/UIState.h"
#include "ui/FlowPanels.h"  
#include "ui/panels/ConsolePanel.h"
#include "ui/panels/FileExplorerPanel.h"
#include "ui/panels/NetworkSettingsPanel.h"
#include "ui/panels/PropertiesPanel.h"
#include "utils/Logger.h"
#include <iostream>

#include <boost/asio.hpp>
#include <openssl/ssl.h>

#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#else
#include <polkit/polkit.h>
#endif

namespace LT = LocalTether;

int main(int argc, char** argv) {
    LT::Core::SDLApp app("LocalTether");
    if (!app.Initialize()) {
        return -1;
    }
    
    // Initialize libraries
    boost::asio::io_context io_context;
    SSL_library_init();

#ifndef _WIN32
    LT::Utils::Logger::GetInstance().Info("PolKit ready");
#endif

    
    LT::UI::DockspaceManager dockspaceManager;
    LT::UI::Panels::ConsolePanel consolePanel;
    LT::UI::Panels::FileExplorerPanel fileExplorerPanel;
    LT::UI::Panels::NetworkSettingsPanel networkSettingsPanel;
    LT::UI::Panels::PropertiesPanel propertiesPanel;
    
    
    consolePanel.AddLogMessage("Application started");
    consolePanel.AddLogMessage("Initialized ImGui with docking support");
    consolePanel.AddLogMessage("Default layout created");

    LT::UI::app_mode = LT::UI::AppMode::None;
    app.SetRenderCallback([&]() {
        
        bool running = app.IsRunning();
        dockspaceManager.CreateDockspace(&running);

        switch (LT::UI::app_mode) {
            case LT::UI::AppMode::None:
                LT::UI::Flow::ShowHomePanel();
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
        }
    });

    
    app.Run();

    return 0;
}