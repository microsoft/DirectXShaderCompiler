///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcassembler.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Assembler object.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/microcom.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxillib.h"
#include "dxcutil.h"

#include "llvm/Support/MemoryBuffer.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/SourceMgr.h"

using namespace llvm;
using namespace hlsl;

// This declaration is used for the locally-linked validator.
HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv);

static bool HasDebugInfo(const Module &M) {
  for (Module::const_named_metadata_iterator NMI = M.named_metadata_begin(),
                                             NME = M.named_metadata_end();
       NMI != NME; ++NMI) {
    if (NMI->getName().startswith("llvm.dbg.")) {
      return true;
    }
  }
  return false;
}

class DxcAssembler : public IDxcAssembler {
private:
  DXC_MICROCOM_TM_REF_FIELDS()      
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcAssembler)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcAssembler>(this, iid, ppvObject);
  }

  // Assemble dxil in ll or llvm bitcode to dxbc container.
  HRESULT STDMETHODCALLTYPE AssembleToContainer(
      _In_ IDxcBlob *pShader, // Shader to assemble.
      _COM_Outptr_ IDxcOperationResult **ppResult // Assemble output status, buffer, and errors
      ) override;
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
  DxcThreadMalloc TM(m_pMalloc);
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
    const bool RequiresNullTerminator = false;
    std::unique_ptr<MemoryBuffer> memBuf =
        MemoryBuffer::getMemBuffer(InputData, "", RequiresNullTerminator);

    // Parse IR
    LLVMContext Context;
    SMDiagnostic Err;
    std::unique_ptr<Module> M =
        parseIR(memBuf->getMemBufferRef(), Err, Context);

    CComPtr<AbstractMemoryStream> pOutputStream;
    IFT(CreateMemoryStream(TM.GetInstalledAllocator(), &pOutputStream));
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

    // Upgrade Validator Version if necessary.
    try {
      DxilModule &program = M->GetOrCreateDxilModule();

      {
        UINT32 majorVer, minorVer;
        dxcutil::GetValidatorVersion(&majorVer, &minorVer);
        if (program.UpgradeValidatorVersion(majorVer, minorVer)) {
          program.UpdateValidatorVersionMetadata();
        }
      }
    } catch (hlsl::Exception &e) {
      CComPtr<IDxcBlobEncoding> pErrorBlob;
      IFT(DxcCreateBlobWithEncodingOnHeapCopy(e.msg.c_str(), e.msg.size(),
                                              CP_UTF8, &pErrorBlob));
      IFT(DxcOperationResult::CreateFromResultErrorStatus(nullptr, pErrorBlob,
                                                          e.hr, ppResult));
      return S_OK;
    }
    // Create bitcode of M.
    WriteBitcodeToFile(M.get(), outStream);
    outStream.flush();

    CComPtr<IDxcBlob> pResultBlob;
    hlsl::SerializeDxilFlags flags = hlsl::SerializeDxilFlags::None;
    if (HasDebugInfo(*M)) {
      flags |= SerializeDxilFlags::IncludeDebugInfoPart;
      flags |= SerializeDxilFlags::IncludeDebugNamePart;
      flags |= SerializeDxilFlags::DebugNameDependOnSource;
    }
    dxcutil::AssembleToContainer(std::move(M), pResultBlob,
                                         TM.GetInstalledAllocator(), flags,
                                         pOutputStream);

    IFT(DxcOperationResult::CreateFromResultErrorStatus(pResultBlob, nullptr, S_OK, ppResult));
  }
  CATCH_CPP_ASSIGN_HRESULT();

  return hr;
}

HRESULT CreateDxcAssembler(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  CComPtr<DxcAssembler> result = DxcAssembler::Alloc(DxcGetThreadMallocNoRef());
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }

  return result.p->QueryInterface(riid, ppv);
}
