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

#include "includes/aliases.h"
#include "includes/htmodloader.h"
#include "utils/logger.h"
#include "utils/globals.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// [SECTION] Mod loader functions.
// ----------------------------------------------------------------------------

static inline i32 fileExists(const wchar_t *path) {
  DWORD attr = GetFileAttributesW(path);
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
    return 0;
  return 1;
}

static inline i32 folderExists(const wchar_t *path) {
  DWORD attr = GetFileAttributesW(path);
  if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    return 0;
  return 1;
}

HTStatus HTLoadMods();
void HTLoadSingleMod();
void HTUnloadSingleMod();
HTStatus HTInjectDll(
  const wchar_t *path);
HTStatus HTRejectDll();

// ----------------------------------------------------------------------------
// [SECTION] Mod data and handle declarations.
// ----------------------------------------------------------------------------

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
static inline ModRuntime *getModRuntime(
  HMODULE hModule
) {
  auto it = gModDataRuntime.find(hModule);
  if (it == gModDataRuntime.end())
    return nullptr;
  return &it->second;
}

static inline bool registerHandle(
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

static inline bool checkHandleType(
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
// [SECTION] Bootstrap declarations.
// ----------------------------------------------------------------------------

extern HTHandle hKeyMenuToggle;

/**
 * Register the loader itself as a single mod. The package name of the loader
 * is "htmodloader", version is HTML_VERSION_NAME.
 */
void HTBootstrap();

// ----------------------------------------------------------------------------
// [SECTION] Graphic declarations.
// ----------------------------------------------------------------------------

/**
 * Show console.
 */
void HTMenuConsole();

/**
 * Initialize ImGui style and colors.
 */
void HTInitGUI();

/**
 * Destroy ImGui context.
 */
void HTDeinitGUI();

/**
 * Show all registered windows. Referenced by layer.cpp.
 */
void HTUpdateGUI();

/**
 * Render HTML main menu. Referenced by bootstrap.cpp.
 */
void HTMainMenu(
  f32, void *);

/**
 * Toggle main menu display status. Referenced by bootstrap.cpp, the callback
 * of hKeyMenuToggle.
 */
void HTToggleMenuState(
  HTKeyEvent *);

// ----------------------------------------------------------------------------
// [SECTION] Input related functions.
// ----------------------------------------------------------------------------

/**
 * Install and uninstall window message detour.
 */
void HTInstallInputHook();
void HTUninstallInputHook();

/**
 * Map HTKeyCode to ImGuiKey.
 * 
 * NOTE: ImGuiKey can't convert to HTKeyCode.
 */
ImGuiKey HTKeyToImGuiKey(
  HTKeyCode key);

// ----------------------------------------------------------------------------
// [SECTION] Key event functions.
// ----------------------------------------------------------------------------

/**
 * Dispatch a key event to all related callbacks.
 */
void HTHotkeyDispatch(
  HTKeyCode key, HTKeyEventFlags flags, u08 *userSetBlocked);

/**
 * After changing the key binding, there is a cooldown to prevent key message
 * transmission. This function is used to update the cooldown.
 * 
 * Call this function every frame.
 */
void HTHotkeyUpdateCooldown();

/**
 * Call this function to set the cooldown and block key events.
 */
void HTHotkeySetCooldown();

#ifdef __cplusplus
}
#endif

#endif
