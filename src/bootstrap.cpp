// ----------------------------------------------------------------------------
// Register the mod loader itself as a single mod.
// ----------------------------------------------------------------------------
#include <stdio.h>
#include <mutex>
#include "htmodloader/htmodloader.h"
#include "utils/texts.h"
#include "htinternal.hpp"

HTHandle hKeyMenuToggle = nullptr;

static HTiSemVer getSelfVersion() {
  i32 major = HTML_VERSION / 10000
    , minor = (HTML_VERSION / 100) % 100
    , patch = HTML_VERSION % 100;
  return HTiSemVer{major, minor, patch};
}

static HTStatus HTMLAPI modOnInit(
  void *
) {
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

void HTiBootstrap() {
  std::lock_guard<std::mutex> lock(gModDataLock);
  ModManifest *manifestSelf = &gModDataLoader[HTTexts_ModLoaderPackageName];
  ModRuntime *runtimeSelf = &gModDataRuntime[gModLoaderHandle];

  // Set manifest data.
  manifestSelf->meta.packageName = HTTexts_ModLoaderPackageName;
  manifestSelf->meta.version = getSelfVersion();
  manifestSelf->author = "HTMonkeyG";
  manifestSelf->description = HTTexts_ModLoaderDesc;
  manifestSelf->gameEditionFlags = HT_ImplNull_EditionAll;
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
