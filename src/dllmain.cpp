// ----------------------------------------------------------------------------
// DLL entry point and initializer of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <windows.h>
#include <ntstatus.h>
#include <stdio.h>
#include <unordered_map>
#include "MinHook.h"

#include "proxy/winhttp-proxy.h"
#include "utils/texts.h"
#include "htinternal.hpp"

static HMODULE hWinHttp;

/**
 * Get path to the dll and the layer config file.
 */
static i32 initPaths(
  HMODULE hModule
) {
  char *p;
  wchar_t tmp[MAX_PATH];

  GetModuleFileNameA(hModule, gPathDll, MAX_PATH);
  GetModuleFileNameA(nullptr, gPathGameExe, MAX_PATH);

  p = strrchr(gPathDll, '\\');
  if (!p)
    return 0;
  *p = 0;

  p = strrchr(gPathGameExe, '\\');
  if (!p)
    return 0;
  *p = 0;
  strcpy(gPathData, gPathGameExe);
  strcat(gPathData, "\\htmodloader");

  strcpy(gPathMods, gPathData);
  strcat(gPathMods, "\\mods");

  // We assume that the path is shorter than MAX_PATH.
  MultiByteToWideChar(
    CP_ACP,
    MB_PRECOMPOSED,
    gPathData,
    strlen(gPathData),
    gPathDataWide,
    MAX_PATH);
  MultiByteToWideChar(
    CP_ACP,
    MB_PRECOMPOSED,
    gPathMods,
    strlen(gPathMods),
    gPathModsWide,
    MAX_PATH);

  // Create mod data folders.
  if (!HTiFolderExists(gPathDataWide))
    CreateDirectoryW(gPathDataWide, nullptr);
  if (!HTiFolderExists(gPathModsWide))
    CreateDirectoryW(gPathModsWide, nullptr);

  // ImGui uses UTF-8 codepage in paths, so we need the conversion below.
  wcscpy(tmp, gPathDataWide);
  wcscat(tmp, L"\\htmlgui.ini");
  wcstoutf8(tmp, gPathGuiIni, MAX_PATH);

  return 1;
}

static DWORD WINAPI onAttach(
  LPVOID lpParam
) {
  HMODULE hModule = (HMODULE)lpParam;

  (void)hModule;

  // Create an independent heap.
  gHeap = HeapCreate(0, 0, 0);
  gEventGuiInit = CreateEventA(nullptr, 0, 0, nullptr);
  HTiInitLDB();

  // Enable mods after the menu is created.
  if (WaitForSingleObject(gEventGuiInit, 30000) == WAIT_TIMEOUT) {
    // The most annoying error message of hSC Plugin LOL :P
    // This error is considered "NEVER TRIGGERED".
    LOGEF("Gui init timed out after 30 seconds.\n");
    return 0;
  }

  HTiEnableMods();

  return 0;
}

BOOL APIENTRY DllMain(
  HMODULE hModule,
  DWORD dwReason,
  LPVOID lpReserved
) {
  if (dwReason == DLL_PROCESS_ATTACH) {
    gModLoaderHandle = hModule;

    // Build proxy dispatch table.
    hWinHttp = LoadLibraryA("C:\\Windows\\System32\\winhttp.dll");
    proxy_importFunctions(hWinHttp);

    if (!HTiBackendExpectProcess())
      // Not the correct game process, act as winhttp.dll.
      return TRUE;

    initPaths(hModule);

    // No log file and console by default.
#ifdef HTML_ENABLE_LOGGER
    HTiInitLogger(L"html-log.log", 0);
#endif
    LOGI("HTML attatched.\n");

    MH_Initialize();

    HTiBackendSetupAll();

    CreateThread(
      nullptr, 0, onAttach, (LPVOID)hModule, 0, nullptr);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    // Forcely update all options.
    HTiOptionsUpdate(114514.1919810f);
    HTiDeinitLDB();
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    FreeLibrary(hWinHttp);
  }

  return TRUE;
}
