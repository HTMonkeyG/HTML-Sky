#include <stdio.h>
#include <string>
#include "cJSON.h"
#include "includes/htmodloader.h"
#include "htinternal.h"

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
  }
}

/**
 * "key_binds": {
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
  cJSON *keyBinds = cJSON_GetObjectItemCaseSensitive(json, "key_binds")
    , *item;
  cJSON_ArrayForEach(item, keyBinds) {
    if (!item->string)
      continue;
    const char *packageName = item->string;
    if (!cJSON_IsObject(item))
      continue;
    deserializeModKeyBinds(
      item,
      &gModLoaderOptions.modOptions[packageName]);
  }
}

HTStatus HTLoadOptionsFromFile(
  const wchar_t *path
) {
  std::string content = HTiReadFileAsUtf8(path);
  if (content.empty())
    return HT_FAIL;

  cJSON *json = cJSON_Parse(content.c_str());
  if (!json)
    return HT_FAIL;

  deserializeAllKeyBinds(json);

  return HT_SUCCESS;
}

void HTLoadOptionsFor(
  ModRuntime *realRT
) {
  auto packageName = realRT->manifest->meta.packageName;
  auto pOption = &gModLoaderOptions.modOptions;

  auto modOption = pOption->find(packageName);
  if (modOption != pOption->end()) {
    realRT->keyBinds = modOption->second.keyBinds;
  }
}

HTStatus HTWriteOptionsToFile(
  const wchar_t *path
) {

}
