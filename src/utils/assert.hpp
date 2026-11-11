#ifndef __UTILS_ASSERT_HPP__
#define __UTILS_ASSERT_HPP__

#include <windows.h>
#include "htaliases.h"

void HTiAssertImpl(
  LPCSTR expression,
  LPCSTR file,
  u32    line
);

void HTiAssertMsgImpl(
  LPCSTR expression,
  LPCSTR file,
  u32    line,
  LPCSTR msg,
  ...
);

#define HTiAssert(expr) (void)(\
  (!!(expr)) || (\
    HTiAssertImpl(#expr, __FILE__, __LINE__),\
    __debugbreak(),\
    0\
  )\
)

#define HTiAssertMsg(expr, msg, ...) (void)(\
  (!!(expr)) || (\
    HTiAssertMsgImpl(#expr, __FILE__, __LINE__, msg, ## __VA_ARGS__),\
    __debugbreak(),\
    0\
  )\
)

#endif
