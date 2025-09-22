// ----------------------------------------------------------------------------
// Mod loader of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <cmath>
#include "cJSON.h"

#include "includes/aliases.h"
#include "includes/htmodloader.h"
#include "utils/logger.h"
#include "utils/globals.h"
#include "utils/texts.h"
#include "htinternal.h"

static std::wstring utf8ToWchar(const char *input) {
  if (!input)
    return std::wstring();
  u64 len = strlen(input);
  std::wstring result;
  i32 size = MultiByteToWideChar(CP_UTF8, 0, input, len, nullptr, 0);
  result.resize(size);
  MultiByteToWideChar(CP_UTF8, 0, input, len, &result[0], size);
  return result;
}

static std::string wcharToUtf8(const wchar_t *input) {
  if (!input)
    return std::string();
  std::string result;
  i32 size = WideCharToMultiByte(CP_UTF8, 0, input, -1, nullptr, 0, nullptr, nullptr);
  result.resize(size);
  WideCharToMultiByte(CP_UTF8, 0, input, -1, &result[0], size, nullptr, nullptr);
  return result;
}

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
 * Helper function for get string value with cJSON.
 */
static inline std::string getStringValueFrom(
  cJSON *json,
  const char *key
) {
  char *s = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, key));
  if (!s)
    return std::string();
  return std::string(s);
}

/**
 * Deserialize the manifest file.
 */
static i32 deserializeManifestJson(
  const char *buffer,
  ModManifest *manifest
) {
  i32 ret = 0;
  cJSON *json;
  char *parsedStr = nullptr;
  std::string version;
  double editionFlag;
  
  json = cJSON_Parse(buffer);
  if (!json)
    goto RET;

  // Get dll path.
  parsedStr = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(json, "main"));
  if (!parsedStr)
    goto RET;
  manifest->paths.dll = manifest->paths.folder + L"\\" + utf8ToWchar(parsedStr);

  // Get package name.
  manifest->meta.packageName = getStringValueFrom(json, "package_name");
  if (manifest->meta.packageName.empty())
    goto RET;
  
  // Get mod version.
  version = getStringValueFrom(json, "version");
  if (!parseVersionNumber(version.data(), manifest->meta.version))
    goto RET;
  
  // Get compatible game edition of the mod.
  editionFlag = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(json, "game_edition"));
  if (std::isnan(editionFlag) || ((u08)editionFlag & 0x03) == 0)
    goto RET;
  manifest->gameEditionFlags = (u08)editionFlag & 0x03;

  // Get display info.
  manifest->modName = getStringValueFrom(json, "mod_name");
  manifest->description = getStringValueFrom(json, "description");
  manifest->author = getStringValueFrom(json, "author");

  ret = 1;
RET:
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
  FILE *fd = nullptr;
  u64 size;
  wchar_t *buffer = nullptr;

  // Get the mod folder.
  folder += L"\\";
  folder += fileName;

  // Check the manifest.json.
  std::wstring jsonPath = folder + L"\\manifest.json";
  if (!fileExists(jsonPath.data()))
    return 0;

  // Open manifest.json.
  fd = _wfopen(jsonPath.data(), L"r, ccs=UTF-8");
  if (!fd)
    return 0;

  // Read file.
  fseek(fd, 0, SEEK_END);
  size = ftell(fd);
  rewind(fd);
  buffer = (wchar_t *)malloc((size + 1) * sizeof(wchar_t));
  if (!buffer) {
    fclose(fd);
    return 0;
  }
  fread(buffer, sizeof(wchar_t), size, fd);
  buffer[size] = 0;
  fclose(fd);

  // Save paths.
  manifest->paths.folder = folder;
  manifest->paths.json = jsonPath;

  // Deserialize manifest.
  std::string content = wcharToUtf8(buffer);
  ret = deserializeManifestJson(
    content.data(),
    manifest);

  // Parse and deserialize the file.
  free(buffer);
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

  modsFolderPath += L"\\*";
  hFindFile = FindFirstFileW(modsFolderPath.data(), &findData);
  if (!hFindFile)
    return;

  do {
    if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      continue;
    if (!wcscmp(findData.cFileName, L".") || !wcscmp(findData.cFileName, L".."))
      continue;
    if (!parseModManifest(findData.cFileName, &manifest))
      continue;
    if (!fileExists(manifest.paths.dll.data()))
      continue;
    
    // When scanning mods, the mod data won't be accessed by multiple threads,
    // so we don't need to protect it.
    gModDataLoader[manifest.meta.packageName] = manifest;

    LOGI("Scanned mod %s.\n", manifest.modName.data());
  } while (FindNextFileW(hFindFile, &findData));
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

  for (auto it = gModDataLoader.begin(); it != gModDataLoader.end(); it++) {
    const char *modName = it->second.modName.data();

    if (it->second.meta.packageName == HTTexts_ModLoaderPackageName)
      // The data of mod loader itself is set in bootstrap(), so we don't need
      // to load it again.
      continue;
    if (!(it->second.gameEditionFlags & gGameStatus.edition))
      // Skip mods that not compatible with current game edition.
      continue;

    // Load library.
    hMod = LoadLibraryW(it->second.paths.dll.data());
    if (hMod)
      LOGI("Loaded mod %s.\n", modName);
    else
      LOGI("Load mod %s failed: No such file.\n", modName);

    // Save runtime data.
    {
      // While the mods are loading one by one, subthreads created by the mods
      // may access mod data structs at the same time, so we need a global
      // lock.
      std::lock_guard<std::mutex> lock(gModDataLock);
      runtimeData = &gModDataRuntime[hMod];
      runtimeData->handle = hMod;
      runtimeData->manifest = &(it->second);
      getModExportedFunctions(runtimeData);
      it->second.runtime = runtimeData;

      registerHandle(hMod, HTHandleType_Mod);
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
    fn = it->second.loaderFunc.pfn_HTModOnInit;
    if (fn)
      (void)fn(nullptr);
  }
}

HTStatus HTLoadMods() {
  HTBootstrap();
  scanMods();
  expandMods();
  initMods();

  return HT_SUCCESS;
}

HTStatus HTInjectDll(
  const wchar_t *path
) {
  return HT_SUCCESS;
}
