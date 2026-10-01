#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <mutex>
#include <string>
#include "htinternal.hpp"

static FILE *gLogFile = NULL;

#ifdef HTML_ENABLE_LOGGER
// Serializes log output so concurrent HTiLogA/HTiLogW calls don't interleave.
static std::mutex gLogMutex;

static void writeTimestamp() {
  SYSTEMTIME time = {0};
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
}
#endif

/**
 * Initialize the logger.
 *
 * Must be called EXACTLY ONCE when the dll is attached.
 */
void HTiInitLogger(
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

void HTiLogA(const char *format, ...) {
#ifdef HTML_ENABLE_LOGGER
  va_list arg;

  va_start(arg, format);
  std::lock_guard<std::mutex> lock(gLogMutex);
  writeTimestamp();
  vprintf(format, arg);
  va_end(arg);
#endif
}

void HTiLogW(const wchar_t *format, ...) {
#ifdef HTML_ENABLE_LOGGER
  va_list arg;

  va_start(arg, format);
  // Format into a wide buffer, then convert to UTF-8 and emit through the byte
  // stream. stdout is byte-oriented (HTiLogA uses printf); mixing wprintf on
  // the same stream is undefined behavior, so never write wchar_t directly.
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
  fputs(utf8.c_str(), stdout);
#endif
}
