// ----------------------------------------------------------------------------
// Mod loader of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <cmath>
#include <algorithm>
#include "cJSON.h"

#include "includes/htmodloader.h"
#include "utils/texts.h"
#include "htinternal.hpp"

static inline i32 parseVersionNumber(
  const char *str,
  u32 *versions
) {
  u32 result[3];
  if (sscanf(str, "%u.%u.%u", result, result + 1, result + 2) != 3)
    return 0;
  memcpy(versions, result, 3 * sizeof(u32));
  return 1;
}

static inline i32 compareVersion(
  u32 *a1,
  u32 *a2
) {
  for (u08 i = 0; i < 3; i++) {
    if (a1[i] < a2[i])
      return -1;
    if (a1[i] > a2[i])
      return -1;
  }
  return 0;
}

/**
 * Package name should only contains `a-z A-Z 0-9 _ . @ / -`.
 */
static inline bool validatePackageName(
  const std::string &packageName
) {
  return std::all_of(
    packageName.begin(),
    packageName.end(),
    [](char ch) -> bool {
      return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z')
        || (ch >= '0' && ch <= '9')
        || ch == '.'
        || ch == '-'
        || ch == '_'
        || ch == '@'
        || ch == '/';
    }
  );
}

/**
 * Dll must not be located out of `mods` folder.
 */
static inline bool validateDllPath(
  const std::wstring &path
) {
  std::wstring rel = HTiPathRelative(
    gPathModsWide,
    path);

  if (rel.empty())
    return false;
  if (HTiPathIsAbsolute(rel))
    return false;
  if (rel.size() > 2 && rel[0] == L'.' && rel[1] == L'.')
    return false;

  return true;
}

/**
 * Helper function for get string value with cJSON.
 */
static inline std::string getStringValueFrom(
  cJSON *json,
  const char *key
) {
  char *s = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, key));
  return s ? s : "";
}

/**
 * Deserialize the manifest file.
 */
static i32 deserializeManifestJson(
  const char *buffer,
  ModManifest *manifest
) {
  const wchar_t *manifestPath = manifest->paths.json.c_str();
  i32 ret = 0;
  cJSON *json;
  char *parsedStr = nullptr;
  std::string version;
  std::wstring dllPathOrigin;
  double editionFlag;

  (void)manifestPath;

  json = cJSON_Parse(buffer);
  if (!json) {
    LOGW("Invalid manifest.json of: %ls\n", manifestPath);
    goto Err;
  }

  // Get package name.
  manifest->meta.packageName = getStringValueFrom(json, "package_name");
  if (
    manifest->meta.packageName.empty()
    || !validatePackageName(manifest->meta.packageName)
  ) {
    LOGW(
      "Invalid package name \"%s\" in: %ls\n",
      manifest->meta.packageName.c_str(),
      manifestPath);
    goto Err;
  }

  // Get dll path from manifest.
  parsedStr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "main"));
  if (!parsedStr) {
    LOGW("Missing dll path of: %ls\n", manifestPath);
    goto Err;
  }

  // Dll path must not be a relative path.
  dllPathOrigin = HTiUtf8ToWstring(parsedStr);
  if (HTiPathIsAbsolute(dllPathOrigin))
    goto InvalidDllPath;

  // Dll must be located in `mods/` folder.
  manifest->paths.dll = HTiPathJoin({
    manifest->paths.folder,
    dllPathOrigin});
  if (!validateDllPath(manifest->paths.dll)) {
InvalidDllPath:
    LOGW("Invalid dll path of: %ls\n", manifestPath);
    goto Err;
  }

  // Get mod version.
  version = getStringValueFrom(json, "version");
  if (!parseVersionNumber(version.data(), manifest->meta.version)) {
    LOGW("Invalid mod version of: %ls\n", manifestPath);
    goto Err;
  }

  // Get compatible game edition of the mod.
  editionFlag = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "game_edition"));
  if (std::isnan(editionFlag)) {
    // Invalid game edition.
    LOGW("Invalid game edition of: %ls\n", manifestPath);
    goto Err;
  }
  manifest->gameEditionFlags = (i32)editionFlag;

  // Get display info.
  manifest->modName = getStringValueFrom(json, "mod_name");
  manifest->description = getStringValueFrom(json, "description");
  manifest->author = getStringValueFrom(json, "author");

  ret = 1;

Err:
  cJSON_Delete(json);
  return ret;
}

/**
 * Parse manifest.json to get the basic data of a mod, and check file integrity
 * of the mod.
 */
static i32 parseModManifest(
  const wchar_t *fileName,
  ModManifest *manifest
) {
  i32 ret;
  std::wstring folder(gPathModsWide);

  // Get the mod folder.
  folder = HTiPathJoin({folder, fileName});

  // Check the manifest.json.
  std::wstring jsonPath = HTiPathJoin({folder, L"\\manifest.json"});
  if (!HTiFileExists(jsonPath.data()))
    return 0;

  // Save paths.
  manifest->paths.folder = folder;
  manifest->paths.json = jsonPath;

  // Open manifest.json.
  std::string content = HTiReadFileAsUtf8(jsonPath);

  // Parse and deserialize the file.
  ret = deserializeManifestJson(
    content.data(),
    manifest);

  return ret;
}

/**
 * Scan all potential mods.
 */
static void scanMods() {
  HANDLE hFindFile;
  WIN32_FIND_DATAW findData;
  ModManifest manifest;
  std::wstring modsFolderPath(gPathModsWide);

  LOGI("Scanning mods...\n");

  modsFolderPath += L"\\*";
  hFindFile = FindFirstFileW(modsFolderPath.data(), &findData);
  if (!hFindFile)
    return;

  do {
    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      continue;
    if (!wcscmp(findData.cFileName, L".") || !wcscmp(findData.cFileName, L".."))
      continue;

    LOGI("Found potential mod folder: %ls\n", findData.cFileName);

    if (!parseModManifest(findData.cFileName, &manifest))
      continue;
    if (!HTiFileExists(manifest.paths.dll.data()))
      continue;

    if (!HTiBackendCheckEdition(manifest.gameEditionFlags))
      // Skip mods that not compatible with current game edition.
      continue;

    // When scanning mods, the mod data won't be accessed by multiple threads,
    // so we don't need to protect it.
    if (gModDataLoader.find(manifest.meta.packageName) != gModDataLoader.end()) {
      // Skip duplicated mods.
      LOGW("Duplicated package name %s, skipped.\n", manifest.meta.packageName.c_str());
      continue;
    }

    gModDataLoader[manifest.meta.packageName] = manifest;

    LOGI("Scanned mod %s.\n", manifest.modName.data());
  } while (FindNextFileW(hFindFile, &findData));

  FindClose(hFindFile);
}

/**
 * Get all exported functions for the loader.
 */
static void getModExportedFunctions(
  ModRuntime *runtimeData
) {
  HMODULE hMod = runtimeData->handle;
  runtimeData->loaderFunc.pfn_HTModRenderGui = (PFN_HTModRenderGui)GetProcAddress(
    hMod, "HTModRenderGui");
  runtimeData->loaderFunc.pfn_HTModOnInit = (PFN_HTModOnInit)GetProcAddress(
    hMod, "HTModOnInit");
  runtimeData->loaderFunc.pfn_HTModOnEnable = (PFN_HTModOnEnable)GetProcAddress(
    hMod, "HTModOnEnable");
}

/**
 * Load all avaliable mods into the game process and register mod runtime data.
 */
static void expandMods() {
  HMODULE hMod;
  ModRuntime *runtimeData;
  std::wstring oldPath;

  for (auto it = gModDataLoader.begin(); it != gModDataLoader.end(); it++) {
    const char *modName = it->second.modName.c_str();

    (void)modName;

    if (it->second.meta.packageName == HTTexts_ModLoaderPackageName)
      // The data of mod loader itself is set in bootstrap(), so we don't need
      // to load it again.
      continue;

    // Save previous dll directory.
    u32 needed = GetDllDirectoryW(0, nullptr);
    if (needed) {
      oldPath.resize(needed + 1);
      GetDllDirectoryW(needed + 1, oldPath.data());
    }

    // Set new dll searching directory.
    SetDllDirectoryW(it->second.paths.folder.c_str());

    // Load library.
    hMod = LoadLibraryW(it->second.paths.dll.c_str());
    if (hMod)
      LOGI("Loaded mod %s.\n", modName);
    else
      LOGW("Load mod %s failed: No such file.\n", modName);

    // Restore saved dll searching directory.
    if (needed)
      SetDllDirectoryW(oldPath.c_str());
    else
      SetDllDirectoryW(nullptr);

    // Save runtime data.
    if (hMod) {
      // While the mods are loading one by one, subthreads created by the mods
      // may access mod data structs at the same time, so we need a global
      // lock.
      std::lock_guard<std::mutex> lock(gModDataLock);
      runtimeData = &gModDataRuntime[hMod];
      runtimeData->handle = hMod;
      runtimeData->manifest = &(it->second);
      getModExportedFunctions(runtimeData);
      it->second.runtime = runtimeData;

      HTiRegisterHandle(hMod, HTHandleType_Mod);

      HTiOptionsLoadFor(runtimeData);
    }
  }
}

/**
 * Call the HTModOnInit() functions exported by the mods one by one. Mods
 * can only use HTAPI within and after the function is called.
 */
void initMods() {
  PFN_HTModOnInit fn;

  for (auto it = gModDataRuntime.begin(); it != gModDataRuntime.end(); it++) {
    const char *modName = it->second.manifest->modName.c_str();
    (void)modName;

    fn = it->second.loaderFunc.pfn_HTModOnInit;

    // We assume that mod that does not export HTModOnInit() has an independent
    // initialization (or enabling, see HTiEnableMods() below) process, so we
    // consider that they are initialized successfully
    if (!fn || fn(nullptr) == HT_SUCCESS)
      LOGI("Initialized mod %s.\n", modName);
    else
      LOGW("Failed to initialize mod %s.\n", modName);
  }
}

HTStatus HTiLoadMods() {
  HTiBootstrap();
  scanMods();
  expandMods();
  initMods();

  return HT_SUCCESS;
}

HTStatus HTiEnableMods() {
  PFN_HTModOnEnable fn;

  for (auto it = gModDataRuntime.begin(); it != gModDataRuntime.end(); it++) {
    const char *modName = it->second.manifest->modName.c_str();
    (void)modName;

    fn = it->second.loaderFunc.pfn_HTModOnEnable;

    if (!fn || fn(nullptr) == HT_SUCCESS)
      LOGI("Enabled mod %s.\n", modName);
    else
      LOGW("Failed to enable mod %s.\n", modName);
  }

  return HT_SUCCESS;
}

HTStatus HTiInjectDll(
  const wchar_t *path
) {
  return HT_SUCCESS;
}
