#include <windows.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"
#include "vulkan/vulkan.h"

#include "ui/input.h"
#include "ui/gui.h"
#include "ui/console.h"

#include "globals.h"
#include "loader.h"

static bool gShowMainMenu = true
  , gFirstFrame = true;
static const ImVec4 modNameColor(1, 1, 1, 1)
  , modDescColor(0.75, 0.75, 0.75, 1);

/**
 * Render mod list tab item.
 */
static void HTMenuAbouts() {
  ImGui::Text("HT's Mod Loader v" HTML_VERSION_NAME " by HTMonkeyG");
  ImGui::Text("A mod loader developed for Sky:CotL.");
  ImGui::Text("<https://www.github.com/HTMonkeyG/HTML-Sky>");
}

/**
 * Render mod list tab item.
 */
static void HTMenuModList() {
  i32 i = 0;

  if (!ImGui::BeginChild("##HTModList"))
    return (void)ImGui::EndChild();
  
  ImGui::PushID("##HTModListItems");
  ImVec2 size(0, ImGui::GetTextLineHeight() * 4);
  ImVec2 spacing = ImGui::GetStyle().ItemSpacing;
  spacing.y = 1;
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, spacing);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3, 3));
  for (auto it = gModDataLoader.begin(); it != gModDataLoader.end(); ++it, i++) {
    ModManifest &manifest = it->second;

    // Show mod info.
    ImGuiID childId = ImGui::GetID((void *)(u64)i);
    ImGui::BeginChild(childId, size, ImGuiChildFlags_Borders);

    // Show mod name.
    ImGui::TextColored(modNameColor, "%s", manifest.modName.data());

    // Show mod description.
    ImGui::PushStyleColor(ImGuiCol_Text, modDescColor);
    ImGui::TextWrapped("%s", manifest.description.data());
    ImGui::PopStyleColor();

    ImGui::EndChild();
  }
  ImGui::PopStyleVar(2);
  ImGui::PopID();
  
  ImGui::EndChild();
}

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
    ImGui::SetNextWindowSize(ImVec2(480, 320));
    gFirstFrame = false;
  }

  if (!ImGui::Begin("HTML Main Menu", &gShowMainMenu, windowFlags))
    return (void)ImGui::End();
  
  if (ImGui::BeginTabBar("HTNavMain")) {
    if (ImGui::BeginTabItem("Abouts")) {
      HTMenuAbouts();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Console")) {
      HTMenuConsole();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Mods")) {
      HTMenuModList();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Settings")) {
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
