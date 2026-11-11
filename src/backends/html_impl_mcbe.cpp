// ----------------------------------------------------------------------------
// Game-specified implementation for MCBE of HTML.
// By HTMonkeyG.
// ----------------------------------------------------------------------------

#include <windows.h>
#include <string.h>
#include "MinHook.h"

#include "htinternal.hpp"
#include "htmodloader/backends/html_impl_mcbe.h"
#include "htmodloader/htconfig.h"

#ifdef HTML_USE_IMPL_MCBE

#define HTTexts_WndNamePostfixW L"Minecraft"
#define HTTexts_WndClassW L"OGLES"

typedef HWND (WINAPI *PFN_CreateWindowExA)(
  DWORD, LPCSTR, LPCSTR, DWORD, i32, i32, i32, i32, HWND, HMENU, HINSTANCE, LPVOID);
typedef HWND (WINAPI *PFN_CreateWindowExW)(
  DWORD, LPCWSTR, LPCWSTR, DWORD, i32, i32, i32, i32, HWND, HMENU, HINSTANCE, LPVOID);

static PFN_CreateWindowExA fn_CreateWindowExA = nullptr;
static PFN_CreateWindowExW fn_CreateWindowExW = nullptr;

static std::string getModulePathA(HMODULE hModule) {
  std::string buf(MAX_PATH, L'\0');
  for (;;) {
    const DWORD n = GetModuleFileNameA(
      hModule, buf.data(), static_cast<DWORD>(buf.size()));
    if (n == 0)
      return {};
    if (n < buf.size()) {
      buf.resize(n);
      return buf;
    }
    buf.resize(buf.size() * 2);
  }
}

// Get the file name (without directory) of the current executable.
static std::string getProcessNameA() {
  const std::string path = getModulePathA(nullptr);
  const size_t pos = path.find_last_of("\\/");
  if (pos == std::string::npos)
    return path;
  return path.substr(pos + 1);
}

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
  status.baseAddr = (void *)GetModuleHandleA(getProcessNameA().c_str());
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

// Return non-zero if the executable name of current process starts with
// HTTexts_ExeNamePrefixA, so that renamed builds are recognized as MCBE too.
int HTi_ImplMCBE_ExpectProcess() {
  const std::string name = getProcessNameA();
  if (name.empty())
    return 0;

  return _strnicmp(name.c_str(), HT_ImplMCBE_ExecutablePrefix, sizeof(HT_ImplMCBE_ExecutablePrefix) - 1) == 0;
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

  // Use the actual executable name, which may have a different postfix.
  const std::string processName = getProcessNameA();
  HTiSetGameProcessName(processName.c_str());

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
