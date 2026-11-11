#ifndef __UTILS_FSUTILS_HPP__
#define __UTILS_FSUTILS_HPP__

#include <string>
#include <windows.h>

namespace HTiFsUtils {

// Get full path of specified module.
std::wstring GetModulePathW(HMODULE hModule);

// Get the executable name of current process.
std::wstring GetProcessExeNameW();

// Read file as UTF-8 encoding with _wfopen.
std::string ReadFileAsUtf8(const std::wstring &path);

// Check if the given file exists.
bool FileExists(const std::wstring &path);

// Check if the given folder exists.
bool FolderExists(const std::wstring &path);

}

#endif
