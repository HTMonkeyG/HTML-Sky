// ----------------------------------------------------------------------------
// Register the mod loader itself as a single mod.
// ----------------------------------------------------------------------------
#include <stdio.h>
#include <mutex>
#include "includes/htmodloader.h"
#include "utils/texts.h"
#include "htinternal.h"

HTHandle hKeyMenuToggle = nullptr;

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

static HTStatus HTMLAPI modOnInit(void *) {
  //HTCommRegFunction(gModLoaderHandle, "HT");
  hKeyMenuToggle = HTHotkeyRegister(
    gModLoaderHandle,
    "Toggle menu display",
    HTKey_GraveAccent);
  HTHotkeyListen(
    hKeyMenuToggle,
    HTiToggleMenuState);
  return HT_SUCCESS;
}

void HTBootstrap() {
  std::lock_guard<std::mutex> lock(gModDataLock);
  ModManifest *manifestSelf = &gModDataLoader[HTTexts_ModLoaderPackageName];
  ModRuntime *runtimeSelf = &gModDataRuntime[gModLoaderHandle];

  // Set manifest data.
  manifestSelf->meta.packageName = HTTexts_ModLoaderPackageName;
  parseVersionNumber(
    HTML_VERSION_NAME,
    manifestSelf->meta.version);
  manifestSelf->author = "HTMonkeyG";
  manifestSelf->description = HTTexts_ModLoaderDesc;
  manifestSelf->gameEditionFlags = 3;
  manifestSelf->modName = HTTexts_ModLoaderName;
  manifestSelf->runtime = runtimeSelf;

  // Set runtime data.
  runtimeSelf->handle = gModLoaderHandle;
  runtimeSelf->manifest = manifestSelf;
  runtimeSelf->loaderFunc.pfn_HTModOnEnable = nullptr;
  runtimeSelf->loaderFunc.pfn_HTModOnInit = modOnInit;
  runtimeSelf->loaderFunc.pfn_HTModRenderGui = HTiRenderGUI;

  HTiOptionsLoadFor(runtimeSelf);
}
