#include <windows.h>
#include "htaliases.h"
#include "utils/stringutils.hpp"

// String cast functions.
std::wstring utf8towcs(
  const std::string &s
) {
  if (s.empty())
    return L"";

  i32 len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  if (len == 0)
    return L"";

  std::wstring wstr(len, L'\0');
  i32 written = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wstr.data(), len);
  if (!written)
    return L"";

  wstr.resize(written - 1);
  return wstr;
}

std::string wcstoutf8(
  const std::wstring &s
) {
  if (s.empty())
    return "";

  int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (len == 0)
    return "";

  std::string str(len, '\0');
  int written = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &str[0], len, nullptr, nullptr);
  if (written == 0)
    return "";

  str.resize(written - 1);
  return str;
}

std::wstring ansitowcs(
  const std::string &s
) {
  if (s.empty())
    return L"";

  i32 len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
  if (len == 0)
    return L"";

  std::wstring wstr(len, L'\0');
  i32 written = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, wstr.data(), len);
  if (!written)
    return L"";

  wstr.resize(written - 1);
  return wstr;
}

std::string wcstoansi(
  const std::wstring &s
) {
  if (s.empty())
    return "";

  int len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (len == 0)
    return "";

  std::string str(len, '\0');
  int written = WideCharToMultiByte(CP_ACP, 0, s.c_str(), -1, &str[0], len, nullptr, nullptr);
  if (written == 0)
    return "";

  str.resize(written - 1);
  return str;
}
