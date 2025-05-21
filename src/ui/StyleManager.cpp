Tether/src/ui/StyleManager.cpp
#include "ui/StyleManager.h"

namespace LocalTether::UI {
    void StyleManager::SetupModernStyle()
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // Set up a modern color scheme
        ImVec4 text_color = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
        ImVec4 text_disabled = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        ImVec4 bg_color = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        ImVec4 bg_light = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        ImVec4 accent_color = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
        ImVec4 accent_hover = ImVec4(0.30f, 0.70f, 1.00f, 0.90f);
        ImVec4 accent_active = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);

        // Window and background colors
        colors[ImGuiCol_WindowBg] = bg_color;
        colors[ImGuiCol_ChildBg] = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

        // Text and titles
        colors[ImGuiCol_Text] = text_color;
        colors[ImGuiCol_TextDisabled] = text_disabled;
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.40f, 0.80f, 0.30f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);

        // Main menu bar
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

        // Frame elements (buttons, sliders, inputs)
        colors[ImGuiCol_FrameBg] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

        // Tabs and docking
        colors[ImGuiCol_Tab] = ImVec4(0.21f, 0.21f, 0.21f, 0.86f);
        colors[ImGuiCol_TabHovered] = accent_hover;
        colors[ImGuiCol_TabActive] = accent_active;
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.18f, 0.18f, 0.97f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_DockingPreview] = accent_color;
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);

        // Scrollbar
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.97f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);

        // Buttons
        colors[ImGuiCol_Button] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = accent_hover;
        colors[ImGuiCol_ButtonActive] = accent_active;

        // Header (collapsing sections)
        colors[ImGuiCol_Header] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = accent_hover;
        colors[ImGuiCol_HeaderActive] = accent_active;

        // Separator and resize grip
        colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered] = accent_hover;
        colors[ImGuiCol_ResizeGripActive] = accent_active;

        // Checkmark
        colors[ImGuiCol_CheckMark] = accent_color;

        // Sliders and progress bar
        colors[ImGuiCol_SliderGrab] = accent_color;
        colors[ImGuiCol_SliderGrabActive] = accent_active;

        // Adjust style spacing and rounding for a modern look
        style.WindowPadding = ImVec2(10, 10);
        style.FramePadding = ImVec2(8, 4);
        style.CellPadding = ImVec2(6, 3);
        style.ItemSpacing = ImVec2(8, 6);
        style.ItemInnerSpacing = ImVec2(6, 4);
        style.TouchExtraPadding = ImVec2(0, 0);
        style.IndentSpacing = 20;
        style.ScrollbarSize = 12;
        style.GrabMinSize = 8;

        // Rounding
        style.WindowRounding = 6.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 12.0f;
        style.GrabRounding = 4.0f;
        style.TabRounding = 6.0f;

        // Alignment
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_Right;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

        // Other parameters
        style.Alpha = 1.0f;
        style.DisabledAlpha = 0.6f;
        style.AntiAliasedLines = true;
        style.AntiAliasedFill = true;
        
        // Configure docking parameters
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigDockingWithShift = false;  // Allow docking without holding Shift
        
        // Fix for the scrolling issue - set DisplayFramebufferScale to 1.0f
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    }
}