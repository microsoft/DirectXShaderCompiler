//===-- WinFunctions.h - Windows Functions for other platforms --*- C++ -*-===//
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

#ifndef LLVM_SUPPORT_WINFUNCTIONS_H
#define LLVM_SUPPORT_WINFUNCTIONS_H

#ifndef _WIN32

#include "dxc/Support/WinAdapter.h"

HRESULT StringCchPrintfA(char *dst, size_t dstSize, const char *format, ...);
HRESULT UIntAdd(UINT uAugend, UINT uAddend, UINT *puResult);
HRESULT IntToUInt(int in, UINT *out);
HRESULT SizeTToInt(size_t in, INT *out);
HRESULT UInt32Mult(UINT a, UINT b, UINT *out);
int _stricmp(const char *str1, const char *str2);
HRESULT CoGetMalloc(DWORD dwMemContext, IMalloc **ppMalloc);

HANDLE CreateFile2(_In_ LPCWSTR lpFileName, _In_ DWORD dwDesiredAccess,
                   _In_ DWORD dwShareMode, _In_ DWORD dwCreationDisposition,
                   _In_opt_ void *pCreateExParams);

HANDLE CreateFileW(_In_ LPCWSTR lpFileName, _In_ DWORD dwDesiredAccess,
                   _In_ DWORD dwShareMode, _In_opt_ void *lpSecurityAttributes,
                   _In_ DWORD dwCreationDisposition,
                   _In_ DWORD dwFlagsAndAttributes,
                   _In_opt_ HANDLE hTemplateFile);

BOOL GetFileSizeEx(_In_ HANDLE hFile, _Out_ PLARGE_INTEGER lpFileSize);

BOOL ReadFile(_In_ HANDLE hFile, _Out_ LPVOID lpBuffer,
              _In_ DWORD nNumberOfBytesToRead,
              _Out_opt_ LPDWORD lpNumberOfBytesRead,
              _Inout_opt_ void *lpOverlapped);
BOOL WriteFile(_In_ HANDLE hFile, _In_ LPCVOID lpBuffer,
               _In_ DWORD nNumberOfBytesToWrite,
               _Out_opt_ LPDWORD lpNumberOfBytesWritten,
               _Inout_opt_ void *lpOverlapped);

BOOL CloseHandle(_In_ HANDLE hObject);

#endif // _WIN32

#endif // LLVM_SUPPORT_WINFUNCTIONS_H
