///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FileIOHelper.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// TODO: consider including an empty blob singleton (possibly UTF-8/16 too). //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/WinFunctions.h"
#include "dxc/dxcapi.h"

#include <algorithm>
#include <memory>

#ifdef _WIN32
#include <intsafe.h>
#endif

#define CP_UTF16 1200

struct HeapMalloc : public IMalloc {
public:
  ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
  ULONG STDMETHODCALLTYPE Release() override { return 1; }
  STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
    return DoBasicQueryInterface<IMalloc>(this, iid, ppvObject);
  }
  virtual void *STDMETHODCALLTYPE Alloc (
    /* [annotation][in] */
    _In_  SIZE_T cb) override {
    return HeapAlloc(GetProcessHeap(), 0, cb);
  }

  virtual void *STDMETHODCALLTYPE Realloc (
    /* [annotation][in] */
    _In_opt_  void *pv,
    /* [annotation][in] */
    _In_  SIZE_T cb) override
  {
    return HeapReAlloc(GetProcessHeap(), 0, pv, cb);
  }

  virtual void STDMETHODCALLTYPE Free (
    /* [annotation][in] */
    _In_opt_  void *pv) override
  {
    HeapFree(GetProcessHeap(), 0, pv);
  }


  virtual SIZE_T STDMETHODCALLTYPE GetSize(
    /* [annotation][in] */
    _In_opt_ _Post_writable_byte_size_(return)  void *pv)
  {
    return HeapSize(GetProcessHeap(), 0, pv);
  }

  virtual int STDMETHODCALLTYPE DidAlloc(
    /* [annotation][in] */
    _In_opt_  void *pv)
  {
    return -1; // don't know
  }


  virtual void STDMETHODCALLTYPE HeapMinimize(void) {}
};

static HeapMalloc g_HeapMalloc;

namespace hlsl {

IMalloc *GetGlobalHeapMalloc() throw() {
  return &g_HeapMalloc;
}

_Use_decl_annotations_
void ReadBinaryFile(IMalloc *pMalloc, LPCWSTR pFileName, void **ppData,
                    DWORD *pDataSize) {
  HANDLE hFile = CreateFileW(pFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    IFT(HRESULT_FROM_WIN32(GetLastError()));
  }

  CHandle h(hFile);

  LARGE_INTEGER FileSize;
  if (!GetFileSizeEx(hFile, &FileSize)) {
    IFT(HRESULT_FROM_WIN32(GetLastError()));
  }
  if (FileSize.u.HighPart != 0) {
    throw(hlsl::Exception(DXC_E_INPUT_FILE_TOO_LARGE, "input file is too large"));
  }

  char *pData = (char *)pMalloc->Alloc(FileSize.u.LowPart);
  if (!pData) {
    throw std::bad_alloc();
  }

  DWORD BytesRead;
  if (!ReadFile(hFile, pData, FileSize.u.LowPart, &BytesRead, nullptr)) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    pMalloc->Free(pData);
    throw ::hlsl::Exception(hr);
  }
  DXASSERT(FileSize.u.LowPart == BytesRead, "ReadFile operation failed");

  *ppData = pData;
  *pDataSize = FileSize.u.LowPart;

}

_Use_decl_annotations_
void ReadBinaryFile(LPCWSTR pFileName, void **ppData, DWORD *pDataSize) {
  return ReadBinaryFile(GetGlobalHeapMalloc(), pFileName, ppData, pDataSize);
}

_Use_decl_annotations_
void WriteBinaryFile(LPCWSTR pFileName, const void *pData, DWORD DataSize) {
  HANDLE hFile = CreateFileW(pFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if(hFile == INVALID_HANDLE_VALUE) {
    IFT(HRESULT_FROM_WIN32(GetLastError()));
  }
  CHandle h(hFile);

  DWORD BytesWritten;
  if(!WriteFile(hFile, pData, DataSize, &BytesWritten, nullptr)) {
    IFT(HRESULT_FROM_WIN32(GetLastError()));
  }
  DXASSERT(DataSize == BytesWritten, "WriteFile operation failed");
}

_Use_decl_annotations_
UINT32 DxcCodePageFromBytes(const char *bytes, size_t byteLen) throw() {
  UINT32 codePage;
  if (byteLen >= 4) {
    // Now try to use the BOM to check for Unicode encodings
    char bom[4] = { bytes[0], bytes[1], bytes[2], bytes[3] };

    if (strncmp(bom, "\xef\xbb\xbf", 3) == 0) {
      codePage = CP_UTF8;
    }
    else if (strncmp(bom, "\xff\xfe\x00\x00", 4) == 0) {
      codePage = 12000; //UTF-32 LE
    }
    else if (strncmp(bom, "\x00\x00\xfe\xff", 4) == 0) {
      codePage = 12001; //UTF-32 BE
    }
    else if (strncmp(bom, "\xff\xfe", 2) == 0) {
      codePage = 1200; //UTF-16 LE
    }
    else if (strncmp(bom, "\xfe\xff", 2) == 0) {
      codePage = 1201; //UTF-16 BE
    }
    else {
      codePage = CP_ACP;
    }
  }
  else {
    codePage = CP_ACP;
  }
  return codePage;
}

#define IsSizeWcharAligned(size) (((size) & (sizeof(wchar_t) - 1)) == 0)

template<typename _char>
bool IsUtfBufferNullTerminated(LPCVOID pBuffer, SIZE_T size) {
  return (size >= sizeof(_char) && (size & (sizeof(_char) - 1)) == 0 &&
    reinterpret_cast<const _char*>(pBuffer)[(size / sizeof(_char)) - 1] == 0);
}
static bool IsBufferNullTerminated(LPCVOID pBuffer, SIZE_T size, UINT32 codePage) {
  switch (codePage) {
  case DXC_CP_UTF8: return IsUtfBufferNullTerminated<char>(pBuffer, size);
  case DXC_CP_UTF16: return IsUtfBufferNullTerminated<wchar_t>(pBuffer, size);
  default: return false;
  }
}
template<typename _char>
bool IsUtfBufferEmptyString(LPCVOID pBuffer, SIZE_T size) {
  return (size == 0 || (size == sizeof(_char) &&
    reinterpret_cast<const _char*>(pBuffer)[0] == 0));
}
static bool IsBufferEmptyString(LPCVOID pBuffer, SIZE_T size, UINT32 codePage) {
  switch (codePage) {
  case DXC_CP_UTF8: return IsUtfBufferEmptyString<char>(pBuffer, size);
  case DXC_CP_UTF16: return IsUtfBufferEmptyString<wchar_t>(pBuffer, size);
  default: return IsUtfBufferEmptyString<char>(pBuffer, size);
  }
}

class DxcBlobNoEncoding_Impl : public IDxcBlobEncoding {
public:
  typedef IDxcBlobEncoding Base;
  static const UINT32 CodePage = CP_ACP;
};

class DxcBlobUtf16_Impl : public IDxcBlobUtf16 {
public:
  static const UINT32 CodePage = CP_UTF16;
  typedef IDxcBlobUtf16 Base;
  virtual LPCWSTR STDMETHODCALLTYPE GetStringPointer(void) override {
    if (GetBufferSize() < sizeof(wchar_t)) {
      return L""; // Special case for empty string blob
    }
    DXASSERT(IsSizeWcharAligned(GetBufferSize()),
             "otherwise, buffer size is not even multiple of wchar_t");
    DXASSERT(*(const wchar_t*)
             ((const BYTE*)GetBufferPointer() + GetBufferSize() - sizeof(wchar_t))
               == L'\0',
             "otherwise buffer is not null terminated.");
    return (LPCWSTR)GetBufferPointer();
  }
  virtual SIZE_T STDMETHODCALLTYPE GetStringLength(void) override {
    SIZE_T bufSize = GetBufferSize();
    return bufSize ? (bufSize / sizeof(wchar_t)) - 1 : 0;
  }
};

class DxcBlobUtf8_Impl : public IDxcBlobUtf8 {
public:
  static const UINT32 CodePage = CP_UTF8;
  typedef IDxcBlobUtf8 Base;
  virtual LPCSTR STDMETHODCALLTYPE GetStringPointer(void) override {
    if (GetBufferSize() < sizeof(char)) {
      return ""; // Special case for empty string blob
    }
    DXASSERT(*((const char*)GetBufferPointer() + GetBufferSize() - 1) == '\0',
             "otherwise buffer is not null terminated.");
    return (LPCSTR)GetBufferPointer();
  }
  virtual SIZE_T STDMETHODCALLTYPE GetStringLength(void) override {
    SIZE_T bufSize = GetBufferSize();
    return bufSize ? (bufSize / sizeof(char)) - 1 : 0;
  }
};

template <typename _T>
class InternalDxcBlobEncoding_Impl : public _T {
private:
  DXC_MICROCOM_TM_REF_FIELDS() // an underlying m_pMalloc that owns this
  LPCVOID m_Buffer = nullptr;
  IUnknown* m_Owner = nullptr; // IMalloc when MallocFree is true, owning the buffer
  SIZE_T m_BufferSize;
  unsigned m_EncodingKnown : 1;
  unsigned m_MallocFree : 1;
  UINT32 m_CodePage;
public:
  DXC_MICROCOM_ADDREF_IMPL(m_dwRef)
  ULONG STDMETHODCALLTYPE Release() override {
    // Because blobs are also used by tests and utilities, we avoid using TLS.
    ULONG result = (ULONG)--m_dwRef;
    if (result == 0) {
      CComPtr<IMalloc> pTmp(m_pMalloc);
      this->~InternalDxcBlobEncoding_Impl();
      pTmp->Free(this);
    }
    return result;
  }
  DXC_MICROCOM_TM_CTOR(InternalDxcBlobEncoding_Impl)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcBlob, IDxcBlobEncoding, typename _T::Base>(this, iid, ppvObject);
  }

  ~InternalDxcBlobEncoding_Impl() {
    if (m_MallocFree) {
      ((IMalloc *)m_Owner)->Free(const_cast<void *>(m_Buffer));
    }
    if (m_Owner != nullptr) {
      m_Owner->Release();
    }
  }

  static HRESULT
  CreateFromBlob(_In_ IDxcBlob *pBlob, _In_ IMalloc *pMalloc, bool encodingKnown, UINT32 codePage,
                 _COM_Outptr_ InternalDxcBlobEncoding_Impl **pEncoding) {
    *pEncoding = InternalDxcBlobEncoding_Impl::Alloc(pMalloc);
    if (*pEncoding == nullptr) {
      return E_OUTOFMEMORY;
    }
    DXASSERT(_T::CodePage == CP_ACP || (encodingKnown && _T::CodePage == codePage), "encoding must match type");
    pBlob->AddRef();
    (*pEncoding)->m_Owner = pBlob;
    (*pEncoding)->m_Buffer = pBlob->GetBufferPointer();
    (*pEncoding)->m_BufferSize = pBlob->GetBufferSize();
    (*pEncoding)->m_EncodingKnown = encodingKnown;
    (*pEncoding)->m_MallocFree = 0;
    (*pEncoding)->m_CodePage = codePage;
    (*pEncoding)->AddRef();
    return S_OK;
  }

  static HRESULT
  CreateFromMalloc(LPCVOID buffer, IMalloc *pIMalloc, SIZE_T bufferSize, bool encodingKnown,
    UINT32 codePage, _COM_Outptr_ InternalDxcBlobEncoding_Impl **pEncoding) {
    *pEncoding = InternalDxcBlobEncoding_Impl::Alloc(pIMalloc);
    if (*pEncoding == nullptr) {
      *pEncoding = nullptr;
      return E_OUTOFMEMORY;
    }
    DXASSERT(_T::CodePage == CP_ACP || (encodingKnown && _T::CodePage == codePage), "encoding must match type");
    DXASSERT(buffer || bufferSize == 0, "otherwise, nullptr with non-zero size provided");
    pIMalloc->AddRef();
    (*pEncoding)->m_Owner = pIMalloc;
    (*pEncoding)->m_Buffer = buffer;
    (*pEncoding)->m_BufferSize = bufferSize;
    (*pEncoding)->m_EncodingKnown = encodingKnown;
    (*pEncoding)->m_MallocFree = buffer != nullptr;
    (*pEncoding)->m_CodePage = codePage;
    (*pEncoding)->AddRef();
    return S_OK;
  }

  void AdjustPtrAndSize(unsigned offset, unsigned size) {
    DXASSERT(offset < m_BufferSize, "else caller will overflow");
    DXASSERT(offset + size <= m_BufferSize, "else caller will overflow");
    m_Buffer = (const uint8_t*)m_Buffer + offset;
    m_BufferSize = size;
  }

  virtual LPVOID STDMETHODCALLTYPE GetBufferPointer(void) override {
    return const_cast<LPVOID>(m_Buffer);
  }
  virtual SIZE_T STDMETHODCALLTYPE GetBufferSize(void) override {
    return m_BufferSize;
  }
  virtual HRESULT STDMETHODCALLTYPE GetEncoding(_Out_ BOOL *pKnown, _Out_ UINT32 *pCodePage) override {
    *pKnown = m_EncodingKnown ? TRUE : FALSE;
    *pCodePage = m_CodePage;
    return S_OK;
  }

  // Relatively dangerous API. This means the buffer should be pinned for as
  // long as this object is alive.
  void ClearFreeFlag() { m_MallocFree = 0; }
};

typedef InternalDxcBlobEncoding_Impl<DxcBlobNoEncoding_Impl> InternalDxcBlobEncoding;
typedef InternalDxcBlobEncoding_Impl<DxcBlobUtf16_Impl> InternalDxcBlobUtf16;
typedef InternalDxcBlobEncoding_Impl<DxcBlobUtf8_Impl> InternalDxcBlobUtf8;

static HRESULT CodePageBufferToUtf16(UINT32 codePage, LPCVOID bufferPointer,
                                     SIZE_T bufferSize,
                                     CDxcMallocHeapPtr<WCHAR> &utf16NewCopy,
                                     _Out_ UINT32 *pConvertedCharCount) {
  *pConvertedCharCount = 0;

  // If the buffer is empty, don't dereference bufferPointer at all.
  // Keep the null terminator post-condition.
  if (IsBufferEmptyString(bufferPointer, bufferSize, codePage)) {
    if (!utf16NewCopy.Allocate(1))
      return E_OUTOFMEMORY;
    utf16NewCopy.m_pData[0] = L'\0';
    DXASSERT(*pConvertedCharCount == 0, "else didn't init properly");
    return S_OK;
  }

  // Calculate the length of the buffer in wchar_t elements.
  int numToConvertUTF16 =
      MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, (LPCSTR)bufferPointer,
                          bufferSize, nullptr, 0);
  if (numToConvertUTF16 == 0)
    return HRESULT_FROM_WIN32(GetLastError());

  // Add an extra character in case we need it for null-termination
  unsigned buffSizeUTF16;
  IFR(Int32ToUInt32(numToConvertUTF16, &buffSizeUTF16));
  IFR(UInt32Add(buffSizeUTF16, 1, &buffSizeUTF16));
  IFR(UInt32Mult(buffSizeUTF16, sizeof(WCHAR), &buffSizeUTF16));
  utf16NewCopy.AllocateBytes(buffSizeUTF16);
  IFROOM(utf16NewCopy.m_pData);

  int numActuallyConvertedUTF16 =
      MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, (LPCSTR)bufferPointer,
                          bufferSize, utf16NewCopy, buffSizeUTF16);

  if (numActuallyConvertedUTF16 == 0)
    return HRESULT_FROM_WIN32(GetLastError());
  if (numActuallyConvertedUTF16 < 0)
    return E_OUTOFMEMORY;

  // If all we have is null terminator, return with zero count.
  if (utf16NewCopy.m_pData[0] == L'\0') {
    DXASSERT(*pConvertedCharCount == 0, "else didn't init properly");
    return S_OK;
  }

  if ((UINT32)numActuallyConvertedUTF16 < (buffSizeUTF16 / sizeof(wchar_t)) &&
      utf16NewCopy.m_pData[numActuallyConvertedUTF16 - 1] != L'\0') {
    utf16NewCopy.m_pData[numActuallyConvertedUTF16++] = L'\0';
  }
  *pConvertedCharCount = (UINT32)numActuallyConvertedUTF16;

  return S_OK;
}

static HRESULT CodePageBufferToUtf8(UINT32 codePage, LPCVOID bufferPointer,
                                    SIZE_T bufferSize,
                                    IMalloc *pMalloc,
                                    CDxcMallocHeapPtr<char> &utf8NewCopy,
                                    _Out_ UINT32 *pConvertedCharCount) {
  *pConvertedCharCount = 0;

  CDxcMallocHeapPtr<WCHAR> utf16NewCopy(pMalloc);
  UINT32 utf16CharCount = 0;
  const WCHAR *utf16Chars = nullptr;
  if (codePage == CP_UTF16) {
    DXASSERT(IsSizeWcharAligned(bufferSize), "otherwise, odd buffer size with UTF-16");
    utf16Chars = (const WCHAR*)bufferPointer;
    utf16CharCount = bufferSize / sizeof(wchar_t);
  } else if (bufferSize) {
    IFR(CodePageBufferToUtf16(codePage, bufferPointer, bufferSize,
                              utf16NewCopy, &utf16CharCount));
    utf16Chars = utf16NewCopy.m_pData;
  }

  // If the buffer is empty, don't dereference bufferPointer at all.
  // Keep the null terminator post-condition.
  if (IsUtfBufferEmptyString<wchar_t>(utf16Chars, utf16CharCount)) {
    if (!utf8NewCopy.Allocate(1))
      return E_OUTOFMEMORY;
    DXASSERT(*pConvertedCharCount == 0, "else didn't init properly");
    utf8NewCopy.m_pData[0] = '\0';
    return S_OK;
  }

  int numToConvertUtf8 =
    WideCharToMultiByte(CP_UTF8, 0, utf16Chars, utf16CharCount,
                        NULL, 0, NULL, NULL);
  if (numToConvertUtf8 == 0)
    return HRESULT_FROM_WIN32(GetLastError());

  UINT32 buffSizeUtf8;
  IFR(Int32ToUInt32(numToConvertUtf8, &buffSizeUtf8));
  if (!IsBufferNullTerminated(utf16Chars, utf16CharCount * sizeof(wchar_t), CP_UTF16)) {
    // If original size doesn't include null-terminator,
    // we have to add one to the converted buffer.
    IFR(UInt32Add(buffSizeUtf8, 1, &buffSizeUtf8));
  }
  utf8NewCopy.AllocateBytes(buffSizeUtf8);
  IFROOM(utf8NewCopy.m_pData);

  int numActuallyConvertedUtf8 =
    WideCharToMultiByte(CP_UTF8, 0, utf16Chars, utf16CharCount,
                        utf8NewCopy, buffSizeUtf8, NULL, NULL);
  if (numActuallyConvertedUtf8 == 0)
    return HRESULT_FROM_WIN32(GetLastError());
  if (numActuallyConvertedUtf8 < 0)
    return E_OUTOFMEMORY;

  if ((UINT32)numActuallyConvertedUtf8 < buffSizeUtf8 &&
      utf8NewCopy.m_pData[numActuallyConvertedUtf8 - 1] != '\0') {
    utf8NewCopy.m_pData[numActuallyConvertedUtf8++] = '\0';
  }
  *pConvertedCharCount = (UINT32)numActuallyConvertedUtf8;

  return S_OK;
}

static bool TryCreateEmptyBlobUtf(
    UINT32 codePage, IMalloc *pMalloc, IDxcBlobEncoding **ppBlobEncoding) {
  if (codePage == CP_UTF8) {
    InternalDxcBlobUtf8 *internalUtf8;
    IFR(InternalDxcBlobUtf8::CreateFromMalloc(
          nullptr, pMalloc, 0, true, codePage, &internalUtf8));
    *ppBlobEncoding = internalUtf8;
    return true;
  } else if (codePage == CP_UTF16) {
    InternalDxcBlobUtf16 *internalUtf16;
    IFR(InternalDxcBlobUtf16::CreateFromMalloc(
          nullptr, pMalloc, 0, true, codePage, &internalUtf16));
    *ppBlobEncoding = internalUtf16;
    return true;
  }
  return false;
}

static bool TryCreateBlobUtfFromBlob(
    IDxcBlob *pFromBlob, UINT32 codePage, IMalloc *pMalloc,
    IDxcBlobEncoding **ppBlobEncoding) {
  // Try to create a IDxcBlobUtf8 or IDxcBlobUtf16
  if (IsBlobNullOrEmpty(pFromBlob)) {
    return TryCreateEmptyBlobUtf(codePage, pMalloc, ppBlobEncoding);
  } else if (IsBufferNullTerminated(pFromBlob->GetBufferPointer(),
                              pFromBlob->GetBufferSize(), codePage)) {
    if (codePage == CP_UTF8) {
      InternalDxcBlobUtf8 *internalUtf8;
      IFR(InternalDxcBlobUtf8::CreateFromBlob(
            pFromBlob, pMalloc, true, codePage, &internalUtf8));
      *ppBlobEncoding = internalUtf8;
      return true;
    } else if (codePage == CP_UTF16) {
      InternalDxcBlobUtf16 *internalUtf16;
      IFR(InternalDxcBlobUtf16::CreateFromBlob(
            pFromBlob, pMalloc, true, codePage, &internalUtf16));
      *ppBlobEncoding = internalUtf16;
      return true;
    }
  }
  return false;
}


HRESULT DxcCreateBlob(
    LPCVOID pPtr, SIZE_T size, bool bPinned, bool bCopy,
    bool encodingKnown, UINT32 codePage,
    IMalloc *pMalloc, IDxcBlobEncoding **ppBlobEncoding) throw() {

  IFRBOOL(!(bPinned && bCopy), E_INVALIDARG);
  IFRBOOL(ppBlobEncoding, E_INVALIDARG);
  *ppBlobEncoding = nullptr;

  bool bNullTerminated = encodingKnown ? IsBufferNullTerminated(pPtr, size, codePage) : false;

  if (!pMalloc)
    pMalloc = DxcGetThreadMallocNoRef();

  // Handle empty blob
  if (!pPtr || !size) {
    if (encodingKnown && TryCreateEmptyBlobUtf(codePage, pMalloc, ppBlobEncoding))
      return S_OK;
    InternalDxcBlobEncoding *pInternalEncoding;
    IFR(InternalDxcBlobEncoding::CreateFromMalloc(nullptr, pMalloc, 0, encodingKnown, codePage, &pInternalEncoding));
    *ppBlobEncoding = pInternalEncoding;
  }

  if (bPinned) {
    if (encodingKnown) {
      if (bNullTerminated) {
        if (codePage == CP_UTF8) {
          InternalDxcBlobUtf8 *internalUtf8;
          IFR(InternalDxcBlobUtf8::CreateFromMalloc(
              pPtr, pMalloc, size, true, codePage, &internalUtf8));
          *ppBlobEncoding = internalUtf8;
          internalUtf8->ClearFreeFlag();
          return S_OK;
        } else if (codePage == CP_UTF16) {
          InternalDxcBlobUtf16 *internalUtf16;
          IFR(InternalDxcBlobUtf16::CreateFromMalloc(
              pPtr, pMalloc, size, true, codePage, &internalUtf16));
          *ppBlobEncoding = internalUtf16;
          internalUtf16->ClearFreeFlag();
          return S_OK;
        }
      }
    }
    InternalDxcBlobEncoding *internalEncoding;
    IFR(InternalDxcBlobEncoding::CreateFromMalloc(
        pPtr, pMalloc, size, encodingKnown, codePage, &internalEncoding));
    *ppBlobEncoding = internalEncoding;
    internalEncoding->ClearFreeFlag();
    return S_OK;
  }

  void *pData = const_cast<void*>(pPtr);
  SIZE_T newSize = size;

  CDxcMallocHeapPtr<char> heapCopy(pMalloc);
  if (bCopy) {
    if (encodingKnown) {
      if (!bNullTerminated) {
        if (codePage == CP_UTF8) {
          newSize += sizeof(char);
          bNullTerminated = true;
        } else if (codePage == CP_UTF16) {
          newSize += sizeof(wchar_t);
          bNullTerminated = true;
        }
      }
    }
    heapCopy.AllocateBytes(newSize);
    pData = heapCopy.m_pData;
    if (pData == nullptr)
      return E_OUTOFMEMORY;
    if (pPtr)
      memcpy(pData, pPtr, size);
    else
      memset(pData, 0, size);
  }

  if (bNullTerminated && codePage == CP_UTF8) {
    if (bCopy && newSize > size)
      ((char*)pData)[newSize - 1] = 0;
    InternalDxcBlobUtf8 *internalUtf8;
    IFR(InternalDxcBlobUtf8::CreateFromMalloc(
        pData, pMalloc, newSize, true, codePage, &internalUtf8));
    *ppBlobEncoding = internalUtf8;
  } else if (bNullTerminated && codePage == CP_UTF16) {
    if (bCopy && newSize > size)
      ((wchar_t*)pData)[(newSize / sizeof(wchar_t)) - 1] = 0;
    InternalDxcBlobUtf16 *internalUtf16;
    IFR(InternalDxcBlobUtf16::CreateFromMalloc(
        pData, pMalloc, newSize, true, codePage, &internalUtf16));
    *ppBlobEncoding = internalUtf16;
  } else {
    InternalDxcBlobEncoding *internalEncoding;
    IFR(InternalDxcBlobEncoding::CreateFromMalloc(
        pData, pMalloc, newSize, encodingKnown, codePage, &internalEncoding));
    *ppBlobEncoding = internalEncoding;
  }
  if (bCopy)
    heapCopy.Detach();
  return S_OK;
}

HRESULT DxcCreateBlobEncodingFromBlob(
    IDxcBlob *pFromBlob, UINT32 offset, UINT32 length,
    bool encodingKnown, UINT32 codePage,
    IMalloc *pMalloc, IDxcBlobEncoding **ppBlobEncoding) throw() {

  IFRBOOL(pFromBlob, E_POINTER);
  IFRBOOL(ppBlobEncoding, E_POINTER);
  *ppBlobEncoding = nullptr;

  if (!pMalloc)
    pMalloc = DxcGetThreadMallocNoRef();

  InternalDxcBlobEncoding *internalEncoding;
  if (offset || length) {
    UINT32 end;
    IFR(UInt32Add(offset, length, &end));
    SIZE_T blobSize = pFromBlob->GetBufferSize();
    if (end > blobSize)
      return E_INVALIDARG;
    IFR(InternalDxcBlobEncoding::CreateFromBlob(
      pFromBlob, pMalloc, encodingKnown, codePage, &internalEncoding));
    internalEncoding->AdjustPtrAndSize(offset, length);
    *ppBlobEncoding = internalEncoding;
    return S_OK;
  }

  if (!encodingKnown || codePage == CP_UTF8) {
    IDxcBlobUtf8 *pBlobUtf8;
    if(SUCCEEDED(pFromBlob->QueryInterface(&pBlobUtf8))) {
      *ppBlobEncoding = pBlobUtf8;
      return S_OK;
    }
  }
  if (!encodingKnown || codePage == CP_UTF16) {
    IDxcBlobUtf16 *pBlobUtf16;
    if (SUCCEEDED(pFromBlob->QueryInterface(&pBlobUtf16))) {
      *ppBlobEncoding = pBlobUtf16;
      return S_OK;
    }
  }
  CComPtr<IDxcBlobEncoding> pBlobEncoding;
  if (SUCCEEDED(pFromBlob->QueryInterface(&pBlobEncoding))) {
    BOOL thisEncodingKnown;
    UINT32 thisEncoding;
    IFR(pBlobEncoding->GetEncoding(&thisEncodingKnown, &thisEncoding));
    bool encodingMatches = thisEncodingKnown && encodingKnown &&
                           codePage == thisEncoding;
    if (!encodingKnown && thisEncodingKnown) {
      codePage = thisEncoding;
      encodingKnown = thisEncodingKnown;
      encodingMatches = true;
    }
    if (encodingMatches) {
      if (!TryCreateBlobUtfFromBlob(pFromBlob, codePage, pMalloc, ppBlobEncoding)) {
        *ppBlobEncoding = pBlobEncoding.Detach();
      }
      return S_OK;
    }
    if (encodingKnown) {
      IFR(InternalDxcBlobEncoding::CreateFromBlob(
          pFromBlob, pMalloc, true, codePage, &internalEncoding));
      *ppBlobEncoding = internalEncoding;
      return S_OK;
    }
    DXASSERT(!encodingKnown && !thisEncodingKnown, "otherwise, missing case");
    *ppBlobEncoding = pBlobEncoding.Detach();
    return S_OK;
  }

  if (encodingKnown && TryCreateBlobUtfFromBlob(pFromBlob, codePage, pMalloc, ppBlobEncoding)) {
    return S_OK;
  }

  IFR(InternalDxcBlobEncoding::CreateFromBlob(
    pFromBlob, pMalloc, encodingKnown, codePage, &internalEncoding));
  *ppBlobEncoding = internalEncoding;
  return S_OK;
}


_Use_decl_annotations_
HRESULT DxcCreateBlobFromBlob(
    IDxcBlob *pBlob, UINT32 offset, UINT32 length, IDxcBlob **ppResult) throw() {
  IFRBOOL(ppResult, E_POINTER);
  *ppResult = nullptr;
  IDxcBlobEncoding *pResult;
  IFR(DxcCreateBlobEncodingFromBlob(pBlob, offset, length, false, 0, DxcGetThreadMallocNoRef(), &pResult));
  *ppResult = pResult;
  return S_OK;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobOnMalloc(LPCVOID pData, IMalloc *pIMalloc, UINT32 size, IDxcBlob **ppResult) throw() {
  IFRBOOL(ppResult, E_POINTER);
  *ppResult = nullptr;
  IDxcBlobEncoding *pResult;
  IFR(DxcCreateBlob(pData, size, false, false, false, 0, pIMalloc, &pResult));
  *ppResult = pResult;
  return S_OK;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobOnHeapCopy(_In_bytecount_(size) LPCVOID pData, UINT32 size,
                        _COM_Outptr_ IDxcBlob **ppResult) throw() {
  IFRBOOL(ppResult, E_POINTER);
  *ppResult = nullptr;
  IDxcBlobEncoding *pResult;
  IFR(DxcCreateBlob(pData, size, false, true, false, 0, DxcGetThreadMallocNoRef(), &pResult));
  *ppResult = pResult;
  return S_OK;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobFromFile(IMalloc *pMalloc, LPCWSTR pFileName, UINT32 *pCodePage,
                      IDxcBlobEncoding **ppBlobEncoding) throw() {
  if (pFileName == nullptr || ppBlobEncoding == nullptr) {
    return E_POINTER;
  }

  LPVOID pData;
  DWORD dataSize;
  *ppBlobEncoding = nullptr;
  try {
    ReadBinaryFile(pMalloc, pFileName, &pData, &dataSize);
  }
  CATCH_CPP_RETURN_HRESULT();

  bool known = (pCodePage != nullptr);
  UINT32 codePage = (pCodePage != nullptr) ? *pCodePage : 0;

  HRESULT hr = DxcCreateBlob(pData, dataSize, false, false, known, codePage, pMalloc, ppBlobEncoding);
  if (FAILED(hr))
    pMalloc->Free(pData);
  return hr;
}

_Use_decl_annotations_
HRESULT DxcCreateBlobFromFile(LPCWSTR pFileName, UINT32 *pCodePage,
                              IDxcBlobEncoding **ppBlobEncoding) throw() {
  return DxcCreateBlobFromFile(DxcGetThreadMallocNoRef(), pFileName, pCodePage, ppBlobEncoding);
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingSet(IMalloc *pMalloc, IDxcBlob *pBlob, UINT32 codePage,
                             IDxcBlobEncoding **ppBlobEncoding) throw() {
  return DxcCreateBlobEncodingFromBlob(pBlob, 0, 0, true, codePage, pMalloc, ppBlobEncoding);
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingSet(IDxcBlob *pBlob, UINT32 codePage,
                             IDxcBlobEncoding **ppBlobEncoding) throw() {
  return DxcCreateBlobEncodingFromBlob(pBlob, 0, 0, true, codePage, nullptr, ppBlobEncoding);
}

_Use_decl_annotations_
HRESULT DxcCreateBlobWithEncodingFromPinned(LPCVOID pText, UINT32 size,
                                            UINT32 codePage,
                                            IDxcBlobEncoding **pBlobEncoding) throw() {
  return DxcCreateBlob(pText, size, true, false, true, codePage, nullptr, pBlobEncoding);
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingFromStream(IStream *pStream, bool newInstanceAlways,
                                    UINT32 codePage,
                                    IDxcBlobEncoding **ppBlobEncoding) throw() {
  IFRBOOL(ppBlobEncoding, E_POINTER);
  *ppBlobEncoding = nullptr;
  if (pStream == nullptr) {
    return S_OK;
  }

  // Try to reuse the existing stream.
  if (!newInstanceAlways) {
    CComPtr<IDxcBlobEncoding> blobEncoding;
    if (SUCCEEDED(pStream->QueryInterface(&blobEncoding))) {
      *ppBlobEncoding = blobEncoding.Detach();
      return S_OK;
    }
  }

  // Layer over the blob if possible.
  CComPtr<IDxcBlob> blob;
  if (SUCCEEDED(pStream->QueryInterface(&blob))) {
    return DxcCreateBlobWithEncodingSet(blob, codePage, ppBlobEncoding);
  }

  // Create a copy of contents, last resort.
  // TODO: implement when we find this codepath internally
  return E_NOTIMPL;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingOnHeapCopy(LPCVOID pText, UINT32 size, UINT32 codePage,
  IDxcBlobEncoding **pBlobEncoding) throw() {
  return DxcCreateBlob(pText, size, false, true, true, codePage, nullptr, pBlobEncoding);
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingOnMalloc(LPCVOID pText, IMalloc *pIMalloc, UINT32 size, UINT32 codePage,
  IDxcBlobEncoding **pBlobEncoding) throw() {
  return DxcCreateBlob(pText, size, false, false, true, codePage, pIMalloc, pBlobEncoding);
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingOnMallocCopy(IMalloc *pIMalloc, LPCVOID pText, UINT32 size, UINT32 codePage,
  IDxcBlobEncoding **ppBlobEncoding) throw() {
  return DxcCreateBlob(pText, size, false, true, true, codePage, pIMalloc, ppBlobEncoding);
}

_Use_decl_annotations_
HRESULT DxcGetBlobAsUtf8(IDxcBlob *pBlob, IMalloc *pMalloc, IDxcBlobUtf8 **pBlobEncoding) throw() {
  IFRBOOL(pBlob, E_POINTER);
  IFRBOOL(pBlobEncoding, E_POINTER);
  *pBlobEncoding = nullptr;

  if (SUCCEEDED(pBlob->QueryInterface(pBlobEncoding)))
    return S_OK;

  HRESULT hr;
  CComPtr<IDxcBlobEncoding> pSourceBlob;
  UINT32 codePage = CP_ACP;
  BOOL known = FALSE;
  if (SUCCEEDED(pBlob->QueryInterface(&pSourceBlob))) {
    if (FAILED(hr = pSourceBlob->GetEncoding(&known, &codePage)))
      return hr;
  }

  SIZE_T blobLen = pBlob->GetBufferSize();
  if (!known) {
    codePage = DxcCodePageFromBytes((char *)pBlob->GetBufferPointer(), blobLen);
  }

  if (!pMalloc)
    pMalloc = DxcGetThreadMallocNoRef();

  CDxcMallocHeapPtr<char> utf8NewCopy(pMalloc);
  UINT32 utf8CharCount = 0;

  // Reuse or copy the underlying blob depending on null-termination
  if (codePage == CP_UTF8) {
    utf8CharCount = blobLen;
    if (IsBufferNullTerminated(pBlob->GetBufferPointer(), blobLen, CP_UTF8)) {
      // Already null-terminated, reference other blob's memory
      InternalDxcBlobUtf8* internalEncoding;
      hr = InternalDxcBlobUtf8::CreateFromBlob(pBlob, pMalloc, true, CP_UTF8, &internalEncoding);
      if (SUCCEEDED(hr)) {
        *pBlobEncoding = internalEncoding;
      }
      return hr;
    } else {
      // Copy to new buffer and null-terminate
      if(!utf8NewCopy.Allocate(utf8CharCount + 1))
        return E_OUTOFMEMORY;
      memcpy(utf8NewCopy.m_pData, pBlob->GetBufferPointer(), blobLen);
      utf8NewCopy.m_pData[utf8CharCount++] = 0;
    }
  } else {
    // Convert and create a blob that owns the encoding.
    if (FAILED(
      hr = CodePageBufferToUtf8(codePage, pBlob->GetBufferPointer(), blobLen,
                                 pMalloc, utf8NewCopy, &utf8CharCount))) {
      return hr;
    }
    DXASSERT(!utf8CharCount ||
             IsBufferNullTerminated(utf8NewCopy.m_pData, utf8CharCount, CP_UTF8),
             "otherwise, CodePageBufferToUtf8 failed to null-terminate buffer.");
  }

  // At this point, we have new utf8NewCopy to wrap in a blob
  InternalDxcBlobUtf8* internalEncoding;
  hr = InternalDxcBlobUtf8::CreateFromMalloc(
      utf8NewCopy.m_pData, pMalloc,
      utf8CharCount, true, CP_UTF8, &internalEncoding);
  if (SUCCEEDED(hr)) {
    *pBlobEncoding = internalEncoding;
    utf8NewCopy.Detach();
  }
  return hr;
}

// This is kept for compatibility.
HRESULT
DxcGetBlobAsUtf8NullTerm(_In_ IDxcBlob *pBlob,
                         _COM_Outptr_ IDxcBlobEncoding **ppBlobEncoding) throw() {
  IFRBOOL(pBlob, E_POINTER);
  IFRBOOL(ppBlobEncoding, E_POINTER);
  *ppBlobEncoding = nullptr;

  CComPtr<IDxcBlobUtf8> pConverted;
  IFR(DxcGetBlobAsUtf8(pBlob, DxcGetThreadMallocNoRef(), &pConverted));
  pConverted->QueryInterface(ppBlobEncoding);
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcGetBlobAsUtf16(IDxcBlob *pBlob, IMalloc *pMalloc, IDxcBlobUtf16 **pBlobEncoding) throw() {
  IFRBOOL(pBlob, E_POINTER);
  IFRBOOL(pBlobEncoding, E_POINTER);
  *pBlobEncoding = nullptr;

  if (SUCCEEDED(pBlob->QueryInterface(pBlobEncoding)))
    return S_OK;

  HRESULT hr;

  CComPtr<IDxcBlobEncoding> pSourceBlob;
  UINT32 codePage = CP_ACP;
  BOOL known = FALSE;
  if (SUCCEEDED(pBlob->QueryInterface(&pSourceBlob))) {
    if (FAILED(hr = pSourceBlob->GetEncoding(&known, &codePage)))
      return hr;
  }

  SIZE_T blobLen = pBlob->GetBufferSize();
  if (!known) {
    codePage = DxcCodePageFromBytes((char *)pBlob->GetBufferPointer(), blobLen);
  }

  if (!pMalloc)
    pMalloc = DxcGetThreadMallocNoRef();

  CDxcMallocHeapPtr<WCHAR> utf16NewCopy(pMalloc);
  UINT32 utf16CharCount = 0;

  // Reuse or copy the underlying blob depending on null-termination
  if (codePage == CP_UTF16) {
    DXASSERT(IsSizeWcharAligned(blobLen),
             "otherwise, UTF-16 blob size not evenly divisible by 2");
    utf16CharCount = blobLen / sizeof(wchar_t);
    if (IsBufferNullTerminated(pBlob->GetBufferPointer(), blobLen, CP_UTF16)) {
      // Already null-terminated, reference other blob's memory
      InternalDxcBlobUtf16* internalEncoding;
      hr = InternalDxcBlobUtf16::CreateFromBlob(pBlob, pMalloc, true, CP_UTF16, &internalEncoding);
      if (SUCCEEDED(hr)) {
        *pBlobEncoding = internalEncoding;
      }
      return hr;
    } else {
      // Copy to new buffer and null-terminate
      if(!utf16NewCopy.Allocate(utf16CharCount + 1))
        return E_OUTOFMEMORY;
      memcpy(utf16NewCopy.m_pData, pBlob->GetBufferPointer(), blobLen);
      utf16NewCopy.m_pData[utf16CharCount++] = 0;
    }
  } else {
    // Convert and create a blob that owns the encoding.
    if (FAILED(
      hr = CodePageBufferToUtf16(codePage, pBlob->GetBufferPointer(), blobLen,
                                 utf16NewCopy, &utf16CharCount))) {
      return hr;
    }
  }

  // At this point, we have new utf16NewCopy to wrap in a blob
  DXASSERT(!utf16CharCount ||
           IsBufferNullTerminated(utf16NewCopy.m_pData, utf16CharCount * sizeof(wchar_t), CP_UTF16),
           "otherwise, failed to null-terminate buffer.");
  InternalDxcBlobUtf16* internalEncoding;
  hr = InternalDxcBlobUtf16::CreateFromMalloc(
      utf16NewCopy.m_pData, pMalloc,
      utf16CharCount * sizeof(WCHAR), true, CP_UTF16, &internalEncoding);
  if (SUCCEEDED(hr)) {
    *pBlobEncoding = internalEncoding;
    utf16NewCopy.Detach();
  }
  return hr;
}

bool IsBlobNullOrEmpty(_In_opt_ IDxcBlob *pBlob) throw() {
  return pBlob == nullptr || pBlob->GetBufferSize() == 0;
}

///////////////////////////////////////////////////////////////////////////////
// Stream implementations.

class MemoryStream : public AbstractMemoryStream, public IDxcBlob {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  LPBYTE m_pMemory = nullptr;
  ULONG m_offset = 0;
  ULONG m_size = 0;
  ULONG m_allocSize = 0;
public:
  DXC_MICROCOM_ADDREF_IMPL(m_dwRef)
  ULONG STDMETHODCALLTYPE Release() override {
    // Because memory streams are also used by tests and utilities,
    // we avoid using TLS.
    ULONG result = (ULONG)--m_dwRef;
    if (result == 0) {
      CComPtr<IMalloc> pTmp(m_pMalloc);
      this->~MemoryStream();
      pTmp->Free(this);
    }
    return result;
  }

  DXC_MICROCOM_TM_CTOR(MemoryStream)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IStream, ISequentialStream, IDxcBlob>(this, iid, ppvObject);
  }

  ~MemoryStream() {
    Reset();
  }

  HRESULT Grow(ULONG targetSize) {
    if (targetSize < m_allocSize * 2) {
      targetSize = m_allocSize * 2;
    }

    return Reserve(targetSize);
  }

  void Reset() {
    if (m_pMemory != nullptr) {
      m_pMalloc->Free(m_pMemory);
    }
    m_pMemory = nullptr;
    m_offset = 0;
    m_size = 0;
    m_allocSize = 0;
  }

  // AbstractMemoryStream implementation.
  LPBYTE GetPtr() throw() override {
    return m_pMemory;
  }

  ULONG GetPtrSize() throw() override {
    return m_size;
  }

  LPBYTE Detach() throw() override {
    LPBYTE result = m_pMemory;
    m_pMemory = nullptr;
    Reset();
    return result;
  }

  UINT64 GetPosition() throw() override {
    return m_offset;
  }

  HRESULT Reserve(ULONG targetSize) throw() override {
    if (m_pMemory == nullptr) {
      m_pMemory = (LPBYTE)m_pMalloc->Alloc(targetSize);
      if (m_pMemory == nullptr) {
        return E_OUTOFMEMORY;
      }
    }
    else {
      void* newPtr = m_pMalloc->Realloc(m_pMemory, targetSize);
      if (newPtr == nullptr) {
        return E_OUTOFMEMORY;
      }
      m_pMemory = (LPBYTE)newPtr;
    }

    m_allocSize = targetSize;

    return S_OK;
  }

  // IDxcBlob implementation. Requires no further writes.
  LPVOID STDMETHODCALLTYPE GetBufferPointer(void) override {
    return m_pMemory;
  }
  SIZE_T STDMETHODCALLTYPE GetBufferSize(void) override {
    return m_size;
  }

  // ISequentialStream implementation.
  HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) override {
    if (!pv || !pcbRead) return E_POINTER;
    // If we seeked past the end, read nothing.
    if (m_offset > m_size) {
      *pcbRead = 0;
      return S_FALSE;
    }
    ULONG cbLeft = m_size - m_offset;
    *pcbRead = std::min(cb, cbLeft);
    memcpy(pv, m_pMemory + m_offset, *pcbRead);
    m_offset += *pcbRead;
    return (*pcbRead == cb) ? S_OK : S_FALSE;
  }

  HRESULT STDMETHODCALLTYPE Write(void const* pv, ULONG cb, ULONG* pcbWritten) override {
    if (!pv || !pcbWritten) return E_POINTER;
    if (cb + m_offset > m_allocSize) {
      HRESULT hr = Grow(cb + m_offset);
      if (FAILED(hr)) return hr;
      // Implicitly extend as needed with zeroes.
      if (m_offset > m_size) {
        memset(m_pMemory + m_size, 0, m_offset - m_size);
      }
    }
    *pcbWritten = cb;
    memcpy(m_pMemory + m_offset, pv, cb);
    m_offset += cb;
    m_size = std::max(m_size, m_offset);
    return S_OK;
  }

  // IStream implementation.
  HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER val) override {
    if (val.u.HighPart != 0) {
      return E_OUTOFMEMORY;
    }
    if (val.u.LowPart > m_allocSize) {
      return Grow(m_allocSize);
    }
    if (val.u.LowPart < m_size) {
      m_size = val.u.LowPart;
      m_offset = std::min(m_offset, m_size);
    }
    else if (val.u.LowPart > m_size) {
      memset(m_pMemory + m_size, 0, val.u.LowPart - m_size);
      m_size = val.u.LowPart;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE CopyTo(IStream *, ULARGE_INTEGER,
    ULARGE_INTEGER *,
    ULARGE_INTEGER *) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Commit(DWORD) override { return E_NOTIMPL; }

  HRESULT STDMETHODCALLTYPE Revert(void) override { return E_NOTIMPL; }

  HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER,
    ULARGE_INTEGER, DWORD) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER,
    ULARGE_INTEGER, DWORD) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Clone(IStream **) override { return E_NOTIMPL; }

  HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER liDistanceToMove,
    DWORD dwOrigin,
    ULARGE_INTEGER *lpNewFilePointer) override {
    if (lpNewFilePointer != nullptr) {
      lpNewFilePointer->QuadPart = 0;
    }

    if (liDistanceToMove.u.HighPart != 0) {
      return E_FAIL;
    }

    ULONG targetOffset;

    switch (dwOrigin) {
    case STREAM_SEEK_SET:
      targetOffset = liDistanceToMove.u.LowPart;
      break;
    case STREAM_SEEK_CUR:
      targetOffset = liDistanceToMove.u.LowPart + m_offset;
      break;
    case STREAM_SEEK_END:
      targetOffset = liDistanceToMove.u.LowPart + m_size;
      break;
    default:
      return STG_E_INVALIDFUNCTION;
    }

    m_offset = targetOffset;
    if (lpNewFilePointer != nullptr) {
      lpNewFilePointer->u.LowPart = targetOffset;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE Stat(STATSTG *pStatstg,
    DWORD grfStatFlag) override {
    if (pStatstg == nullptr) {
      return E_POINTER;
    }
    ZeroMemory(pStatstg, sizeof(*pStatstg));
    pStatstg->type = STGTY_STREAM;
    pStatstg->cbSize.u.LowPart = m_size;
    return S_OK;
  }
};

class ReadOnlyBlobStream : public IStream {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  CComPtr<IDxcBlob> m_pSource;
  LPBYTE m_pMemory;
  ULONG m_offset;
  ULONG m_size;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(ReadOnlyBlobStream)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IStream, ISequentialStream>(this, iid, ppvObject);
  }

  void Init(IDxcBlob *pSource) {
    m_pSource = pSource;
    m_offset = 0;
    m_size = m_pSource->GetBufferSize();
    m_pMemory = (LPBYTE)m_pSource->GetBufferPointer();
    // If Utf8Blob, exclude terminating null character
    CComPtr<IDxcBlobUtf8> utf8Source;
    if (m_size && SUCCEEDED(pSource->QueryInterface(&utf8Source)))
      m_size = utf8Source->GetStringLength();
  }

  // ISequentialStream implementation.
  HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb,
    ULONG *pcbRead) override {
    if (!pv || !pcbRead)
      return E_POINTER;
    ULONG cbLeft = m_size - m_offset;
    *pcbRead = std::min(cb, cbLeft);
    memcpy(pv, m_pMemory + m_offset, *pcbRead);
    m_offset += *pcbRead;
    return (*pcbRead == cb) ? S_OK : S_FALSE;
  }

  HRESULT STDMETHODCALLTYPE Write(void const *, ULONG, ULONG *) override {
    return STG_E_ACCESSDENIED;
  }

  // IStream implementation.
  HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER val) override {
    return STG_E_ACCESSDENIED;
  }

  HRESULT STDMETHODCALLTYPE CopyTo(IStream *, ULARGE_INTEGER,
    ULARGE_INTEGER *,
    ULARGE_INTEGER *) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Commit(DWORD) override { return E_NOTIMPL; }

  HRESULT STDMETHODCALLTYPE Revert(void) override { return E_NOTIMPL; }

  HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER,
    ULARGE_INTEGER, DWORD) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER,
    ULARGE_INTEGER, DWORD) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Clone(IStream **) override { return E_NOTIMPL; }

  HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER liDistanceToMove,
    DWORD dwOrigin,
    ULARGE_INTEGER *lpNewFilePointer) override {
    if (lpNewFilePointer != nullptr) {
      lpNewFilePointer->QuadPart = 0;
    }

    if (liDistanceToMove.u.HighPart != 0) {
      return E_FAIL;
    }

    ULONG targetOffset;

    switch (dwOrigin) {
    case STREAM_SEEK_SET:
      targetOffset = liDistanceToMove.u.LowPart;
      break;
    case STREAM_SEEK_CUR:
      targetOffset = liDistanceToMove.u.LowPart + m_offset;
      break;
    case STREAM_SEEK_END:
      targetOffset = liDistanceToMove.u.LowPart + m_size;
      break;
    default:
      return STG_E_INVALIDFUNCTION;
    }

    // Do not implicility extend.
    if (targetOffset > m_size) {
      return E_FAIL;
    }

    m_offset = targetOffset;
    if (lpNewFilePointer != nullptr) {
      lpNewFilePointer->u.LowPart = targetOffset;
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE Stat(STATSTG *pStatstg,
    DWORD grfStatFlag) override {
    if (pStatstg == nullptr) {
      return E_POINTER;
    }
    ZeroMemory(pStatstg, sizeof(*pStatstg));
    pStatstg->type = STGTY_STREAM;
    pStatstg->cbSize.u.LowPart = m_size;
    return S_OK;
  }
};

class FixedSizeMemoryStream : public AbstractMemoryStream {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
  LPBYTE m_pBuffer;
  ULONG m_offset;
  ULONG m_size;
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(FixedSizeMemoryStream)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IStream, ISequentialStream>(this, iid, ppvObject);
  }

  void Init(LPBYTE pBuffer, size_t size) {
    m_pBuffer = pBuffer;
    m_offset = 0;
    m_size = size;
  }

  // ISequentialStream implementation.
  HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb, ULONG *pcbRead) override {
    if (!pv || !pcbRead)
      return E_POINTER;
    ULONG cbLeft = m_size - m_offset;
    *pcbRead = std::min(cb, cbLeft);
    memcpy(pv, m_pBuffer + m_offset, *pcbRead);
    m_offset += *pcbRead;
    return (*pcbRead == cb) ? S_OK : S_FALSE;
  }

  HRESULT STDMETHODCALLTYPE Write(void const *pv, ULONG cb, ULONG *pcbWritten) override {
    if (!pv || !pcbWritten)
      return E_POINTER;
    ULONG cbLeft = m_size - m_offset;
    *pcbWritten = std::min(cb, cbLeft);
    memcpy(m_pBuffer + m_offset, pv, *pcbWritten);
    m_offset += *pcbWritten;
    return (*pcbWritten == cb) ? S_OK : S_FALSE;
  }

  // IStream implementation.
  HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER val) override {
    return STG_E_ACCESSDENIED;
  }

  HRESULT STDMETHODCALLTYPE CopyTo(IStream *, ULARGE_INTEGER, 
    ULARGE_INTEGER *, ULARGE_INTEGER *) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Commit(DWORD) override { return E_NOTIMPL; }

  HRESULT STDMETHODCALLTYPE Revert(void) override { return E_NOTIMPL; }

  HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER,
    ULARGE_INTEGER, DWORD) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER,
    ULARGE_INTEGER, DWORD) override {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE Clone(IStream **) override { return E_NOTIMPL; }

  HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER, DWORD, ULARGE_INTEGER *) override {
    return E_NOTIMPL;
  }
 
  HRESULT STDMETHODCALLTYPE Stat(STATSTG *pStatstg,
    DWORD grfStatFlag) override {
    if (pStatstg == nullptr) {
      return E_POINTER;
    }
    ZeroMemory(pStatstg, sizeof(*pStatstg));
    pStatstg->type = STGTY_STREAM;
    pStatstg->cbSize.u.LowPart = m_size;
    return S_OK;
  }

  // AbstractMemoryStream implementation
  LPBYTE GetPtr() throw() override {
    return m_pBuffer;
  }

  ULONG GetPtrSize() throw() override {
    return m_size;
  }

  LPBYTE Detach() throw() override {
    LPBYTE result = m_pBuffer;
    m_pBuffer = nullptr;
    m_size = 0;
    m_offset = 0;
    return result;
  }

  UINT64 GetPosition() throw() override {
    return m_offset;
  }

  HRESULT Reserve(ULONG targetSize) throw() override {
    return targetSize <= m_size ? S_OK : E_BOUNDS;
  }
};

HRESULT CreateMemoryStream(_In_ IMalloc *pMalloc, _COM_Outptr_ AbstractMemoryStream** ppResult) throw() {
  if (pMalloc == nullptr || ppResult == nullptr) {
    return E_POINTER;
  }

  CComPtr<MemoryStream> stream = MemoryStream::Alloc(pMalloc);
  *ppResult = stream.Detach();
  return (*ppResult == nullptr) ? E_OUTOFMEMORY : S_OK;
}

HRESULT CreateReadOnlyBlobStream(_In_ IDxcBlob *pSource, _COM_Outptr_ IStream** ppResult) throw() {
  if (pSource == nullptr || ppResult == nullptr) {
    return E_POINTER;
  }

  CComPtr<ReadOnlyBlobStream> stream = ReadOnlyBlobStream::Alloc(DxcGetThreadMallocNoRef());
  if (stream.p) {
    stream->Init(pSource);
  }
  *ppResult = stream.Detach();
  return (*ppResult == nullptr) ? E_OUTOFMEMORY : S_OK;
}

HRESULT CreateFixedSizeMemoryStream(_In_ LPBYTE pBuffer, size_t size, _COM_Outptr_ AbstractMemoryStream** ppResult) throw() {
  if (pBuffer == nullptr || ppResult == nullptr) {
    return E_POINTER;
  }

  CComPtr<FixedSizeMemoryStream> stream = FixedSizeMemoryStream::Alloc(DxcGetThreadMallocNoRef());
  if (stream.p) {
    stream->Init(pBuffer, size);
  }
  *ppResult = stream.Detach();
  return (*ppResult == nullptr) ? E_OUTOFMEMORY : S_OK;
}

}  // namespace hlsl
