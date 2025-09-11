#include <string>
#include <unordered_map>
#include <vector>
#include <cmath>
#include "cJSON.h"
#include "logger.h"
#include "loader.h"
#include "globals.h"
#include "aliases.h"
#include "htmodloader.h"

struct ModPaths {
  // The folder contains manifest.json.
  std::wstring folder;
  // "main" in manifest.json. The name of the main executable file of the mod.
  std::wstring dll;
  // The path to manifest.json.
  std::wstring json;
};

struct ModMeta {
  // This name is used to identify mods and add dependencies, and must be
  // unique for every mod.
  std::string packageName;
  // The version number of the mod.
  u32 version[3];
};

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
};

std::unordered_map<std::string, ModManifest> gModDataInitial;

static inline i32 fileExists(const wchar_t *path) {
  DWORD attr = GetFileAttributesW(path);
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
    return 0;
  return 1;
}

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
  if (std::isnan(editionFlag) || ((u08)editionFlag & 0x03 == 0))
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
    
    gModDataInitial[manifest.meta.packageName] = manifest;

    LOGI("Scanned mod %s.\n", manifest.modName.data());
  } while (FindNextFileW(hFindFile, &findData));
}

static void loadMods() {

}

void HTLoadMods() {
  // Create the mods folder if not exist.
  DWORD attr = GetFileAttributesW(gPathModsWide);

  if (
    attr == INVALID_FILE_ATTRIBUTES
    || !(attr & FILE_ATTRIBUTE_DIRECTORY)
  ) {
    CreateDirectoryW(gPathModsWide, nullptr);
    return;
  }

  scanMods();
  loadMods();
}
