#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"
#include "vulkan/vulkan.h"
#include "gui.h"
#include "globals.h"
#include "input.h"

/**
 * Initialize ImGui context and window message hook.
 * 
 * Only called after the game window is catched.
 */
void HTInitGUI() {
  if (!gGameStatus.window)
    return;
  if (ImGui::GetCurrentContext())
    return;
  ImGui::CreateContext();
  ImGui_ImplWin32_Init(gGameStatus.window);
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = io.LogFilename = nullptr;
  HTInstallInputHook();
}

void HTUpdateGUI() {
  ImGui::ShowDemoWindow();
}
