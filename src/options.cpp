#include <stdio.h>
#include <string>
#include "cJSON.h"
#include "imgui.h"
#include "includes/htmodloader.h"
#include "htinternal.h"

#define HT_OPTIONS_SAVE_RATE 5.0f

static f32 gOptionsDirtyTimer = 0.0f;

ModLoaderOptions gModLoaderOptions;

static void deserializeModKeyBinds(
  const cJSON *json,
  ModRuntime *fakeRT
) {
  cJSON *item;
  cJSON_ArrayForEach(item, json) {
    if (!item->string)
      continue;
    const char *keyName = item->string;
    if (!cJSON_IsNumber(item))
      continue;
    HTKeyCode key = (HTKeyCode)(i32)cJSON_GetNumberValue(item);
    fakeRT->keyBinds[keyName].key = key;
    fakeRT->keyBinds[keyName].isRegistered = 0;
    LOGI("  Loaded key '%s': '%s'\n", keyName, HTHotkeyGetName(key));
  }
}

/**
 * "key_bindings": {
 *   "<mod name>": {
 *     "<key name>": <key code>,
 *     ...
 *   },
 *   ...
 * }
 */
static void deserializeAllKeyBinds(
  const cJSON *json
) {
  cJSON *keyBinds = cJSON_GetObjectItemCaseSensitive(json, "key_bindings")
    , *item;
  cJSON_ArrayForEach(item, keyBinds) {
    if (!item->string)
      continue;
    const char *packageName = item->string;

    LOGI("Loading key binds for '%s'\n", packageName);

    if (!cJSON_IsObject(item))
      continue;
    deserializeModKeyBinds(
      item,
      &gModLoaderOptions.modOptions[packageName]);
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

  deserializeAllKeyBinds(json);

  cJSON_Delete(json);

  return HT_SUCCESS;
}

void HTiOptionsLoadFor(
  ModRuntime *realRT
) {
  auto packageName = realRT->manifest->meta.packageName;
  auto pOption = &gModLoaderOptions.modOptions;

  auto modOption = pOption->find(packageName);
  if (modOption != pOption->end()) {
    // Assign key bindings.
    realRT->keyBinds = modOption->second.keyBinds;
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

static void saveOptionsForMod(
  cJSON *root,
  ModRuntime *rt
) {
  auto packageName = rt->manifest->meta.packageName.c_str();

  // Save key bindings.
  if (!rt->keyBinds.empty()) {
    cJSON *keyBindings = cJSON_GetObjectItemCaseSensitive(root, "key_bindings");
    cJSON *mod = cJSON_CreateObject();
    for (auto it = rt->keyBinds.begin(); it != rt->keyBinds.end(); it++)
      cJSON_AddNumberToObject(
        mod,
        it->first.c_str(),
        (double)(int)it->second.key);
    cJSON_AddItemToObject(keyBindings, packageName, mod);
  }
}

cJSON *HTiOptionsWriteToMem() {
  cJSON *json = cJSON_CreateObject();

  cJSON_AddItemToObject(json, "key_bindings", cJSON_CreateObject());

  for (auto it = gModDataRuntime.begin(); it != gModDataRuntime.end(); it++)
    saveOptionsForMod(json, &it->second);
  
  return json;
}

void HTiOptionsWriteToFile(
  const wchar_t *path
) {
  FILE *fd = _wfopen(path, L"wb+");
  cJSON *json = HTiOptionsWriteToMem();

  const char *string = cJSON_Print(json);
  fwrite(string, sizeof(char), strlen(string), fd);

  cJSON_Delete(json);
  fclose(fd);

  LOGI("Options saved to %ls\n", path);
}
