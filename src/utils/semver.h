#ifndef __SEMVER_H__
#define __SEMVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
  uint32_t isNumeric;
  union {
    uint32_t numericValue;
    char *stringValue;
  };
} SemVerField;

typedef struct {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;

  uint32_t releaseCount;
  SemVerField *pRelease;

  uint32_t buildCount;
  SemVerField *pBuild;
} SemVer;

// Parse a semver from string.
const SemVer *HTiSemVerParse(
  const char *);

// Delete a semver object.
void HTiSemVerDelete(
  const SemVer *);

// Compare two semver object.
int HTiSemVerCompare(
  const SemVer *,
  const SemVer *);

#ifdef __cplusplus
}
#endif

#endif
