#pragma once

#include <SDL.h>
#include <glad/glad.h>
#include <string>
#include <functional>
#include <memory>
#include "imgui_include.h"

namespace LocalTether::Core {

class SDLApp {
public:
    SDLApp(const std::string& title, int width = 1280, int height = 720);
    ~SDLApp();

    bool Initialize();
    
    void Run();
    
    bool IsRunning() const { return running; }
    

    void Quit() { running = false; }
    

    void SetRenderCallback(std::function<void()> callback) { renderCallback = callback; }
    
    ImGuiIO& GetIO() { return *io; }

    SDL_Window* GetWindow() { return window; }

    SDL_GLContext GetGLContext() { return gl_context; }

    static SDLApp& GetInstance() { return *instance; }

private:
    // Handle SDL events
    void ProcessEvents();

    void StartFrame();
    
  
    void Render();
    

    void Cleanup();

private:

    std::string title;
    

    int width;
    int height;
    

    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    

    ImGuiIO* io = nullptr;
    

    bool running = false;
    

    std::function<void()> renderCallback;
    
    // Singleton instance
    static SDLApp* instance;
};

} 
