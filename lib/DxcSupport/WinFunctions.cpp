//===-- WinFunctions.cpp - Windows Functions for other platforms --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines Windows-specific functions used in the codebase for
// non-Windows platforms.
//
//===----------------------------------------------------------------------===//

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dxc/Support/WinFunctions.h"

HRESULT StringCchPrintfA(char *dst, size_t dstSize, const char *format, ...) {
  va_list args;
  va_start(args, format);
  // C++11 snprintf can return the size of the resulting string if it was to be
  // constructed.
  size_t size = snprintf(nullptr, 0, format, args) + 1; // Extra space for '\0'
  if (size > dstSize) {
    *dst = '\0';
  } else {
    snprintf(dst, size, format, args);
  }
  va_end(args);
  return S_OK;
}
HRESULT UIntAdd(UINT uAugend, UINT uAddend, UINT *puResult) {
  HRESULT hr;
  if ((uAugend + uAddend) >= uAugend) {
    *puResult = (uAugend + uAddend);
    hr = S_OK;
  } else {
    *puResult = 0xffffffff;
    hr = ERROR_ARITHMETIC_OVERFLOW;
  }
  return hr;
}
HRESULT IntToUInt(int in, UINT *out) {
  HRESULT hr;
  if (in >= 0) {
    *out = (UINT)in;
    hr = S_OK;
  } else {
    *out = 0xffffffff;
    hr = ERROR_ARITHMETIC_OVERFLOW;
  }
  return hr;
}
HRESULT SizeTToInt(size_t in, int *out) {
  HRESULT hr;
  if(in <= INT_MAX) {
    *out = (int)in;
    hr = S_OK;
  }
  else {
    *out = 0xffffffff;
    hr = ERROR_ARITHMETIC_OVERFLOW;
  }
  return hr;
}
HRESULT UInt32Mult(UINT a, UINT b, UINT *out) {
  uint64_t result = (uint64_t)a * (uint64_t)b;
  if (result > uint64_t(UINT_MAX))
    return ERROR_ARITHMETIC_OVERFLOW;

  *out = (uint32_t)result;
  return S_OK;
}

int _stricmp(const char *str1, const char *str2) {
  size_t i = 0;
  for (; str1[i] && str2[i]; ++i) {
    int d = std::tolower(str1[i]) - std::tolower(str2[i]);
    if (d != 0)
      return d;
  }
  return str1[i] - str2[i];
}

HRESULT CoGetMalloc(DWORD dwMemContext, IMalloc **ppMalloc) {
  *ppMalloc = new IMalloc;
  (*ppMalloc)->AddRef();
  return S_OK;
}

HANDLE CreateFile2(_In_ LPCWSTR lpFileName, _In_ DWORD dwDesiredAccess,
                   _In_ DWORD dwShareMode, _In_ DWORD dwCreationDisposition,
                   _In_opt_ void *pCreateExParams) {
  return CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, pCreateExParams,
                     dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);
}

HANDLE CreateFileW(_In_ LPCWSTR lpFileName, _In_ DWORD dwDesiredAccess,
                   _In_ DWORD dwShareMode, _In_opt_ void *lpSecurityAttributes,
                   _In_ DWORD dwCreationDisposition,
                   _In_ DWORD dwFlagsAndAttributes,
                   _In_opt_ HANDLE hTemplateFile) {
  CW2A pUtf8FileName(lpFileName);
  size_t fd = -1;
  int flags = 0;

  if (dwDesiredAccess & GENERIC_WRITE)
    if (dwDesiredAccess & GENERIC_READ)
      flags |= O_RDWR;
    else
      flags |= O_WRONLY;
  else // dwDesiredAccess may be 0, but open() demands something here. This is mostly harmless
    flags |= O_RDONLY;

  if (dwCreationDisposition == CREATE_ALWAYS)
    flags |= (O_CREAT | O_TRUNC);
  if (dwCreationDisposition == OPEN_ALWAYS)
    flags |= O_CREAT;
  else if (dwCreationDisposition == CREATE_NEW)
    flags |= (O_CREAT | O_EXCL);
  else if (dwCreationDisposition == TRUNCATE_EXISTING)
    flags |= O_TRUNC;
  // OPEN_EXISTING represents default open() behavior

  // Catch Implementation limitations.
  assert(!lpSecurityAttributes && "security attributes not supported in CreateFileW yet");
  assert(!hTemplateFile && "template file not supported in CreateFileW yet");
  assert(dwFlagsAndAttributes == FILE_ATTRIBUTE_NORMAL &&
         "Attributes other than NORMAL not supported in CreateFileW yet");

  fd = open(pUtf8FileName, flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

  return (HANDLE)fd;
}

BOOL GetFileSizeEx(_In_ HANDLE hFile, _Out_ PLARGE_INTEGER lpFileSize) {
  int fd = (size_t)hFile;
  struct stat fdstat;
  int rv = fstat(fd, &fdstat);
  if (!rv) {
    lpFileSize->QuadPart = (LONGLONG)fdstat.st_size;
    return true;
  }
  return false;
}

BOOL ReadFile(_In_ HANDLE hFile, _Out_ LPVOID lpBuffer,
              _In_ DWORD nNumberOfBytesToRead,
              _Out_opt_ LPDWORD lpNumberOfBytesRead,
              _Inout_opt_ void *lpOverlapped) {
  size_t fd = (size_t)hFile;
  ssize_t rv = -1;

  // Implementation limitation
  assert(!lpOverlapped && "Overlapping not supported in ReadFile yet.");

  rv = read(fd, lpBuffer, nNumberOfBytesToRead);
  if (rv < 0)
    return false;
  *lpNumberOfBytesRead = rv;
  return true;
}

BOOL WriteFile(_In_ HANDLE hFile, _In_ LPCVOID lpBuffer,
               _In_ DWORD nNumberOfBytesToWrite,
               _Out_opt_ LPDWORD lpNumberOfBytesWritten,
               _Inout_opt_ void *lpOverlapped) {
  size_t fd = (size_t)hFile;
  ssize_t rv = -1;

  // Implementation limitation
  assert(!lpOverlapped && "Overlapping not supported in WriteFile yet.");

  rv = write(fd, lpBuffer, nNumberOfBytesToWrite);
  if (rv < 0)
    return false;
  *lpNumberOfBytesWritten = rv;
  return true;
}

BOOL CloseHandle(_In_ HANDLE hObject) {
  int fd = (size_t)hObject;
  return !close(fd);
}

#endif // _WIN32
