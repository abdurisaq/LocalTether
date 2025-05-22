#pragma once

namespace LocalTether::UI {
    enum class AppMode;
    extern AppMode app_mode;
}

namespace LocalTether::UI::Flow {

void ShowHomePanel(); 
void ShowHostSetupPanel();
void ShowJoinSetupPanel();
void ShowConnectingModal();
void ShowHostDashboard(); 
void ShowClientDashboard();

}