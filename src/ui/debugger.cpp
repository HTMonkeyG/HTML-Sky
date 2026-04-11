// ----------------------------------------------------------------------------
// Debugging data display of HTModLoader.
// ----------------------------------------------------------------------------

#include "includes/htconfig.h"
#include "htinternal.hpp"

#ifdef HTML_ENABLE_DEBUGGER

// Show active backend and basic information.
static void renderBackendData() {
  ImGui::BulletText("GL Backend: %s", gActiveGLBackendName);
  ImGui::BulletText("Game Backend: %s", gActiveGameBackendName);
  ImGui::BulletText("Game id: %d", gGameStatus.edition);
  ImGui::BulletText("HWND: 0x%p", gGameStatus.window);
  ImGui::BulletText("Executable file: <%ls>.%p", gGameProcessName.c_str(), gGameStatus.baseAddr);
}

// Show scanned mods.
static void renderMods() {
  for (auto &it: gModDataLoader) {
    auto &mod = it.second;

    ImGui::Separator();
    ImGui::Text(it.first.c_str());
    ImGui::BulletText("Dll folder: %ls", mod.paths.folder.c_str());
    ImGui::BulletText("Dll path: %ls", mod.paths.dll.c_str());
    ImGui::BulletText(
      "Version: %u.%u.%u",
      mod.meta.version[0],
      mod.meta.version[1],
      mod.meta.version[2]);

    if (mod.gameEditionFlags == (i32)HT_ImplNull_EditionAll)
      ImGui::BulletText("Compatible game id: <Any>");
    else
      ImGui::BulletText("Compatible game id: %d", mod.gameEditionFlags);

    if (mod.runtime)
      ImGui::BulletText("HMODULE: 0x%p", mod.runtime->handle);
    else
      ImGui::BulletText("HMODULE: <NULL>");
  }
}

// Show all created hooks.
static void renderHooks() {
  for (auto &it: gModDataRuntime) {
    if (!ImGui::TreeNode(
      it.second.manifest->meta.packageName.c_str()
    ))
      continue;

    std::vector<ModHook *> hooks = HTiAsmHookFindFor(
      it.first);

    for (auto &hook: hooks) {
      ImGui::Separator();
      ImGui::Text(hook->name.c_str());
      ImGui::BulletText("Addr: 0x%p", hook->intent);
      ImGui::BulletText("Detour: 0x%p", hook->detour);
      ImGui::BulletText("Status: %s", hook->isEnabled ? "<ENABLED>" : "<DISABLED>");
    }

    ImGui::TreePop();
  }
}

void HTiRenderDebugger() {
  if (ImGui::CollapsingHeader("Backend"))
    renderBackendData();
  if (ImGui::CollapsingHeader("Mods"))
    renderMods();
  if (ImGui::CollapsingHeader("Hooks"))
    renderHooks();
}

#endif
