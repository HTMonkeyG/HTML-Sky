// ----------------------------------------------------------------------------
// Register the mod loader itself as a single mod.
// ----------------------------------------------------------------------------
#include <stdio.h>
#include <mutex>
#include "includes/htmodloader.h"
#include "utils/texts.h"
#include "htinternal.hpp"

HTHandle hKeyMenuToggle = nullptr;
static HTHandle hKeyReleaseInput = nullptr;

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
  hKeyMenuToggle = HTHotkeyRegisterEx(
    gModLoaderHandle,
    "Toggle menu display",
    HTKey_GraveAccent,
    HTHotkeyFlags_NoBlock);
  HTHotkeyListen(
    hKeyMenuToggle,
    HTiToggleMenuState);

  hKeyReleaseInput = HTHotkeyRegisterEx(
    gModLoaderHandle,
    "Release mouse and ImGui focus",
    HTKey_LeftAlt,
    HTHotkeyFlags_NoBlock);
  HTHotkeyListen(
    hKeyReleaseInput,
    HTiReleaseInputCapture);
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
