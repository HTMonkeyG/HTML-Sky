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
  if (!gWriteOptions || gReadOptions)
    return 0;

  return 1;
}

i32 HTiDeinitLDB() {
  if (gLevelDB)
    leveldb_close(gLevelDB);

  return 1;
}

static inline std::string concatModDataKey(
  std::string &modName,
  const char *key,
  UINT64 keyLen
) {
  std::string fullKey{modName.c_str()};
  fullKey += '_';
  fullKey.append(key, keyLen);

  return fullKey;
}

HTMLAPIATTR HTStatus HTMLAPI HTDataStore(
  HMODULE hModule,
  const char *key,
  UINT64 keyLen,
  const char *value,
  UINT64 valueLen
) {
  char *err = nullptr;
  ModRuntime *rt;

  if (!hModule || !key || !keyLen || !value)
    return HTiErrAndRet(HTError_InvalidParam, HT_FAIL);
  if (!gReadOptions || !gLevelDB)
    return HTiErrAndRet(HTError_AccessDenied, HT_FAIL);

  rt = HTiGetModRuntime(hModule);
  if (!rt)
    return HTiErrAndRet(HTError_InvalidHandle, HT_FAIL);

  std::string &modName = rt->manifest->meta.packageName;
  std::string fullKey = concatModDataKey(modName, key, keyLen);

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

HTMLAPIATTR char *HTMLAPI HTDataGet(
  HMODULE hModule,
  const char *key,
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

  rt = HTiGetModRuntime(hModule);
  if (!rt)
    return HTiErrAndRet(HTError_InvalidHandle, nullptr);
  
  std::string &modName = rt->manifest->meta.packageName;
  std::string fullKey = concatModDataKey(modName, key, keyLen);

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
  const char *key,
  const char *value,
  UINT64 valueLen
) {
  return HTDataStore(hModule, key, strlen(key), value, valueLen);
}

HTMLAPIATTR char *HTMLAPI HTDataGetStringKey(
  HMODULE hModule,
  const char *key,
  UINT64 *valueLen
) {
  return HTDataGet(hModule, key, strlen(key), valueLen);
}

HTMLAPIATTR void HTMLAPI HTDataFree(
  char *value
) {
  free(value);
}
