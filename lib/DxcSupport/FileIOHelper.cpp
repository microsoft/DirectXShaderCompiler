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

#ifdef _WIN32
struct HeapMalloc : public IMalloc {
public:
  ULONG STDMETHODCALLTYPE AddRef() {
    return 1;
  }
  ULONG STDMETHODCALLTYPE Release() {
    return 1;
  }
  STDMETHODIMP QueryInterface(REFIID iid, void** ppvObject) override {
    return DoBasicQueryInterface<IMalloc>(this, iid, ppvObject);
  }
  virtual void *STDMETHODCALLTYPE Alloc(
    /* [annotation][in] */
    _In_  SIZE_T cb) {
    return HeapAlloc(GetProcessHeap(), 0, cb);
  }

  virtual void *STDMETHODCALLTYPE Realloc(
    /* [annotation][in] */
    _In_opt_  void *pv,
    /* [annotation][in] */
    _In_  SIZE_T cb)
  {
    return HeapReAlloc(GetProcessHeap(), 0, pv, cb);
  }

  virtual void STDMETHODCALLTYPE Free(
    /* [annotation][in] */
    _In_opt_  void *pv)
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


  virtual void STDMETHODCALLTYPE HeapMinimize(void)
  {
  }
};
#else
typedef IMalloc HeapMalloc;
#endif

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
    else if (strncmp(bom, "\xff\xfe**", 2) == 0) {
      codePage = 1200; //UTF-16 LE
    }
    else if (strncmp(bom, "\xfe\xff**", 2) == 0) {
      codePage = 1201; //UTF-16 BE
    }
    else if (strncmp(bom, "\xff\xfe\x00\x00", 4) == 0) {
      codePage = 12000; //UTF-32 LE
    }
    else if (strncmp(bom, "\x00\x00\xfe\xff", 4) == 0) {
      codePage = 12001; //UTF-32 BE
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

class InternalDxcBlobEncoding : public IDxcBlobEncoding {
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
    ULONG result = (ULONG)llvm::sys::AtomicDecrement(&m_dwRef);
    if (result == 0) {
      CComPtr<IMalloc> pTmp(m_pMalloc);
      this->~InternalDxcBlobEncoding();
      pTmp->Free(this);
    }
    return result;
  }
  DXC_MICROCOM_TM_CTOR(InternalDxcBlobEncoding)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcBlob, IDxcBlobEncoding>(this, iid, ppvObject);
  }

  ~InternalDxcBlobEncoding() {
    if (m_MallocFree) {
      ((IMalloc *)m_Owner)->Free((void *)m_Buffer);
    }
    if (m_Owner != nullptr) {
      m_Owner->Release();
    }
  }

  static HRESULT
  CreateFromHeap(LPCVOID buffer, SIZE_T bufferSize, bool encodingKnown,
                 UINT32 codePage,
                 _COM_Outptr_ InternalDxcBlobEncoding **ppEncoding) {
    return CreateFromMalloc(buffer, DxcGetThreadMallocNoRef(), bufferSize,
                            encodingKnown, codePage, ppEncoding);
  }

  static HRESULT
  CreateFromBlob(_In_ IDxcBlob *pBlob, _In_ IMalloc *pMalloc, bool encodingKnown, UINT32 codePage,
                 _COM_Outptr_ InternalDxcBlobEncoding **pEncoding) {
    *pEncoding = InternalDxcBlobEncoding::Alloc(pMalloc);
    if (*pEncoding == nullptr) {
      return E_OUTOFMEMORY;
    }
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
    UINT32 codePage, _COM_Outptr_ InternalDxcBlobEncoding **pEncoding) {
    *pEncoding = InternalDxcBlobEncoding::Alloc(pIMalloc);
    if (*pEncoding == nullptr) {
      *pEncoding = nullptr;
      return E_OUTOFMEMORY;
    }
    pIMalloc->AddRef();
    (*pEncoding)->m_Owner = pIMalloc;
    (*pEncoding)->m_Buffer = buffer;
    (*pEncoding)->m_BufferSize = bufferSize;
    (*pEncoding)->m_EncodingKnown = encodingKnown;
    (*pEncoding)->m_MallocFree = 1;
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
    return (LPVOID)m_Buffer;
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

static HRESULT CodePageBufferToUtf16(UINT32 codePage, LPCVOID bufferPointer,
                                     SIZE_T bufferSize,
                                     CDxcMallocHeapPtr<WCHAR> &utf16NewCopy,
                                     _Out_ UINT32 *pConvertedCharCount) {
  *pConvertedCharCount = 0;

  // If the buffer is empty, don't dereference bufferPointer at all.
  // Keep the null terminator post-condition.
  if (bufferSize == 0) {
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

  // Add an extra character to make this more developer-friendly.
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
  ((LPWSTR)utf16NewCopy)[numActuallyConvertedUTF16] = L'\0';
  *pConvertedCharCount = numActuallyConvertedUTF16;

  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcCreateBlobFromBlob(
    IDxcBlob *pBlob, UINT32 offset, UINT32 length, IDxcBlob **ppResult) throw() {
  if (pBlob == nullptr || ppResult == nullptr) {
    return E_POINTER;
  }
  *ppResult = nullptr;
  SIZE_T blobSize = pBlob->GetBufferSize();
  if (offset > blobSize)
    return E_INVALIDARG;
  UINT32 end;
  IFR(UInt32Add(offset, length, &end));
  BOOL encodingKnown = FALSE;
  UINT32 codePage = CP_ACP;
  CComPtr<IDxcBlobEncoding> pBlobEncoding;
  if (SUCCEEDED(pBlob->QueryInterface(&pBlobEncoding))) {
    IFR(pBlobEncoding->GetEncoding(&encodingKnown, &codePage));
  }
  CComPtr<InternalDxcBlobEncoding> pCreated;
  IFR(InternalDxcBlobEncoding::CreateFromBlob(pBlob, DxcGetThreadMallocNoRef(), encodingKnown, codePage,
                                              &pCreated));
  pCreated->AdjustPtrAndSize(offset, length);
  *ppResult = pCreated.Detach();
  return S_OK;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobOnHeap(LPCVOID pData, UINT32 size, IDxcBlob **ppResult) throw() {
  if (pData == nullptr || ppResult == nullptr) {
    return E_POINTER;
  }

  *ppResult = nullptr;
  CComPtr<InternalDxcBlobEncoding> blob;
  IFR(InternalDxcBlobEncoding::CreateFromHeap(pData, size, false, 0, &blob));
  *ppResult = blob.Detach();
  return S_OK;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobOnHeapCopy(_In_bytecount_(size) LPCVOID pData, UINT32 size,
                        _COM_Outptr_ IDxcBlob **ppResult) throw() {
  if (pData == nullptr || ppResult == nullptr) {
    return E_POINTER;
  }

  *ppResult = nullptr;

  CComHeapPtr<char> heapCopy;
  if (!heapCopy.AllocateBytes(size)) {
    return E_OUTOFMEMORY;
  }
  memcpy(heapCopy.m_pData, pData, size);

  CComPtr<InternalDxcBlobEncoding> blob;
  IFR(InternalDxcBlobEncoding::CreateFromHeap(heapCopy.m_pData, size, false, 0, &blob));
  heapCopy.Detach();
  *ppResult = blob.Detach();
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

  InternalDxcBlobEncoding *internalEncoding;
  HRESULT hr = InternalDxcBlobEncoding::CreateFromMalloc(
    pData, pMalloc, dataSize, known, codePage, &internalEncoding);
  if (SUCCEEDED(hr)) {
    *ppBlobEncoding = internalEncoding;
  }
  else {
    pMalloc->Free(pData);
  }
  return hr;
}

_Use_decl_annotations_
HRESULT DxcCreateBlobFromFile(LPCWSTR pFileName, UINT32 *pCodePage,
                              IDxcBlobEncoding **ppBlobEncoding) throw() {
  CComPtr<IMalloc> pMalloc;
  IFR(CoGetMalloc(1, &pMalloc));
  return DxcCreateBlobFromFile(pMalloc, pFileName, pCodePage, ppBlobEncoding);
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingSet(IMalloc *pMalloc, IDxcBlob *pBlob, UINT32 codePage,
                             IDxcBlobEncoding **ppBlobEncoding) throw() {
  DXASSERT_NOMSG(pMalloc != nullptr);
  DXASSERT_NOMSG(pBlob != nullptr);
  DXASSERT_NOMSG(ppBlobEncoding != nullptr);
  *ppBlobEncoding = nullptr;

  InternalDxcBlobEncoding *internalEncoding;
  HRESULT hr = InternalDxcBlobEncoding::CreateFromBlob(
      pBlob, pMalloc, true, codePage, &internalEncoding);
  if (SUCCEEDED(hr)) {
    *ppBlobEncoding = internalEncoding;
  }
  return hr;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingSet(IDxcBlob *pBlob, UINT32 codePage,
                             IDxcBlobEncoding **ppBlobEncoding) throw() {
  return DxcCreateBlobWithEncodingSet(DxcGetThreadMallocNoRef(), pBlob,
                                      codePage, ppBlobEncoding);
}

_Use_decl_annotations_
HRESULT DxcCreateBlobWithEncodingFromPinned(LPCVOID pText, UINT32 size,
                                            UINT32 codePage,
                                            IDxcBlobEncoding **pBlobEncoding) throw() {
  *pBlobEncoding = nullptr;

  InternalDxcBlobEncoding *internalEncoding;
  HRESULT hr = InternalDxcBlobEncoding::CreateFromHeap(
      pText, size, true, codePage, &internalEncoding);
  if (SUCCEEDED(hr)) {
    internalEncoding->ClearFreeFlag();
    *pBlobEncoding = internalEncoding;
  }
  return hr;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingFromStream(IStream *pStream, bool newInstanceAlways,
                                    UINT32 codePage,
                                    IDxcBlobEncoding **ppBlobEncoding) throw() {
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
DxcCreateBlobWithEncodingOnHeap(LPCVOID pText, UINT32 size, UINT32 codePage,
                                IDxcBlobEncoding **pBlobEncoding) throw() {
  *pBlobEncoding = nullptr;

  InternalDxcBlobEncoding *internalEncoding;
  HRESULT hr = InternalDxcBlobEncoding::CreateFromHeap(
      pText, size, true, codePage, &internalEncoding);
  if (SUCCEEDED(hr)) {
    *pBlobEncoding = internalEncoding;
  }
  return hr;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingOnHeapCopy(LPCVOID pText, UINT32 size, UINT32 codePage,
  IDxcBlobEncoding **pBlobEncoding) throw() {
  *pBlobEncoding = nullptr;

  CDxcMallocHeapPtr<char> heapCopy(DxcGetThreadMallocNoRef());
  if (!heapCopy.Allocate(size)) {
    return E_OUTOFMEMORY;
  }
  memcpy(heapCopy.m_pData, pText, size);

  InternalDxcBlobEncoding* internalEncoding;
  HRESULT hr = InternalDxcBlobEncoding::CreateFromHeap(heapCopy.m_pData, size, true, codePage, &internalEncoding);
  if (SUCCEEDED(hr)) {
    *pBlobEncoding = internalEncoding;
    heapCopy.Detach();
  }
  return hr;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingOnMalloc(LPCVOID pText, IMalloc *pIMalloc, UINT32 size, UINT32 codePage,
  IDxcBlobEncoding **pBlobEncoding) throw() {

  *pBlobEncoding = nullptr;
  InternalDxcBlobEncoding* internalEncoding;
  HRESULT hr = InternalDxcBlobEncoding::CreateFromMalloc(pText, pIMalloc, size, true, codePage, &internalEncoding);
  if (SUCCEEDED(hr)) {
    *pBlobEncoding = internalEncoding;
  }
  return hr;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingOnMallocCopy(IMalloc *pIMalloc, LPCVOID pText, UINT32 size, UINT32 codePage,
  IDxcBlobEncoding **ppBlobEncoding) throw() {
  *ppBlobEncoding = nullptr;
  void *pData = pIMalloc->Alloc(size);
  if (pData == nullptr)
    return E_OUTOFMEMORY;
  memcpy(pData, pText, size);
  HRESULT hr = DxcCreateBlobWithEncodingOnMalloc(pData, pIMalloc, size, codePage, ppBlobEncoding);
  if (FAILED(hr)) {
    pIMalloc->Free(pData);
    return hr;
  }
  return S_OK;
}


_Use_decl_annotations_
HRESULT DxcGetBlobAsUtf8(IDxcBlob *pBlob, IDxcBlobEncoding **pBlobEncoding) throw() {
  *pBlobEncoding = nullptr;

  HRESULT hr;
  CComPtr<IDxcBlobEncoding> pSourceBlob;
  UINT32 codePage = CP_ACP;
  BOOL known = FALSE;
  if (SUCCEEDED(pBlob->QueryInterface(&pSourceBlob))) {
    hr = pSourceBlob->GetEncoding(&known, &codePage);
    if (FAILED(hr)) {
      return hr;
    }

    // If it's known to be CP_UTF8, there is nothing else to be done.
    if (known && codePage == CP_UTF8) {
      *pBlobEncoding = pSourceBlob.Detach();
      return S_OK;
    }
  }
  else {
    known = FALSE;
  }

  SIZE_T blobLen = pBlob->GetBufferSize();
  if (!known && blobLen > 0) {
    codePage = DxcCodePageFromBytes((const char *)pBlob->GetBufferPointer(), blobLen);
  }

  if (codePage == CP_UTF8) {
    // Reuse the underlying blob but create an object with the encoding known.
    InternalDxcBlobEncoding* internalEncoding;
    hr = InternalDxcBlobEncoding::CreateFromBlob(pBlob, DxcGetThreadMallocNoRef(), true, CP_UTF8, &internalEncoding);
    if (SUCCEEDED(hr)) {
      *pBlobEncoding = internalEncoding;
    }
    return hr;
  }

  // Convert and create a blob that owns the encoding.

  // Any UTF-16 output must be converted to UTF-16 first, then
  // back to the target code page.
  CDxcMallocHeapPtr<WCHAR> utf16NewCopy(DxcGetThreadMallocNoRef());
  const wchar_t* utf16Chars = nullptr;
  UINT32 utf16CharCount;
  if (codePage == CP_UTF16) {
    utf16Chars = (const wchar_t*)pBlob->GetBufferPointer();
    utf16CharCount = blobLen / sizeof(wchar_t);
  }
  else {
    hr = CodePageBufferToUtf16(codePage, pBlob->GetBufferPointer(), blobLen,
                               utf16NewCopy, &utf16CharCount);
    if (FAILED(hr)) {
      return hr;
    }
    utf16Chars = utf16NewCopy;
  }

  const UINT32 targetCodePage = CP_UTF8;
  CDxcTMHeapPtr<char> finalNewCopy;
  int numToConvertFinal = WideCharToMultiByte(
    targetCodePage, 0, utf16Chars, utf16CharCount,
    finalNewCopy, 0, NULL, NULL);
  if (numToConvertFinal == 0)
    return HRESULT_FROM_WIN32(GetLastError());

  unsigned buffSizeFinal;
  IFR(Int32ToUInt32(numToConvertFinal, &buffSizeFinal));
  IFR(UInt32Add(buffSizeFinal, 1, &buffSizeFinal));
  finalNewCopy.AllocateBytes(buffSizeFinal);
  IFROOM(finalNewCopy.m_pData);

  int numActuallyConvertedFinal = WideCharToMultiByte(
    targetCodePage, 0, utf16Chars, utf16CharCount,
    finalNewCopy, buffSizeFinal, NULL, NULL);
  if (numActuallyConvertedFinal == 0)
    return HRESULT_FROM_WIN32(GetLastError());
  ((LPSTR)finalNewCopy)[numActuallyConvertedFinal] = '\0';

  InternalDxcBlobEncoding* internalEncoding;
  hr = InternalDxcBlobEncoding::CreateFromMalloc(finalNewCopy.m_pData,
    DxcGetThreadMallocNoRef(),
    numActuallyConvertedFinal, true, targetCodePage, &internalEncoding);
  if (SUCCEEDED(hr)) {
    *pBlobEncoding = internalEncoding;
    finalNewCopy.Detach();
  }
  return hr;
}

HRESULT
DxcGetBlobAsUtf8NullTerm(_In_ IDxcBlob *pBlob,
                         _COM_Outptr_ IDxcBlobEncoding **ppBlobEncoding) throw() {
  *ppBlobEncoding = nullptr;

  HRESULT hr;
  CComPtr<IDxcBlobEncoding> pSourceBlob;
  unsigned blobSize = pBlob->GetBufferSize();

  // Check whether we already have a null-terminated UTF-8 blob.
  if (SUCCEEDED(pBlob->QueryInterface(&pSourceBlob))) {
    UINT32 codePage = CP_ACP;
    BOOL known = FALSE;
    hr = pSourceBlob->GetEncoding(&known, &codePage);
    if (FAILED(hr)) {
      return hr;
    }
    if (known && codePage == CP_UTF8) {
      char *pChars = (char *)pBlob->GetBufferPointer();
      if (blobSize > 0) {
        if (pChars[blobSize - 1] == '\0') {
          *ppBlobEncoding = pSourceBlob.Detach();
          return S_OK;
        }
      }
      
      // We have a non-null-terminated UTF-8 stream. Copy to a new location.
      CDxcTMHeapPtr<char> pCopy;
      if (!pCopy.Allocate(blobSize + 1))
        return E_OUTOFMEMORY;
      memcpy(pCopy.m_pData, pChars, blobSize);
      pCopy.m_pData[blobSize] = '\0';
      IFR(DxcCreateBlobWithEncodingOnMalloc(
          pCopy.m_pData, DxcGetThreadMallocNoRef(), blobSize + 1, CP_UTF8,
          ppBlobEncoding));
      pCopy.Detach();
      return S_OK;
    }
  }

  // Perform the conversion, which typically adds a new null terminator,
  // but run this again just in case.
  CComPtr<IDxcBlobEncoding> pConverted;
  IFR(DxcGetBlobAsUtf8(pBlob, &pConverted));
  return DxcGetBlobAsUtf8NullTerm(pConverted, ppBlobEncoding);
}

_Use_decl_annotations_
HRESULT DxcGetBlobAsUtf16(IDxcBlob *pBlob, IMalloc *pMalloc, IDxcBlobEncoding **pBlobEncoding) throw() {
  *pBlobEncoding = nullptr;

  HRESULT hr;
  CComPtr<IDxcBlobEncoding> pSourceBlob;
  UINT32 codePage = CP_ACP;
  BOOL known = FALSE;
  if (SUCCEEDED(pBlob->QueryInterface(&pSourceBlob))) {
    hr = pSourceBlob->GetEncoding(&known, &codePage);
    if (FAILED(hr)) {
      return hr;
    }

    // If it's known to be CP_UTF8, there is nothing else to be done.
    if (known && codePage == CP_UTF16) {
      *pBlobEncoding = pSourceBlob.Detach();
      return S_OK;
    }
  }
  else {
    known = FALSE;
  }

  SIZE_T blobLen = pBlob->GetBufferSize();
  if (!known) {
    codePage = DxcCodePageFromBytes((char *)pBlob->GetBufferPointer(), blobLen);
  }

  // Reuse the underlying blob but create an object with the encoding known.
  if (codePage == CP_UTF16) {
    InternalDxcBlobEncoding* internalEncoding;
    hr = InternalDxcBlobEncoding::CreateFromBlob(pBlob, pMalloc, true, CP_UTF16, &internalEncoding);
    if (SUCCEEDED(hr)) {
      *pBlobEncoding = internalEncoding;
    }
    return hr;
  }

  // Convert and create a blob that owns the encoding.
  CDxcMallocHeapPtr<WCHAR> utf16NewCopy(pMalloc);
  UINT32 utf16CharCount;
  hr = CodePageBufferToUtf16(codePage, pBlob->GetBufferPointer(), blobLen,
                             utf16NewCopy, &utf16CharCount);
  if (FAILED(hr)) {
    return hr;
  }

  InternalDxcBlobEncoding* internalEncoding;
  hr = InternalDxcBlobEncoding::CreateFromMalloc(
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
    ULONG result = (ULONG)llvm::sys::AtomicDecrement(&m_dwRef);
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

}  // namespace hlsl
