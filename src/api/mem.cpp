// ----------------------------------------------------------------------------
// Memory manager APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <stdlib.h>
#include <unordered_set>
#include <mutex>
#include "includes/htmodloader.h"
#include "htinternal.hpp"

static std::mutex gMutex;
// Stores pointers to all allocated mem blocks.
static std::unordered_set<void *> gAllocated;

HTMLAPIATTR LPVOID HTMLAPI HTMemAlloc(UINT64 size) {
  std::lock_guard<std::mutex> lock(gMutex);
  void *result = HeapAlloc(gHeap, 0, size);
  if (result)
    gAllocated.insert(result);
  return result;
}

HTMLAPIATTR LPVOID HTMLAPI HTMemNew(UINT64 count, UINT64 size) {
  std::lock_guard<std::mutex> lock(gMutex);
  void *result = HeapAlloc(gHeap, 0, count * size);
  if (result)
    gAllocated.insert(result);
  return result;
}

HTMLAPIATTR HTStatus HTMLAPI HTMemFree(LPVOID pointer) {
  std::lock_guard<std::mutex> lock(gMutex);
  auto it = gAllocated.find(pointer);

  if (it == gAllocated.end())
    return HT_FAIL;

  gAllocated.erase(pointer);
  HeapFree(gHeap, 0, pointer);

  return HT_SUCCESS;
}
