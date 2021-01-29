///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MSFileSysTest.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides tests for the file system abstraction API.                       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <stdint.h>
#include <dxc/Support/WinIncludes.h>
#include "WexTestClass.h"
#include "dxc/Test/HlslTestUtils.h"

#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/Atomic.h"

#include <D3Dcommon.h>
#include "dxc/dxcapi.internal.h"

#include <algorithm>
#include <vector>
#include <memory>

using namespace llvm;
using namespace llvm::sys;
using namespace llvm::sys::fs;

const GUID DECLSPEC_SELECTANY GUID_NULL = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };

#define SIMPLE_IUNKNOWN_IMPL1(_IFACE_) \
  private: volatile std::atomic<llvm::sys::cas_flag> m_dwRef; \
  public:\
  ULONG STDMETHODCALLTYPE AddRef() { return (ULONG)++m_dwRef; } \
  ULONG STDMETHODCALLTYPE Release() { \
    ULONG result = (ULONG)--m_dwRef; \
    if (result == 0) delete this; \
    return result; \
  } \
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) { \
    if (ppvObject == nullptr) return E_POINTER; \
    if (IsEqualIID(iid, __uuidof(IUnknown)) || \
      IsEqualIID(iid, __uuidof(INoMarshal)) || \
      IsEqualIID(iid, __uuidof(_IFACE_))) { \
      *ppvObject = reinterpret_cast<_IFACE_*>(this); \
      reinterpret_cast<_IFACE_*>(this)->AddRef(); \
      return S_OK; \
    } \
    return E_NOINTERFACE; \
  }

class MSFileSysTest
{
public:
  BEGIN_TEST_CLASS(MSFileSysTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_METHOD(CreationWhenInvokedThenNonNull)

  TEST_METHOD(FindFirstWhenInvokedThenHasFile)
  TEST_METHOD(FindFirstWhenInvokedThenFailsIfNoMatch)
  TEST_METHOD(FindNextWhenLastThenNoMatch)
  TEST_METHOD(FindNextWhenExistsThenMatch)

  TEST_METHOD(OpenWhenNewThenZeroSize)
};

static
LPWSTR CoTaskMemDup(LPCWSTR text)
{
  if (text == nullptr) return nullptr;
  size_t len = wcslen(text) + 1;
  LPWSTR result = (LPWSTR)CoTaskMemAlloc(sizeof(wchar_t) * len);
  StringCchCopyW(result, len, text);
  return result;
}

class FixedEnumSTATSTG : public IEnumSTATSTG
{
  SIMPLE_IUNKNOWN_IMPL1(IEnumSTATSTG)
private:
  std::vector<STATSTG> m_items;
  unsigned m_index;
public:
  FixedEnumSTATSTG(_In_count_(itemCount) const STATSTG* items, unsigned itemCount)
  {
    m_dwRef = 0;
    m_index = 0;
    m_items.reserve(itemCount);
    for (unsigned i = 0; i < itemCount; ++i)
    {
      m_items.push_back(items[i]);
      m_items[i].pwcsName = CoTaskMemDup(m_items[i].pwcsName);
    }
  }
  ~FixedEnumSTATSTG()
  {
    for (auto& item : m_items) CoTaskMemFree(item.pwcsName);
  }
  virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt, _Out_writes_to_(celt, *pceltFetched)  STATSTG *rgelt, _Out_opt_  ULONG *pceltFetched)
  {
    if (celt != 1 || pceltFetched == nullptr) return E_NOTIMPL;
    if (m_index >= m_items.size())
    {
      *pceltFetched = 0;
      return S_FALSE;
    }
    
    *pceltFetched = 1;
    *rgelt = m_items[m_index];
    (*rgelt).pwcsName = CoTaskMemDup((*rgelt).pwcsName);
    ++m_index;
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt)  { return E_NOTIMPL; }
  virtual HRESULT STDMETHODCALLTYPE Reset(void)  { return E_NOTIMPL; }
  virtual HRESULT STDMETHODCALLTYPE Clone(IEnumSTATSTG **) { return E_NOTIMPL; }
};

class MockDxcSystemAccess : public IDxcSystemAccess
{
  SIMPLE_IUNKNOWN_IMPL1(IDxcSystemAccess)
private:
  LPCSTR m_fileName;
  LPCSTR m_contents;
  unsigned m_length;
public:
  unsigned findCount;
  MockDxcSystemAccess() : findCount(1), m_dwRef(0)
  {
  }

  static HRESULT Create(MockDxcSystemAccess** pResult)
  {
    *pResult = new (std::nothrow) MockDxcSystemAccess();
    if (*pResult == nullptr) return E_OUTOFMEMORY;
    (*pResult)->AddRef();
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE EnumFiles(LPCWSTR fileName, IEnumSTATSTG** pResult) override {
    STATSTG items[] =
    {
      { L"filename.hlsl", STGTY_STREAM, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, 0, 0, GUID_NULL, 0, 0 },
      { L"filename2.fx",  STGTY_STREAM, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, 0, 0, GUID_NULL, 0, 0 }
    };
    unsigned testCount = (unsigned)_countof(items);
    FixedEnumSTATSTG* resultEnum = new (std::nothrow) FixedEnumSTATSTG(items, std::min(testCount, findCount));
    if (resultEnum == nullptr) {
        *pResult = nullptr;
        return E_OUTOFMEMORY;
    }
    resultEnum->AddRef();
    *pResult = resultEnum;
    return S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE OpenStorage(
    _In_      LPCWSTR lpFileName,
    _In_      DWORD dwDesiredAccess,
    _In_      DWORD dwShareMode,
    _In_      DWORD dwCreationDisposition,
    _In_      DWORD dwFlagsAndAttributes,
    IUnknown** pResult) override {
    *pResult = SHCreateMemStream(nullptr, 0);
    return (*pResult == nullptr) ? E_OUTOFMEMORY : S_OK;
  }
  virtual HRESULT STDMETHODCALLTYPE SetStorageTime(_In_ IUnknown* storage,
    _In_opt_  const FILETIME *lpCreationTime,
    _In_opt_  const FILETIME *lpLastAccessTime,
    _In_opt_  const FILETIME *lpLastWriteTime) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GetFileInformationForStorage(_In_ IUnknown* storage, _Out_ LPBY_HANDLE_FILE_INFORMATION lpFileInformation) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GetFileTypeForStorage(_In_ IUnknown* storage, _Out_ DWORD* fileType) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE CreateHardLinkInStorage(_In_ LPCWSTR lpFileName, _In_ LPCWSTR lpExistingFileName) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE MoveStorage(_In_ LPCWSTR lpExistingFileName, _In_opt_ LPCWSTR lpNewFileName, _In_ DWORD dwFlags) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GetFileAttributesForStorage(_In_ LPCWSTR lpFileName, _Out_ DWORD* pResult) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE DeleteStorage(_In_ LPCWSTR lpFileName) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE RemoveDirectoryStorage(LPCWSTR lpFileName) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE CreateDirectoryStorage(_In_ LPCWSTR lpPathName) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GetCurrentDirectoryForStorage(DWORD nBufferLength, _Out_writes_(nBufferLength) LPWSTR lpBuffer, _Out_ DWORD* len) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GetMainModuleFileNameW(DWORD nBufferLength, _Out_writes_(nBufferLength) LPWSTR lpBuffer, _Out_ DWORD* len) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GetTempStoragePath(DWORD nBufferLength, _Out_writes_(nBufferLength) LPWSTR lpBuffer, _Out_ DWORD* len) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE SupportsCreateSymbolicLink(_Out_ BOOL* pResult) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE CreateSymbolicLinkInStorage(_In_ LPCWSTR lpSymlinkFileName, _In_ LPCWSTR lpTargetFileName, DWORD dwFlags) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE CreateStorageMapping(
    _In_      IUnknown* hFile,
    _In_      DWORD flProtect,
    _In_      DWORD dwMaximumSizeHigh,
    _In_      DWORD dwMaximumSizeLow,
    _Outptr_  IUnknown** pResult) override {
    return E_NOTIMPL;
  }
  virtual HRESULT MapViewOfFile(
    _In_  IUnknown* hFileMappingObject,
    _In_  DWORD dwDesiredAccess,
    _In_  DWORD dwFileOffsetHigh,
    _In_  DWORD dwFileOffsetLow,
    _In_  SIZE_T dwNumberOfBytesToMap,
    _Outptr_ ID3D10Blob** pResult) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE OpenStdStorage(int standardFD, _Outptr_ IUnknown** pResult) override {
    return E_NOTIMPL;
  }
  virtual HRESULT STDMETHODCALLTYPE GetStreamDisplay(_COM_Outptr_result_maybenull_ ITextFont** textFont, _Out_ unsigned* columnCount) override {
    return E_NOTIMPL;
  }
};

void MSFileSysTest::CreationWhenInvokedThenNonNull()
{
  CComPtr<MockDxcSystemAccess> access;
  VERIFY_SUCCEEDED(MockDxcSystemAccess::Create(&access));

  MSFileSystem* fileSystem;
  VERIFY_SUCCEEDED(CreateMSFileSystemForIface(access, &fileSystem));
  VERIFY_IS_NOT_NULL(fileSystem);

  delete fileSystem;
}

void MSFileSysTest::FindFirstWhenInvokedThenHasFile()
{
  CComPtr<MockDxcSystemAccess> access;
  MockDxcSystemAccess::Create(&access);

  MSFileSystem* fileSystem;
  CreateMSFileSystemForIface(access, &fileSystem);
  WIN32_FIND_DATAW findData;
  HANDLE h = fileSystem->FindFirstFileW(L"foobar", &findData);
  VERIFY_ARE_EQUAL_WSTR(L"filename.hlsl", findData.cFileName);
  VERIFY_ARE_NOT_EQUAL(INVALID_HANDLE_VALUE, h);
  fileSystem->FindClose(h);
  delete fileSystem;
}

void MSFileSysTest::FindFirstWhenInvokedThenFailsIfNoMatch()
{
  CComPtr<MockDxcSystemAccess> access;
  MockDxcSystemAccess::Create(&access);
  access->findCount = 0;

  MSFileSystem* fileSystem;
  CreateMSFileSystemForIface(access, &fileSystem);
  WIN32_FIND_DATAW findData;
  HANDLE h = fileSystem->FindFirstFileW(L"foobar", &findData);
  VERIFY_ARE_EQUAL(ERROR_FILE_NOT_FOUND, GetLastError());
  VERIFY_ARE_EQUAL(INVALID_HANDLE_VALUE, h);
  VERIFY_ARE_EQUAL_WSTR(L"", findData.cFileName);
  delete fileSystem;
}

void MSFileSysTest::FindNextWhenLastThenNoMatch()
{
  CComPtr<MockDxcSystemAccess> access;
  MockDxcSystemAccess::Create(&access);

  MSFileSystem* fileSystem;
  CreateMSFileSystemForIface(access, &fileSystem);
  WIN32_FIND_DATAW findData;
  HANDLE h = fileSystem->FindFirstFileW(L"foobar", &findData);
  VERIFY_ARE_NOT_EQUAL(INVALID_HANDLE_VALUE, h);
  BOOL findNext = fileSystem->FindNextFileW(h, &findData);
  VERIFY_IS_FALSE(findNext);
  VERIFY_ARE_EQUAL(ERROR_FILE_NOT_FOUND, GetLastError());
  fileSystem->FindClose(h);
  delete fileSystem;
}

void MSFileSysTest::FindNextWhenExistsThenMatch()
{
  CComPtr<MockDxcSystemAccess> access;
  MockDxcSystemAccess::Create(&access);
  access->findCount = 2;

  MSFileSystem* fileSystem;
  CreateMSFileSystemForIface(access, &fileSystem);
  WIN32_FIND_DATAW findData;
  HANDLE h = fileSystem->FindFirstFileW(L"foobar", &findData);
  VERIFY_ARE_NOT_EQUAL(INVALID_HANDLE_VALUE, h);
  BOOL findNext = fileSystem->FindNextFileW(h, &findData);
  VERIFY_IS_TRUE(findNext);
  VERIFY_ARE_EQUAL_WSTR(L"filename2.fx", findData.cFileName);
  VERIFY_IS_FALSE(fileSystem->FindNextFileW(h, &findData));
  fileSystem->FindClose(h);
  delete fileSystem;
}

void MSFileSysTest::OpenWhenNewThenZeroSize()
{
  CComPtr<MockDxcSystemAccess> access;
  MockDxcSystemAccess::Create(&access);

  MSFileSystem* fileSystem;
  CreateMSFileSystemForIface(access, &fileSystem);
  HANDLE h = fileSystem->CreateFileW(L"new.hlsl", 0, 0, 0, 0);
  VERIFY_ARE_NOT_EQUAL(INVALID_HANDLE_VALUE, h);
  char buf[4];
  DWORD bytesRead;
  VERIFY_IS_TRUE(fileSystem->ReadFile(h, buf, _countof(buf), &bytesRead));
  VERIFY_ARE_EQUAL(0, bytesRead);
  fileSystem->CloseHandle(h);
  delete fileSystem;
}
