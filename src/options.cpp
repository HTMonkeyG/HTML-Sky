#include <stdio.h>
#include <string>
#include <mutex>
#include "cJSON.h"
#include "imgui.h"
#include "includes/htmodloader.h"
#include "htinternal.hpp"

#define HT_OPTIONS_SAVE_RATE 5.0f

static f32 gOptionsDirtyTimer = 0.0f;
static std::mutex gOptionsMutex;

ModLoaderOptions gModLoaderOptions;

/**
 * "key_bindings": {
 *   "<key name>": <key code>,
 *   ...
 * }
 */
static void deserializeModKeyBinds(
  const cJSON *json,
  ModRuntime *fakeRT
) {
  cJSON *keyBindings = cJSON_GetObjectItemCaseSensitive(json, "key_bindings")
    , *item;

  cJSON_ArrayForEach(item, keyBindings) {
    if (!item->string)
      continue;
    if (!cJSON_IsNumber(item))
      continue;

    const char *keyName = item->string;
    HTKeyCode key = (HTKeyCode)(i32)cJSON_GetNumberValue(item);
    fakeRT->keyBinds[keyName].key = key;
    fakeRT->keyBinds[keyName].isRegistered = 0;

    LOGI("  Loaded key '%s': '%s'\n", keyName, HTHotkeyGetName(key));
  }
}

/**
 * "customized": {
 *   "<key name>": <value>,
 *   ...
 * }
 */
static void deserializeModCustomOptions(
  const cJSON *json,
  ModRuntime *fakeRT
) {
  cJSON *customized = cJSON_GetObjectItemCaseSensitive(json, "customized")
    , *item;

  cJSON_ArrayForEach(item, customized) {
    if (!item->string)
      continue;

    const char *keyName = item->string;
    ModCustomOption &option = fakeRT->options[keyName];

    if (cJSON_IsNumber(item)) {
      option.type = HTOptionType_Double;
      option.valueNumber = cJSON_GetNumberValue(item);
      LOGI(
        "  Loaded mod option '%s' (Double): %lf\n",
        keyName,
        option.valueNumber);
    } else if (cJSON_IsString(item)) {
      option.type = HTOptionType_String;
      option.valueString = cJSON_GetStringValue(item);
      LOGI(
        "  Loaded mod option '%s' (String): '%s'\n",
        keyName,
        option.valueString.c_str());
    } else if (cJSON_IsBool(item)) {
      option.valueBool = !!cJSON_IsTrue(item);
      option.type = HTOptionType_Bool;
      LOGI(
        "  Loaded mod option '%s' (Bool): '%d'\n",
        keyName,
        option.valueBool);
    }
  }
}

/**
 * "mod_options": {
 *   "<mod name>": {
 *     "key_bindings": {
 *       "<key name>": <key code>,
 *       ...
 *     },
 *     "customized": {
 *       "<key name>": <value>,
 *       ...
 *     }
 *   },
 *   ...
 * }
 */
static void deserializeAllMods(
  const cJSON *root
) {
  cJSON *modOptions = cJSON_GetObjectItemCaseSensitive(root, "mod_options")
    , *item;
  cJSON_ArrayForEach(item, modOptions) {
    if (!item->string)
      continue;
    const char *packageName = item->string;
    LOGI("Loading options for '%s'\n", packageName);

    if (!cJSON_IsObject(item))
      continue;

    ModRuntime *fakeRT = &gModLoaderOptions.modOptions[packageName];

    deserializeModKeyBinds(
      item,
      fakeRT);
    deserializeModCustomOptions(
      item,
      fakeRT);
  }
}

HTStatus HTiOptionsLoadFromFile(
  const wchar_t *path
) {
  std::string content = HTiReadFileAsUtf8(path);
  if (content.empty())
    return HT_FAIL;

  LOGI("Loading options from %ls\n", path);

  cJSON *json = cJSON_Parse(content.c_str());
  if (!json)
    return HT_FAIL;

  deserializeAllMods(json);

  cJSON_Delete(json);

  return HT_SUCCESS;
}

void HTiOptionsLoadFor(
  ModRuntime *realRT
) {
  auto &packageName = realRT->manifest->meta.packageName;
  auto pOption = &gModLoaderOptions.modOptions;

  auto modOption = pOption->find(packageName);
  if (modOption != pOption->end()) {
    // Assign key bindings.
    realRT->keyBinds = modOption->second.keyBinds;
    // Assign saved options.
    realRT->options = modOption->second.options;
  }
}

void HTiOptionsMarkDirty() {
  std::lock_guard<std::mutex> lock(gOptionsMutex);
  if (gOptionsDirtyTimer <= 0.0f)
    gOptionsDirtyTimer = HT_OPTIONS_SAVE_RATE;
}

void HTiOptionsUpdate(
  f32 timeElapsed
) {
  bool shouldSave = false;
  {
    std::lock_guard<std::mutex> lock(gOptionsMutex);
    if (gOptionsDirtyTimer > 0.0f) {
      gOptionsDirtyTimer -= timeElapsed;
      shouldSave = gOptionsDirtyTimer <= 0.0f;
      if (shouldSave)
        gOptionsDirtyTimer = 0.0f;
    }
  }

  if (shouldSave) {
    std::wstring path(gPathDataWide);
    path += L"\\options.json";
    HTiOptionsWriteToFile(path.c_str());
  }
}

// Merge mod options from `readRT` to `fakeRT`.
static void mergeOptionsForMod(
  ModRuntime *fakeRT,
  ModRuntime *realRT
) {
  for (auto it = realRT->keyBinds.begin(); it != realRT->keyBinds.end(); it++) {
    // We only care about the key codes.
    fakeRT->keyBinds[it->first].key = it->second.key;
    fakeRT->keyBinds[it->first].isRegistered = 0;
  }

  for (auto it = realRT->options.begin(); it != realRT->options.end(); it++)
    fakeRT->options[it->first] = it->second;
}

// Serialize the saved options of the given mod to JSON.
static void saveOptionsForMod(
  cJSON *modOptions,
  const std::string &packageName
) {
  auto fakeRT = &gModLoaderOptions.modOptions[packageName];
  cJSON *singleMod = cJSON_CreateObject();

  // Save key bindings. The child object is created lazily so that mods without
  // key bindings don't leak an unattached cJSON object every save.
  if (!fakeRT->keyBinds.empty()) {
    cJSON *keyBindings = cJSON_CreateObject();
    for (auto it = fakeRT->keyBinds.begin(); it != fakeRT->keyBinds.end(); it++)
      cJSON_AddNumberToObject(
        keyBindings,
        it->first.c_str(),
        (double)(int)it->second.key);
    cJSON_AddItemToObject(singleMod, "key_bindings", keyBindings);
  }

  // Save customized options.
  if (!fakeRT->options.empty()) {
    cJSON *customized = cJSON_CreateObject();
    for (auto it = fakeRT->options.begin(); it != fakeRT->options.end(); it++) {
      ModCustomOption &option = it->second;

      switch(option.type) {
        case HTOptionType_Bool:
          cJSON_AddBoolToObject(
            customized,
            it->first.c_str(),
            option.valueBool);
          break;
        case HTOptionType_Double:
          cJSON_AddNumberToObject(
            customized,
            it->first.c_str(),
            option.valueNumber);
          break;
        case HTOptionType_String:
          cJSON_AddStringToObject(
            customized,
            it->first.c_str(),
            option.valueString.c_str());
          break;
        default:
          continue;
      }
    }

    cJSON_AddItemToObject(singleMod, "customized", customized);
  }

  cJSON_AddItemToObject(modOptions, packageName.c_str(), singleMod);
}

// Build the options JSON tree. Caller MUST hold gModDataLock.
static cJSON *buildOptionsJsonLocked() {
  auto &memOptions = gModLoaderOptions.modOptions;
  cJSON *root = cJSON_CreateObject()
    , *modOptions = cJSON_CreateObject();

  cJSON_AddItemToObject(root, "mod_options", modOptions);

  // Merge all options to gModLoaderOptions.modOptions.
  for (auto it = gModDataRuntime.begin(); it != gModDataRuntime.end(); it++) {
    auto fakeRT = &memOptions[it->second.manifest->meta.packageName];
    // Merge options.
    mergeOptionsForMod(fakeRT, &it->second);
  }

  for (auto it = memOptions.begin(); it != memOptions.end(); it++)
    saveOptionsForMod(modOptions, it->first);

  return root;
}

// Write all options to a JSON object.
static cJSON *HTiOptionsWriteToMem() {
  std::lock_guard<std::mutex> lock(gModDataLock);
  return buildOptionsJsonLocked();
}

// Serialize a prebuilt options tree to `path` atomically, consuming `root`.
static void writeOptionsJsonToFile(
  const wchar_t *path,
  cJSON *root
) {
  if (!root)
    return;

  char *string = cJSON_Print(root);
  cJSON_Delete(root);
  if (!string)
    return;

  // Build the full JSON before touching the target file, then write to a
  // temporary file and atomically swap it in. This guarantees options.json is
  // never left truncated or half-written if serialization, allocation or the
  // write itself fails midway (the old "wb+" opened and truncated the real
  // file before anything was serialized).
  size_t length = strlen(string);
  std::wstring tempPath = std::wstring(path) + L".tmp";

  FILE *fd = _wfopen(tempPath.c_str(), L"wb");
  if (!fd) {
    cJSON_free(string);
    return;
  }

  bool ok = fwrite(string, sizeof(char), length, fd) == length;
  if (ok)
    // Flush buffered data to the OS before the rename so the swapped-in file
    // is complete.
    ok = (fflush(fd) == 0);
  fclose(fd);
  cJSON_free(string);

  if (!ok || !MoveFileExW(tempPath.c_str(), path, MOVEFILE_REPLACE_EXISTING)) {
    _wremove(tempPath.c_str());
    return;
  }

  LOGI("Options saved to %ls\n", path);
}

// Write options to `options.json`.
void HTiOptionsWriteToFile(
  const wchar_t *path
) {
  writeOptionsJsonToFile(path, HTiOptionsWriteToMem());
}

// Best-effort options flush for the process-exit / unload path. Uses try_lock
// so it can never deadlock on a lock still held by a thread terminated during
// ExitProcess; if the lock is unavailable it simply skips (the periodic
// autosave will usually have persisted recent changes already).
void HTiOptionsFlushBestEffort() {
  cJSON *root;
  {
    std::unique_lock<std::mutex> lock(gModDataLock, std::try_to_lock);
    if (!lock.owns_lock())
      return;
    root = buildOptionsJsonLocked();
  }

  std::wstring path(gPathDataWide);
  path += L"\\options.json";
  writeOptionsJsonToFile(path.c_str(), root);
}
