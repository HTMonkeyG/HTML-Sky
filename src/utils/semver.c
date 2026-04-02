// ----------------------------------------------------------------------------
// Semantic version parser and comparator.
// ----------------------------------------------------------------------------

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "semver.h"

static inline int isLetter(
  char ch
) {
  return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z');
}

static inline int isNumeric(
  char ch
) {
  return '0' <= ch && ch <= '9';
}

static const char *parseNumber(
  const char *string,
  uint32_t *result,
  int isNumericIdentifier
) {
  const char *p = string;
  uint32_t value = 0;
  
  if (!p || !(*p))
    return NULL;
  if (!isNumeric(*p))
    return NULL;
  printf("%c\n", *p);

  while (isNumeric(*p)) {
    value = 10 * value + (uint32_t)*p - (uint32_t)'0';
    p++;
    if (isNumericIdentifier && !value)
      // <numeric identifier> does not allow leading '0' of numbers.
      break;
  }

  if (result)
    *result = value;

  return p;
}

static const char *parseString(
  const char *string,
  char **result
) {
  const char *begin = string
    , *end = string;

  if (!string)
    return NULL;

  while (isLetter(*end) || isNumeric(*end) || *end == '-')
    end++;

  if (begin == end)
    return NULL;

  if (result) {
    size_t length = (size_t)(end - begin);
    *result = (char *)malloc(length + 1);
    memcpy(*result, begin, length);
    (*result)[length] = '\0';
  }

  return end;
}

#define match(p, ch, label) {\
  if (*(p) != (ch))\
    goto label;\
  else\
    (p)++;\
}

SemVer *HTiSemVerParse(
  const char *string
) {
  const char *p = string;
  char *t;
  SemVer *result = (SemVer *)malloc(sizeof(SemVer));
  memset(result, 0, sizeof(SemVer));

  // <valid semver> ::= <version core>
  //   | <version core> "+" <build>
  //   | <version core> "-" <pre-release>
  //   | <version core> "-" <pre-release> "+" <build>

  // <version core> ::= <major> "." <minor> "." <patch>

  // <major> ::= <numeric identifier>
  p = parseNumber(p, &result->major, 1);
  if (!p)
    goto ErrRet;

  match(p, '.', ErrRet);

  // <minor> ::= <numeric identifier>
  p = parseNumber(p, &result->minor, 1);
  if (!p)
    goto ErrRet;

  match(p, '.', ErrRet);

  // <patch> ::= <numeric identifier>
  p = parseNumber(p, &result->patch, 1);
  if (!p)
    goto ErrRet;

  if (*p != '+' && *p != '-')
    return result;

  // <pre-release> ::= <dot-separated pre-release identifiers>
  // <dot-separated pre-release identifiers> ::= <pre-release identifier>
  //   | <pre-release identifier> "." <dot-separated pre-release identifiers>

  if (*p == '-') {
    match(p, '-', ErrRet);

    // <pre-release identifier> ::= <alphanumeric identifier>
    //   | <numeric identifier>

    do {
      const char *begin = p;
      p = parseString(p, &t);
      if (!p)
        return result;

      // Expand the array.
      uint32_t count = ++result->releaseCount;
      SemVerField *fields = (SemVerField *)malloc(
        count * sizeof(SemVerField));
      memset(fields, 0, count * sizeof(SemVerField));
      SemVerField *field = &fields[count - 1];

      // Try to convert to <numeric identifier>.
      uint32_t numericValue;
      if (parseNumber(begin, &numericValue, 1) == p) {
        field->isNumeric = 1;
        field->numericValue = numericValue;
      } else
        field->stringValue = t;

      if (count > 1 && result->pRelease) {
        memcpy(
          fields,
          result->pRelease,
          (count - 1) * sizeof(SemVerField));
        free(result->pRelease);
      }
      result->pRelease = fields;
    } while(*p == '.' && p++);
  }

  // <build> ::= <dot-separated build identifiers>
  // <dot-separated build identifiers> ::= <build identifier>
  //   | <build identifier> "." <dot-separated build identifiers>

  if (*p == '+') {
    match(p, '+', ErrRet);

    // <build identifier> ::= <alphanumeric identifier>
    //   | <digits>

    do {
      const char *begin = p;
      p = parseString(p, &t);
      if (!p)
        return result;

      // Expand the array.
      uint32_t count = ++result->buildCount;
      SemVerField *fields = (SemVerField *)malloc(
        count * sizeof(SemVerField));
      memset(fields, 0, count * sizeof(SemVerField));
      SemVerField *field = &fields[count - 1];

      // Try to convert to <digits>.
      uint32_t numericValue;
      if (parseNumber(begin, &numericValue, 0) == p) {
        field->isNumeric = 1;
        field->numericValue = numericValue;
      } else
        field->stringValue = t;

      if (count > 1 && result->pBuild) {
        memcpy(
          fields,
          result->pBuild,
          (count - 1) * sizeof(SemVerField));
        free(result->pBuild);
      }
      result->pBuild = fields;
    } while(*p == '.' && p++);
  }

  return result;

ErrRet:
  free(result);
  return NULL;
}

void HTiSemVerDelete(
  const SemVer *semver
) {
  for (uint32_t i = 0; i < semver->releaseCount; i++) {
    if (!semver->pRelease[i].isNumeric)
      free(semver->pRelease[i].stringValue);
  }
  free(semver->pRelease);

  for (uint32_t i = 0; i < semver->buildCount; i++) {
    if (!semver->pBuild[i].isNumeric)
      free(semver->pBuild[i].stringValue);
  }
  free(semver->pBuild);

  free(semver);
}

int HTiSemVerCompare(
  const SemVer *a,
  const SemVer *b
) {
  int result = 0;

  if (a->major > b->major)
    return 1;
  else if (a->major < b->major)
    return -1;
  
  if (a->minor > b->minor)
    return 2;
  else if (a->minor < b->minor)
    return -2;
  
  if (a->patch > b->patch)
    return 3;
  else if (a->patch < b->patch)
    return -3;

  return 0;
}
