#pragma once

namespace LocalTether::UI {
    enum class AppMode;
    extern AppMode app_mode;
    namespace Panels { class FileExplorerPanel; } 
}

namespace LocalTether::UI::Flow {

void ShowHomePanel(); 
void ShowHostSetupPanel();
void ShowJoinSetupPanel();
void ShowConnectingModal();
void ShowHostDashboard(); 
void ShowClientDashboard();
void ShowGeneratingServerAssetsPanel();
LocalTether::UI::Panels::FileExplorerPanel& GetFileExplorerPanelInstance();

}