// ----------------------------------------------------------------------------
// API exports of HT's Mod Loader.
// Copyright (c) HTMonkeyG 2025
// https://www.github.com/HTMonkeyG/HTML-Sky
// ----------------------------------------------------------------------------

// #pragma once
#ifndef __HTMODLOADER_H__
#define __HTMODLOADER_H__

// Throws an error when compiled on other architectures.
#if !(defined(_M_X64) || defined(_WIN64) || defined(__x86_64__) || defined(__amd64__))
#error HT's Mod Loader and it's related mods is only avaliable on x86-64!
#endif

// Mod loader version.
// Version number is used for pre-processing statements handling version
// compatibility.
#define HTML_VERSION 10000
#define HTML_VERSION_NAME "1.0.0"

#define HTMLAPI __fastcall

// Includes.
#include <windows.h>
#include "aliases.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// [SECTION] HTML metadata APIs.
// ----------------------------------------------------------------------------

// Whether the execution was successful or not.
typedef enum {
  HT_FAIL = 0,
  HT_SUCCESS = 1
} HTStatus;

// Game editions.
typedef enum {
  // Uninitialized state.
  HT_EDITION_UNKNOWN = 0,
  // Chinese edition.
  HT_EDITION_CHINESE,
  // International edition.
  HT_EDITION_INTERNATIONAL
} HTGameEdition;

// Game status.
typedef struct {
  // Base address of game executable file.
  void *baseAddr;
  // The edition of the game.
  HTGameEdition edition;
  // The window handle of the game.
  HWND window;
  // The process id of the game.
  DWORD pid;
} HTGameStatus;

/**
 * Get game status object.
 */
void HTGetGameStatus(
  HTGameStatus *status);

// Function prototype.
typedef void *(HTMLAPI *PFN_HTVoidFunction)(
  void);

/**
 * Get the address of a function with name.
 */
PFN_HTVoidFunction HTML_GetProcAddr(
  const char *name);

// ----------------------------------------------------------------------------
// [SECTION] HTML signature scan APIs.
// ----------------------------------------------------------------------------

// Method for obtaining the final address.
typedef enum {
  // The Signature represents the function body.
  HT_SCAN_DIRECT = 0,
  // The signature represents the E8 or E9 instruction that calls the function.
  HT_SCAN_E8,
  // The signature represents the FF15 instruction that calls the function.
  HT_SCAN_FF15,
} HTSigScanType;

// Signature code config.
typedef struct {
  // Signature code.
  const char *sig;
  // Function name, only for debug use.
  const char *name;
  // Method for obtaining the final address.
  HTSigScanType indirect;
  // The byte offset of 0xE8 or 0x15 byte for HT_SCAN_E8 and HT_SCAN_FF15, or
  // the byte offset to the first instruction for HT_SCAN_DIRECT.
  i32 offset;
} HTSignature;

/**
 * Scan with signature.
 */
HTMLAPI void *HTSigScan(
  const HTSignature *signature);
typedef void *(HTMLAPI *PFN_HTSigScan)(
  const HTSignature *signature);

// Function address config.
typedef struct {
  // The address of the detour function if hooked.
  void *detour;
  // The address of the scanned function.
  void *fn;
  // The address of the trampoline function if hooked.
  void *origin;
} HTHookFunction;

/**
 * Scan a single function.
 */
HTMLAPI void *HTSigScanFunc(
  const HTSignature *signature, HTHookFunction *func);
typedef void *(HTMLAPI *PFN_HTSigScanFunc)(
  const HTSignature *signature, HTHookFunction *func);

/**
 * Scan an array of functions.
 */
HTMLAPI i32 HTSigScanFuncEx(
  const HTSignature **signature, HTHookFunction **func, u32 size);
typedef i32 (HTMLAPI *PFN_HTSigScanFuncEx)(
  const HTSignature **signature, HTHookFunction **func, u32 size);

// ----------------------------------------------------------------------------
// [SECTION] HTML inline hook APIs.
// ----------------------------------------------------------------------------

/**
 * Install hook with MinHook.
 */
HTMLAPI HTStatus HTInstallHook(
  void *fn, void *detour, void **origin);
typedef HTStatus (HTMLAPI *PFN_HTInstallHook)(
  void *fn, void *detour, void **origin);

/**
 * Enable hook on specified function.
 */
HTMLAPI HTStatus HTEnableHook(
  void *fn);
typedef HTStatus (HTMLAPI *PFN_HTEnableHook)(
  void *fn);

/**
 * Disable hook on specified function.
 */
HTMLAPI HTStatus HTDisableHook(
  void *fn);
typedef HTStatus (HTMLAPI *PFN_HTDisableHook)(
  void *fn);

/**
 * Install hook from HTHookFunction struct.
 */
HTMLAPI HTStatus HTInstallHookEx(
  HTHookFunction *func);
typedef HTStatus (HTMLAPI *PFN_HTInstallHookEx)(
  HTHookFunction *func);

/**
 * Enable hook on specified function.
 */ 
HTMLAPI HTStatus HTEnableHookEx(
  HTHookFunction *func);
typedef void (HTMLAPI *PFN_HTEnableHookEx)(
  HTHookFunction *func);

/**
 * Disable hook on specified function.
 */
HTMLAPI HTStatus HTDisableHookEx(
  HTHookFunction *func);
typedef void (HTMLAPI *PFN_HTDisableHookEx)(
  HTHookFunction *func);

// ----------------------------------------------------------------------------
// [SECTION] HTML memory manager APIs.
// ----------------------------------------------------------------------------

/**
 * Allocate a sized memory block.
 */
void *HTMemAlloc(
  u64 size);

/**
 * Allocate space for an array of `count` objects, each of `size` bytes.
 * Different from calloc(), HTMemNew won't initialize the memory block.
 */
void *HTMemNew(
  u64 count, u64 size);

/**
 * Free a memory block allocated with HTMemAlloc() or HTMemNew(). Returns
 * HT_FAIL when the pointer is invalid or is already freed.
 * 
 * Mod needs to reset pointer variables to prevent dangling pointers.
 */
HTStatus HTMemFree(
  void *pointer);

#ifdef __cplusplus
}
#endif

#endif
