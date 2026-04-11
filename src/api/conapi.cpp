#include <stdarg.h>

#include "includes/htmodloader.h"
#include "htinternal.hpp"

HTMLAPIATTR HTStatus HTMLAPI HTTellText(
  LPCSTR format,
  ...
) {
  va_list args;

  va_start(args, format);
  HTiAddConsoleLineV(false, format, args);
  va_end(args);

  return HT_SUCCESS;
}

HTMLAPIATTR HTStatus HTMLAPI HTTellTextV(
  LPCSTR format,
  va_list v
) {
  HTiAddConsoleLineV(false, format, v);

  return HT_SUCCESS;
}

HTMLAPIATTR HTStatus HTMLAPI HTTellRaw(
  LPCSTR format,
  ...
) {
  va_list args;

  va_start(args, format);
  HTiAddConsoleLineV(true, format, args);
  va_end(args);

  return HT_SUCCESS;
}

HTMLAPIATTR HTStatus HTMLAPI HTTellRawV(
  LPCSTR format,
  va_list v
) {
  HTiAddConsoleLineV(true, format, v);

  return HT_SUCCESS;
}
