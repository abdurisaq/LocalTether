#include "ui/UIState.h"

namespace LocalTether::UI {
    
    bool show_example_panel = true;
    bool show_network_settings = true;
    bool show_file_explorer = true;
    bool show_console = true;
    bool show_properties = true;
    std::mutex g_mutex;
    AppMode app_mode = AppMode::None;
    
}