//===- llvm/Support/Windows/MSFileSystemImpl.cpp DXComplier Impl *- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MSFileSystemImpl.cpp                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the DXCompiler specific implementation of the Path API.//
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>

#include "llvm/Support/MSFileSystem.h"

#include <new>

///////////////////////////////////////////////////////////////////////////////////////////////////
// Externally visible functions.

HRESULT CreateMSFileSystemForDisk(_COM_Outptr_ ::llvm::sys::fs::MSFileSystem** pResult) throw();

///////////////////////////////////////////////////////////////////////////////////////////////////
// Win32-and-CRT-based MSFileSystem implementation with direct filesystem access.

namespace llvm {
namespace sys  {
namespace fs {

class MSFileSystemForDisk : public MSFileSystem
{
public:
  unsigned _defaultAttributes;
  MSFileSystemForDisk();

  virtual BOOL FindNextFileW(_In_ HANDLE hFindFile, _Out_ LPWIN32_FIND_DATAW lpFindFileData) throw() override;
  virtual HANDLE FindFirstFileW(_In_ LPCWSTR lpFileName, _Out_ LPWIN32_FIND_DATAW lpFindFileData) throw() override;
  virtual void FindClose(HANDLE findHandle) throw() override;
  virtual HANDLE CreateFileW(_In_ LPCWSTR lpFileName, _In_ DWORD dwDesiredAccess, _In_ DWORD dwShareMode, _In_ DWORD dwCreationDisposition, _In_ DWORD dwFlagsAndAttributes) throw() override;
  virtual BOOL SetFileTime(_In_ HANDLE hFile, _In_opt_ const FILETIME *lpCreationTime, _In_opt_ const FILETIME *lpLastAccessTime, _In_opt_ const FILETIME *lpLastWriteTime) throw() override;
  virtual BOOL GetFileInformationByHandle(_In_ HANDLE hFile, _Out_ LPBY_HANDLE_FILE_INFORMATION lpFileInformation) throw() override;
  virtual DWORD GetFileType(_In_ HANDLE hFile) throw() override;
  virtual BOOL CreateHardLinkW(_In_ LPCWSTR lpFileName, _In_ LPCWSTR lpExistingFileName) throw() override;
  virtual BOOL MoveFileExW(_In_ LPCWSTR lpExistingFileName, _In_opt_ LPCWSTR lpNewFileName, _In_ DWORD dwFlags) throw() override;
  virtual DWORD GetFileAttributesW(_In_ LPCWSTR lpFileName) throw() override;
  virtual BOOL CloseHandle(_In_ HANDLE hObject) throw() override;
  virtual BOOL DeleteFileW(_In_ LPCWSTR lpFileName) throw() override;
  virtual BOOL RemoveDirectoryW(_In_ LPCWSTR lpFileName) throw() override;
  virtual BOOL CreateDirectoryW(_In_ LPCWSTR lpPathName) throw() override;
  _Success_(return != 0 && return < nBufferLength)
  virtual DWORD GetCurrentDirectoryW(_In_ DWORD nBufferLength, _Out_writes_to_opt_(nBufferLength, return + 1) LPWSTR lpBuffer) throw() override;
  _Success_(return != 0 && return < nSize)
  virtual DWORD GetMainModuleFileNameW(__out_ecount_part(nSize, return + 1) LPWSTR lpFilename, DWORD nSize) throw() override;
  virtual DWORD GetTempPathW(DWORD nBufferLength, _Out_writes_to_opt_(nBufferLength, return + 1) LPWSTR lpBuffer) throw() override;
  virtual BOOLEAN CreateSymbolicLinkW(_In_ LPCWSTR lpSymlinkFileName, _In_ LPCWSTR lpTargetFileName, DWORD dwFlags) throw() override;
  virtual bool SupportsCreateSymbolicLink() throw() override;
  virtual BOOL ReadFile(_In_ HANDLE hFile, _Out_bytecap_(nNumberOfBytesToRead) LPVOID lpBuffer, _In_ DWORD nNumberOfBytesToRead, _Out_opt_ LPDWORD lpNumberOfBytesRead) throw() override;
  virtual HANDLE CreateFileMappingW(_In_ HANDLE hFile, _In_ DWORD flProtect, _In_ DWORD dwMaximumSizeHigh, _In_ DWORD dwMaximumSizeLow) throw() override;
  virtual LPVOID MapViewOfFile(_In_ HANDLE hFileMappingObject, _In_ DWORD dwDesiredAccess, _In_ DWORD dwFileOffsetHigh, _In_ DWORD dwFileOffsetLow, _In_ SIZE_T dwNumberOfBytesToMap) throw() override;
  virtual BOOL UnmapViewOfFile(_In_ LPCVOID lpBaseAddress) throw() override;
  
  // Console APIs.
  virtual bool FileDescriptorIsDisplayed(int fd) throw() override;
  virtual unsigned GetColumnCount(DWORD nStdHandle) throw() override;
  virtual unsigned GetConsoleOutputTextAttributes() throw() override;
  virtual void SetConsoleOutputTextAttributes(unsigned attributes) throw() override;
  virtual void ResetConsoleOutputTextAttributes() throw() override;

  // CRT APIs.
  virtual int open_osfhandle(intptr_t osfhandle, int flags) throw() override;
  virtual intptr_t get_osfhandle(int fd) throw() override;
  virtual int close(int fd) throw() override;
  virtual long lseek(int fd, long offset, int origin) throw() override;
  virtual int setmode(int fd, int mode) throw() override;
  virtual errno_t resize_file(_In_ LPCWSTR path, uint64_t size) throw() override;
  virtual int Read(int fd, _Out_bytecap_(count) void* buffer, unsigned int count) throw() override;
  virtual int Write(int fd, _In_bytecount_(count) const void* buffer, unsigned int count) throw() override;
};

MSFileSystemForDisk::MSFileSystemForDisk()
{
  _defaultAttributes = GetConsoleOutputTextAttributes();
}

_Use_decl_annotations_
BOOL MSFileSystemForDisk::FindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData)
{
  return ::FindNextFileW(hFindFile, lpFindFileData);
}

_Use_decl_annotations_
HANDLE MSFileSystemForDisk::FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
  return ::FindFirstFileW(lpFileName, lpFindFileData);
}

void MSFileSystemForDisk::FindClose(HANDLE findHandle)
{
  ::FindClose(findHandle);
}

_Use_decl_annotations_
HANDLE MSFileSystemForDisk::CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes)
{
  return ::CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, nullptr, dwCreationDisposition, dwFlagsAndAttributes, nullptr);
}

_Use_decl_annotations_
BOOL MSFileSystemForDisk::SetFileTime(HANDLE hFile, _In_opt_ const FILETIME *lpCreationTime, _In_opt_ const FILETIME *lpLastAccessTime, _In_opt_ const FILETIME *lpLastWriteTime)
{
  return ::SetFileTime(hFile, lpCreationTime, lpLastAccessTime, lpLastWriteTime);
}

_Use_decl_annotations_
BOOL MSFileSystemForDisk::GetFileInformationByHandle(HANDLE hFile, LPBY_HANDLE_FILE_INFORMATION lpFileInformation)
{
  return ::GetFileInformationByHandle(hFile, lpFileInformation);
}

_Use_decl_annotations_
DWORD MSFileSystemForDisk::GetFileType(HANDLE hFile)
{
  return ::GetFileType(hFile);
}

_Use_decl_annotations_
BOOL MSFileSystemForDisk::CreateHardLinkW(LPCWSTR lpFileName, LPCWSTR lpExistingFileName)
{
  return ::CreateHardLinkW(lpFileName, lpExistingFileName, nullptr);
}

_Use_decl_annotations_
BOOL MSFileSystemForDisk::MoveFileExW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, DWORD dwFlags)
{
  return ::MoveFileExW(lpExistingFileName, lpNewFileName, dwFlags);
}

_Use_decl_annotations_
DWORD MSFileSystemForDisk::GetFileAttributesW(LPCWSTR lpFileName)
{
  return ::GetFileAttributesW(lpFileName);
}

_Use_decl_annotations_
BOOL MSFileSystemForDisk::CloseHandle(HANDLE hObject)
{
  return ::CloseHandle(hObject);
}

_Use_decl_annotations_
BOOL MSFileSystemForDisk::DeleteFileW(LPCWSTR lpFileName)
{
  return ::DeleteFileW(lpFileName);
}

_Use_decl_annotations_
BOOL MSFileSystemForDisk::RemoveDirectoryW(LPCWSTR lpFileName)
{
  return ::RemoveDirectoryW(lpFileName);
}

_Use_decl_annotations_
BOOL MSFileSystemForDisk::CreateDirectoryW(LPCWSTR lpPathName)
{
  return ::CreateDirectoryW(lpPathName, nullptr);
}

_Use_decl_annotations_
DWORD MSFileSystemForDisk::GetCurrentDirectoryW(DWORD nBufferLength,  LPWSTR lpBuffer)
{
  return ::GetCurrentDirectoryW(nBufferLength, lpBuffer);
}

_Use_decl_annotations_
DWORD MSFileSystemForDisk::GetMainModuleFileNameW(LPWSTR lpFilename, DWORD nSize)
{
  // Add some code to ensure that the result is null terminated.
  if (nSize <= 1)
  {
    ::SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return 0;
  }

  DWORD result = ::GetModuleFileNameW(nullptr, lpFilename, nSize - 1);
  if (result == 0) return result;
  lpFilename[result] = L'\0';
  return result;
}

_Use_decl_annotations_
DWORD MSFileSystemForDisk::GetTempPathW(DWORD nBufferLength, LPWSTR lpBuffer)
{
  return ::GetTempPathW(nBufferLength, lpBuffer);
}

namespace {
  typedef BOOLEAN(WINAPI *PtrCreateSymbolicLinkW)(
    /*__in*/ LPCWSTR lpSymlinkFileName,
    /*__in*/ LPCWSTR lpTargetFileName,
    /*__in*/ DWORD dwFlags);

  PtrCreateSymbolicLinkW create_symbolic_link_api =
    PtrCreateSymbolicLinkW(::GetProcAddress(
    ::GetModuleHandleW(L"Kernel32.dll"), "CreateSymbolicLinkW"));
}

_Use_decl_annotations_
BOOLEAN MSFileSystemForDisk::CreateSymbolicLinkW(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags)
{
  return create_symbolic_link_api(lpSymlinkFileName, lpTargetFileName, dwFlags);
}

bool MSFileSystemForDisk::SupportsCreateSymbolicLink()
{
  return create_symbolic_link_api != nullptr;
}

_Use_decl_annotations_
BOOL MSFileSystemForDisk::ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, _Out_opt_ LPDWORD lpNumberOfBytesRead)
{
  return ::ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, nullptr);
}

_Use_decl_annotations_
HANDLE MSFileSystemForDisk::CreateFileMappingW(HANDLE hFile, DWORD flProtect, DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow)
{
  return ::CreateFileMappingW(hFile, nullptr, flProtect, dwMaximumSizeHigh, dwMaximumSizeLow, nullptr);
}

_Use_decl_annotations_
LPVOID MSFileSystemForDisk::MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap)
{
  return ::MapViewOfFile(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap);
}

_Use_decl_annotations_
BOOL MSFileSystemForDisk::UnmapViewOfFile(LPCVOID lpBaseAddress) throw()
{
  return ::UnmapViewOfFile(lpBaseAddress);
}

bool MSFileSystemForDisk::FileDescriptorIsDisplayed(int fd)
{
  DWORD Mode;  // Unused
  return (GetConsoleMode((HANDLE)_get_osfhandle(fd), &Mode) != 0);
}

unsigned MSFileSystemForDisk::GetColumnCount(DWORD nStdHandle)
{
  unsigned Columns = 0;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (::GetConsoleScreenBufferInfo(GetStdHandle(nStdHandle), &csbi))
    Columns = csbi.dwSize.X;
  return Columns;
}

unsigned MSFileSystemForDisk::GetConsoleOutputTextAttributes() throw()
{
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (::GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
    return csbi.wAttributes;
  return 0;
}

void MSFileSystemForDisk::SetConsoleOutputTextAttributes(unsigned attributes)
{
  ::SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), attributes);
}

void MSFileSystemForDisk::ResetConsoleOutputTextAttributes()
{
  ::SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), _defaultAttributes);
}

int MSFileSystemForDisk::open_osfhandle(intptr_t osfhandle, int flags)
{
  return ::_open_osfhandle(osfhandle, flags);
}

intptr_t MSFileSystemForDisk::get_osfhandle(int fd)
{
  return ::_get_osfhandle(fd);
}

int MSFileSystemForDisk::close(int fd)
{
  return ::_close(fd);
}

long MSFileSystemForDisk::lseek(int fd, long offset, int origin)
{
  return ::_lseek(fd, offset, origin);
}

int MSFileSystemForDisk::setmode(int fd, int mode)
{
  return ::_setmode(fd, mode);
}

_Use_decl_annotations_
errno_t MSFileSystemForDisk::resize_file(LPCWSTR path, uint64_t size)
{
  int fd = ::_wopen(path, O_BINARY | _O_RDWR, S_IWRITE);
  if (fd == -1)
    return errno;
#ifdef HAVE__CHSIZE_S
  errno_t error = ::_chsize_s(fd, size);
#else
  errno_t error = ::_chsize(fd, size);
#endif
  ::_close(fd);
  return error;
}

_Use_decl_annotations_
int MSFileSystemForDisk::Read(int fd, void* buffer, unsigned int count)
{
  return ::_read(fd, buffer, count);
}

_Use_decl_annotations_
int MSFileSystemForDisk::Write(int fd, const void* buffer, unsigned int count)
{
  return ::_write(fd, buffer, count);
}

} // end namespace fs
} // end namespace sys
} // end namespace llvm

///////////////////////////////////////////////////////////////////////////////////////////////////
// Externally visible functions.

HRESULT CreateMSFileSystemForDisk(_COM_Outptr_ ::llvm::sys::fs::MSFileSystem** pResult)
{
  *pResult = new (std::nothrow) ::llvm::sys::fs::MSFileSystemForDisk();
  return (*pResult != nullptr) ? S_OK : E_OUTOFMEMORY;
}
