// ----------------------------------------------------------------------------
// Game-specified implementation for MCBE of HTML.
// By HTMonkeyG.
// ----------------------------------------------------------------------------

#include <windows.h>
#include "MinHook.h"

#include "htinternal.hpp"
#include "includes/backends/html_impl_mcbe.h"
#include "includes/htconfig.h"

#ifdef HTML_USE_IMPL_MCBE

#define HTTexts_WndNamePostfixW L"Minecraft"
#define HTTexts_WndClassW L"OGLES"

typedef HWND (WINAPI *PFN_CreateWindowExA)(
  DWORD, LPCSTR, LPCSTR, DWORD, i32, i32, i32, i32, HWND, HMENU, HINSTANCE, LPVOID);
typedef HWND (WINAPI *PFN_CreateWindowExW)(
  DWORD, LPCWSTR, LPCWSTR, DWORD, i32, i32, i32, i32, HWND, HMENU, HINSTANCE, LPVOID);

static PFN_CreateWindowExA fn_CreateWindowExA = nullptr;
static PFN_CreateWindowExW fn_CreateWindowExW = nullptr;

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

static i32 checkWindowAndSetupAW(
  HWND hWnd
) {
  wchar_t buffer[32];
  HTGameStatus status;
  HTGameEdition edition = HT_ImplNull_EditionUnknown;

  if (gGameStatus.window)
    return 0;

  LOG("[ImplMCBE][INFO] checkWindowAndSetupAW() called for hWnd: 0x%p.\n", hWnd);

  // Get the game edition from window name.
  GetWindowTextW(hWnd, buffer, 32);
  buffer[31] = 0;
  if (wcsstr(buffer, HTTexts_WndNamePostfixW))
    edition = HT_ImplMCBE_EditionChinese;
  else
    return 0;

  // Check the window's class name.
  GetClassNameW(hWnd, buffer, 32);
  buffer[31] = 0;
  if (wcscmp(buffer, HTTexts_WndClassW))
    return 0;

  // Set game edition and hWnd.
  status.baseAddr = (void *)GetModuleHandleA("Minecraft.Windows.exe");
  status.edition = edition;
  status.pid = GetCurrentProcessId();
  status.window = hWnd;
  HTiSetGameStatus(&status);

  LOG("[ImplMCBE][INFO] Game status set for hWnd: 0x%p.\n", hWnd);

  // Set edition check function.
  HTiBackendSetEditionCheckFunc((PFN_HTVoidFunction)editionCheck);

  // Load mods.
  HTiSetupAll();

  return 1;
}

static HWND WINAPI hook_CreateWindowExA(
  DWORD dwExStyle,
  LPCSTR lpClassName,
  LPCSTR lpWindowName,
  DWORD dwStyle,
  int X,
  int Y,
  int nWidth,
  int nHeight,
  HWND hWndParent,
  HMENU hMenu,
  HINSTANCE hInstance,
  LPVOID lpParam
) {
  HWND result = fn_CreateWindowExA(
    dwExStyle,
    lpClassName,
    lpWindowName,
    dwStyle,
    X,
    Y,
    nWidth,
    nHeight,
    hWndParent,
    hMenu,
    hInstance,
    lpParam);
  DWORD lastError = GetLastError();

  if (!result)
    return result;

  checkWindowAndSetupAW(result);

  SetLastError(lastError);
  return result;
}

static HWND WINAPI hook_CreateWindowExW(
  DWORD dwExStyle,
  LPCWSTR lpClassName,
  LPCWSTR lpWindowName,
  DWORD dwStyle,
  int X,
  int Y,
  int nWidth,
  int nHeight,
  HWND hWndParent,
  HMENU hMenu,
  HINSTANCE hInstance,
  LPVOID lpParam
) {
  HWND result = fn_CreateWindowExW(
    dwExStyle,
    lpClassName,
    lpWindowName,
    dwStyle,
    X,
    Y,
    nWidth,
    nHeight,
    hWndParent,
    hMenu,
    hInstance,
    lpParam);
  DWORD lastError = GetLastError();

  if (!result)
    return result;

  checkWindowAndSetupAW(result);

  SetLastError(lastError);
  return result;
}

int HTi_ImplMCBE_ExpectProcess() {
  return !!GetModuleHandleA(HT_ImplMCBE_ExecutableName);
}

/**
 * Install hooks on WinAPI functions that we need. Setup procedure is in the
 * detour functions on CreateWindowEx().
 */
int HTi_ImplMCBE_Init() {
  MH_STATUS s;
  void *function;

  if (!HTi_ImplMCBE_ExpectProcess())
    return 0;

  HTiSetGameBackendName(HT_ImplMCBE_Name);
  HTiSetGameProcessName(HT_ImplMCBE_ExecutableName);

  s = MH_CreateHookApiEx(
    L"user32.dll",
    "CreateWindowExA",
    (void *)hook_CreateWindowExA,
    (void **)&fn_CreateWindowExA,
    &function
  );
  if (s != MH_OK)
    return 0;
  if (MH_EnableHook(function) != MH_OK)
    return 0;

  LOG("[ImplMCBE][INFO] Hooked CreateWindowExA(): 0x%p.\n", function);

  s = MH_CreateHookApiEx(
    L"user32.dll",
    "CreateWindowExW",
    (void *)hook_CreateWindowExW,
    (void **)&fn_CreateWindowExW,
    &function
  );
  if (s != MH_OK)
    return 0;
  if (MH_EnableHook(function) != MH_OK)
    return 0;

  LOG("[ImplMCBE][INFO] Hooked CreateWindowExW(): 0x%p.\n", function);

  return 1;
}

const HTiBackendRegister g_register_ImplMCBE{
  HT_ImplMCBE_Name,
  HTi_ImplMCBE_Init,
  HTi_ImplMCBE_ExpectProcess
};

#endif
