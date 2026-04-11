#ifndef __INTERNAL_H__
#define __INTERNAL_H__

#include <windows.h>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "imgui.h"

#include "includes/htmodloader.h"
#include "includes/htconfig.h"
#include "htaliases.h"

// ----------------------------------------------------------------------------
// [SECTION] Mod loader logger.
// ----------------------------------------------------------------------------

#ifdef HTML_ENABLE_LOGGER
#define LOG(format, ...) HTiLogA(format, ##__VA_ARGS__)
#define WLOG(format, ...) HTiLogW(format, ##__VA_ARGS__)

#define LOGI(format, ...) HTiLogA("[INFO] " format, ##__VA_ARGS__)
#define WLOGI(format, ...) HTiLogW(L"[INFO] " format, ##__VA_ARGS__)
#define LOGW(format, ...) HTiLogA("[WARN] " format, ##__VA_ARGS__)
#define WLOGW(format, ...) HTiLogW(L"[WARN] " format, ##__VA_ARGS__)
#define LOGE(format, ...) HTiLogA("[ERR] " format, ##__VA_ARGS__)
#define WLOGE(format, ...) HTiLogW(L"[ERR] " format, ##__VA_ARGS__)
#define LOGEF(format, ...) HTiLogA("[ERR][FATAL] " format, ##__VA_ARGS__)
#define WLOGEF(format, ...) HTiLogW(L"[ERR][FATAL] " format, ##__VA_ARGS__)
#else
#define LOG(format, ...) 
#define WLOG(format, ...)

#define LOGI(format, ...)
#define WLOGI(format, ...)
#define LOGW(format, ...)
#define WLOGW(format, ...)
#define LOGE(format, ...)
#define WLOGE(format, ...)
#define LOGEF(format, ...)
#define WLOGEF(format, ...)
#endif

void HTiInitLogger(
  const wchar_t *fileName,
  i08 allocConsole);
void HTiLogA(
  const char *format,
  ...);
void HTiLogW(
  const wchar_t *format, 
  ...);

// ----------------------------------------------------------------------------
// [SECTION] Mod loader globals.
// ----------------------------------------------------------------------------

#define HTiErrAndRet(e, v) (HTSetLastError(e), v)

typedef std::mutex HTMutex;
typedef std::shared_mutex HTMutexShared;

typedef std::shared_lock<HTMutexShared> HTLockReadable;
typedef std::lock_guard<HTMutexShared> HTLockShared;

typedef std::lock_guard<HTMutex> HTLockMutex;

extern HTGameStatus gGameStatus;
extern char gPathDll[MAX_PATH]
  , gPathGameExe[MAX_PATH]
  , gPathData[MAX_PATH]
  , gPathMods[MAX_PATH]
  , gPathGuiIni[MAX_PATH];
extern wchar_t gPathModsWide[MAX_PATH]
  , gPathDataWide[MAX_PATH];
extern HANDLE gHeap
  , gEventGuiInit;
extern HMODULE gModLoaderHandle;

// ----------------------------------------------------------------------------
// [SECTION] Codepage, file and path.
// ----------------------------------------------------------------------------

// Convert string from wchar_t to UTF-8.
static inline void wcstoutf8(
  const wchar_t *wcs,
  char *utf8,
  i32 max
) {
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
static inline std::wstring HTiUtf8ToWstring(
  const char *input
) {
  if (!input)
    return L"";
  u64 len = strlen(input);
  std::wstring result;
  i32 size = MultiByteToWideChar(CP_UTF8, 0, input, len, nullptr, 0);
  result.resize(size);
  MultiByteToWideChar(CP_UTF8, 0, input, len, &result[0], size);
  return result;
}

// Convert UTF-16LE to UTF-8, returned std::string.
static inline std::string HTiWstringToUtf8(
  const wchar_t *input
) {
  if (!input)
    return "";
  std::string result;
  i32 size = WideCharToMultiByte(CP_UTF8, 0, input, -1, nullptr, 0, nullptr, nullptr);
  result.resize(size);
  WideCharToMultiByte(CP_UTF8, 0, input, -1, result.data(), size, nullptr, nullptr);
  return result;
}

// Read file as UTF-8 encoding with _wfopen.
static inline std::string HTiReadFileAsUtf8(
  const std::wstring &path
) {
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
static inline i32 HTiFileExists(
  const wchar_t *path
) {
  DWORD attr = GetFileAttributesW(path);
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
    return 0;
  return 1;
}

// Check if the given folder exists.
static inline i32 HTiFolderExists(
  const wchar_t *path
) {
  DWORD attr = GetFileAttributesW(path);
  if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    return 0;
  return 1;
}

// Normalizes the given path, resolving '..' and '.' segments.
std::wstring HTiPathNormalize(
  const std::wstring &);

// Joins all given path segments together using the platform-specific separator
// as a delimiter, then normalizes the resulting path.
std::wstring HTiPathJoin(
  const std::vector<std::wstring> &);

// Resolves a sequence of paths or path segments into an absolute path.
std::wstring HTiPathResolve(
  const std::vector<std::wstring> &);

// Returns the relative path from `from` to `to` based on the current working
// directory. If from and to each resolve to the same path (after calling
// `HTiPathResolve()` on each), a zero-length string is returned.
// If a zero-length string is passed as from or to, the current working directory
// will be used instead of the zero-length strings.
std::wstring HTiPathRelative(
  const std::wstring &from,
  const std::wstring &to);

// Determines if the literal path is absolute.
bool HTiPathIsAbsolute(
  const std::wstring &);

// ----------------------------------------------------------------------------
// [SECTION] Mod loader functions.
// ----------------------------------------------------------------------------

// Scan and load all mods, then call the exported HTModOnInit() function of
// each mod.
// This function is called after the game window has created.
HTStatus HTiLoadMods();
// Enable all mods, that is, call the exported HTModOnEnable() function of
// each mod.
// This function is called after ImGui started rendering.
HTStatus HTiEnableMods();
// [Invalid] Load a mod and its dependencies.
void HTiLoadSingleMod();
// [Invalid] Unload a mod and its dependents.
void HTiUnloadSingleMod();
// [Invalid] Inject a dll.
HTStatus HTiInjectDll(
  const wchar_t *path);
// [Invalid] Free a dll.
HTStatus HTiRejectDll();

// ----------------------------------------------------------------------------
// [SECTION] Mod data and handle declarations.
// ----------------------------------------------------------------------------

// Handle types.
typedef enum {
  HTHandleType_Invalid = 0,
  HTHandleType_Manifest,
  HTHandleType_Mod,
  HTHandleType_Hotkey,
  HTHandleType_Command,
  HTHandleType_Asm
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
  HTGameEdition gameEditionFlags;
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

// Option data saved by the mod. This struct will be stored at options.json
struct ModCustomOption {
  HTOptionType type;

  bool valueBool;
  double valueNumber;
  std::string valueString;
};

// Mod runtime data. This struct is associated with mod handle.
struct ModRuntime {
  HMODULE handle;
  ModManifest *manifest;
  ModInternalFunctions loaderFunc;
  // Shared functions.
  std::map<std::string, PFN_HTVoidFunction> functions;
  // True if the mod has explictly registered key bindings.
  u08 hasRegisteredKeys;
  // Registered hotkeys.
  std::map<std::string, ModKeyBind> keyBinds;
  // Customized options.
  std::map<std::string, ModCustomOption> options;
};

// Contexts of a patch.
struct ModPatch {
  // Begin address of the patch.
  void *addr;
  // Original data of the patch.
  std::vector<u08> original;
  // Patch data to be set.
  std::vector<u08> patched;
  // Owner of this patch.
  HMODULE owner;
};

// Contexts of a hook.
struct ModHook {
  // Address of the function intended to be detoured.
  PFN_HTVoidFunction intent;
  // Address of the actually hooked function due to the hook chain.
  PFN_HTVoidFunction actual;
  // Address of the detour function.
  PFN_HTVoidFunction detour;
  // Trampoline function to be called.
  PFN_HTVoidFunction trampoline;
  // The owner of the hook.
  HMODULE owner;
  // The status of the hook.
  bool isEnabled;
  // [Invalid] The binary data of the leading JMP instructions.
  ModPatch *header;
  // [Invalid] Hook chain datas.
  ModHook *next;
  ModHook *prev;
  // Debug only.
  std::string name;
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

static inline bool HTiIsExecutableAddr(
  void *address
) {
  MEMORY_BASIC_INFORMATION mbi = {0};

  // Check the protection of given address.
  if (!VirtualQuery((void *)address, &mbi, sizeof(mbi)))
    return false;
  if (!(mbi.Protect & 0xF0))
    // Not executable.
    return false;

  return true;
}

// Remove all event callbacks registered by the mod.
void HTiRemoveAllEventCallbacksOf(
  HMODULE hModuleOwner);

// Find all hooks of the given mod.
std::vector<ModHook *> HTiAsmHookFindFor(
  HMODULE owner);

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
// MaHTiBootstrapng to save options.
void HTiOptionsMarkDirty();
// Save all options to gModLoaderOptions. Called by HTiUpdateGUI().
void HTiOptionsUpdate(
  f32);
// Write options to the specified file.
void HTiOptionsWriteToFile(
  const wchar_t *);

// ----------------------------------------------------------------------------
// [SECTION] Bootstrap and setup declarations.
// ----------------------------------------------------------------------------

// Handle of the menu key.
extern HTHandle hKeyMenuToggle;

// Backend related data.
extern char gActiveGameBackendName[32];
extern char gActiveGLBackendName[32];
extern std::wstring gGameProcessName;

// Backend registerer.
class HTiBackendRegister {
  using PFN_BackendXXExpectProcess = int (*)();
  using PFN_BackendXXInit = int (*)();

public:
  HTiBackendRegister(
    const char *name,
    PFN_BackendXXInit fnInit,
    PFN_BackendXXExpectProcess fnExpectProcess = nullptr
  )
    : name(name)
    , fnExpectProcess(fnExpectProcess)
    , fnInit(fnInit)
  {
    prev = list();
    list() = this;
  }

  HTiBackendRegister(const HTiBackendRegister &) = delete;
  HTiBackendRegister &operator=(const HTiBackendRegister &) = delete;

  static const HTiBackendRegister *&list() {
    static const HTiBackendRegister *p = nullptr;
    return p;
  }

  const char *name;
  const HTiBackendRegister *prev;
  PFN_BackendXXExpectProcess fnExpectProcess;
  PFN_BackendXXInit fnInit;
};

// Set the name of currently active backends.
// Backends should call these functions after it's actived.
int HTiSetGameBackendName(
  const char *);
int HTiSetGLBackendName(
  const char *);

// Set the name of the game executable file. The signature scanner only scans
// codes in the executable file of the game.
int HTiSetGameProcessName(
  const char *);

// Set the name of the game executable file with wchar_t.
int HTiSetGameProcessName(
  const wchar_t *);

// Check if the backend expects the process module name.
// Used to quickly confirm whether full functionality needs to be enabled.
int HTiBackendExpectProcess();

// Backend critical section implementation.
//
// The following functions must be invoked in pairs; each call to
// `HTiBackendGLEnterCritical()` must be subsequently matched by a call to
// `HTiBackendGLLeaveCritical()`.
//
// The backend must check the return value of `HTiBackendGLEnterCritical()` to
// determine if ImGui requires initialization.
int HTiBackendGLEnterCritical();
int HTiBackendGLLeaveCritical();

// Call this function to send event to the HTML after ImGui initialization is
// completed.
int HTiBackendGLInitComplete();

// Check if the mod is compatible with the target game version.
int HTiBackendCheckEdition(
  HTGameEdition);
int HTiBackendSetEditionCheckFunc(
  PFN_HTVoidFunction);

// Setup all enabled backends.
int HTiBackendSetupAll();

// Set the game status.
void HTiSetGameStatus(
  HTGameStatus *);

// Scan and load mods into the game, then initialize all loaded mods. Called by
// backends.
void HTiSetupAll();

// Register the loader itself as a single mod. The package name of the loader
// is "htmodloader", version is HTML_VERSION_NAME.
void HTiBootstrap();

// ----------------------------------------------------------------------------
// [SECTION] Graphic declarations.
// ----------------------------------------------------------------------------

extern bool gShowMainMenu
  , gShowDebugger;

// Initialize ImGui context and window message hook.
//
// Backends should call this function at least once before calling
// `HTiBackendGLEnterCritical()`.
void HTiInitGUI();
// Destroy ImGui context.
void HTiDeinitGUI();
// Show all registered windows. Referenced by layer.cpp.
void HTiUpdateGUI();
// Render HTML windows. Referenced by bootstrap.cpp.
void HTiRenderGUI(
  f32,
  void *);

// Toggle main menu display status. Referenced by bootstrap.cpp, the callback
// of hKeyMenuToggle.
void HTiToggleMenuState(
  HTKeyEvent *);

// Submenus.
void HTiMenuAbouts();
void HTiMenuConsole();
void HTiMenuModList();
void HTiMenuSettings();

// ImGui windows
void HTiWindowDebugger(
  bool *show);
void HTiWindowMain(
  bool *show);

// Console functions.
void HTiClearConsole();
void HTiRenderConsoleTexts();
void HTiConsoleScrollEnd();
void HTiAddConsoleLineV(
  bool raw,
  const char *fmt,
  va_list args);
void HTiAddConsoleLine(
  bool raw,
  const char *fmt,
  ...);

#ifdef HTML_ENABLE_DEBUGGER

// Debugger ui functions.
void HTiRenderDebugger();

#endif

// ----------------------------------------------------------------------------
// [SECTION] Input handler related functions.
// ----------------------------------------------------------------------------

// Modified from ImGui. Check whether a key has name string.
static inline bool HTiIsNamedKey(
  HTKeyCode key
) {
  return key >= HTKey_NamedKey_BEGIN && key < HTKey_NamedKey_END;
}

// Install and uninstall window message detour.
void HTiInstallInputHook();
void HTiUninstallInputHook();

// Map HTKeyCode to ImGuiKey.
// NOTE: ImGuiKey can't convert to HTKeyCode.
ImGuiKey HTKeyToImGuiKey(
  HTKeyCode key);

// ----------------------------------------------------------------------------
// [SECTION] Key event functions.
// ----------------------------------------------------------------------------

// Call key event callbacks.
void HTiHotkeyDispatch(
  HTKeyCode key,
  HTKeyEventFlags flags,
  u08 *userSetBlocked);

// Set key event cooldown.
void HTiHotkeyUpdateCooldown();
void HTiHotkeySetCooldown();

// ----------------------------------------------------------------------------
// [SECTION] LevelDB functions.
// ----------------------------------------------------------------------------

i32 HTiInitLDB();
i32 HTiDeinitLDB();

#endif
