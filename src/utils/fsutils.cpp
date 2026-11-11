#include <string>
#include <windows.h>
#include "htaliases.h"

namespace HTiFsUtils {

std::wstring GetModulePathW(HMODULE hModule) {
  std::wstring buf(MAX_PATH, L'\0');
  for (;;) {
    const DWORD n = GetModuleFileNameW(
      hModule, buf.data(), static_cast<DWORD>(buf.size()));
    if (n == 0)
      return {};
    if (n < buf.size()) {
      buf.resize(n);
      return buf;
    }
    buf.resize(buf.size() * 2);
  }
}

std::wstring GetProcessExeNameW() {
  const std::wstring path = GetModulePathW(nullptr);
  const size_t pos = path.find_last_of(L"\\/");
  if (pos == std::string::npos)
    return path;
  return path.substr(pos + 1);
}

// Read file as UTF-8 encoding with _wfopen.
std::string ReadFileAsUtf8(
  const std::wstring &path
) {
  std::string buffer;
  FILE *fd = _wfopen(path.c_str(), L"rb");
  u64 size;

  if (!fd)
    return std::string();

  fseek(fd, 0, SEEK_END);
  size = ftell(fd);
  rewind(fd);
  buffer.resize(size + 1);
  fread(buffer.data(), sizeof(char), size, fd);
  buffer[size] = 0;
  fclose(fd);

  return buffer;
}

bool FileExists(
  const std::wstring &path
) {
  DWORD attr = GetFileAttributesW(path.c_str());
  return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool FolderExists(
  const std::wstring &path
) {
  DWORD attr = GetFileAttributesW(path.c_str());
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

} // namespace HTiFsUtils
