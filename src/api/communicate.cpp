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
  const char *name
) {
  std::lock_guard<std::mutex> lock(gModDataLock);

  if (!hModule || !name)
    return nullptr;

  ModRuntime *rt = getModRuntime(hModule);
  if (!rt)
    return nullptr;

  auto it = rt->functions.find(name);
  if (it == rt->functions.end())
    return nullptr;

  return it->second;
}

HTMLAPIATTR HTStatus HTMLAPI HTCommRegFunction(
  HMODULE hModule,
  const char *name,
  PFN_HTVoidFunction func
) {
  std::lock_guard<std::mutex> lock(gModDataLock);

  if (!hModule || !name || !func)
    return HT_FAIL;

  ModRuntime *rt = getModRuntime(hModule);
  if (!rt)
    return HT_FAIL;

  rt->functions[name] = func;

  return HT_SUCCESS;
}

HTMLAPIATTR HTStatus HTMLAPI HTCommOnEvent(
  const char *name,
  PFN_HTEventCallback callback
) {
  std::unique_lock<std::shared_mutex> lock(gMutex);

  if (!name || !callback)
    return HT_FAIL;
  
  gEventCallbacks[name].insert(callback);

  return HT_SUCCESS;
}

HTMLAPIATTR HTStatus HTMLAPI HTCommOffEvent(
  const char *name,
  PFN_HTEventCallback callback
) {
  std::unique_lock<std::shared_mutex> lock(gMutex);

  if (!name || !callback)
    return HT_FAIL;
  
  auto it = gEventCallbacks.find(name);
  if (it == gEventCallbacks.end())
    return HT_FAIL;

  it->second.erase(callback);

  if (it->second.empty())
    gEventCallbacks.erase(it);

  return HT_SUCCESS;
}

HTMLAPIATTR HTStatus HTMLAPI HTCommEmitEvent(
  const char *name,
  void *reserved,
  void *data
) {
  std::vector<PFN_HTEventCallback> localCallbacks;

  if (!name)
    return HT_FAIL;
  
  {
    std::shared_lock<std::shared_mutex> lock(gMutex);

    auto it = gEventCallbacks.find(name);
    if (it == gEventCallbacks.end())
      return HT_FAIL;
    if (it->second.empty())
      return HT_SUCCESS;

    // Save a local copy of the callbacks to avoid deadlock.
    localCallbacks.assign(it->second.begin(), it->second.end());
  }

  for (auto it = localCallbacks.begin(); it != localCallbacks.end(); it++) {
    PFN_HTEventCallback cb = *it;
    if (cb)
      cb(data);
  }

  return HT_SUCCESS;
}
