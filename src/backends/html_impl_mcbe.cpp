// ----------------------------------------------------------------------------
// Game-specified implementation for MCBE of HTML.
// By HTMonkeyG.
// ----------------------------------------------------------------------------

#include <windows.h>

#include "htinternal.hpp"
#include "includes/backends/html_impl_mcbe.h"
#include "includes/htconfig.h"

#ifdef HTML_USE_IMPL_MCBE

#define HTTexts_WndNamePostfixW L"Minecraft"
#define HTTexts_WndClassW L"OGLES"

static i32 editionCheck(
  HTGameEdition edition
) {
  HTGameEdition local = gGameStatus.edition;
  if (
    edition == HT_ImplMCBE_EditionAll
    && (local == HT_ImplMCBE_EditionChinese || local == HT_ImplMCBE_EditionInternation)
  )
    return 1;

  if (edition == local)
    return 1;

  return 0;
}

// Map a window title to a MCBE game edition.
static HTGameEdition matchEdition(
  const wchar_t *title
) {
  if (wcsstr(title, HTTexts_WndNamePostfixW))
    return HT_ImplMCBE_EditionChinese;
  return HT_ImplNull_EditionUnknown;
}

int HTi_ImplMCBE_ExpectProcess() {
  return !!GetModuleHandleA(HT_ImplMCBE_ExecutableName);
}

/**
 * Install hooks on WinAPI functions that we need. Setup procedure is in the
 * shared CreateWindowEx() detours (see html_impl_window.cpp).
 */
int HTi_ImplMCBE_Init() {
  if (!HTi_ImplMCBE_ExpectProcess())
    return 0;

  static const HTiWindowBackendDesc desc = {
    HT_ImplMCBE_Name,
    HT_ImplMCBE_ExecutableName,
    HTTexts_WndClassW,
    matchEdition,
    (PFN_HTVoidFunction)editionCheck
  };
  return HTiInstallWindowBackend(&desc);
}

const HTiBackendRegister g_register_ImplMCBE{
  HT_ImplMCBE_Name,
  HTi_ImplMCBE_Init,
  HTi_ImplMCBE_ExpectProcess
};

#endif
