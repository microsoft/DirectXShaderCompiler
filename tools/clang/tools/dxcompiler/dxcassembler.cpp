///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcassembler.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Implements the DirectX Assembler object.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/microcom.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/Support/dxcapi.impl.h"

#include "llvm/Support/MemoryBuffer.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/SourceMgr.h"

using namespace llvm;
using namespace hlsl;

class DxcAssembler : public IDxcAssembler {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)      
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDxcAssembler>(this, iid, ppvObject);
  }

  DxcAssembler() : m_dwRef(0) { }

  // Assemble dxil in ll or llvm bitcode to dxbc container.
  __override HRESULT STDMETHODCALLTYPE AssembleToContainer(
      _In_ IDxcBlob *pShader, // Shader to assemble.
      _COM_Outptr_ IDxcOperationResult **ppResult // Assemble output status, buffer, and errors
      );
};

// Assemble dxil in ll or llvm bitcode to dxbc container.
HRESULT STDMETHODCALLTYPE DxcAssembler::AssembleToContainer(
    _In_ IDxcBlob *pShader, // Shader to assemble.
    _COM_Outptr_ IDxcOperationResult **ppResult // Assemble output status, buffer, and errors
    ) {
  if (pShader == nullptr || ppResult == nullptr)
    return E_POINTER;

  *ppResult = nullptr;
  HRESULT hr = S_OK;
  try {
    // Setup input buffer.
    // The ir parsing requires the buffer to be null terminated. We deal with
    // both source and bitcode input, so the input buffer may not be null terminated.
    // Create a new membuf that copies the buffer and adds a null terminator.
    unsigned char *pBytes = (unsigned char *)(pShader->GetBufferPointer());
    unsigned bytesLen = pShader->GetBufferSize();
    bool bytesAreText = !isBitcode(pBytes, pBytes + bytesLen);
    CComPtr<IDxcBlob> readingBlob;
    CComPtr<IDxcBlobEncoding> bytesEncoded;
    if (bytesAreText) {
      // IR parsing requires a null terminator in the buffer.
      IFT(hlsl::DxcGetBlobAsUtf8NullTerm(pShader, &bytesEncoded));
      pBytes = (unsigned char *)bytesEncoded->GetBufferPointer();
      bytesLen = bytesEncoded->GetBufferSize() - 1; // nullterm not included
    }

    StringRef InputData((char *)pBytes, bytesLen);
    std::unique_ptr<MemoryBuffer> memBuf =
        MemoryBuffer::getMemBuffer(InputData);

    // Parse IR
    LLVMContext Context;
    SMDiagnostic Err;
    std::unique_ptr<Module> M =
        parseIR(memBuf->getMemBufferRef(), Err, Context);

    CComPtr<IMalloc> pMalloc;
    IFT(CoGetMalloc(1, &pMalloc));

    CComPtr<AbstractMemoryStream> pOutputStream;
    IFT(CreateMemoryStream(pMalloc, &pOutputStream));
    raw_stream_ostream outStream(pOutputStream.p);

    // Check for success.
    if (M.get() == 0) {
      Err.print("shader", outStream, false, false);
      outStream.flush();

      CComPtr<IDxcBlob> pStreamBlob;
      CComPtr<IDxcBlobEncoding> pErrorBlob;
      DXVERIFY_NOMSG(SUCCEEDED(pOutputStream.QueryInterface(&pStreamBlob)));
      IFT(DxcCreateBlobWithEncodingSet(pStreamBlob, CP_UTF8, &pErrorBlob));
      IFT(DxcOperationResult::CreateFromResultErrorStatus(nullptr, pErrorBlob, E_FAIL, ppResult));
      return S_OK;
    }

    DxilModule program(M.get());
    try {
      program.LoadDxilMetadata();
    }
    catch (hlsl::Exception &e) {
      CComPtr<IDxcBlobEncoding> pErrorBlob;
      IFT(DxcCreateBlobWithEncodingOnHeapCopy(e.msg.c_str(), e.msg.size(), CP_UTF8, &pErrorBlob));
      IFT(DxcOperationResult::CreateFromResultErrorStatus(nullptr, pErrorBlob, e.hr, ppResult));
      return S_OK;
    }

    WriteBitcodeToFile(M.get(), outStream);
    outStream.flush();

    CComPtr<AbstractMemoryStream> pFinalStream;
    IFT(CreateMemoryStream(pMalloc, &pFinalStream));

    SerializeDxilContainerForModule(M.get(), pOutputStream, pFinalStream);

    CComPtr<IDxcBlob> pResultBlob;
    IFT(pFinalStream->QueryInterface(&pResultBlob));

    IFT(DxcOperationResult::CreateFromResultErrorStatus(pResultBlob, nullptr, S_OK, ppResult));
  }
  CATCH_CPP_ASSIGN_HRESULT();

  return hr;
}

HRESULT CreateDxcAssembler(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  CComPtr<DxcAssembler> result = new (std::nothrow) DxcAssembler();
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }

  return result.p->QueryInterface(riid, ppv);
}
