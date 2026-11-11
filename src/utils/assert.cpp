#include <stdarg.h>
#include <stdio.h>
#include "htmodloader/htmodloader.h"

void HTiAssertImpl(
  LPCSTR expression,
  LPCSTR file,
  UINT32 line
) {
  char buf[1024];

  snprintf(
    buf,
    sizeof(buf),
    "File: %s\n Line: %d\nExpression: %s",
    file,
    line,
    expression);

  MessageBoxA(
    nullptr,
    buf,
    "Assertion failed",
    MB_ICONERROR);
}

void HTiAssertMsgImpl(
  LPCSTR expression,
  LPCSTR file,
  UINT32 line,
  LPCSTR msg,
  ...
) {
  char buf1[1024];
  char buf2[1024];

  va_list v;
  va_start(v, msg);
  
  vsnprintf(
    buf2,
    sizeof(buf2),
    msg,
    v);
  
  va_end(v);

  snprintf(
    buf1,
    sizeof(buf1),
    "%s\nFile: %s\n Line: %d\nExpression: %s",
    buf2,
    file,
    line,
    expression);

  MessageBoxA(
    nullptr,
    buf1,
    "Assertion failed",
    MB_ICONERROR);
}
