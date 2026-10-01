#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <mutex>
#include <string>
#include "htinternal.hpp"

// Dedicated log file handle. We write here instead of reopening the host
// process's stdout, so logging never disturbs the game's own output.
static FILE *gLogFile = NULL;

#ifdef HTML_ENABLE_LOGGER
// Serializes log output so concurrent HTiLogA/HTiLogW calls don't interleave.
static std::mutex gLogMutex;

static void writeTimestamp() {
  SYSTEMTIME time = {0};
  GetLocalTime(&time);
  fprintf(
    gLogFile,
    "[%04d-%02d-%02d %02d:%02d:%02d.%03d]",
    time.wYear,
    time.wMonth,
    time.wDay,
    time.wHour,
    time.wMinute,
    time.wSecond,
    time.wMilliseconds
  );
}
#endif

/**
 * Initialize the logger.
 *
 * Must be called EXACTLY ONCE when the dll is attached. When `fileName` is set
 * the log is written to that file; otherwise, when `allocConsole` is set, a
 * console is attached and used instead.
 */
void HTiInitLogger(
  const wchar_t *fileName,
  i08 allocConsole
) {
  if (fileName) {
    gLogFile = _wfopen(fileName, L"w");
    if (gLogFile)
      // Unbuffered, so lines survive an abrupt crash.
      setvbuf(gLogFile, NULL, _IONBF, 0);
  } else if (allocConsole) {
    FreeConsole();
    AllocConsole();
    gLogFile = freopen("CONOUT$", "w", stdout);
  }
}

void HTiLogA(const char *format, ...) {
#ifdef HTML_ENABLE_LOGGER
  if (!gLogFile)
    return;

  va_list arg;
  va_start(arg, format);
  std::lock_guard<std::mutex> lock(gLogMutex);
  writeTimestamp();
  vfprintf(gLogFile, format, arg);
  va_end(arg);
#endif
}

void HTiLogW(const wchar_t *format, ...) {
#ifdef HTML_ENABLE_LOGGER
  if (!gLogFile)
    return;

  va_list arg;
  va_start(arg, format);
  // Format into a wide buffer, then convert to UTF-8 and emit as bytes. The log
  // file is byte-oriented; never write wchar_t to it directly.
  int wlen = _vscwprintf(format, arg);
  if (wlen < 0) {
    va_end(arg);
    return;
  }
  std::wstring wbuf;
  wbuf.resize((size_t)wlen + 1);
  vswprintf(wbuf.data(), wbuf.size(), format, arg);
  va_end(arg);
  wbuf.resize((size_t)wlen);

  std::string utf8 = HTiWstringToUtf8(wbuf.c_str());

  std::lock_guard<std::mutex> lock(gLogMutex);
  writeTimestamp();
  fputs(utf8.c_str(), gLogFile);
#endif
}
