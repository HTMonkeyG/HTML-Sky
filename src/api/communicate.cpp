// ----------------------------------------------------------------------------
// Mod communication APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <unordered_set>
#include "includes/htmodloader.h"
#include "htinternal.h"

static std::unordered_map<std::string, std::unordered_set<PFN_HTEventCallback>> gEventCallbacks;
// This mutex is only for gEventCallbacks.
static std::shared_mutex gMutex;

HTMLAPIATTR PFN_HTVoidFunction HTMLAPI HTGetProcAddr(
  HMODULE hModule,
  LPCSTR name
) {
  std::lock_guard<std::mutex> lock(gModDataLock);

  if (!hModule || !name)
    return HTiErrAndRet(HTError_InvalidParam, nullptr);
  if (!HTiCheckHandleType(hModule, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, nullptr);

  ModRuntime *rt = HTiGetModRuntime(hModule);
  if (!rt)
    return HTiErrAndRet(HTError_InvalidParam, nullptr);

  auto it = rt->functions.find(name);
  if (it == rt->functions.end())
    return HTiErrAndRet(HTError_NoMoreMatches, nullptr);

  return HTiErrAndRet(HTError_Success, it->second);
}

HTMLAPIATTR HTStatus HTMLAPI HTCommRegFunction(
  HMODULE hModule,
  LPCSTR name,
  PFN_HTVoidFunction func
) {
  std::lock_guard<std::mutex> lock(gModDataLock);
  ModRuntime *rt;

  if (!hModule || !name || !func)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!HTiCheckHandleType(hModule, HTHandleType_Mod))
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  rt = HTiGetModRuntime(hModule);
  if (!rt)
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  rt->functions[name] = func;

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTCommOnEvent(
  LPCSTR name,
  PFN_HTEventCallback callback
) {
  std::unique_lock<std::shared_mutex> lock(gMutex);

  if (!name || !callback)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  
  gEventCallbacks[name].insert(callback);

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTCommOffEvent(
  LPCSTR name,
  PFN_HTEventCallback callback
) {
  std::unique_lock<std::shared_mutex> lock(gMutex);

  if (!name || !callback)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  
  auto it = gEventCallbacks.find(name);
  if (it == gEventCallbacks.end())
    return HTiErrAndRet(HTError_NoMoreMatches, HT_FAIL);

  it->second.erase(callback);

  if (it->second.empty())
    gEventCallbacks.erase(it);

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR HTStatus HTMLAPI HTCommEmitEvent(
  LPCSTR name,
  LPVOID reserved,
  LPVOID data
) {
  std::vector<PFN_HTEventCallback> localCallbacks;

  if (!name)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);

  {
    std::shared_lock<std::shared_mutex> lock(gMutex);

    auto it = gEventCallbacks.find(name);
    if (it == gEventCallbacks.end())
      return HTiErrAndRet(HTError_NoMoreMatches, HT_FAIL);
    if (it->second.empty())
      return HTiErrAndRet(HTError_Success, HT_SUCCESS);

    // Save a local copy of the callbacks to avoid deadlock.
    localCallbacks.assign(it->second.begin(), it->second.end());
  }

  for (auto it = localCallbacks.begin(); it != localCallbacks.end(); it++) {
    PFN_HTEventCallback cb = *it;
    if (cb)
      cb(data);
  }

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}
