// ----------------------------------------------------------------------------
// Null game backend implementation for HTML.
// 
// This backend neither hook or patch anything, nor call HTML functions.
// You may use this backend as a template.
// ----------------------------------------------------------------------------

#include "htinternal.hpp"
#include "includes/htconfig.h"

//#ifdef USE_IMPL_NULLGAME
#if 0

static i32 editionCheck(
  HTGameEdition edition
) {
  // The edition flag is passed into this function during mod scanning and
  // validation.
  // When the passed edition can be accepted, return 1.
  return 0;
}

static void fakeInitWindow() {
  // Simulate the operations when got the main window of the game.
  HTGameStatus status;

  // Set game edition and hWnd.
  status.baseAddr = (void *)GetModuleHandleA("Minecraft.Windows.exe");
  status.edition = HT_ImplNull_EditionUnknown;
  status.pid = GetCurrentProcessId();
  status.window = nullptr;
  HTiSetGameStatus(&status);

  // Set edition check function.
  HTiBackendSetEditionCheckFunc((PFN_HTVoidFunction)editionCheck);

  // Load mods.
  HTiSetupAll();
}

/**
 * Detect the name of the executable file of the process.
 */
int HTi_ImplGameNull_ExpectProcess() {
  return !!GetModuleHandleA(nullptr);
}

/**
 * Install hooks on WinAPI functions that we need. Setup procedure is in the
 * detour functions on CreateWindowEx().
 */
int HTi_ImplGameNull_Init() {
  // Check the process name.
  if (!HTi_ImplGameNull_ExpectProcess())
    return 0;

  // Set the name of the backend.
  HTiSetGameBackendName("Impl_GameNull");
  // Set the name of the main executable file.
  HTiSetGameProcessName("");

  // Simulate the window creation of the game.
  // This operation should be injected right after the game creates its main
  // window.
  fakeInitWindow();

  // Other initialize codes...

  return 1;
}

const HTiBackendRegister g_register_ImplGameNull{
  "GameNull",
  HTi_ImplGameNull_Init,
  HTi_ImplGameNull_ExpectProcess
};

#endif
