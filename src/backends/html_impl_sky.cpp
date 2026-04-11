// ----------------------------------------------------------------------------
// Game-specified implementation for Sky:CotL of HTML.
// By HTMonkeyG.
// ----------------------------------------------------------------------------

#include <windows.h>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include "MinHook.h"

#include "htinternal.hpp"
#include "includes/backends/html_impl_sky.h"
#include "includes/htconfig.h"

#ifdef HTML_USE_IMPL_SKY

#define HTTexts_WndClassW L"TgcMainWindow"
#define HTTexts_WndNameInW L"Sky"
#define HTTexts_WndNameChW L"光·遇"

typedef HWND (WINAPI *PFN_CreateWindowExA)(
  DWORD, LPCSTR, LPCSTR, DWORD, i32, i32, i32, i32, HWND, HMENU, HINSTANCE, LPVOID);
typedef HWND (WINAPI *PFN_CreateWindowExW)(
  DWORD, LPCWSTR, LPCWSTR, DWORD, i32, i32, i32, i32, HWND, HMENU, HINSTANCE, LPVOID);

static PFN_CreateWindowExA fn_CreateWindowExA;
static PFN_CreateWindowExW fn_CreateWindowExW;

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

static i32 checkWindowAndSetupAW(
  HWND hWnd
) {
  wchar_t buffer[32];
  HTGameStatus status;
  HTGameEdition edition = HT_ImplNull_EditionUnknown;

  if (gGameStatus.window)
    return 0;

  LOG("[ImplSky][INFO] checkWindowAndSetupAW() called for hWnd: 0x%p.\n", hWnd);

  // Get the game edition from window name.
  GetWindowTextW(hWnd, buffer, 32);
  buffer[31] = 0;
  if (!wcscmp(buffer, HTTexts_WndNameChW))
    edition = HT_ImplSky_EditionChinese;
  else if (!wcscmp(buffer, HTTexts_WndNameInW))
    edition = HT_ImplSky_EditionInternation;
  else
    return 0;

  // Check the window's class name.
  GetClassNameW(hWnd, buffer, 32);
  buffer[31] = 0;
  if (wcscmp(buffer, HTTexts_WndClassW))
    return 0;

  // Set game edition and hWnd.
  status.baseAddr = (void *)GetModuleHandleA("Sky.exe");
  status.edition = edition;
  status.pid = GetCurrentProcessId();
  status.window = hWnd;
  HTiSetGameStatus(&status);

  LOG("[ImplSky][INFO] Game status set for hWnd: 0x%p.\n", hWnd);

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

int HTi_ImplSky_ExpectProcess() {
  return !!GetModuleHandleA(HT_ImplSky_ExecutableName);
}

/**
 * Install hooks on WinAPI functions that we need. Setup procedure is in the
 * detour functions on CreateWindowEx().
 */
int HTi_ImplSky_Init() {
  MH_STATUS s;
  void *function;

  if (!HTi_ImplSky_ExpectProcess())
    return 0;

  LOG("[ImplSky][INFO] HTi_ImplSky_Init() called.\n");

  HTiSetGameBackendName(HT_ImplSky_Name);
  HTiSetGameProcessName(HT_ImplSky_ExecutableName);

  s = MH_CreateHookApiEx(
    L"user32.dll",
    "CreateWindowExA",
    (void *)hook_CreateWindowExA,
    (void **)&fn_CreateWindowExA,
    &function
  );
  if (s != MH_OK) return 0;
  if (MH_EnableHook(function) != MH_OK)
    return 0;

  LOG("[ImplSky][INFO] Hooked CreateWindowExA(): 0x%p.\n", function);

  s = MH_CreateHookApiEx(
    L"user32.dll",
    "CreateWindowExW",
    (void *)hook_CreateWindowExW,
    (void **)&fn_CreateWindowExW,
    &function
  );
  if (s != MH_OK) return 0;
  if (MH_EnableHook(function) != MH_OK)
    return 0;

  LOG("[ImplSky][INFO] Hooked CreateWindowExW(): 0x%p.\n", function);

  return 1;
}

const HTiBackendRegister g_register_ImplSky{
  HT_ImplSky_Name,
  HTi_ImplSky_Init,
  HTi_ImplSky_ExpectProcess
};

#endif
