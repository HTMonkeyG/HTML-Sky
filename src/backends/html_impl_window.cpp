// ----------------------------------------------------------------------------
// Shared implementation for "window-hook" game backends.
//
// Game backends that detect their target by hooking user32!CreateWindowExA/W
// and matching the created window's title and class all share the exact same
// hook bodies and setup flow. They differ only in a handful of values (window
// class, executable name, title->edition mapping, edition check), which are
// provided through HTiWindowBackendDesc. This file holds the common code so the
// per-game backends stay tiny.
// ----------------------------------------------------------------------------

#include <windows.h>
#include "MinHook.h"

#include "htinternal.hpp"

typedef HWND (WINAPI *PFN_CreateWindowExA)(
  DWORD, LPCSTR, LPCSTR, DWORD, i32, i32, i32, i32, HWND, HMENU, HINSTANCE, LPVOID);
typedef HWND (WINAPI *PFN_CreateWindowExW)(
  DWORD, LPCWSTR, LPCWSTR, DWORD, i32, i32, i32, i32, HWND, HMENU, HINSTANCE, LPVOID);

// The currently active window-hook backend. Only one game backend runs at a
// time, so a single pointer is sufficient.
static const HTiWindowBackendDesc *gActiveWindowBackend = nullptr;
static PFN_CreateWindowExA fn_CreateWindowExA = nullptr;
static PFN_CreateWindowExW fn_CreateWindowExW = nullptr;

// Inspect a freshly created window; if it is the game's main window, record the
// game status and kick off mod loading. Returns 1 when the game was set up.
static i32 checkWindowAndSetup(
  HWND hWnd
) {
  const HTiWindowBackendDesc *desc = gActiveWindowBackend;
  wchar_t buffer[32];
  HTGameStatus status;
  HTGameEdition edition;

  if (!desc || gGameStatus.window)
    return 0;

  // Get the game edition from the window name.
  GetWindowTextW(hWnd, buffer, 32);
  buffer[31] = 0;
  edition = desc->matchEdition(buffer);
  if (edition == HT_ImplNull_EditionUnknown)
    return 0;

  // Check the window's class name.
  GetClassNameW(hWnd, buffer, 32);
  buffer[31] = 0;
  if (wcscmp(buffer, desc->className))
    return 0;

  // Set game edition and hWnd.
  status.baseAddr = (void *)GetModuleHandleA(desc->exeName);
  status.edition = edition;
  status.pid = GetCurrentProcessId();
  status.window = hWnd;
  HTiSetGameStatus(&status);

  // Set edition check function.
  HTiBackendSetEditionCheckFunc(desc->editionCheck);

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
    dwExStyle, lpClassName, lpWindowName, dwStyle,
    X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
  DWORD lastError = GetLastError();

  if (!result)
    return result;

  checkWindowAndSetup(result);

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
    dwExStyle, lpClassName, lpWindowName, dwStyle,
    X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
  DWORD lastError = GetLastError();

  if (!result)
    return result;

  checkWindowAndSetup(result);

  SetLastError(lastError);
  return result;
}

int HTiInstallWindowBackend(
  const HTiWindowBackendDesc *desc
) {
  MH_STATUS s;
  void *function;

  if (!desc)
    return 0;

  gActiveWindowBackend = desc;
  HTiSetGameBackendName(desc->backendName);
  HTiSetGameProcessName(desc->exeName);

  s = MH_CreateHookApiEx(
    L"user32.dll",
    "CreateWindowExA",
    (void *)hook_CreateWindowExA,
    (void **)&fn_CreateWindowExA,
    &function);
  if (s != MH_OK)
    return 0;
  if (MH_EnableHook(function) != MH_OK)
    return 0;

  s = MH_CreateHookApiEx(
    L"user32.dll",
    "CreateWindowExW",
    (void *)hook_CreateWindowExW,
    (void **)&fn_CreateWindowExW,
    &function);
  if (s != MH_OK)
    return 0;
  if (MH_EnableHook(function) != MH_OK)
    return 0;

  return 1;
}
