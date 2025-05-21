#include "core/SDLApp.h"
#include "ui/StyleManager.h"
#include "utils/Logger.h"
#include <iostream>


namespace LocalTether::Core {

// Initialize static instance
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

    // Check your display environment
    std::cout << "Environment:" << std::endl;
    std::cout << "  XDG_SESSION_TYPE: " << (getenv("XDG_SESSION_TYPE") ? getenv("XDG_SESSION_TYPE") : "not set") << std::endl;
    std::cout << "  WAYLAND_DISPLAY: " << (getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "not set") << std::endl;
    std::cout << "  DISPLAY: " << (getenv("DISPLAY") ? getenv("DISPLAY") : "not set") << std::endl;

    // Initialize SDL - let it choose the best driver automatically
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Print which driver SDL chose
    std::cout << "Using video driver: " << SDL_GetCurrentVideoDriver() << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Print the driver being used
    std::cout << "Using video driver: " << SDL_GetCurrentVideoDriver() << std::endl;
    // Request OpenGL 3.2 Core context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Create window with maximized flag for desktop-like appearance
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
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // --- glad initialization ---
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        SDL_Log("Failed to initialize GLAD");
        std::cout<<"FAiled to initialize glad"<<std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }

    // --- ImGui initialization ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO();
    
    // Set a nicer default font with proper size for DPI awareness
    #ifdef _WIN32
    io->Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    #else
        io->Fonts->AddFontDefault();

    #endif
    
    // Enable docking and viewports
    io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io->ConfigViewportsNoAutoMerge = true;
    io->ConfigViewportsNoTaskBarIcon = false;

    // Apply modern style
    UI::StyleManager::SetupModernStyle();

    // Setup platform/renderer backends
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
    
    // Main application loop
    while (running) {
        ProcessEvents();
        StartFrame();
        
        // Call user-defined render function
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

    // Update and Render additional Platform Windows
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

} // namespace LocalTether::Core
