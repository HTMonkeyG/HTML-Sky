#include "dllmain.h"

typedef LONG (WINAPI *PFN_RegEnumValueA)(
  HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef NTSTATUS (WINAPI *PFN_NtQueryKey)(
  HANDLE, u64, PVOID, ULONG, PULONG);

static PFN_RegEnumValueA fn_RegEnumValueA;
static std::unordered_map<HKEY, DWORD> gRegKeys;
static char gDllPath[MAX_PATH]
  , gLayerConfigPath[MAX_PATH];

/**
 * Get path to the dll and the layer config file.
 */
static i32 initPaths(HMODULE hModule) {
  char *p;
  if (!GetModuleFileNameA(hModule, gDllPath, MAX_PATH))
    return 0;
  p = strrchr(gDllPath, '\\');
  if (!p)
    return 0;
  *p = 0;
  strcpy(gLayerConfigPath, gDllPath);
  strcat(gLayerConfigPath, "\\html-config.json");
  return 1;
}

/**
 * Check if the key name is a Vulkan implicit layer list.
 */
static i32 checkKeyName(HKEY key) {
  HMODULE ntdll = ::GetModuleHandleA("ntdll.dll");
  PFN_NtQueryKey fn_NtQueryKey;
  DWORD size = 0;
  NTSTATUS result = STATUS_SUCCESS;
  wchar_t *buffer;
  i32 r = 0;

  if (!key || !ntdll)
    return 0;

  fn_NtQueryKey = (PFN_NtQueryKey)::GetProcAddress(ntdll, "NtQueryKey");
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
      // The layer loader is visiting the layer list.
      // Set the key as Vulkan implicit layer.
      gRegKeys[hKey] = 1;

      // Inject the layer.
      if (lpValueName)
        strcpy(lpValueName, gLayerConfigPath);
      if (lpcchValueName)
        *lpcchValueName = strlen(gLayerConfigPath) + 1;
      if (lpType)
        *lpType = REG_DWORD;
      if (lpData)
        *((i32 *)lpData) = 0;
      if (lpcbData)
        *lpcbData = 4;

      return ERROR_SUCCESS;
    } else
      gRegKeys[hKey] = 2;
  }

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
    gRegKeys.erase(hKey);

  return result;
}

BOOL APIENTRY DllMain(
  HMODULE hModule,
  DWORD dwReason,
  LPVOID lpReserved
) {
  if (dwReason == DLL_PROCESS_ATTACH) {
    HMODULE hWinHttp = LoadLibraryA("C:\\Windows\\System32\\winhttp.dll");
    proxy_importFunctions(hWinHttp);
    initPaths(hModule);

    FreeConsole();
    AllocConsole();
    freopen("CONOUT$", "w+t", stdout);
    freopen("CONIN$", "r+t", stdin);

    MH_Initialize();
    MH_CreateHookApi(
      L"advapi32.dll",
      "RegEnumValueA",
      (void *)hook_RegEnumValueA,
      (void **)&fn_RegEnumValueA
    );
    MH_EnableHook(MH_ALL_HOOKS);
  } else if (dwReason == DLL_PROCESS_DETACH) {
  }

  return TRUE;
}
