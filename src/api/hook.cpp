// ----------------------------------------------------------------------------
// Inline hook APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <vector>
#include <unordered_map>
#include <mutex>
#include "MinHook.h"

#include "htmodloader.h"

static std::mutex gMutex;

HTMLAPI HTStatus HTInstallHook(
  void *fn,
  void *detour,
  void **origin
) {
  std::lock_guard<std::mutex> lock(gMutex);
  if (MH_CreateHook(fn, detour, origin) == MH_OK)
    return HT_SUCCESS;
  return HT_FAIL;
}

HTMLAPI HTStatus HTEnableHook(
  void *fn
) {
  std::lock_guard<std::mutex> lock(gMutex);
  if (MH_EnableHook(fn) == MH_OK)
    return HT_SUCCESS;
  return HT_FAIL;
}

HTMLAPI HTStatus HTDisableHook(
  void *fn
) {
  std::lock_guard<std::mutex> lock(gMutex);
  if (MH_DisableHook(fn) == MH_OK)
    return HT_SUCCESS;
  return HT_FAIL;
}

HTMLAPI HTStatus HTInstallHookEx(
  HTHookFunction *func
) {
  return HTInstallHook(func->fn, func->detour, &func->origin);
}

HTMLAPI HTStatus HTEnableHookEx(
  HTHookFunction *func
) {
  return HTEnableHook(func->fn);
}

HTMLAPI HTStatus HTDisableHookEx(
  HTHookFunction *func
) {
  return HTDisableHook(func->fn);
}
