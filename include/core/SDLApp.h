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

    // Initialize SDL, OpenGL, and ImGui
    bool Initialize();
    
    // Run the main application loop
    void Run();
    
    // Whether the application is running
    bool IsRunning() const { return running; }
    
    // Request application to stop
    void Quit() { running = false; }
    
    // Set the render callback function
    void SetRenderCallback(std::function<void()> callback) { renderCallback = callback; }
    
    // Get ImGui IO
    ImGuiIO& GetIO() { return *io; }
    
    // Get window
    SDL_Window* GetWindow() { return window; }
    
    // Get GL context
    SDL_GLContext GetGLContext() { return gl_context; }
    
    // Static instance accessor
    static SDLApp& GetInstance() { return *instance; }

private:
    // Handle SDL events
    void ProcessEvents();
    
    // Start a new ImGui frame
    void StartFrame();
    
    // Render the current frame
    void Render();
    
    // Clean up resources
    void Cleanup();

private:
    // Application title
    std::string title;
    
    // Window dimensions
    int width;
    int height;
    
    // SDL objects
    SDL_Window* window = nullptr;
    SDL_GLContext gl_context = nullptr;
    
    // ImGui IO pointer
    ImGuiIO* io = nullptr;
    
    // Running state
    bool running = false;
    
    // Render callback
    std::function<void()> renderCallback;
    
    // Singleton instance
    static SDLApp* instance;
};

} // namespace LocalTether::Core
