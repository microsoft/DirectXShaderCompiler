///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcutil.cpp                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides helper class for dxcompiler.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/Support/WinIncludes.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/dxcapi.h"
#include "dxcutil.h"
#include "dxillib.h"
#include "clang/Basic/Diagnostic.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;
using namespace hlsl;

// This declaration is used for the locally-linked validator.
HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv);
// This internal call allows the validator to avoid having to re-deserialize
// the module. It trusts that the caller didn't make any changes and is
// kept internal because the layout of the module class may change based
// on changes across modules, or picking a different compiler version or CRT.
HRESULT RunInternalValidator(_In_ IDxcValidator *pValidator,
                             _In_ llvm::Module *pModule,
                             _In_ llvm::Module *pDebugModule,
                             _In_ IDxcBlob *pShader, UINT32 Flags,
                             _In_ IDxcOperationResult **ppResult);

namespace {

bool CreateValidator(CComPtr<IDxcValidator> &pValidator) {
  if (DxilLibIsEnabled()) {
    DxilLibCreateInstance(CLSID_DxcValidator, &pValidator);
  }
  bool bInternalValidator = false;
  if (pValidator == nullptr) {
    IFT(CreateDxcValidator(IID_PPV_ARGS(&pValidator)));
    bInternalValidator = true;
  }
  return bInternalValidator;
}

// Class to manage lifetime of llvm module and provide some utility
// functions used for generating compiler output.
class DxilCompilerLLVMModuleOutput {
public:
  DxilCompilerLLVMModuleOutput(std::unique_ptr<llvm::Module> module)
      : m_llvmModule(std::move(module)) {}

  void CloneForDebugInfo() {
    m_llvmModuleWithDebugInfo.reset(llvm::CloneModule(m_llvmModule.get()));
  }

  void WrapModuleInDxilContainer(IMalloc *pMalloc,
                                 AbstractMemoryStream *pModuleBitcode,
                                 CComPtr<IDxcBlob> &pDxilContainerBlob,
                                 SerializeDxilFlags Flags) {
    CComPtr<AbstractMemoryStream> pContainerStream;
    IFT(CreateMemoryStream(pMalloc, &pContainerStream));
    SerializeDxilContainerForModule(&m_llvmModule->GetOrCreateDxilModule(),
                                    pModuleBitcode, pContainerStream, Flags);

    pDxilContainerBlob.Release();
    IFT(pContainerStream.QueryInterface(&pDxilContainerBlob));
  }

  llvm::Module *get() { return m_llvmModule.get(); }
  llvm::Module *getWithDebugInfo() { return m_llvmModuleWithDebugInfo.get(); }

private:
  std::unique_ptr<llvm::Module> m_llvmModule;
  std::unique_ptr<llvm::Module> m_llvmModuleWithDebugInfo;
};

} // namespace

namespace dxcutil {
void GetValidatorVersion(unsigned *pMajor, unsigned *pMinor) {
  if (pMajor == nullptr || pMinor == nullptr)
    return;

  CComPtr<IDxcValidator> pValidator;
  CreateValidator(pValidator);

  CComPtr<IDxcVersionInfo> pVersionInfo;
  if (SUCCEEDED(pValidator.QueryInterface(&pVersionInfo))) {
    IFT(pVersionInfo->GetVersion(pMajor, pMinor));
  }
}

void AssembleToContainer(std::unique_ptr<llvm::Module> pM,
                         CComPtr<IDxcBlob> &pOutputBlob,
                         CComPtr<IMalloc> &pMalloc,
                         SerializeDxilFlags SerializeFlags,
                         CComPtr<AbstractMemoryStream> &pOutputStream) {
  // Take ownership of the module from the action.
  DxilCompilerLLVMModuleOutput llvmModule(std::move(pM));

  llvmModule.WrapModuleInDxilContainer(pMalloc, pOutputStream, pOutputBlob,
                                       SerializeFlags);
}

HRESULT ValidateAndAssembleToContainer(
    std::unique_ptr<llvm::Module> pM, CComPtr<IDxcBlob> &pOutputBlob,
    CComPtr<IMalloc> &pMalloc, SerializeDxilFlags SerializeFlags,
    CComPtr<AbstractMemoryStream> &pOutputStream, bool bDebugInfo,
    clang::DiagnosticsEngine &Diag) {
  HRESULT valHR = S_OK;

  // Take ownership of the module from the action.
  DxilCompilerLLVMModuleOutput llvmModule(std::move(pM));

  CComPtr<IDxcValidator> pValidator;
  bool bInternalValidator = CreateValidator(pValidator);
  // If using the internal validator, we'll use the modules directly.
  // In this case, we'll want to make a clone to avoid
  // SerializeDxilContainerForModule stripping all the debug info. The debug
  // info will be stripped from the orginal module, but preserved in the cloned
  // module.
  if (bInternalValidator && bDebugInfo)
    llvmModule.CloneForDebugInfo();

  llvmModule.WrapModuleInDxilContainer(pMalloc, pOutputStream, pOutputBlob,
                                       SerializeFlags);

  CComPtr<IDxcOperationResult> pValResult;
  // Important: in-place edit is required so the blob is reused and thus
  // dxil.dll can be released.
  if (bInternalValidator) {
    IFT(RunInternalValidator(pValidator, llvmModule.get(),
                             llvmModule.getWithDebugInfo(), pOutputBlob,
                             DxcValidatorFlags_InPlaceEdit, &pValResult));
  } else {
    IFT(pValidator->Validate(pOutputBlob, DxcValidatorFlags_InPlaceEdit,
                             &pValResult));
  }
  IFT(pValResult->GetStatus(&valHR));
  if (FAILED(valHR)) {
    CComPtr<IDxcBlobEncoding> pErrors;
    CComPtr<IDxcBlobEncoding> pErrorsUtf8;
    IFT(pValResult->GetErrorBuffer(&pErrors));
    IFT(hlsl::DxcGetBlobAsUtf8(pErrors, &pErrorsUtf8));
    StringRef errRef((const char *)pErrorsUtf8->GetBufferPointer(),
                     pErrorsUtf8->GetBufferSize());
    unsigned DiagID = Diag.getCustomDiagID(clang::DiagnosticsEngine::Error,
                                           "validation errors\r\n%0");
    Diag.Report(DiagID) << errRef;
  }
  CComPtr<IDxcBlob> pValidatedBlob;
  IFT(pValResult->GetResult(&pValidatedBlob));
  if (pValidatedBlob != nullptr) {
    std::swap(pOutputBlob, pValidatedBlob);
  }
  pValidator.Release();

  return valHR;
}

} // namespace dxcutil