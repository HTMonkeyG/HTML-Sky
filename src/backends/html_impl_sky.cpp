// ----------------------------------------------------------------------------
// Game-specified implementation for Sky:CotL of HTML.
// By HTMonkeyG.
// ----------------------------------------------------------------------------

#include <windows.h>

#include "htinternal.hpp"
#include "includes/backends/html_impl_sky.h"
#include "includes/htconfig.h"

#ifdef HTML_USE_IMPL_SKY

#define HTTexts_WndClassW L"TgcMainWindow"
#define HTTexts_WndNameInW L"Sky"
#define HTTexts_WndNameChW L"光·遇"

static i32 editionCheck(
  HTGameEdition edition
) {
  HTGameEdition local = gGameStatus.edition;
  if (
    edition == HT_ImplSky_EditionAll
    && (local == HT_ImplSky_EditionChinese || local == HT_ImplSky_EditionInternation)
  )
    return 1;

  if (edition == local)
    return 1;

  return 0;
}

// Map a window title to a Sky game edition.
static HTGameEdition matchEdition(
  const wchar_t *title
) {
  if (!wcscmp(title, HTTexts_WndNameChW))
    return HT_ImplSky_EditionChinese;
  if (!wcscmp(title, HTTexts_WndNameInW))
    return HT_ImplSky_EditionInternation;
  return HT_ImplNull_EditionUnknown;
}

int HTi_ImplSky_ExpectProcess() {
  return !!GetModuleHandleA(HT_ImplSky_ExecutableName);
}

/**
 * Install hooks on WinAPI functions that we need. Setup procedure is in the
 * shared CreateWindowEx() detours (see html_impl_window.cpp).
 */
int HTi_ImplSky_Init() {
  if (!HTi_ImplSky_ExpectProcess())
    return 0;

  LOG("[ImplSky][INFO] HTi_ImplSky_Init() called.\n");

  static const HTiWindowBackendDesc desc = {
    HT_ImplSky_Name,
    HT_ImplSky_ExecutableName,
    HTTexts_WndClassW,
    matchEdition,
    (PFN_HTVoidFunction)editionCheck
  };
  return HTiInstallWindowBackend(&desc);
}

const HTiBackendRegister g_register_ImplSky{
  HT_ImplSky_Name,
  HTi_ImplSky_Init,
  HTi_ImplSky_ExpectProcess
};

#endif
