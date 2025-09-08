// ----------------------------------------------------------------------------
// Memory manager APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <stdlib.h>
#include <unordered_set>
#include <mutex>
#include "aliases.h"
#include "htmodloader.h"

static std::mutex gMutex;
// Stores pointers to all allocated mem blocks.
static std::unordered_set<void *> gAllocated;

void *HTMemAlloc(u64 size) {
  std::lock_guard<std::mutex> lock(gMutex);
  void *result = malloc(size);
  if (result)
    gAllocated.insert(result);
  return result;
}

void *HTMemNew(u64 count, u64 size) {
  std::lock_guard<std::mutex> lock(gMutex);
  void *result = malloc(count * size);
  if (result)
    gAllocated.insert(result);
  return result;
}

HTStatus HTMemFree(void *pointer) {
  std::lock_guard<std::mutex> lock(gMutex);
  auto it = gAllocated.find(pointer);

  if (it == gAllocated.end())
    return HT_FAIL;

  gAllocated.erase(pointer);
  free(pointer);

  return HT_SUCCESS;
}
