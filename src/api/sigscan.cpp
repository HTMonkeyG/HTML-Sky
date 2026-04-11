// ----------------------------------------------------------------------------
// Signature code scanner APIs of HT's Mod Loader.
// ----------------------------------------------------------------------------
#include <windows.h>

#include "htaliases.h"
#include "htinternal.hpp"
#include "includes/htmodloader.h"

/**
 * Convert signature string to byte pattern.
 */
static i32 *sigToPattern(
  const char *sig,
  u64 *patternLen
) {
  u64 l, i;
  i32 *pattern;
  const char *p;
  char *q;

  l = strlen(sig);
  if (l <= 1)
    return nullptr;
  
  // We can ensure that (l >> 1) is larger than the actual signature array.
  pattern = (i32 *)malloc(l * sizeof(i32));
  p = sig;

  for (i = 0; p < (sig + l); p++) {
    if (*p == '?') {
      // Wildcard characters.
      p++;
      if (*p == '?')
        p++;
      pattern[i] = -1;
      i++;
    } else if (*p == ' ')
      continue;
    else {
      pattern[i] = strtoul(p, &q, 16);
      p = q;
      i++;
    }
  }

  *patternLen = i;

  return pattern;
}

/**
 * Scan the specified signature in given module.
 */
static void *sigScan(
  const wchar_t *moduleName,
  const char *sig,
  i32 offset
) {
  HINSTANCE handle;
  PIMAGE_DOS_HEADER dosHeader;
  PIMAGE_NT_HEADERS ntHeaders;
  MEMORY_BASIC_INFORMATION mbi;
  u64 imageSize, patternLen;
  i32 *pattern;
  u08 *image
    , found;

  handle = GetModuleHandleW(moduleName);
  if (!handle)
    return nullptr;

  dosHeader = (PIMAGE_DOS_HEADER)handle;
  ntHeaders = (PIMAGE_NT_HEADERS)((u08 *)handle + dosHeader->e_lfanew);
  imageSize = ntHeaders->OptionalHeader.SizeOfImage;
  image = (u08 *)handle;

  pattern = sigToPattern(sig, &patternLen);
  if (!pattern || patternLen == 0) {
    if (pattern)
      free(pattern);
    return nullptr;
  }

  u08 *scanStart = image;
  u08 *scanEnd = image + imageSize;

  while (scanStart < scanEnd) {
    if (!VirtualQuery(scanStart, &mbi, sizeof(mbi)))
      break;

    u08 *blockEnd = (u08 *)mbi.BaseAddress + mbi.RegionSize;
    if (blockEnd > scanEnd)
      blockEnd = scanEnd;

    if (
      mbi.State == MEM_COMMIT
      && (mbi.Protect & (
        PAGE_READONLY
        | PAGE_READWRITE
        | PAGE_EXECUTE_READ
        | PAGE_EXECUTE_READWRITE
      ))
    ) {
      for (u08 *ptr = (u08 *)mbi.BaseAddress; ptr + patternLen <= blockEnd; ptr++) {
        found = 1;
        for (u64 j = 0; j < patternLen; j++) {
          if (pattern[j] != -1 && ptr[j] != (u08)pattern[j]) {
            found = 0;
            break;
          }
        }
        if (found) {
          free(pattern);
          return (void *)((char *)(ptr + offset));
        }
      }
    }
    scanStart = blockEnd;
  }

  free(pattern);

  return nullptr;
}

/**
 * Scan a specified signature, and calculate address using E8 or E9 relative
 * jump instructions.
 */
static void *sigScanE8(
  const wchar_t *moduleName,
  const char *sig,
  i32 offset
) {
  u08 *initial = (u08 *)sigScan(moduleName, sig, 0)
    , *result
    , opCode;
  i32 rel;

  if (!initial)
    return nullptr;

  opCode = *(initial + offset);
  if (opCode == 0xE8 || opCode == 0xE9) {
    // Calculate offset.
    result = initial + offset + 5;
    rel = *(initial + offset + 1)
      | (*(initial + offset + 2) << 8)
      | (*(initial + offset + 3) << 16)
      | (*(initial + offset + 4) << 24);
    result += rel;
  } else
    return nullptr;

  return (void *)result;
}

/**
 * Scan a specified signature, and calculate address using FF15 or FF25
 * relative jump instructions.
 */
static void *sigScanFF15(
  const wchar_t *moduleName,
  const char *sig,
  i32 offset
) {
  u08 *initial = (u08 *)sigScan(moduleName, sig, 0)
    , *ptr, *result
    , opCode;
  i32 rel;
  u64 len;

  if (!initial)
    return nullptr;

  opCode = *(initial + offset);
  if (opCode == 0x15 || opCode == 0x25) {
    // Calculate offset.
    ptr = initial + offset + 5;
    rel = *(initial + offset + 1)
      | (*(initial + offset + 2) << 8)
      | (*(initial + offset + 3) << 16)
      | (*(initial + offset + 4) << 24);
    ptr += rel;
  } else
    return nullptr;

  if (
    !ReadProcessMemory(
      GetCurrentProcess(),
      (void *)ptr,
      (void *)&result,
      sizeof(void *),
      &len)
    || len != sizeof(void *)
  )
    return nullptr;

  return (void *)result;
}

HTMLAPIATTR void *HTMLAPI HTSigScan(
  const HTAsmSig *signature
) {
  return HTSigScanEx(
    nullptr,
    signature);
}

HTMLAPIATTR void *HTMLAPI HTSigScanEx(
  const wchar_t *moduleName,
  const HTAsmSig *signature
) {
  void *result = nullptr;

  if (!signature)
    return nullptr;

  if (!moduleName)
    moduleName = gGameProcessName.c_str();

  switch (signature->indirect) {
    case HT_SCAN_DIRECT:
      result = sigScan(
        moduleName,
        signature->sig,
        signature->offset);
      break;

    case HT_SCAN_E8:
      result = sigScanE8(
        moduleName,
        signature->sig,
        signature->offset);
      break;

    case HT_SCAN_FF15:
      result = sigScanFF15(
        moduleName,
        signature->sig,
        signature->offset);
      break;
  }

  return result;
}

HTMLAPIATTR void *HTMLAPI HTSigScanFunc(
  const HTAsmSig *signature,
  HTAsmFunction *func
) {
  if (!signature)
    return nullptr;

  func->fn = HTSigScan(signature);

  return func->fn;
}

HTMLAPIATTR HTStatus HTMLAPI HTSigScanFuncEx(
  const HTAsmSig **signature,
  HTAsmFunction **func,
  UINT32 count
) {
  HTStatus result = HT_SUCCESS;
  const HTAsmSig *sig;
  HTAsmFunction *f;

  if (!signature || !func || !count)
    return HT_FAIL;

  for (u32 i = 0; i < count; i++) {
    sig = signature[i];
    f = func[i];
    if (!sig || !f)
      continue;
    if (!HTSigScanFunc(sig, f))
      // Marked as failed if failed to scan any of the signature codes.
      result = HT_FAIL;
  }

  return result;
}
