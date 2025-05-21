#include "core/SDLApp.h"
#include "ui/StyleManager.h"
#include "ui/DockspaceManager.h"
#include "ui/panels/ConsolePanel.h"
#include "ui/panels/FileExplorerPanel.h"
#include "ui/panels/NetworkSettingsPanel.h"
#include "ui/panels/PropertiesPanel.h"
#include "utils/Logger.h"

#include <boost/asio.hpp>
#include <openssl/ssl.h>

#ifdef _WIN32
#define _WIN32_WINNT 0x0601
#else
#include <polkit/polkit.h>
#endif

using namespace LocalTether;

// Global panel flags
bool show_example_panel = true;
bool show_network_settings = true;
bool show_file_explorer = true;
bool show_console = true;
bool show_properties = true;

int main(int argc, char** argv) {
    // Create and initialize the application
    Core::SDLApp app("LocalTether");
    if (!app.Initialize()) {
        return -1;
    }
    
    // Initialize libraries
    boost::asio::io_context io_context;
    SSL_library_init();

#ifndef _WIN32
    Utils::Logger::GetInstance().Info("PolKit ready");
#endif

    // Create UI panels
    UI::DockspaceManager dockspaceManager;
    UI::Panels::ConsolePanel consolePanel;
    UI::Panels::FileExplorerPanel fileExplorerPanel;
    UI::Panels::NetworkSettingsPanel networkSettingsPanel;
    UI::Panels::PropertiesPanel propertiesPanel;
    
    // Add initial log messages
    consolePanel.AddLogMessage("Application started");
    consolePanel.AddLogMessage("Initialized ImGui with docking support");
    consolePanel.AddLogMessage("Default layout created");

    // Set up the render callback
    app.SetRenderCallback([&]() {
        // Create the dockspace
        dockspaceManager.CreateDockspace(&app.IsRunning());

        // Show panels
        if (show_file_explorer)
            fileExplorerPanel.Show(&show_file_explorer);
        
        if (show_console)
            consolePanel.Show(&show_console);
        
        if (show_properties)
            propertiesPanel.Show(&show_properties);
        
        if (show_network_settings)
            networkSettingsPanel.Show(&show_network_settings);

        // Example panel
        if (show_example_panel) {
            ImGui::Begin("Example Panel", &show_example_panel);
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