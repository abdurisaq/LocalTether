cmake_minimum_required(VERSION 3.15)
project(LocalTether)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_SHARED_LIBS OFF)

# ImGui and platform-specific definitions
add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS)
add_definitions(-DIMGUI_ENABLE_DOCKING)

if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# vcpkg integration
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/scripts/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Find common packages
find_package(SDL2 CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(glad CONFIG REQUIRED)

# Platform specific dependencies
if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(POLKIT polkit-gobject-1)
        if(POLKIT_FOUND)
            add_definitions(-DHAS_POLKIT)
            message(STATUS "Found PolKit: ${POLKIT_LIBRARIES}")
        endif()
    endif()
endif()

# ImGui directories
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
set(IMGUI_BACKEND_DIR "${CMAKE_SOURCE_DIR}/external/imgui/backends")

# ImGui sources
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_BACKEND_DIR}/imgui_impl_sdl2.cpp
    ${IMGUI_BACKEND_DIR}/imgui_impl_opengl3.cpp
)

file(GLOB_RECURSE CORE_SOURCES "src/core/*.cpp")
file(GLOB_RECURSE UI_SOURCES "src/ui/*.cpp")
file(GLOB_RECURSE UI_PANEL_SOURCES "src/ui/panels/*.cpp")
file(GLOB_RECURSE UTILS_SOURCES "src/utils/*.cpp")
file(GLOB_RECURSE NETWORK_SOURCES "src/network/*.cpp")

# Combine all sources
set(SOURCES
    src/main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${UI_PANEL_SOURCES}
    ${UTILS_SOURCES}
    ${NETWORK_SOURCES}
)

# Create executable
add_executable(LocalTether ${SOURCES} ${IMGUI_SOURCES})

# Include paths
target_include_directories(LocalTether PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_BACKEND_DIR}
)

# Common libraries
target_link_libraries(LocalTether PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    asio::asio
    OpenSSL::SSL
    OpenSSL::Crypto
    glad::glad
)

# Platform-specific setup
if(UNIX AND NOT APPLE)
    if(POLKIT_FOUND)
        target_include_directories(LocalTether PRIVATE ${POLKIT_INCLUDE_DIRS})
        target_link_libraries(LocalTether PRIVATE ${POLKIT_LIBRARIES})
    endif()
endif()

# No console on Windows
if (WIN32)
    set_target_properties(LocalTether PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Installation rules
install(TARGETS LocalTether
    RUNTIME DESTINATION bin
)

# Copy assets if they exist
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    add_custom_command(TARGET LocalTether POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" 
        "$<TARGET_FILE_DIR:LocalTether>/assets"
    )
endif()
