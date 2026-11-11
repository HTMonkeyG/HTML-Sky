#ifndef __UTILS_PATHUTILS_HPP__
#define __UTILS_PATHUTILS_HPP__

#include <string>
#include <vector>

namespace HTiPathUtils {

// Normalizes the given path, resolving '..' and '.' segments.
std::wstring Normalize(
  const std::wstring &);

// Joins all given path segments together using the platform-specific separator
// as a delimiter, then normalizes the resulting path.
std::wstring Join(
  const std::vector<std::wstring> &);

// Resolves a sequence of paths or path segments into an absolute path.
std::wstring Resolve(
  const std::vector<std::wstring> &);

// Returns the relative path from `from` to `to` based on the current working
// directory. If from and to each resolve to the same path (after calling
// `Resolve()` on each), a zero-length string is returned.
// If a zero-length string is passed as from or to, the current working directory
// will be used instead of the zero-length strings.
std::wstring Relative(
  const std::wstring &from,
  const std::wstring &to);

// Determines if the literal path is absolute.
bool IsAbsolute(
  const std::wstring &);

}

#endif
