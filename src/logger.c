#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#include "aliases.h"

static FILE *gLogFile = NULL;

/**
 * Initialize the logger.
 * 
 * Must be called EXACTLY ONCE when the dll is attached.
 */
void HTInitLogger(
  const wchar_t *fileName,
  i08 allocConsole
) {
  if (fileName) {
    gLogFile = _wfreopen(fileName, L"w+t", stdout);
    setvbuf(stdout, NULL, _IONBF, 2);
  } else if (allocConsole) {
    FreeConsole();
    AllocConsole();
    freopen("CONOUT$", "w+t", stdout);
  }
}

void HTLogA(const char *format, ...) {
  SYSTEMTIME time = {0};
  va_list arg;

  va_start(arg, format);
  GetLocalTime(&time);
  printf(
    "[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
    time.wYear,
    time.wMonth,
    time.wDay,
    time.wHour,
    time.wMinute,
    time.wSecond,
    time.wMilliseconds
  );
  vprintf(format, arg);
  va_end(arg);
}

void HTLogW(const wchar_t *format, ...) {
  SYSTEMTIME time = {0};
  va_list arg;

  va_start(arg, format);
  GetLocalTime(&time);
  wprintf(
    L"[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
    time.wYear,
    time.wMonth,
    time.wDay,
    time.wHour,
    time.wMinute,
    time.wSecond,
    time.wMilliseconds
  );
  vwprintf(format, arg);
  va_end(arg);
}
