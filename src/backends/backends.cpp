// ----------------------------------------------------------------------------
// Backend dispatcher.
// You must put your backend initialize functions under the function below.
// ----------------------------------------------------------------------------
#include <windows.h>
#include <mutex>
#include "imgui.h"

#include "htinternal.hpp"
#include "includes/htconfig.h"

typedef int (HTMLAPI *PFN_HTiGameEditionCheck)(
  HTGameEdition);

static i32 checkEditionDefault(
  HTGameEdition);
static CRITICAL_SECTION gGraphicInitMutex;
static PFN_HTiGameEditionCheck gEditionCheck = checkEditionDefault;

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
  return !ImGui::GetIO().BackendRendererUserData;
}

int HTiBackendGLLeaveCritical() {
  LeaveCriticalSection(&gGraphicInitMutex);
  return 1;
}

int HTiBackendGLInitComplete() {
  SetEvent(gEventGuiInit);
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

  for (const HTiBackendRegister *p = HTiBackendRegister::list(); p; p = p->prev)
    if (p->fnExpectProcess)
      success |= p->fnExpectProcess();

  return success;
}

int HTiBackendSetupAll() {
  int success = 0;

  InitializeCriticalSection(&gGraphicInitMutex);

  for (const HTiBackendRegister *p = HTiBackendRegister::list(); p; p = p->prev)
    if (p->fnInit)
      success |= p->fnInit();

  return success;
}