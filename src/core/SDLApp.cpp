#include "core/SDLApp.h"
#include "ui/StyleManager.h"
#include "utils/Logger.h"
#include "ui/Icons.h"
#include <iostream>
#include <filesystem>  
#include "ui/FlowPanels.h"
#include "ui/panels/FileExplorerPanel.h"

namespace LocalTether::Core {

SDLApp* SDLApp::instance = nullptr;

SDLApp::SDLApp(const std::string& title, int width, int height)
    : title(title), width(width), height(height) {
    instance = this;
}

SDLApp::~SDLApp() {
    Cleanup();
    instance = nullptr;
}

bool SDLApp::Initialize() {
     
    std::cout << "Available SDL video drivers:" << std::endl;
    for (int i = 0; i < SDL_GetNumVideoDrivers(); i++) {
        std::cout << " - " << SDL_GetVideoDriver(i) << std::endl;
    }

    std::cout << "Environment:" << std::endl;
    std::cout << "  XDG_SESSION_TYPE: " << (getenv("XDG_SESSION_TYPE") ? getenv("XDG_SESSION_TYPE") : "not set") << std::endl;
    std::cout << "  WAYLAND_DISPLAY: " << (getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "not set") << std::endl;
    std::cout << "  DISPLAY: " << (getenv("DISPLAY") ? getenv("DISPLAY") : "not set") << std::endl;

    
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "Using video driver: " << SDL_GetCurrentVideoDriver() << std::endl;


    std::cout << "Using video driver: " << SDL_GetCurrentVideoDriver() << std::endl;


     
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    SDL_EventState(SDL_DROPBEGIN, SDL_ENABLE); 
    SDL_EventState(SDL_DROPCOMPLETE, SDL_ENABLE);

    os_drag_active_ = false;

    window = SDL_CreateWindow(title.c_str(),
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              width, height,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window) {
        SDL_Log("SDL_CreateWindow Error: %s", SDL_GetError());
        std::cout<<"sdl create window error"<< SDL_GetError()<<std::endl;
        SDL_Quit();
        return false;
    }
    SDL_ShowWindow(window);

    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        SDL_Log("SDL_GL_CreateContext Error: %s", SDL_GetError());

        std::cout<<"SDL_GL Crete context error"<< SDL_GetError()<<std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); 


    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        SDL_Log("Failed to initialize GLAD");
        std::cout<<"FAiled to initialize glad"<<std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO();
    
    std::string project_root_str = PROJECT_ROOT_DIR_CMAKE;
    std::filesystem::path project_root_path(project_root_str);
    
    std::filesystem::path primary_font_fs_path = project_root_path / "assets" / "fonts" / "IBMPlexMono-Regular.ttf";
    std::filesystem::path fa_font_fs_path = project_root_path / "assets" / "fonts" / "fa-solid-900.ttf";  

    float font_size = 18.0f;

    if (std::filesystem::exists(primary_font_fs_path)) {
        io->Fonts->AddFontFromFileTTF(primary_font_fs_path.string().c_str(), font_size);  
        Utils::Logger::GetInstance().Info("Loaded primary font: " + primary_font_fs_path.string());
    } else {
        Utils::Logger::GetInstance().Error("Primary font not found at: " + primary_font_fs_path.string() + ". Using ImGui default.");
        io->Fonts->AddFontDefault();  
    }

     
    if (std::filesystem::exists(fa_font_fs_path)) {
        ImFontConfig font_config;
        font_config.MergeMode = true;  
        font_config.PixelSnapH = true;
         
        
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        
        io->Fonts->AddFontFromFileTTF(fa_font_fs_path.string().c_str(), font_size * 0.85f, &font_config, icons_ranges);
        Utils::Logger::GetInstance().Info("Loaded and merged icon font: " + fa_font_fs_path.string());
    } else {
        Utils::Logger::GetInstance().Error("Icon font (Font Awesome) not found at: " + fa_font_fs_path.string());
    }
    
     
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io->ConfigViewportsNoAutoMerge = true;
    io->ConfigViewportsNoTaskBarIcon = false;


    UI::StyleManager::SetupModernStyle();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 150");

    running = true;
    return true;
}

void SDLApp::Run() {
    if (!running) {
        if (!Initialize()) {
            Utils::Logger::GetInstance().Error("Failed to initialize SDLApp");
            return;
        }
    }

    while (running) {
         
        if (os_drag_active_) {
            int mouse_x, mouse_y;
            SDL_GetMouseState(&mouse_x, &mouse_y);
            ImVec2 current_mouse_pos = ImVec2(static_cast<float>(mouse_x), static_cast<float>(mouse_y));
             
            auto& fep = LocalTether::UI::Flow::GetFileExplorerPanelInstance();
            fep.HandleExternalFileDragOver(current_mouse_pos);
        } else {
             
            auto& fep = LocalTether::UI::Flow::GetFileExplorerPanelInstance();
            fep.ClearExternalDragState();
        }

        ProcessEvents();  
        StartFrame();     
        
        if (renderCallback) {
            renderCallback();  
        }
        
        Render();  
    }
    
    Cleanup();
}

void SDLApp::ProcessEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT)
            running = false;
        if (event.type == SDL_WINDOWEVENT && 
            event.window.event == SDL_WINDOWEVENT_CLOSE && 
            event.window.windowID == SDL_GetWindowID(window))
            running = false;
        
        if (event.type == SDL_DROPBEGIN) {
            Utils::Logger::GetInstance().Debug("Drag begin over window.");
            os_drag_active_ = true;
             
             
            int mouse_x, mouse_y;
            SDL_GetMouseState(&mouse_x, &mouse_y);  
            auto& fep = LocalTether::UI::Flow::GetFileExplorerPanelInstance();
            fep.HandleExternalFileDragOver(ImVec2(static_cast<float>(mouse_x), static_cast<float>(mouse_y)));

        } else if (event.type == SDL_DROPFILE) {
            char* dropped_file_sdl = event.drop.file; 
            std::string dropped_file_path_str = dropped_file_sdl;
            
             
             
             
             

            Utils::Logger::GetInstance().Info("File dropped: " + dropped_file_path_str);

            auto& fep = LocalTether::UI::Flow::GetFileExplorerPanelInstance();
            fep.HandleExternalFileDrop(dropped_file_path_str);  

            SDL_free(dropped_file_sdl); 
            os_drag_active_ = false;  
            fep.ClearExternalDragState();  

        } else if (event.type == SDL_DROPCOMPLETE) {
            Utils::Logger::GetInstance().Debug("Drag complete over window (SDL_DROPCOMPLETE).");
            os_drag_active_ = false;
            auto& fep = LocalTether::UI::Flow::GetFileExplorerPanelInstance();
            fep.ClearExternalDragState();
        }
    }
}

void SDLApp::StartFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void SDLApp::Render() {
    ImGui::Render();

    int display_w, display_h;
    SDL_GetWindowSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f); 
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
    }

    SDL_GL_SwapWindow(window);
}

void SDLApp::Cleanup() {
    if (gl_context) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        
        SDL_GL_DeleteContext(gl_context);
        gl_context = nullptr;
    }
    
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    
    SDL_Quit();
}

}
