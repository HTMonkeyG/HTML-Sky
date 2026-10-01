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

  // Paths are std::string / std::wstring now, so there is no fixed-buffer
  // truncation to guard against.
  gPathDataWide = dataDir;
  gPathModsWide = modsDir;
  gPathDll = HTiWstringToUtf8(dllDir.c_str());
  gPathGameExe = HTiWstringToUtf8(gameDir.c_str());
  gPathData = HTiWstringToUtf8(dataDir.c_str());
  gPathMods = HTiWstringToUtf8(modsDir.c_str());

  // ImGui uses UTF-8 codepage in paths, so we need the conversion below.
  std::wstring guiPath = dataDir + L"\\htmlgui.ini";
  gPathGuiIni = HTiWstringToUtf8(guiPath.c_str());

  return 1;
}

// Create the htmodloader data folders. Only done once a supported game has been
// confirmed, so we never scatter folders into unrelated processes' directories.
static void ensureDataFolders() {
  if (!HTiFolderExists(gPathDataWide.c_str()))
    CreateDirectoryW(gPathDataWide.c_str(), nullptr);
  if (!HTiFolderExists(gPathModsWide.c_str()))
    CreateDirectoryW(gPathModsWide.c_str(), nullptr);
}

static DWORD WINAPI onAttach(
  LPVOID lpParam
) {
  HMODULE hModule = (HMODULE)lpParam;

  // Derive paths first: we need them to know where to write the log and where
  // html-config.json lives.
  if (!initPaths(hModule))
    return 0;

#ifdef HTML_ENABLE_LOGGER
  // Per-process log file, named after the host executable. The DLL is loaded by
  // every process that pulls in this winhttp.dll from the game folder (the game
  // plus helpers like crashpad_handler.exe), and they all share one folder, so
  // a single html-log.log would be truncated and interleaved by concurrent
  // writers. One file per executable keeps each process's log intact.
  wchar_t exePath[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  std::wstring exeStem = exePath;
  size_t sep = exeStem.find_last_of(L"\\/");
  if (sep != std::wstring::npos)
    exeStem = exeStem.substr(sep + 1);
  size_t dot = exeStem.find_last_of(L'.');
  if (dot != std::wstring::npos)
    exeStem = exeStem.substr(0, dot);

  std::wstring logPath = HTiUtf8ToWstring(gPathDll.c_str())
    + L"\\html-log-" + exeStem + L".log";
  HTiInitLogger(logPath.c_str(), 0);
#endif
  LOGI("HTML attached.\n");

  // Apply html-config.json overrides (target executable / forced backend)
  // before deciding whether this process hosts a supported game.
  HTiLoadLoaderConfig();

  if (!HTiBackendExpectProcess()) {
#ifdef HTML_ENABLE_LOGGER
    WLOGW(L"No supported game detected in process \"%ls\"; HTML will not "
      L"activate. If your game executable was renamed, set "
      L"\"ht_mod_loader.target_executable\" in html-config.json.\n", exePath);
#endif
    return 0;
  }

  // Supported game confirmed: create the data folders now (never for unrelated
  // processes).
  ensureDataFolders();

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

    // Persist options on ANY detach, including normal process exit (where
    // lpReserved != NULL and we return early below). This is best-effort and
    // never blocks, so it is safe even here in DllMain: the previous code only
    // saved on the rarely-taken dynamic-unload path, silently dropping the
    // last changes on a normal game shutdown.
    if (gLoaderInitialized.load())
      HTiOptionsFlushBestEffort();

    if (lpReserved)
      return TRUE;

    // Dynamic FreeLibrary unload only (lpReserved == NULL). Note: MinHook's
    // uninitialize suspends other threads, which is a loader-lock hazard in
    // DllMain; this path is essentially never taken for a winhttp proxy (the
    // game holds the DLL for its whole lifetime), so it is left as-is.
    if (gLoaderInitialized.load() && gInitThread
        && WaitForSingleObject(gInitThread, 0) == WAIT_OBJECT_0) {
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
