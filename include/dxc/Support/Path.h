#pragma once

#include "llvm/ADT/StringRef.h"
#include <string>

namespace hlsl {

template <typename CharTy>
bool IsAbsoluteOrCurDirRelativeShared(const CharTy *Path) {
  if (!Path || !Path[0])
    return false;
  // Current dir-relative path.
  if (Path[0] == '.') {
    return Path[1] == '\0' || Path[1] == '/' || Path[1] == '\\';
  }
  // Disk designator, then absolute path.
  if (Path[1] == ':' && (Path[2] == '\\' || Path[2] == '/')) {
    return true;
  }
  // UNC name
  if (Path[0] == '\\') {
    return Path[1] == '\\';
  }

#ifndef _WIN32
  // Absolute paths on unix systems start with '/'
  if (Path[0] == '/') {
    return true;
  }
#endif

  //
  // NOTE: there are a number of cases we don't handle, as they don't play well
  // with the simple file system abstraction we use:
  // - current directory on disk designator (eg, D:file.ext), requires per-disk
  // current dir
  // - parent paths relative to current directory (eg, ..\\file.ext)
  //
  // The current-directory support is available to help in-memory handlers.
  // On-disk handlers will typically have absolute paths to begin with.
  //
  return false;
}

inline bool IsAbsoluteOrCurDirRelativeW(const wchar_t *Path) {
  return IsAbsoluteOrCurDirRelativeShared<wchar_t>(Path);
}
inline bool IsAbsoluteOrCurDirRelative(const char *Path) {
  return IsAbsoluteOrCurDirRelativeShared<char>(Path);
}

template <typename CharT, typename StringTy>
StringTy NormalizePathImpl(const CharT *Path, size_t Length,
                                  bool PrefixWithDot) {
  StringTy PathCopy(Path, Length);

#ifdef _WIN32
  constexpr CharT SlashFrom = '/';
  constexpr CharT SlashTo = '\\';
#else
  constexpr CharT SlashFrom = '\\';
  constexpr CharT SlashTo = '/';
#endif

  for (unsigned i = 0; i < PathCopy.size(); i++) {
    if (PathCopy[i] == SlashFrom)
      PathCopy[i] = SlashTo;
  }
  if (IsAbsoluteOrCurDirRelativeShared<CharT>(PathCopy.c_str())) {
    return PathCopy;
  }

  if (PrefixWithDot)
    return StringTy(1, CharT('.')) + StringTy(1, SlashTo) + PathCopy;

  return PathCopy;
}

inline std::string NormalizePath(const char *Path, bool PrefixWithDot = true) {
  return NormalizePathImpl<char, std::string>(Path, ::strlen(Path),
                                              PrefixWithDot);
}
inline std::wstring NormalizePathW(const wchar_t *Path,
                                   bool PrefixWithDot = true) {
  return NormalizePathImpl<wchar_t, std::wstring>(Path, ::wcslen(Path),
                                                  PrefixWithDot);
}
inline std::string NormalizePath(llvm::StringRef Path,
                                 bool PrefixWithDot = true) {
  return NormalizePathImpl<char, std::string>(Path.data(), Path.size(),
                                              PrefixWithDot);
}

} // namespace hlsl
