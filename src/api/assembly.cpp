// ----------------------------------------------------------------------------
// Assembly patch and hook APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <windows.h>
#include "MinHook.h"

#include "includes/htmodloader.h"
#include "htinternal.h"

// Contexts of a patch.
struct ModPatch {
  // Begin address of the patch.
  void *addr;
  // Original data of the patch.
  std::vector<u08> original;
  // Patch data to be set.
  std::vector<u08> patched;
  // Owner of this patch.
  HMODULE owner;
};

// Contexts of a hook.
struct ModHook {
  // Address of the function intended to be detoured.
  PFN_HTVoidFunction intent;
  // Address of the actually hooked function due to the hook chain.
  PFN_HTVoidFunction actual;
  // Address of the detour function.
  PFN_HTVoidFunction detour;
  // Trampoline function to be called.
  PFN_HTVoidFunction trampoline;
  // The binary data of the leading JMP instructions.
  ModPatch *header;
  // Hook chain datas.
  HMODULE owner;
  ModHook *next;
  ModHook *prev;
  // Debug only.
  std::string name;
};

static std::mutex gMutex;
static std::map<void *, ModPatch> gPatches;
static std::map<void *, ModHook> gHooks;

static bool isExecutableAddr(void *) {
  return true;
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmHookCreateRaw(
  HMODULE hModuleOwner,
  LPVOID fn,
  LPVOID detour,
  LPVOID *origin
) {
  std::lock_guard<std::mutex> lock(gMutex);
  ModHook hook;
  MH_STATUS s;

  if (!hModuleOwner || !fn || !detour)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hModuleOwner, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  if (!isExecutableAddr(fn) || !isExecutableAddr(detour))
    // Not executable address.
    return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

  if (gHooks.find(fn) != gHooks.end())
    // Already hooked.
    return HTiErrAndRet(HTError_AlreadyExists, HT_FAIL);

  hook.intent = hook.actual = (PFN_HTVoidFunction)fn;
  hook.owner = hModuleOwner;
  hook.name = "";

  s = MH_CreateHook(
    (void *)hook.actual,
    (void *)hook.detour,
    (void **)&hook.trampoline);

  if (s != MH_OK)
    return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

  gHooks[(void *)hook.intent] = hook;

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmHookCreateAPI(
  HMODULE hModuleOwner,
  LPCWSTR module,
  LPCSTR function,
  LPVOID detour,
  LPVOID *origin,
  LPVOID *target
) {
  HTStatus s;
  HTAsmFunction f;
  
  if (!module || !hModuleOwner || !function)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hModuleOwner, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  // Get target module.
  HMODULE hTarget = GetModuleHandleW(module);
  if (!hTarget)
    return HTiErrAndRet(HTError_ModuleNotFound, HT_FAIL);

  // Get target function address.
  LPVOID targetFn = (LPVOID)GetProcAddress(hTarget, function);
  if (!targetFn)
    return HTiErrAndRet(HTError_NotFound, HT_FAIL);

  f.fn = targetFn;
  f.detour = detour;
  f.name = function;

  s = HTAsmHookCreate(hModuleOwner, &f);
  if (s != HT_SUCCESS)
    // We directly pass the error code to the caller.
    return s;
  
  if (origin)
    *origin = f.origin;
  if (target)
    *target = targetFn;

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmHookCreate(
  HMODULE hModuleOwner,
  HTAsmFunction *func
) {
  std::lock_guard<std::mutex> lock(gMutex);
  ModHook hook;
  MH_STATUS s;

  if (!hModuleOwner || !func)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hModuleOwner, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  if (!isExecutableAddr(func->fn))
    // Not executable address.
    return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

  if (gHooks.find(func->fn) != gHooks.end())
    // Already hooked.
    return HTiErrAndRet(HTError_AlreadyExists, HT_FAIL);

  hook.intent = hook.actual = (PFN_HTVoidFunction)func->fn;
  hook.owner = hModuleOwner;
  hook.name = func->name;

  s = MH_CreateHook(
    (void *)hook.actual,
    (void *)hook.detour,
    (void **)&hook.trampoline);
  
  if (s != MH_OK)
    return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

  gHooks[(void *)hook.intent] = hook;

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

static HTStatus enableHook(
  HMODULE hModuleOwner,
  LPVOID fn,
  bool action
) {
  std::lock_guard<std::mutex> lock(gMutex);
  MH_STATUS (*mh)(LPVOID)
    , s;

  if (!hModuleOwner)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hModuleOwner, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  if (gHooks.find(fn) == gHooks.end())
    // Not hooked.
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  
  mh = action
    ? MH_EnableHook
    : MH_DisableHook;

  if (fn != HT_ALL_HOOKS) {
    s = mh((LPVOID)fn);
    if (s != MH_OK)
      return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);
    return HTiErrAndRet(HTError_Success, HT_SUCCESS);
  }

  for (auto it = gHooks.begin(); it != gHooks.end(); it++) {
    if (it->second.owner != hModuleOwner)
      continue;

    s = mh((LPVOID)it->second.actual);
    if (s != MH_OK)
      return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);
  }

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmHookEnable(
  HMODULE hModuleOwner,
  LPVOID fn
) {
  return enableHook(hModuleOwner, fn, true);
}

HTMLAPIATTR HTStatus HTMLAPI HTAsmHookDisable(
  HMODULE hModuleOwner,
  LPVOID fn
) {
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
