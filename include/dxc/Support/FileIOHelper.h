///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FileIOHelper.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Provides utitlity functions to work with files.                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

// Forward declarations.
struct IDxcBlob;
struct IDxcBlobEncoding;

namespace hlsl {

void ReadBinaryFile(_In_z_ LPCWSTR pFileName,
                    _Outptr_result_bytebuffer_(*pDataSize) void **ppData,
                    _Out_ DWORD *pDataSize);
void WriteBinaryFile(_In_z_ LPCWSTR pFileName,
                     _In_reads_bytes_(DataSize) const void *pData,
                     _In_ DWORD DataSize);

///////////////////////////////////////////////////////////////////////////////
// Blob and encoding manipulation functions.

UINT32 DxcCodePageFromBytes(_In_count_(byteLen) const char *bytes,
                            size_t byteLen) throw();

HRESULT DxcCreateBlobFromFile(LPCWSTR pFileName, _In_opt_ UINT32 *pCodePage,
                              _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) throw();

// Given a blob, creates a subrange view.
HRESULT DxcCreateBlobFromBlob(_In_ IDxcBlob *pBlob, UINT32 offset,
                              UINT32 length,
                              _COM_Outptr_ IDxcBlob **ppResult) throw();

HRESULT
DxcCreateBlobOnHeap(_In_bytecount_(size) LPCVOID pData, UINT32 size,
                    _COM_Outptr_ IDxcBlob **ppResult) throw();

HRESULT
DxcCreateBlobOnHeapCopy(_In_bytecount_(size) LPCVOID pData, UINT32 size,
                        _COM_Outptr_ IDxcBlob **ppResult) throw();

// Given a blob, creates a new instance with a specific code page set.
HRESULT
DxcCreateBlobWithEncodingSet(_In_ IDxcBlob *pBlob, UINT32 codePage,
                             _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) throw();

HRESULT DxcCreateBlobWithEncodingFromPinned(
    _In_bytecount_(size) LPCVOID pText, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) throw();

HRESULT
DxcCreateBlobWithEncodingFromStream(
    IStream *pStream, bool newInstanceAlways, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) throw();

HRESULT
DxcCreateBlobWithEncodingOnHeap(_In_bytecount_(size) LPCVOID pText, UINT32 size,
                                UINT32 codePage,
                                _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) throw();

HRESULT
DxcCreateBlobWithEncodingOnHeapCopy(
    _In_bytecount_(size) LPCVOID pText, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) throw();

HRESULT
DxcCreateBlobWithEncodingOnMalloc(
  _In_bytecount_(size) LPCVOID pText, IMalloc *pIMalloc, UINT32 size, UINT32 codePage,
  _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) throw();

HRESULT DxcGetBlobAsUtf8(_In_ IDxcBlob *pBlob,
                         _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) throw();
HRESULT
DxcGetBlobAsUtf8NullTerm(
    _In_ IDxcBlob *pBlob,
    _COM_Outptr_ IDxcBlobEncoding **ppBlobEncoding) throw();

HRESULT DxcGetBlobAsUtf16(_In_ IDxcBlob *pBlob,
                          _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) throw();

bool IsBlobNullOrEmpty(_In_opt_ IDxcBlob *pBlob) throw();

///////////////////////////////////////////////////////////////////////////////
// Stream implementations.
class AbstractMemoryStream : public IStream {
public:
  virtual LPBYTE GetPtr() throw() = 0;
  virtual ULONG GetPtrSize() throw() = 0;
  virtual LPBYTE Detach() throw() = 0;
  virtual UINT64 GetPosition() throw() = 0;
  virtual HRESULT Reserve(ULONG targetSize) throw() = 0;
};
HRESULT CreateMemoryStream(_In_ IMalloc *pMalloc, _COM_Outptr_ AbstractMemoryStream** ppResult) throw();
HRESULT CreateReadOnlyBlobStream(_In_ IDxcBlob *pSource, _COM_Outptr_ IStream** ppResult) throw();

template <typename T>
HRESULT WriteStreamValue(AbstractMemoryStream *pStream, const T& value) {
  ULONG cb;
  return pStream->Write(&value, sizeof(value), &cb);
}

} // namespace hlsl
