// ----------------------------------------------------------------------------
// DLL entry point and initializer of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <windows.h>
#include <ntstatus.h>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include "MinHook.h"

#include "proxy/winhttp-proxy.h"
#include "utils/texts.h"
#include "htinternal.hpp"

static HMODULE hWinHttp;

static HMODULE loadSystemWinHttp() {
  wchar_t systemDir[MAX_PATH] = {0};
  UINT len = GetSystemDirectoryW(systemDir, MAX_PATH);
  if (!len || len >= MAX_PATH)
    return nullptr;
  std::wstring path(systemDir, len);
  path += L"\\winhttp.dll";
  return LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
}

/**
 * Get path to the dll and the layer config file.
 */
static i32 initPaths(
  HMODULE hModule
) {
  wchar_t dllFile[MAX_PATH] = {0};
  wchar_t gameFile[MAX_PATH] = {0};
  DWORD dllLen = GetModuleFileNameW(hModule, dllFile, MAX_PATH);
  DWORD gameLen = GetModuleFileNameW(nullptr, gameFile, MAX_PATH);
  if (!dllLen || !gameLen || dllLen >= MAX_PATH || gameLen >= MAX_PATH)
    return 0;

  std::wstring dllPath(dllFile, dllLen);
  std::wstring gamePath(gameFile, gameLen);
  size_t dllSep = dllPath.find_last_of(L"\\/");
  size_t gameSep = gamePath.find_last_of(L"\\/");
  if (dllSep == std::wstring::npos || gameSep == std::wstring::npos)
    return 0;

  std::wstring dllDir = dllPath.substr(0, dllSep);
  std::wstring gameDir = gamePath.substr(0, gameSep);
  std::wstring dataDir = gameDir + L"\\htmodloader";
  std::wstring modsDir = dataDir + L"\\mods";
  if (dllDir.size() >= MAX_PATH || gameDir.size() >= MAX_PATH
      || dataDir.size() >= MAX_PATH || modsDir.size() >= MAX_PATH)
    return 0;

  wcsncpy_s(gPathDataWide, MAX_PATH, dataDir.c_str(), _TRUNCATE);
  wcsncpy_s(gPathModsWide, MAX_PATH, modsDir.c_str(), _TRUNCATE);
  if (WideCharToMultiByte(CP_UTF8, 0, dllDir.c_str(), -1,
      gPathDll, MAX_PATH, nullptr, nullptr) <= 0
      || WideCharToMultiByte(CP_UTF8, 0, gameDir.c_str(), -1,
      gPathGameExe, MAX_PATH, nullptr, nullptr) <= 0
      || WideCharToMultiByte(CP_UTF8, 0, dataDir.c_str(), -1,
      gPathData, MAX_PATH, nullptr, nullptr) <= 0
      || WideCharToMultiByte(CP_UTF8, 0, modsDir.c_str(), -1,
      gPathMods, MAX_PATH, nullptr, nullptr) <= 0)
    return 0;

  // Create mod data folders.
  if (!HTiFolderExists(gPathDataWide))
    CreateDirectoryW(gPathDataWide, nullptr);
  if (!HTiFolderExists(gPathModsWide))
    CreateDirectoryW(gPathModsWide, nullptr);

  // ImGui uses UTF-8 codepage in paths, so we need the conversion below.
  std::wstring guiPath = dataDir + L"\\htmlgui.ini";
  if (guiPath.size() >= MAX_PATH)
    return 0;
  wcstoutf8(guiPath.c_str(), gPathGuiIni, MAX_PATH);

  return 1;
}

static DWORD WINAPI onAttach(
  LPVOID lpParam
) {
  HMODULE hModule = (HMODULE)lpParam;

  if (!HTiBackendExpectProcess() || !initPaths(hModule))
    return 0;

#ifdef HTML_ENABLE_LOGGER
  HTiInitLogger(L"html-log.log", 0);
#endif
  LOGI("HTML attached.\n");

  if (MH_Initialize() != MH_OK)
    return 0;
  gHeap = HeapCreate(0, 0, 0);
  gEventGuiInit = CreateEventA(nullptr, 0, 0, nullptr);
  if (!gHeap || !gEventGuiInit)
    return 0;
  HTiInitLDB();
  HTiBackendSetupAll();
  gLoaderInitialized.store(true);

  // Enable mods after the menu is created.
  if (WaitForSingleObject(gEventGuiInit, 30000) == WAIT_TIMEOUT
      && !gLoaderShuttingDown.load()) {
    // The most annoying error message of hSC Plugin LOL :P
    // This error is considered "NEVER TRIGGERED".
    LOGEF("Gui init timed out after 30 seconds.\n");
    return 0;
  }

  return 0;
}

BOOL APIENTRY DllMain(
  HMODULE hModule,
  DWORD dwReason,
  LPVOID lpReserved
) {
  if (dwReason == DLL_PROCESS_ATTACH) {
    gModLoaderHandle = hModule;
    DisableThreadLibraryCalls(hModule);
    gLoaderShuttingDown.store(false);

    // Build proxy dispatch table.
    hWinHttp = loadSystemWinHttp();
    if (hWinHttp)
      proxy_importFunctions(hWinHttp);

    gInitThread = CreateThread(
      nullptr, 0, onAttach, (LPVOID)hModule, 0, nullptr);
  } else if (dwReason == DLL_PROCESS_DETACH) {
    gLoaderShuttingDown.store(true);
    if (gEventGuiInit)
      SetEvent(gEventGuiInit);

    if (lpReserved)
      return TRUE;

    // Forcely update all options.
    if (gLoaderInitialized.load() && gInitThread
        && WaitForSingleObject(gInitThread, 0) == WAIT_OBJECT_0) {
      HTiOptionsUpdate(114514.1919810f);
      HTiDeinitLDB();
      MH_DisableHook(MH_ALL_HOOKS);
      MH_Uninitialize();
      CloseHandle(gInitThread);
      gInitThread = nullptr;
      if (gEventGuiInit) {
        CloseHandle(gEventGuiInit);
        gEventGuiInit = nullptr;
      }
      if (gHeap) {
        HeapDestroy(gHeap);
        gHeap = nullptr;
      }
      gLoaderInitialized.store(false);
    }
    if (hWinHttp)
      FreeLibrary(hWinHttp);
  }

  return TRUE;
}
