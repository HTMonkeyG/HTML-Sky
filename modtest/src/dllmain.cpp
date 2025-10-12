#include <windows.h>
#include "imgui.h"
#include "includes/htmod.h"

static HTHandle hKeyTest = nullptr;
static HMODULE gModule = nullptr;

void HTModRenderGui(f32 timeElapsed, void *) {
  ImGui::Begin("Mod Test");
  ImGui::Text("HT Mod Loader test mod.");
  ImGui::Text("FPS %.1f", 1.0 / timeElapsed);
  if (HTHotkeyPressed(hKeyTest))
    ImGui::Text("Key M pressed!");
  ImGui::End();
}

void callback(HTKeyEvent *event) {
  event->preventFlags |= HTKeyEventPreventFlags_Game;
}

HTStatus HTModOnInit(void *) {
  hKeyTest = HTHotkeyRegister(
    gModule,
    "Test key",
    HTKey_M);
  HTHotkeyListen(hKeyTest, callback);
  return HT_SUCCESS;
}

BOOL APIENTRY DllMain(
  HMODULE hModule,
  DWORD dwReason,
  LPVOID lpReserved
) {
  if (dwReason == DLL_PROCESS_ATTACH)
    gModule = hModule;
  return TRUE;
}
