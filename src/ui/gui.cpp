#include <windows.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"
#include "vulkan/vulkan.h"
#include "ui/input.h"
#include "ui/gui.h"
#include "ui/console.h"
#include "globals.h"

static bool gShowMainMenu = true
  , gFirstFrame = true;

/**
 * Initialize ImGui context and window message hook.
 * 
 * This function only be called after the game window is catched.
 */
void HTInitGUI() {
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
  io.IniFilename = io.LogFilename = nullptr;

  // Scale the window by dpi.
  f32 dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(gGameStatus.window);
  style.ScaleAllSizes(dpiScale);
  io.FontGlobalScale = dpiScale;

  // Install window process hook.
  HTInstallInputHook();
}

/**
 * Show HTML Menus.
 */
void HTUpdateGUI() {
  // Press "~" key to show or hide.
  if (GetAsyncKeyState(VK_OEM_3) & 0x1)
    gShowMainMenu = !gShowMainMenu;

  if (!gShowMainMenu)
    return;

  ImGuiStyle &style = ImGui::GetStyle();
  style.ScrollbarRounding = 0.0f;
  style.WindowTitleAlign.x = 0.5f;
  style.WindowTitleAlign.y = 0.5f;
  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
  if (gFirstFrame) {
    // Resize window on the first frame.
    windowFlags = ImGuiWindowFlags_AlwaysAutoResize;
    gFirstFrame = false;
  }

  if (!ImGui::Begin("HTML Main Menu", &gShowMainMenu, windowFlags))
    return (void)ImGui::End();
  
  if (ImGui::BeginTabBar("Nav Main")) {
    if (ImGui::BeginTabItem("Console")) {
      HTMenuConsole();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Mods")) {
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Settings")) {
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Abouts")) {
      ImGui::Text("HT's Mod Loader v" HTML_VERSION_NAME " by HTMonkeyG");
      ImGui::Text("A mod loader developed for Sky:CotL.");
      ImGui::Text("<https://www.github.com/HTMonkeyG/HTML-Sky>");
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  
  ImGui::End();
}

void HTDeinitGUI() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}
