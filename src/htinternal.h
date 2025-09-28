#ifndef __INTERNAL_H__
#define __INTERNAL_H__

#include <windows.h>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <mutex>
#include <unordered_map>

#include "imgui.h"

#include "includes/htmodloader.h"
#include "htaliases.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// [SECTION] Mod loader logger.
// ----------------------------------------------------------------------------

#define LOGI(format, ...) HTLogA("[INFO] " format, ##__VA_ARGS__)
#define WLOGI(format, ...) HTLogW(L"[INFO] " format, ##__VA_ARGS__)
#define LOGW(format, ...) HTLogA("[WARN] " format, ##__VA_ARGS__)
#define WLOGW(format, ...) HTLogW(L"[WARN] " format, ##__VA_ARGS__)
#define LOGE(format, ...) HTLogA("[ERR] " format, ##__VA_ARGS__)
#define WLOGE(format, ...) HTLogW(L"[ERR] " format, ##__VA_ARGS__)
#define LOGEF(format, ...) HTLogA("[ERR][FATAL] " format, ##__VA_ARGS__)
#define WLOGEF(format, ...) HTLogW(L"[ERR][FATAL] " format, ##__VA_ARGS__)

void HTInitLogger(const wchar_t *fileName, i08 allocConsole);
void HTLogA(const char *format, ...);
void HTLogW(const wchar_t *format, ...);

// ----------------------------------------------------------------------------
// [SECTION] Mod loader globals and internal functions.
// ----------------------------------------------------------------------------

#define HTSetErrorAndReturn(e, v) (HTSetLastError(e), v)

extern HTGameStatus gGameStatus;
extern char gPathDll[MAX_PATH]
  , gPathGameExe[MAX_PATH]
  , gPathLayerConfig[MAX_PATH]
  , gPathData[MAX_PATH]
  , gPathMods[MAX_PATH]
  , gPathGuiIni[MAX_PATH];
extern wchar_t gPathModsWide[MAX_PATH]
  , gPathDataWide[MAX_PATH];
extern HANDLE gHeap
  , gEventGuiInit;
extern HMODULE gModLoaderHandle;

// Convert string from wchar_t to UTF-8.
static inline void wcstoutf8(const wchar_t *wcs, char *utf8, i32 max) {
  char *buf;
  i32 size;

  if (!wcs || !utf8)
    return;

  *utf8 = 0;
  size = WideCharToMultiByte(CP_UTF8, 0, wcs, -1, NULL, 0, NULL, NULL);
  buf = (char *)malloc(size);
  if (!buf)
    return;
  WideCharToMultiByte(CP_UTF8, 0, wcs, -1, buf, size, NULL, NULL);
  strcpy_s(utf8, max, buf);
  free(buf);
  utf8[max - 1] = 0;
}

// Convert UTF-8 to UTF-16LE, returned std::wstring.
static inline std::wstring utf8ToWchar(const char *input) {
  if (!input)
    return std::wstring();
  u64 len = strlen(input);
  std::wstring result;
  i32 size = MultiByteToWideChar(CP_UTF8, 0, input, len, nullptr, 0);
  result.resize(size);
  MultiByteToWideChar(CP_UTF8, 0, input, len, &result[0], size);
  return result;
}

// Convert UTF-16LE to UTF-8, returned std::string.
static inline std::string wcharToUtf8(const wchar_t *input) {
  if (!input)
    return std::string();
  std::string result;
  i32 size = WideCharToMultiByte(CP_UTF8, 0, input, -1, nullptr, 0, nullptr, nullptr);
  result.resize(size);
  WideCharToMultiByte(CP_UTF8, 0, input, -1, &result[0], size, nullptr, nullptr);
  return result;
}

// Read file as UTF-8 encoding with _wfopen.
static inline std::string HTiReadFileAsUtf8(std::wstring path) {
  std::string buffer;
  FILE *fd = _wfopen(path.c_str(), L"rb");
  u64 size;

  if (!fd)
    return std::string();

  fseek(fd, 0, SEEK_END);
  size = ftell(fd);
  rewind(fd);
  buffer.resize(size + 1);
  fread(buffer.data(), sizeof(char), size, fd);
  buffer[size] = 0;
  fclose(fd);

  return buffer;
}

// Check if the given file exists.
static inline i32 HTiFileExists(const wchar_t *path) {
  DWORD attr = GetFileAttributesW(path);
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
    return 0;
  return 1;
}

// Check if the given folder exists.
static inline i32 HTiFolderExists(const wchar_t *path) {
  DWORD attr = GetFileAttributesW(path);
  if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    return 0;
  return 1;
}

// Scan and load all mods.
HTStatus HTLoadMods();
// [Invalid] Load a mod and its dependencies.
void HTLoadSingleMod();
// [Invalid] Unload a mod and its dependents.
void HTUnloadSingleMod();
// [Invalid] Inject a dll.
HTStatus HTInjectDll(
  const wchar_t *path);
// [Invalid] Free a dll.
HTStatus HTRejectDll();

// ----------------------------------------------------------------------------
// [SECTION] Mod data and handle declarations.
// ----------------------------------------------------------------------------

// Handle types.
typedef enum {
  HTHandleType_Invalid = 0,
  HTHandleType_Manifest,
  HTHandleType_Mod,
  HTHandleType_Hotkey,
  HTHandleType_Command
} HTHandleType;

// Paths to related files of the mod.
struct ModPaths {
  // The folder contains manifest.json.
  std::wstring folder;
  // "main" in manifest.json. The name of the main executable file of the mod.
  std::wstring dll;
  // The path to manifest.json.
  std::wstring json;
};

// Identifier data of the mod.
struct ModMeta {
  // This name is used to identify mods and add dependencies, and must be
  // unique for every mod.
  std::string packageName;
  // The version number of the mod.
  u32 version[3];
};

struct ModRuntime;
// Mod manifest data. This struct is associated with package name.
struct ModManifest {
  // Mod identification data.
  ModMeta meta;
  // Paths to the related files of the mod.
  ModPaths paths;
  // This name is used to display mod basic information.
  std::string modName;
  // The description of the mod.
  std::string description;
  // The author of the mod.
  std::string author;
  // Game edition the mod supports.
  u08 gameEditionFlags;
  // Dependencies of the mod.
  std::vector<ModMeta> dependencies;
  // Mod runtime data.
  ModRuntime *runtime;
};

// HTML expected functions.
struct ModInternalFunctions {
  PFN_HTModOnInit pfn_HTModOnInit;
  PFN_HTModOnEnable pfn_HTModOnEnable;
  PFN_HTModRenderGui pfn_HTModRenderGui;
};

// Registered key bind of a mod.
struct ModKeyBind {
  // Key internal identifier. May be prewritten.
  std::string keyName;
  // Key code to be actually detected. May be prewritten.
  HTKeyCode key;
  // Display name of the key. In current version, it's simply equal to keyName.
  std::string displayName;
  // Default key code.
  HTKeyCode defaultKey;
  // Callback to be called when the state of this key is changed.
  PFN_HTHotkeyCallback listener;
  // Whether the key is explictly registered. False when the struct is set by 
  // options reader.
  u08 isRegistered;
  // Key binding config.
  HTHotkeyFlags flags;
  // True when the key is down.
  u08 isDown;
};

// Mod runtime data. This struct is associated with mod handle.
struct ModRuntime {
  HMODULE handle;
  ModManifest *manifest;
  ModInternalFunctions loaderFunc;
  // Shared functions.
  std::map<std::string, PFN_HTVoidFunction> functions;
  // Registered hotkeys.
  std::map<std::string, ModKeyBind> keyBinds; 
};

extern std::map<std::string, ModManifest> gModDataLoader;
extern std::map<HMODULE, ModRuntime> gModDataRuntime;
extern std::unordered_map<HTHandle, HTHandleType> gHandleTypes;
// We use a single global lock to protect all of the above structs simply.
extern std::mutex gModDataLock;

// We assume that the API function has already obtained the mutex when calling
// the following function.
static inline ModRuntime *HTiGetModRuntime(
  HMODULE hModule
) {
  auto it = gModDataRuntime.find(hModule);
  if (it == gModDataRuntime.end())
    return nullptr;
  return &it->second;
}

// Register a handle as given type.
static inline bool HTiRegisterHandle(
  HTHandle handle,
  HTHandleType type
) {
  if (!handle)
    return false;
  auto it = gHandleTypes.find(handle);
  if (it != gHandleTypes.end())
    return false;
  gHandleTypes[handle] = type;
  return true;
}

// Check if the handle is registered as given type.
static inline bool HTiCheckHandleType(
  HTHandle handle,
  HTHandleType type
) {
  auto it = gHandleTypes.find(handle);
  if (it == gHandleTypes.end())
    return false;
  if (type == HTHandleType_Invalid)
    return true;
  return it->second == type;
}

// ----------------------------------------------------------------------------
// [SECTION] Option loader declarations.
// ----------------------------------------------------------------------------

struct ModLoaderOptions {
  // Mod options is related with package name.
  std::map<std::string, ModRuntime> modOptions;
};

extern ModLoaderOptions gModLoaderOptions;

// Load options from the specified file.
HTStatus HTiOptionsLoadFromFile(
  const wchar_t *);
// Try to assign the loaded options for a mod's runtime data.
void HTiOptionsLoadFor(
  ModRuntime *);
// Mark as needing to save options.
void HTiOptionsMarkDirty();
// Save all options to gModLoaderOptions. Called by HTiUpdateGUI().
void HTiOptionsUpdate(
  f32);
// Write options to the specified file.
void HTiOptionsWriteToFile(
  const wchar_t *);

// ----------------------------------------------------------------------------
// [SECTION] Bootstrap declarations.
// ----------------------------------------------------------------------------

extern HTHandle hKeyMenuToggle;

// Register the loader itself as a single mod. The package name of the loader
// is "htmodloader", version is HTML_VERSION_NAME.
void HTBootstrap();

// ----------------------------------------------------------------------------
// [SECTION] Graphic declarations.
// ----------------------------------------------------------------------------

extern bool gShowMainMenu
  , gShowDebugger;

// Initialize ImGui style and colors.
void HTiInitGUI();
// Destroy ImGui context.
void HTiDeinitGUI();
// Show all registered windows. Referenced by layer.cpp.
void HTiUpdateGUI();
// Render HTML windows. Referenced by bootstrap.cpp.
void HTiRenderGUI(
  f32, void *);

// Toggle main menu display status. Referenced by bootstrap.cpp, the callback
// of hKeyMenuToggle.
void HTiToggleMenuState(
  HTKeyEvent *);

// Submenus.
void HTMenuAbouts();
void HTMenuConsole();
void HTMenuModList();
void HTMenuSettings();

// ImGui windows
void HTWindowDebugger(
  bool *show);
void HTWindowMain(
  bool *show);

// Console functions.
void HTClearConsole();
void HTAddConsoleLine(
  const char* fmt, ...);

// ----------------------------------------------------------------------------
// [SECTION] Input handler related functions.
// ----------------------------------------------------------------------------

// Modified from ImGui. Check whether a key has name string.
static inline bool HTiIsNamedKey(HTKeyCode key) {
  return key >= HTKey_NamedKey_BEGIN && key < HTKey_NamedKey_END;
}

// Install and uninstall window message detour.
void HTInstallInputHook();
void HTUninstallInputHook();

// Map HTKeyCode to ImGuiKey.
// NOTE: ImGuiKey can't convert to HTKeyCode.
ImGuiKey HTKeyToImGuiKey(
  HTKeyCode key);

// ----------------------------------------------------------------------------
// [SECTION] Key event functions.
// ----------------------------------------------------------------------------

// Call key event callbacks.
void HTHotkeyDispatch(
  HTKeyCode key, HTKeyEventFlags flags, u08 *userSetBlocked);

// Set key event cooldown.
void HTHotkeyUpdateCooldown();
void HTHotkeySetCooldown();

#ifdef __cplusplus
}
#endif

#endif
