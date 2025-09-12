// ----------------------------------------------------------------------------
// DLL entry point and initializer of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include "dllmain.h"

typedef LONG (WINAPI *PFN_RegEnumValueA)(
  HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef NTSTATUS (WINAPI *PFN_NtQueryKey)(
  HANDLE, u64, PVOID, ULONG, PULONG);

static PFN_RegEnumValueA fn_RegEnumValueA;
static std::unordered_map<HKEY, DWORD> gRegKeys;
static HMODULE hWinHttp;

/**
 * Get path to the dll and the layer config file.
 */
static i32 initPaths(HMODULE hModule) {
  char *p;

  GetModuleFileNameA(hModule, gPathDll, MAX_PATH);
  GetModuleFileNameA(nullptr, gPathGameExe, MAX_PATH);

  p = strrchr(gPathDll, '\\');
  if (!p)
    return 0;
  *p = 0;
  strcpy(gPathLayerConfig, gPathDll);
  strcat(gPathLayerConfig, "\\html-config.json");

  p = strrchr(gPathGameExe, '\\');
  if (!p)
    return 0;
  *p = 0;
  strcpy(gPathMods, gPathGameExe);
  strcat(gPathMods, "\\htmods");

  // We assume that the path is shorter than MAX_PATH.
  MultiByteToWideChar(
    CP_ACP,
    MB_PRECOMPOSED,
    gPathMods,
    strlen(gPathMods),
    gPathModsWide,
    MAX_PATH);

  return 1;
}

/**
 * Check if the key name is a Vulkan implicit layer list.
 */
static i32 checkKeyName(HKEY key) {
  HMODULE ntdll = GetModuleHandleA("ntdll.dll");
  PFN_NtQueryKey fn_NtQueryKey;
  DWORD size = 0;
  NTSTATUS result = STATUS_SUCCESS;
  wchar_t *buffer;
  i32 r = 0;

  if (!key || !ntdll)
    return 0;

  fn_NtQueryKey = (PFN_NtQueryKey)GetProcAddress(ntdll, "NtQueryKey");
  if (!fn_NtQueryKey)
    return 0;

  result = fn_NtQueryKey(key, 3, 0, 0, &size);
  if (result == STATUS_BUFFER_TOO_SMALL) {
    buffer = (wchar_t *)malloc(size + 2);
    if (!buffer)
      return 0;

    result = fn_NtQueryKey(key, 3, buffer, size, &size);
    if (result == STATUS_SUCCESS)
      buffer[size / sizeof(wchar_t)] = 0;
    
    r = !wcscmp(buffer + 2, L"\\REGISTRY\\MACHINE\\SOFTWARE\\Khronos\\Vulkan\\ImplicitLayers");
    free(buffer);
  }

  return r;
}

/**
 * Inject HTML layer on index 0.
 */
static LONG WINAPI hook_RegEnumValueA(
  HKEY hKey,
  DWORD dwIndex,
  LPSTR lpValueName,
  LPDWORD lpcchValueName,
  LPDWORD lpReserved,
  LPDWORD lpType,
  LPBYTE lpData,
  LPDWORD lpcbData
) {
  LONG result;
  auto it = gRegKeys.find(hKey);
  bool notSaved = it == gRegKeys.end();

  if (notSaved && !dwIndex) {
    // The handle isn't recorded and it's the first call on this key.
    if (checkKeyName(hKey)) {
      // Set the current registry handle as access for Vulkan layer loader.
      gRegKeys[hKey] = 1;

      // Inject the layer.
      if (lpValueName)
        strcpy(lpValueName, gPathLayerConfig);
      if (lpcchValueName)
        *lpcchValueName = strlen(gPathLayerConfig) + 1;
      if (lpType)
        *lpType = REG_DWORD;
      if (lpData)
        *((i32 *)lpData) = 0;
      if (lpcbData)
        *lpcbData = 4;

      return ERROR_SUCCESS;
    } else
      // Set the current registry handle as regular access.
      gRegKeys[hKey] = 2;
  }

  // Return the enumerate result.
  result = fn_RegEnumValueA(
    hKey,
    (!notSaved && gRegKeys[hKey] == 1) ? dwIndex - 1 : dwIndex,
    lpValueName,
    lpcchValueName,
    lpReserved,
    lpType,
    lpData,
    lpcbData);
  if (result == ERROR_NO_MORE_ITEMS)
    // Enumeration ended.
    gRegKeys.erase(hKey);

  return result;
}

/**
 * Find the game window.
 */
static BOOL CALLBACK enumWndProc(HWND hWnd, LPARAM lParam) {
  DWORD pid;
  HTGameEdition edition = HT_EDITION_UNKNOWN;
  wchar_t buffer[32];
  HWND *result = (HWND *)lParam;

  // Check the pid of the window.
  GetWindowThreadProcessId(hWnd, &pid);
  if (pid != gGameStatus.pid)
    return 1;

  // Get the game edition from window name.
  GetWindowTextW(hWnd, buffer, 32);
  buffer[31] = 0;
  if (!wcscmp(buffer, L"光·遇"))
    edition = HT_EDITION_CHINESE;
  else if (!wcscmp(buffer, L"Sky"))
    edition = HT_EDITION_INTERNATIONAL;
  else
    return 1;

  // Check the window's class name.
  GetClassNameW(hWnd, buffer, 32);
  buffer[31] = 0;
  if (wcscmp(buffer, L"TgcMainWindow"))
    return 1;
  
  *result = hWnd;
  gGameStatus.edition = edition;
  gGameStatus.window = hWnd;
  return 0;
}

static DWORD WINAPI onAttach(LPVOID lpParam) {
  HMODULE hModule = (HMODULE)lpParam;
  HWND gameWnd = nullptr;

  (void)hModule;

  // Create an independent heap.
  gHeap = HeapCreate(0, 0, 0);
  gEventGuiInit = CreateEventA(nullptr, 0, 0, nullptr);

  // Find the game window and game edition.
  while (!gameWnd) {
    Sleep(250);
    EnumWindows(enumWndProc, (LPARAM)&gameWnd);
  }

  // Load mods after the menu is created.
  WaitForSingleObject(gEventGuiInit, 30000);
  HTLoadMods();

  return 0;
}

BOOL APIENTRY DllMain(
  HMODULE hModule,
  DWORD dwReason,
  LPVOID lpReserved
) {
  if (dwReason == DLL_PROCESS_ATTACH) {
    // Build proxy dispatch table.
    hWinHttp = LoadLibraryA("C:\\Windows\\System32\\winhttp.dll");
    proxy_importFunctions(hWinHttp);

    gGameStatus.baseAddr = (void *)GetModuleHandleA("Sky.exe");
    if (!gGameStatus.baseAddr)
      // Not the correct game process, act as winhttp.dll.
      return TRUE;
    gGameStatus.pid = GetCurrentProcessId();

    HTInitLogger(nullptr, 0);
    initPaths(hModule);

    LOGI("HTML attatched.\n");
    LOGI("Game info: \n");
    LOGI("  pid = 0x%lu\n", gGameStatus.pid);
    LOGI("  baseAddr = 0x%p\n", gGameStatus.baseAddr);

    MH_Initialize();
    MH_CreateHookApi(
      L"advapi32.dll",
      "RegEnumValueA",
      (void *)hook_RegEnumValueA,
      (void **)&fn_RegEnumValueA
    );
    MH_EnableHook(MH_ALL_HOOKS);

    CreateThread(
      nullptr, 0, onAttach, (LPVOID)hModule, 0, nullptr);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    FreeLibrary(hWinHttp);
  }

  return TRUE;
}
