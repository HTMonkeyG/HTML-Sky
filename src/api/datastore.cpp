#include <string>

#include "leveldb_c.h"
#include "htinternal.hpp"

static leveldb_t *gLevelDB = nullptr;
static leveldb_writeoptions_t *gWriteOptions = nullptr;
static leveldb_readoptions_t *gReadOptions = nullptr;

i32 HTiInitLDB() {
  char *err = nullptr;

  leveldb_options_t *options = leveldb_options_create();
  if (!options)
    return 0;

  leveldb_options_set_compression(options, leveldb_zlib_compression);
  leveldb_options_set_create_if_missing(options, 1);

  std::wstring dbPathWide{gPathDataWide};
  dbPathWide += L"\\persistent";

  gLevelDB = leveldb_open(
    options,
    HTiWstringToUtf8(dbPathWide.c_str()).c_str(),
    &err);

  leveldb_options_destroy(options);

  if (!gLevelDB) {
    if (err)
      free(err);
    return 0;
  }

  gWriteOptions = leveldb_writeoptions_create();
  gReadOptions = leveldb_readoptions_create();
  if (!gWriteOptions || !gReadOptions) {
    if (gWriteOptions) {
      leveldb_writeoptions_destroy(gWriteOptions);
      gWriteOptions = nullptr;
    }
    if (gReadOptions) {
      leveldb_readoptions_destroy(gReadOptions);
      gReadOptions = nullptr;
    }
    leveldb_close(gLevelDB);
    gLevelDB = nullptr;
    return 0;
  }

  return 1;
}

i32 HTiDeinitLDB() {
  if (gLevelDB)
    leveldb_close(gLevelDB);

  if (gWriteOptions)
    leveldb_writeoptions_destroy(gWriteOptions);
  if (gReadOptions)
    leveldb_readoptions_destroy(gReadOptions);

  gLevelDB = nullptr;
  gWriteOptions = nullptr;
  gReadOptions = nullptr;

  return 1;
}

static inline std::string concatModDataKey(
  std::string &modName,
  const char *key,
  u64 keyLen
) {
  std::string fullKey{modName.c_str()};
  fullKey += '_';
  fullKey.append(key, keyLen);

  return fullKey;
}

HTMLAPIATTR HTStatus HTMLAPI HTDataStore(
  HMODULE hModule,
  LPCSTR key,
  UINT64 keyLen,
  LPCSTR value,
  UINT64 valueLen
) {
  char *err = nullptr;
  ModRuntime *rt;

  if (!hModule || !key || !keyLen || (!value && valueLen))
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!gWriteOptions || !gLevelDB)
    return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

  // Hold gModDataLock only while touching the mod runtime, then copy out the
  // key so the (potentially slow) LevelDB write runs without the lock. The old
  // code read rt->manifest->meta.packageName with no lock, racing mod load.
  std::string fullKey;
  {
    std::lock_guard<std::mutex> lock(gModDataLock);
    rt = HTiGetModRuntime(hModule);
    if (!rt)
      return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);
    fullKey = concatModDataKey(rt->manifest->meta.packageName, key, keyLen);
  }

  leveldb_put(
    gLevelDB,
    gWriteOptions,
    fullKey.data(),
    fullKey.size(),
    value,
    valueLen,
    &err);

  if (err) {
    printf("%s\n", err);
    free(err);
    return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);
  }

  return HTiErrAndRet(HTError_Success, HT_SUCCESS);
}

HTMLAPIATTR LPSTR HTMLAPI HTDataGet(
  HMODULE hModule,
  LPCSTR key,
  UINT64 keyLen,
  UINT64 *valueLen
) {
  char *err = nullptr
    , *result;
  ModRuntime *rt;

  if (!hModule || !valueLen || !key || !keyLen)
    return HTiErrAndRet(HTError_InvalidParam, nullptr);
  if (!gReadOptions || !gLevelDB)
    return HTiErrAndRet(HTError_AccessDenied, nullptr);

  // Copy the key out under gModDataLock, then run the LevelDB read unlocked.
  std::string fullKey;
  {
    std::lock_guard<std::mutex> lock(gModDataLock);
    rt = HTiGetModRuntime(hModule);
    if (!rt)
      return HTiErrAndRet(HTError_InvalidHandle, nullptr);
    fullKey = concatModDataKey(rt->manifest->meta.packageName, key, keyLen);
  }

  result = leveldb_get(
    gLevelDB,
    gReadOptions,
    fullKey.data(),
    fullKey.size(),
    valueLen,
    &err);
  if (!result) {
    HTError type = HTError_AccessDenied;
    *valueLen = 0;
    if (err) {
      printf("%s\n", err);
      if (strstr(err, "NotFound:") == err)
        type = HTError_NotFound;
      free(err);
    }
    return HTiErrAndRet(type, nullptr);
  }

  return HTiErrAndRet(HTError_Success, result);
}

HTMLAPIATTR HTStatus HTMLAPI HTDataStoreStringKey(
  HMODULE hModule,
  LPCSTR key,
  LPCSTR value,
  UINT64 valueLen
) {
  if (!key)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  return HTDataStore(hModule, key, strlen(key), value, valueLen);
}

HTMLAPIATTR LPSTR HTMLAPI HTDataGetStringKey(
  HMODULE hModule,
  LPCSTR key,
  UINT64 *valueLen
) {
  if (!key)
    return HTiErrAndRet(HTError_InvalidParam, nullptr);
  return HTDataGet(hModule, key, strlen(key), valueLen);
}

HTMLAPIATTR VOID HTMLAPI HTDataFree(
  LPVOID value
) {
  free(value);
}
