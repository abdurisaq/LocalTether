#include "core/SDLApp.h"
#include "ui/StyleManager.h"
#include "ui/DockspaceManager.h"
#include "ui/UIState.h"
#include "ui/panels/ConsolePanel.h"
#include "ui/panels/FileExplorerPanel.h"
#include "ui/panels/NetworkSettingsPanel.h"
#include "ui/panels/PropertiesPanel.h"
#include "utils/Logger.h"

// OpenSSL and other external includes after your UI headers
#include <boost/asio.hpp>
#include <openssl/ssl.h>

#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#else
#include <polkit/polkit.h>
#endif



// Don't use "using namespace LocalTether" to avoid ambiguity
// Create an alias instead for shorter code
namespace LT = LocalTether;

int main(int argc, char** argv) {
    // Create and initialize the application
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

    // Create UI panels - fully qualify with LT:: prefix
    LT::UI::DockspaceManager dockspaceManager;
    LT::UI::Panels::ConsolePanel consolePanel;
    LT::UI::Panels::FileExplorerPanel fileExplorerPanel;
    LT::UI::Panels::NetworkSettingsPanel networkSettingsPanel;
    LT::UI::Panels::PropertiesPanel propertiesPanel;
    
    // Add initial log messages
    consolePanel.AddLogMessage("Application started");
    consolePanel.AddLogMessage("Initialized ImGui with docking support");
    consolePanel.AddLogMessage("Default layout created");

    // Set up the render callback
    app.SetRenderCallback([&]() {
        // Create the dockspace
        bool running = app.IsRunning();
        dockspaceManager.CreateDockspace(&running);

        // Show panels
        if (LT::UI::show_file_explorer)
            fileExplorerPanel.Show(&LT::UI::show_file_explorer);
        
        if (LT::UI::show_console)
            consolePanel.Show(&LT::UI::show_console);
        
        if (LT::UI::show_properties)
            propertiesPanel.Show(&LT::UI::show_properties);
        
        if (LT::UI::show_network_settings)
            networkSettingsPanel.Show(&LT::UI::show_network_settings);

        // Example panel
        if (LT::UI::show_example_panel) {
            ImGui::Begin("Example Panel", &LT::UI::show_example_panel);
            ImGui::Text("Boost.Asio io_context is %s", io_context.stopped() ? "stopped" : "running");
            ImGui::Text("This window can be dragged and docked.");
            
            ImGui::Separator();
            
            ImGui::Text("Widgets Demo:");
            
            ImGui::Spacing();
            
            // Basic widgets
            static float f = 0.0f;
            static int counter = 0;
            ImGui::Text("Hello, world!");
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            
            if (ImGui::Button("Button")) {
                counter++;
                consolePanel.AddLogMessage("Button clicked " + std::to_string(counter) + " times");
            }
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);
            
            ImGui::Spacing();
            
            // More widgets
            static float value = 0.5f;
            ImGui::ProgressBar(value, ImVec2(-1, 0));
            if (ImGui::Button("Increment")) {
                value = (value + 0.1f > 1.0f) ? 0.0f : value + 0.1f;
            }
            
            static bool check = true;
            ImGui::Checkbox("Checkbox", &check);
            
            static int radio_value = 0;
            ImGui::RadioButton("Radio 1", &radio_value, 0); ImGui::SameLine();
            ImGui::RadioButton("Radio 2", &radio_value, 1); ImGui::SameLine();
            ImGui::RadioButton("Radio 3", &radio_value, 2);
            
            static int combo_selected = 0;
            const char* combo_items[] = { "Option 1", "Option 2", "Option 3", "Option 4" };
            ImGui::Combo("Combo", &combo_selected, combo_items, IM_ARRAYSIZE(combo_items));
            
            ImGui::End();
        }
    });

    // Start the main loop
    app.Run();

    return 0;
}