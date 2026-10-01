// ----------------------------------------------------------------------------
// Backend dispatcher.
// You must put your backend initialize functions under the function below.
// ----------------------------------------------------------------------------
#include <windows.h>
#include <mutex>
#include <thread>
#include "imgui.h"

#include "htinternal.hpp"
#include "includes/htconfig.h"

typedef int (HTMLAPI *PFN_HTiGameEditionCheck)(
  HTGameEdition);

static i32 checkEditionDefault(
  HTGameEdition);
static CRITICAL_SECTION gGraphicInitMutex;
static PFN_HTiGameEditionCheck gEditionCheck = checkEditionDefault;
static std::once_flag gGuiInitOnce;

char gActiveGameBackendName[32] = {0};
char gActiveGLBackendName[32] = {0};
std::wstring gGameProcessName;

static i32 checkEditionDefault(
  HTGameEdition edition
) {
  return edition == gGameStatus.edition;
}

int HTiBackendGLEnterCritical() {
  EnterCriticalSection(&gGraphicInitMutex);
  if (!ImGui::GetCurrentContext())
    return 1;
  return !ImGui::GetIO().BackendRendererUserData;
}

int HTiBackendGLLeaveCritical() {
  LeaveCriticalSection(&gGraphicInitMutex);
  return 1;
}

int HTiBackendGLInitComplete() {
  std::call_once(gGuiInitOnce, [] {
    if (gEventGuiInit)
      SetEvent(gEventGuiInit);
    HTiEnableMods();
  });
  return 1;
}

int HTiBackendCheckEdition(
  HTGameEdition edition
) {
  if ((u32)edition == (u32)HT_ImplNull_EditionAll)
    return 1;

  return gEditionCheck(edition);
}

int HTiBackendSetEditionCheckFunc(
  PFN_HTVoidFunction func
) {
  if (!func)
    return 0;
  gEditionCheck = (PFN_HTiGameEditionCheck)func;
  return 1;
}

int HTiSetGLBackendName(
  const char *gl
) {
  strncpy(gActiveGLBackendName, gl, 31);
  gActiveGLBackendName[31] = 0;
  return 1;
}

int HTiSetGameBackendName(
  const char *game
) {
  strncpy(gActiveGameBackendName, game, 31);
  gActiveGameBackendName[31] = 0;

  return 1;
}

int HTiSetGameProcessName(
  const char *name
) {
  gGameProcessName = HTiUtf8ToWstring(name);

  return 1;
}

int HTiSetGameProcessName(
  const wchar_t *name
) {
  gGameProcessName = name;

  return 1;
}

int HTiBackendExpectProcess() {
  int success = 0;

  for (const HTiBackendRegister *p = HTiBackendRegister::list(); p; p = p->prev) {
    if (!p->fnExpectProcess)
      continue;
    // When a backend is forced via html-config.json, only consider that one.
    if (!gConfigForceBackend.empty()
        && (!p->name || gConfigForceBackend != p->name))
      continue;
    success |= p->fnExpectProcess();
  }

  return success;
}

HMODULE HTiResolveGameModule(
  const char *defaultExe
) {
  // Prefer the configured target executable (e.g. a renamed "Sky-test.exe")
  // when present and currently loaded, otherwise fall back to the backend's
  // default executable name.
  if (!gConfigTargetExe.empty()) {
    HMODULE h = GetModuleHandleA(gConfigTargetExe.c_str());
    if (h)
      return h;
  }
  return defaultExe ? GetModuleHandleA(defaultExe) : nullptr;
}

int HTiBackendSetupAll() {
  int success = 0;

  InitializeCriticalSection(&gGraphicInitMutex);

  for (const HTiBackendRegister *p = HTiBackendRegister::list(); p; p = p->prev)
    if (p->fnInit)
      success |= p->fnInit();

  return success;
}
