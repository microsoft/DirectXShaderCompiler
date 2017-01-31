///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FileIOHelper.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //

//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/dxcapi.h"

#include <algorithm>
#include <memory>
#include <intsafe.h>

#define CP_UTF16 1200

namespace hlsl {

_Use_decl_annotations_
void ReadBinaryFile(LPCWSTR pFileName, void **ppData, DWORD *pDataSize) {
  HANDLE hFile = CreateFileW(pFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if(hFile == INVALID_HANDLE_VALUE) {
    IFT(HRESULT_FROM_WIN32(GetLastError()));
  }
  CHandle h(hFile);

  LARGE_INTEGER FileSize;
  if(!GetFileSizeEx(hFile, &FileSize)) {
    IFT(HRESULT_FROM_WIN32(GetLastError()));
  }
  if(FileSize.HighPart != 0) {
    throw(hlsl::Exception(DXC_E_INPUT_FILE_TOO_LARGE, "input file is too large"));
  }
  CComHeapPtr<char> pData;
  if (!pData.AllocateBytes(FileSize.LowPart)) {
    throw std::bad_alloc();
  }

  DWORD BytesRead;
  if(!ReadFile(hFile, pData.m_pData, FileSize.LowPart, &BytesRead, nullptr)) {
    IFT(HRESULT_FROM_WIN32(GetLastError()));
  }
  DXASSERT(FileSize.LowPart == BytesRead, "ReadFile operation failed");

  *ppData = pData.Detach();
  *pDataSize = FileSize.LowPart;
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
UINT32 DxcCodePageFromBytes(const char *bytes, size_t byteLen) {
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
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  LPCVOID m_Buffer = nullptr;
  IUnknown* m_Owner = nullptr; // IMalloc when MallocFree is true
  SIZE_T m_BufferSize;
  unsigned m_HeapFree : 1;
  unsigned m_EncodingKnown : 1;
  unsigned m_MallocFree : 1;
  UINT32 m_CodePage;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  InternalDxcBlobEncoding() : m_dwRef(0) {
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface2<IDxcBlob, IDxcBlobEncoding>(this, iid, ppvObject);
  }

  ~InternalDxcBlobEncoding() {
    if (m_MallocFree) {
      ((IMalloc *)m_Owner)->Free((void *)m_Buffer);
    }
    if (m_Owner != nullptr) {
      m_Owner->Release();
    }
    if (m_HeapFree) {
      CoTaskMemFree((LPVOID)m_Buffer);
    }
  }

  static HRESULT
  CreateFromHeap(LPCVOID buffer, SIZE_T bufferSize, bool encodingKnown,
                 UINT32 codePage,
                 _COM_Outptr_ InternalDxcBlobEncoding **pEncoding) {
    *pEncoding = new (std::nothrow) InternalDxcBlobEncoding();
    if (*pEncoding == nullptr) {
      return E_OUTOFMEMORY;
    }
    (*pEncoding)->m_Buffer = buffer;
    (*pEncoding)->m_BufferSize = bufferSize;
    (*pEncoding)->m_HeapFree = 1;
    (*pEncoding)->m_EncodingKnown = encodingKnown;
    (*pEncoding)->m_MallocFree = 0;
    (*pEncoding)->m_CodePage = codePage;
    (*pEncoding)->AddRef();
    return S_OK;
  }

  static HRESULT
  CreateFromBlob(_In_ IDxcBlob *pBlob, bool encodingKnown, UINT32 codePage,
                 _COM_Outptr_ InternalDxcBlobEncoding **pEncoding) {
    *pEncoding = new (std::nothrow) InternalDxcBlobEncoding();
    if (*pEncoding == nullptr) {
      return E_OUTOFMEMORY;
    }
    pBlob->AddRef();
    (*pEncoding)->m_Owner = pBlob;
    (*pEncoding)->m_Buffer = pBlob->GetBufferPointer();
    (*pEncoding)->m_BufferSize = pBlob->GetBufferSize();
    (*pEncoding)->m_HeapFree = 0;
    (*pEncoding)->m_EncodingKnown = encodingKnown;
    (*pEncoding)->m_MallocFree = 0;
    (*pEncoding)->m_CodePage = codePage;
    (*pEncoding)->AddRef();
    return S_OK;
  }
  static HRESULT
  CreateFromMalloc(LPCVOID buffer, IMalloc *pIMalloc, SIZE_T bufferSize, bool encodingKnown,
    UINT32 codePage, _COM_Outptr_ InternalDxcBlobEncoding **pEncoding) {
    *pEncoding = new (std::nothrow) InternalDxcBlobEncoding();
    if (*pEncoding == nullptr) {
      return E_OUTOFMEMORY;
    }
    pIMalloc->AddRef();
    (*pEncoding)->m_Owner = pIMalloc;
    (*pEncoding)->m_Buffer = buffer;
    (*pEncoding)->m_BufferSize = bufferSize;
    (*pEncoding)->m_HeapFree = 0;
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
  virtual HRESULT STDMETHODCALLTYPE GetEncoding(_Out_ BOOL *pKnown, _Out_ UINT32 *pCodePage) {
    *pKnown = m_EncodingKnown ? TRUE : FALSE;
    *pCodePage = m_CodePage;
    return S_OK;
  }

  // Relatively dangerous API. This means the buffer should be pinned for as
  // long as this object is alive.
  void ClearFreeFlag() { m_HeapFree = 0; }
};

static HRESULT CodePageBufferToUtf16(UINT32 codePage, LPCVOID bufferPointer,
                                     SIZE_T bufferSize,
                                     CComHeapPtr<WCHAR> &utf16NewCopy,
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
      MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, (char *)bufferPointer,
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
      MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, (char *)bufferPointer,
                          bufferSize, utf16NewCopy, buffSizeUTF16);
  if (numActuallyConvertedUTF16 == 0)
    return HRESULT_FROM_WIN32(GetLastError());

  ((LPWSTR)utf16NewCopy)[numActuallyConvertedUTF16] = L'\0';
  *pConvertedCharCount = numActuallyConvertedUTF16;

  return S_OK;
}

_Use_decl_annotations_
HRESULT DxcCreateBlobFromBlob(
    IDxcBlob *pBlob, UINT32 offset, UINT32 length, IDxcBlob **ppResult) {
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
  IFR(InternalDxcBlobEncoding::CreateFromBlob(pBlob, encodingKnown, codePage,
                                              &pCreated));
  pCreated->AdjustPtrAndSize(offset, length);
  *ppResult = pCreated.Detach();
  return S_OK;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobOnHeap(LPCVOID pData, UINT32 size, IDxcBlob **ppResult) {
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
                        _COM_Outptr_ IDxcBlob **ppResult) {
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
HRESULT DxcCreateBlobFromFile(LPCWSTR pFileName, UINT32 *pCodePage,
                              IDxcBlobEncoding **ppBlobEncoding) {
  if (pFileName == nullptr || ppBlobEncoding == nullptr) {
    return E_POINTER;
  }

  CComHeapPtr<char> pData;
  DWORD dataSize;
  *ppBlobEncoding = nullptr;
  try {
    ReadBinaryFile(pFileName, (void **)(&pData), &dataSize);
  }
  CATCH_CPP_RETURN_HRESULT();

  bool known = (pCodePage != nullptr);
  UINT32 codePage = (pCodePage != nullptr) ? *pCodePage : 0;

  InternalDxcBlobEncoding *internalEncoding;
  HRESULT hr = InternalDxcBlobEncoding::CreateFromHeap(
      pData, dataSize, known, codePage, &internalEncoding);
  if (SUCCEEDED(hr)) {
    *ppBlobEncoding = internalEncoding;
    pData.Detach();
  }
  return hr;
}

_Use_decl_annotations_
HRESULT
DxcCreateBlobWithEncodingSet(IDxcBlob *pBlob, UINT32 codePage,
                             IDxcBlobEncoding **pBlobEncoding) {
  *pBlobEncoding = nullptr;

  InternalDxcBlobEncoding *internalEncoding;
  HRESULT hr = InternalDxcBlobEncoding::CreateFromBlob(pBlob, true, codePage,
                                                       &internalEncoding);
  if (SUCCEEDED(hr)) {
    *pBlobEncoding = internalEncoding;
  }
  return hr;
}

_Use_decl_annotations_
HRESULT DxcCreateBlobWithEncodingFromPinned(LPCVOID pText, UINT32 size,
                                            UINT32 codePage,
                                            IDxcBlobEncoding **pBlobEncoding) {
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
                                    IDxcBlobEncoding **ppBlobEncoding) {
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
                                IDxcBlobEncoding **pBlobEncoding) {
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
  IDxcBlobEncoding **pBlobEncoding) {
  *pBlobEncoding = nullptr;

  CComHeapPtr<char> heapCopy;
  if (!heapCopy.AllocateBytes(size)) {
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
  IDxcBlobEncoding **pBlobEncoding) {

  *pBlobEncoding = nullptr;
  InternalDxcBlobEncoding* internalEncoding;
  HRESULT hr = InternalDxcBlobEncoding::CreateFromMalloc(pText, pIMalloc, size, true, codePage, &internalEncoding);
  if (SUCCEEDED(hr)) {
    *pBlobEncoding = internalEncoding;
  }
  return hr;
}


_Use_decl_annotations_
HRESULT DxcGetBlobAsUtf8(IDxcBlob *pBlob, IDxcBlobEncoding **pBlobEncoding) {
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
    codePage = DxcCodePageFromBytes((char *)pBlob->GetBufferPointer(), blobLen);
  }

  if (codePage == CP_UTF8) {
    // Reuse the underlying blob but create an object with the encoding known.
    InternalDxcBlobEncoding* internalEncoding;
    hr = InternalDxcBlobEncoding::CreateFromBlob(pBlob, true, CP_UTF8, &internalEncoding);
    if (SUCCEEDED(hr)) {
      *pBlobEncoding = internalEncoding;
    }
    return hr;
  }

  // Convert and create a blob that owns the encoding.

  // Any UTF-16 output must be converted to UTF-16 first, then
  // back to the target code page.
  CComHeapPtr<WCHAR> utf16NewCopy;
  wchar_t* utf16Chars = nullptr;
  UINT32 utf16CharCount;
  if (codePage == CP_UTF16) {
    utf16Chars = (wchar_t*)pBlob->GetBufferPointer();
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
  CComHeapPtr<char> finalNewCopy;
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
  hr = InternalDxcBlobEncoding::CreateFromHeap(finalNewCopy.m_pData,
    numActuallyConvertedFinal, true, targetCodePage, &internalEncoding);
  if (SUCCEEDED(hr)) {
    *pBlobEncoding = internalEncoding;
    finalNewCopy.Detach();
  }
  return hr;
}

HRESULT
DxcGetBlobAsUtf8NullTerm(_In_ IDxcBlob *pBlob,
                         _COM_Outptr_ IDxcBlobEncoding **ppBlobEncoding) {
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
      CComHeapPtr<char> pCopy;
      if (!pCopy.Allocate(blobSize + 1))
        return E_OUTOFMEMORY;
      memcpy(pCopy.m_pData, pChars, blobSize);
      pCopy.m_pData[blobSize] = '\0';
      IFR(DxcCreateBlobWithEncodingOnHeap(pCopy.m_pData, blobSize + 1, CP_UTF8,
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
HRESULT DxcGetBlobAsUtf16(IDxcBlob *pBlob, IDxcBlobEncoding **pBlobEncoding) {
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
    hr = InternalDxcBlobEncoding::CreateFromBlob(pBlob, true, CP_UTF16, &internalEncoding);
    if (SUCCEEDED(hr)) {
      *pBlobEncoding = internalEncoding;
    }
    return hr;
  }

  // Convert and create a blob that owns the encoding.
  CComHeapPtr<WCHAR> utf16NewCopy;
  UINT32 utf16CharCount;
  hr = CodePageBufferToUtf16(codePage, pBlob->GetBufferPointer(), blobLen,
                             utf16NewCopy, &utf16CharCount);
  if (FAILED(hr)) {
    return hr;
  }

  InternalDxcBlobEncoding* internalEncoding;
  hr = InternalDxcBlobEncoding::CreateFromHeap(utf16NewCopy.m_pData,
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
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CComPtr<IMalloc> m_pMalloc;
  LPBYTE m_pMemory;
  ULONG m_offset;
  ULONG m_size;
  ULONG m_allocSize;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface3<IStream, ISequentialStream, IDxcBlob>(this, iid, ppvObject);
  }

  MemoryStream(_In_ IMalloc *pMalloc)
    : m_dwRef(0), m_pMalloc(pMalloc), m_pMemory(nullptr), m_offset(0),
    m_size(0), m_allocSize(0) {}

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
  __override LPBYTE GetPtr() {
    return m_pMemory;
  }

  __override ULONG GetPtrSize() {
    return m_size;
  }

  __override LPBYTE Detach() {
    LPBYTE result = m_pMemory;
    m_pMemory = nullptr;
    Reset();
    return result;
  }

  __override HRESULT Reserve(ULONG targetSize) {
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
  __override LPVOID STDMETHODCALLTYPE GetBufferPointer(void) {
    return m_pMemory;
  }
  __override SIZE_T STDMETHODCALLTYPE GetBufferSize(void) {
    return m_size;
  }
  __override UINT64 GetPosition() {
    return m_offset;
  }

  // ISequentialStream implementation.
  __override HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) {
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

  __override HRESULT STDMETHODCALLTYPE Write(void const* pv, ULONG cb, ULONG* pcbWritten) {
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
  __override HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER val) {
    if (val.HighPart != 0) {
      return E_OUTOFMEMORY;
    }
    if (val.LowPart > m_allocSize) {
      return Grow(m_allocSize);
    }
    if (val.LowPart < m_size) {
      m_size = val.LowPart;
      m_offset = std::min(m_offset, m_size);
    }
    else if (val.LowPart > m_size) {
      memset(m_pMemory + m_size, 0, val.LowPart - m_size);
      m_size = val.LowPart;
    }
    return S_OK;
  }

  __override HRESULT STDMETHODCALLTYPE CopyTo(IStream *, ULARGE_INTEGER,
    ULARGE_INTEGER *,
    ULARGE_INTEGER *) {
    return E_NOTIMPL;
  }

  __override HRESULT STDMETHODCALLTYPE Commit(DWORD) { return E_NOTIMPL; }

  __override HRESULT STDMETHODCALLTYPE Revert(void) { return E_NOTIMPL; }

  __override HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER,
    ULARGE_INTEGER, DWORD) {
    return E_NOTIMPL;
  }

  __override HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER,
    ULARGE_INTEGER, DWORD) {
    return E_NOTIMPL;
  }

  __override HRESULT STDMETHODCALLTYPE Clone(IStream **) { return E_NOTIMPL; }

  __override HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER liDistanceToMove,
    DWORD dwOrigin,
    ULARGE_INTEGER *lpNewFilePointer) {
    if (lpNewFilePointer != nullptr) {
      lpNewFilePointer->QuadPart = 0;
    }

    if (liDistanceToMove.HighPart != 0) {
      return E_FAIL;
    }

    ULONG targetOffset;

    switch (dwOrigin) {
    case STREAM_SEEK_SET:
      targetOffset = liDistanceToMove.LowPart;
      break;
    case STREAM_SEEK_CUR:
      targetOffset = liDistanceToMove.LowPart + m_offset;
      break;
    case STREAM_SEEK_END:
      targetOffset = liDistanceToMove.LowPart + m_size;
      break;
    default:
      return STG_E_INVALIDFUNCTION;
    }

    m_offset = targetOffset;
    if (lpNewFilePointer != nullptr) {
      lpNewFilePointer->LowPart = targetOffset;
    }
    return S_OK;
  }

  __override HRESULT STDMETHODCALLTYPE Stat(STATSTG *pStatstg,
    DWORD grfStatFlag) {
    if (pStatstg == nullptr) {
      return E_POINTER;
    }
    ZeroMemory(pStatstg, sizeof(*pStatstg));
    pStatstg->type = STGTY_STREAM;
    pStatstg->cbSize.LowPart = m_size;
    return S_OK;
  }
};

class ReadOnlyBlobStream : public IStream {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CComPtr<IDxcBlob> m_pSource;
  LPBYTE m_pMemory;
  ULONG m_offset;
  ULONG m_size;
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface2<IStream, ISequentialStream>(this, iid, ppvObject);
  }

  ReadOnlyBlobStream(IDxcBlob *pSource) : m_pSource(pSource), m_offset(0), m_dwRef(0) {
    m_size = m_pSource->GetBufferSize();
    m_pMemory = (LPBYTE)m_pSource->GetBufferPointer();
  }

  // ISequentialStream implementation.
  __override HRESULT STDMETHODCALLTYPE Read(void *pv, ULONG cb,
    ULONG *pcbRead) {
    if (!pv || !pcbRead)
      return E_POINTER;
    ULONG cbLeft = m_size - m_offset;
    *pcbRead = std::min(cb, cbLeft);
    memcpy(pv, m_pMemory + m_offset, *pcbRead);
    m_offset += *pcbRead;
    return (*pcbRead == cb) ? S_OK : S_FALSE;
  }

  __override HRESULT STDMETHODCALLTYPE Write(void const *, ULONG, ULONG *) {
    return STG_E_ACCESSDENIED;
  }

  // IStream implementation.
  __override HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER val) {
    return STG_E_ACCESSDENIED;
  }

  __override HRESULT STDMETHODCALLTYPE CopyTo(IStream *, ULARGE_INTEGER,
    ULARGE_INTEGER *,
    ULARGE_INTEGER *) {
    return E_NOTIMPL;
  }

  __override HRESULT STDMETHODCALLTYPE Commit(DWORD) { return E_NOTIMPL; }

  __override HRESULT STDMETHODCALLTYPE Revert(void) { return E_NOTIMPL; }

  __override HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER,
    ULARGE_INTEGER, DWORD) {
    return E_NOTIMPL;
  }

  __override HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER,
    ULARGE_INTEGER, DWORD) {
    return E_NOTIMPL;
  }

  __override HRESULT STDMETHODCALLTYPE Clone(IStream **) { return E_NOTIMPL; }

  __override HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER liDistanceToMove,
    DWORD dwOrigin,
    ULARGE_INTEGER *lpNewFilePointer) {
    if (lpNewFilePointer != nullptr) {
      lpNewFilePointer->QuadPart = 0;
    }

    if (liDistanceToMove.HighPart != 0) {
      return E_FAIL;
    }

    ULONG targetOffset;

    switch (dwOrigin) {
    case STREAM_SEEK_SET:
      targetOffset = liDistanceToMove.LowPart;
      break;
    case STREAM_SEEK_CUR:
      targetOffset = liDistanceToMove.LowPart + m_offset;
      break;
    case STREAM_SEEK_END:
      targetOffset = liDistanceToMove.LowPart + m_size;
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
      lpNewFilePointer->LowPart = targetOffset;
    }
    return S_OK;
  }

  __override HRESULT STDMETHODCALLTYPE Stat(STATSTG *pStatstg,
    DWORD grfStatFlag) {
    if (pStatstg == nullptr) {
      return E_POINTER;
    }
    ZeroMemory(pStatstg, sizeof(*pStatstg));
    pStatstg->type = STGTY_STREAM;
    pStatstg->cbSize.LowPart = m_size;
    return S_OK;
  }
};

HRESULT CreateMemoryStream(_In_ IMalloc *pMalloc, _COM_Outptr_ AbstractMemoryStream** ppResult) {
  if (pMalloc == nullptr || ppResult == nullptr) {
    return E_POINTER;
  }

  CComPtr<MemoryStream> stream = new (std::nothrow) MemoryStream(pMalloc);
  *ppResult = stream.Detach();
  return (*ppResult == nullptr) ? E_OUTOFMEMORY : S_OK;
}

HRESULT CreateReadOnlyBlobStream(_In_ IDxcBlob *pSource, _COM_Outptr_ IStream** ppResult) {
  if (pSource == nullptr || ppResult == nullptr) {
    return E_POINTER;
  }

  CComPtr<ReadOnlyBlobStream> stream = new (std::nothrow) ReadOnlyBlobStream(pSource);
  *ppResult = stream.Detach();
  return (*ppResult == nullptr) ? E_OUTOFMEMORY : S_OK;
}

}  // namespace hlsl
