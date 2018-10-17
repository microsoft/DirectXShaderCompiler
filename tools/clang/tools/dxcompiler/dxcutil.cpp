///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcutil.cpp                                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides helper code for dxcompiler.                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/DxilContainer/DxilContainerAssembler.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/dxcapi.h"
#include "dxcutil.h"
#include "dxillib.h"
#include "clang/Basic/Diagnostic.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/Support/HLSLOptions.h"

#include "llvm/Support/Path.h"

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
// AssembleToContainer helper functions.

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
  } else {
    // Default to 1.0
    *pMajor = 1;
    *pMinor = 0;
  }
}

void AssembleToContainer(std::unique_ptr<llvm::Module> pM,
                         CComPtr<IDxcBlob> &pOutputBlob,
                         IMalloc *pMalloc,
                         SerializeDxilFlags SerializeFlags,
                         CComPtr<AbstractMemoryStream> &pOutputStream) {
  // Take ownership of the module from the action.
  DxilCompilerLLVMModuleOutput llvmModule(std::move(pM));

  llvmModule.WrapModuleInDxilContainer(pMalloc, pOutputStream, pOutputBlob,
                                       SerializeFlags);
}

void ReadOptsAndValidate(hlsl::options::MainArgs &mainArgs,
                         hlsl::options::DxcOpts &opts,
                         AbstractMemoryStream *pOutputStream,
                         _COM_Outptr_ IDxcOperationResult **ppResult,
                         bool &finished) {
  const llvm::opt::OptTable *table = ::options::getHlslOptTable();
  raw_stream_ostream outStream(pOutputStream);
  if (0 != hlsl::options::ReadDxcOpts(table, hlsl::options::CompilerFlags,
                                      mainArgs, opts, outStream)) {
    CComPtr<IDxcBlob> pErrorBlob;
    IFT(pOutputStream->QueryInterface(&pErrorBlob));
    CComPtr<IDxcBlobEncoding> pErrorBlobWithEncoding;
    outStream.flush();
    IFT(DxcCreateBlobWithEncodingSet(pErrorBlob.p, CP_UTF8,
                                     &pErrorBlobWithEncoding));
    IFT(DxcOperationResult::CreateFromResultErrorStatus(
        nullptr, pErrorBlobWithEncoding.p, E_INVALIDARG, ppResult));
    finished = true;
    return;
  }
  DXASSERT(opts.HLSLVersion > 2015,
           "else ReadDxcOpts didn't fail for non-isense");
  finished = false;
}

HRESULT ValidateAndAssembleToContainer(
    std::unique_ptr<llvm::Module> pM, CComPtr<IDxcBlob> &pOutputBlob,
    IMalloc *pMalloc, SerializeDxilFlags SerializeFlags,
    CComPtr<AbstractMemoryStream> &pOutputStream, bool bDebugInfo,
    clang::DiagnosticsEngine &Diag) {
  HRESULT valHR = S_OK;

  // Take ownership of the module from the action.
  DxilCompilerLLVMModuleOutput llvmModule(std::move(pM));

  CComPtr<IDxcValidator> pValidator;
  bool bInternalValidator = CreateValidator(pValidator);
  // Warning on internal Validator

  if (bInternalValidator) {
    unsigned diagID =
        Diag.getCustomDiagID(clang::DiagnosticsEngine::Level::Warning,
                             "DXIL.dll not found.  Resulting DXIL will not be "
                             "signed for use in release environments.\r\n");
    Diag.Report(diagID);
    // If using the internal validator, we'll use the modules directly.
    // In this case, we'll want to make a clone to avoid
    // SerializeDxilContainerForModule stripping all the debug info. The debug
    // info will be stripped from the orginal module, but preserved in the cloned
    // module.
    if (bDebugInfo) {
      llvmModule.CloneForDebugInfo();
    }
  }

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

void CreateOperationResultFromOutputs(
    IDxcBlob *pResultBlob, CComPtr<IStream> &pErrorStream,
    const std::string &warnings, bool hasErrorOccurred,
    _COM_Outptr_ IDxcOperationResult **ppResult) {
  CComPtr<IDxcBlobEncoding> pErrorBlob;

  if (pErrorStream != nullptr) {
    CComPtr<IDxcBlob> pErrorStreamBlob;
    IFT(pErrorStream.QueryInterface(&pErrorStreamBlob));
    IFT(DxcCreateBlobWithEncodingSet(pErrorStreamBlob, CP_UTF8, &pErrorBlob));
  }
  if (IsBlobNullOrEmpty(pErrorBlob)) {
    pErrorBlob.Release();
    IFT(DxcCreateBlobWithEncodingOnHeapCopy(warnings.c_str(), warnings.size(),
                                            CP_UTF8, &pErrorBlob));
  }

  HRESULT status = hasErrorOccurred ? E_FAIL : S_OK;
  IFT(DxcOperationResult::CreateFromResultErrorStatus(pResultBlob, pErrorBlob,
                                                      status, ppResult));
}

bool IsAbsoluteOrCurDirRelative(const llvm::Twine &T) {
  if (llvm::sys::path::is_absolute(T)) {
    return true;
  }
  if (T.isSingleStringRef()) {
    StringRef r = T.getSingleStringRef();
    if (r.size() < 2) return false;
    const char *pData = r.data();
    return pData[0] == '.' && (pData[1] == '\\' || pData[1] == '/');
  }
  DXASSERT(false, "twine kind not supported");
  return false;
}

} // namespace dxcutil
