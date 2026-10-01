// ----------------------------------------------------------------------------
// Loader configuration, read from html-config.json's "ht_mod_loader" section.
//
// html-config.json lives next to winhttp.dll and doubles as the Vulkan layer
// manifest. The "ht_mod_loader" object is ignored by the Vulkan loader and used
// by HTML to override game detection (e.g. a renamed executable) or force a
// specific backend.
// ----------------------------------------------------------------------------

#include <string>

#include "cJSON.h"

#include "htinternal.hpp"
#include "utils/texts.h"

// Read a string field from a cJSON object into `out`. Leaves `out` untouched
// when the field is missing or not a string.
static void readStringField(
  const cJSON *obj,
  const char *key,
  std::string &out
) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
  if (cJSON_IsString(item) && item->valuestring)
    out = item->valuestring;
}

void HTiLoadLoaderConfig() {
  // Build the config path next to the DLL.
  std::wstring path = HTiUtf8ToWstring(gPathDll.c_str());
  path += L"\\html-config.json";

  // Ensure the file exists so users always have something to edit; create it
  // from the default template (which includes an empty ht_mod_loader section).
  if (!HTiFileExists(path.c_str())) {
    FILE *fd = _wfopen(path.c_str(), L"wb");
    if (fd) {
      fwrite(
        HTTexts_DefaultLayerConfig,
        sizeof(char),
        sizeof(HTTexts_DefaultLayerConfig) - 1,
        fd);
      fclose(fd);
    }
  }

  std::string content = HTiReadFileAsUtf8(path);
  if (content.empty())
    return;

  cJSON *root = cJSON_Parse(content.c_str());
  if (!root)
    return;

  const cJSON *section = cJSON_GetObjectItemCaseSensitive(root, "ht_mod_loader");
  if (cJSON_IsObject(section)) {
    readStringField(section, "target_executable", gConfigTargetExe);
    readStringField(section, "backend", gConfigForceBackend);

    if (!gConfigTargetExe.empty())
      LOGI("Config: target executable overridden to '%s'.\n", gConfigTargetExe.c_str());
    if (!gConfigForceBackend.empty())
      LOGI("Config: forced backend '%s'.\n", gConfigForceBackend.c_str());
  }

  cJSON_Delete(root);
}
