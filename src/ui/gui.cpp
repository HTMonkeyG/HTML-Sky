// ----------------------------------------------------------------------------
// Graphics renderer.
// ----------------------------------------------------------------------------
#include <windows.h>
#include <unordered_map>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"
#include "vulkan/vulkan.h"

#include "utils/texts.h"
#include "htinternal.h"

bool gShowMainMenu = true
  , gShowDebugger = false;

/**
 * Initialize ImGui context and window message hook.
 * 
 * This function only be called after the game window is catched.
 */
void HTiInitGUI() {
  if (!gGameStatus.window)
    return;
  if (ImGui::GetCurrentContext())
    // Prevent duplicate initialization.
    return;

  // Initialize ImGui. ImGui_ImplVulkan_Init() is called in layer.cpp by
  // renderGui().
  ImGui::CreateContext();
  ImGui_ImplWin32_Init(gGameStatus.window);
  ImGuiIO &io = ImGui::GetIO();
  ImGuiStyle &style = ImGui::GetStyle();
  io.IniFilename = gPathGuiIni;
  io.LogFilename = nullptr;

  // Scale the window by dpi.
  f32 dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(gGameStatus.window);
  style.ScaleAllSizes(dpiScale);
  io.FontGlobalScale = dpiScale;

  // Setup colors.
  ImVec4 *colors = ImGui::GetStyle().Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.51f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.82f);
  colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.39f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.47f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.47f, 0.47f, 0.47f, 0.47f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.00f, 0.00f, 0.00f, 0.47f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 0.82f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.63f, 0.63f, 0.59f, 0.75f);

  // Setup sizes.
  style.WindowBorderSize = style.ChildBorderSize = 1;
  style.PopupBorderSize = style.FrameBorderSize = 0;
  style.WindowRounding = style.ChildRounding = style.FrameRounding
    = style.PopupRounding = style.ScrollbarRounding = style.GrabRounding = 10;
  style.ScrollbarSize = 10;
  style.SeparatorTextAlign = ImVec2(0, 0.5);
  style.SeparatorTextPadding = ImVec2(0, 3);
  style.WindowTitleAlign = ImVec2(0.5, 0.5);

  // Install window process hook.
  HTInstallInputHook();
}

/**
 * Shutdown ImGui. Currently unused.
 */
void HTiDeinitGUI() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}

/**
 * Render all windows registered by mods.
 */
void HTiUpdateGUI() {
  ImGuiIO &io = ImGui::GetIO();

  // Draw all windows.
  for (auto it = gModDataRuntime.begin(); it != gModDataRuntime.end(); it++) {
    PFN_HTModRenderGui guiRenderer = (PFN_HTModRenderGui)it->second.loaderFunc.pfn_HTModRenderGui;
    if (guiRenderer)
      guiRenderer(io.DeltaTime, nullptr);
  }

  // Update all options.
  HTiOptionsUpdate(io.DeltaTime);
}

void HTiRenderGUI(f32, void *) {
  if (gShowDebugger)
    HTWindowDebugger(&gShowDebugger);
  if (gShowMainMenu)
    HTWindowMain(&gShowMainMenu);
}

void HTiToggleMenuState(HTKeyEvent *event) {
  if ((event->flags & HTKeyEventFlags_Mask) == HTKeyEventFlags_Down)
    gShowMainMenu = !gShowMainMenu;
}
