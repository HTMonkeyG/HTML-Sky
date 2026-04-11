// ----------------------------------------------------------------------------
// Assembly patch and hook APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <windows.h>
#include <mutex>
#include <shared_mutex>
#include "MinHook.h"

#include "includes/htmodloader.h"
#include "htinternal.hpp"

static HTMutexShared gMutexAsm;
static std::map<void *, ModPatch> gPatches;
static std::map<void *, ModHook> gHooks;

std::vector<ModHook *> HTiAsmHookFindFor(
  HMODULE owner
) {
  HTLockReadable lock{gMutexAsm};
  std::vector<ModHook *> result;

  for (auto &it: gHooks) {
    if (it.second.owner == owner)
      result.push_back(&it.second);
  }

  return result;
}

static HTStatus createHook(
  HMODULE hModuleOwner,
  const std::string &name,
  void *fn,
  void *detour,
  void **origin
) {
  ModHook hook;
  MH_STATUS s;

  if (!hModuleOwner || !fn || !detour)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hModuleOwner, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  if (!HTiIsExecutableAddr(fn) || !HTiIsExecutableAddr(detour))
    // Not executable address.
    return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

  if (gHooks.find(fn) != gHooks.end())
    // Already hooked.
    return HTiErrAndRet(HTError_AlreadyExists, HT_FAIL);

  hook.intent = hook.actual = (PFN_HTVoidFunction)fn;
  hook.detour = (PFN_HTVoidFunction)detour;
  hook.owner = hModuleOwner;
  hook.name = name;
  hook.isEnabled = false;

  s = MH_CreateHook(
    (void *)hook.actual,
    (void *)hook.detour,
    (void **)&hook.trampoline);

  if (s != MH_OK)
    return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

  if (origin)
    *origin = (void *)hook.trampoline;

  gHooks[(void *)hook.intent] = hook;

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmHookCreateRaw(
  HMODULE hModuleOwner,
  LPVOID fn,
  LPVOID detour,
  LPVOID *origin
) {
  HTLockShared lock{gMutexAsm};

  return createHook(
    hModuleOwner,
    "<Raw>",
    fn,
    detour,
    origin);
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmHookCreateAPI(
  HMODULE hModuleOwner,
  LPCWSTR module,
  LPCSTR function,
  LPVOID detour,
  LPVOID *origin,
  LPVOID *target
) {
  HTLockShared lock{gMutexAsm};
  HTStatus s;
  LPVOID origin_;
  
  if (!module || !function)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);

  // Get target module.
  HMODULE hTarget = GetModuleHandleW(module);
  if (!hTarget)
    return HTiErrAndRet(HTError_ModuleNotFound, HT_FAIL);

  // Get target function address.
  LPVOID targetFn = (LPVOID)GetProcAddress(hTarget, function);
  if (!targetFn)
    return HTiErrAndRet(HTError_NotFound, HT_FAIL);

  s = createHook(
    hModuleOwner,
    HTiWstringToUtf8(module) + ":" + function,
    targetFn,
    detour,
    &origin_);
  if (s != HT_SUCCESS)
    // We directly pass the error code to the caller.
    return s;

  if (origin)
    *origin = origin_;
  if (target)
    *target = targetFn;

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmHookCreate(
  HMODULE hModuleOwner,
  HTAsmFunction *func
) {
  HTLockShared lock{gMutexAsm};

  if (!func)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);

  return createHook(
    hModuleOwner,
    func->name ? func->name : "<Unknown>",
    func->fn,
    func->detour,
    &func->origin);
}

static HTStatus enableHook(
  HMODULE hModuleOwner,
  LPVOID fn,
  bool action
) {
  MH_STATUS (*mh)(LPVOID)
    , s;

  if (!hModuleOwner)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hModuleOwner, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  auto hook = gHooks.find(fn);
  if (hook == gHooks.end())
    // Not hooked.
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  
  mh = action
    ? MH_EnableHook
    : MH_DisableHook;

  if (fn != HT_ALL_HOOKS) {
    s = mh((LPVOID)fn);

    if (s != MH_OK)
      return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

    hook->second.isEnabled = action;

    return HTiErrAndRet(HTError_Success, HT_SUCCESS);
  }

  for (auto it = gHooks.begin(); it != gHooks.end(); it++) {
    if (it->second.owner != hModuleOwner)
      continue;

    s = mh((LPVOID)it->second.actual);

    if (s != MH_OK)
      return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

    it->second.isEnabled = action;
  }

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmHookEnable(
  HMODULE hModuleOwner,
  LPVOID fn
) {
  HTLockShared lock{gMutexAsm};

  return enableHook(hModuleOwner, fn, true);
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmHookDisable(
  HMODULE hModuleOwner,
  LPVOID fn
) {
  HTLockShared lock{gMutexAsm};

  return enableHook(hModuleOwner, fn, false);
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmPatchCreate(
  HMODULE hModuleOwner,
  LPVOID target,
  LPCVOID data,
  UINT64 size
) {
  return HT_FAIL;
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmPatchEnable(
  HMODULE hModuleOwner,
  LPVOID target
) {
  return HT_FAIL;
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmPatchDisable(
  HMODULE hModuleOwner,
  LPVOID target
) {
  return HT_FAIL;
}
