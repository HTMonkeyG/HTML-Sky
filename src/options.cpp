#include <stdio.h>
#include <string>
#include "cJSON.h"
#include "imgui.h"
#include "includes/htmodloader.h"
#include "htinternal.hpp"

#define HT_OPTIONS_SAVE_RATE 5.0f

static f32 gOptionsDirtyTimer = 0.0f;

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
  if (gOptionsDirtyTimer <= 0.0f)
    // FIXME: Needs mutex or atomic operation to avoid race conditions.
    gOptionsDirtyTimer = HT_OPTIONS_SAVE_RATE;
}

void HTiOptionsUpdate(
  f32 timeElapsed
) {
  if (gOptionsDirtyTimer > 0.0f) {
    gOptionsDirtyTimer -= timeElapsed;
    if (gOptionsDirtyTimer <= 0.0f) {
      // Save options.
      std::wstring path(gPathDataWide);
      path += L"\\options.json";
      HTiOptionsWriteToFile(path.c_str());
      gOptionsDirtyTimer = 0.0f;
    }
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
  cJSON *singleMod = cJSON_CreateObject()
    , *keyBindings = cJSON_CreateObject()
    , *customized = cJSON_CreateObject();

  // Save key bindings.
  if (!fakeRT->keyBinds.empty()) {
    for (auto it = fakeRT->keyBinds.begin(); it != fakeRT->keyBinds.end(); it++)
      cJSON_AddNumberToObject(
        keyBindings,
        it->first.c_str(),
        (double)(int)it->second.key);
    cJSON_AddItemToObject(singleMod, "key_bindings", keyBindings);
  }

  // Save customized options.
  if (!fakeRT->options.empty()) {
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

// Write all options to a JSON object.
static cJSON *HTiOptionsWriteToMem() {
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

// Write options to `options.json`.
void HTiOptionsWriteToFile(
  const wchar_t *path
) {
  FILE *fd = _wfopen(path, L"wb+");
  cJSON *json = HTiOptionsWriteToMem();

  const char *string = cJSON_Print(json);
  fwrite(string, sizeof(char), strlen(string), fd);

  cJSON_Delete(json);
  cJSON_free((void *)string);
  fclose(fd);

  LOGI("Options saved to %ls\n", path);
}
