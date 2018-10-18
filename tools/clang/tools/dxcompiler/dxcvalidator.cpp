///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcvalidator.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Validator object.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DiagnosticPrinter.h"

#include "dxc/Support/WinIncludes.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/HLSL/DxilValidation.h"

#include "dxc/Support/Global.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/DxilRootSignature/DxilRootSignature.h"

#ifdef _WIN32
#include "dxcetw.h"
#endif

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
#include "clang/Basic/Version.h"
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO

using namespace llvm;
using namespace hlsl;

// Utility class for setting and restoring the diagnostic context so we may capture errors/warnings
struct DiagRestore {
  LLVMContext &Ctx;
  void *OrigDiagContext;
  LLVMContext::DiagnosticHandlerTy OrigHandler;

  DiagRestore(llvm::LLVMContext &Ctx, void *DiagContext) : Ctx(Ctx) {
    OrigHandler = Ctx.getDiagnosticHandler();
    OrigDiagContext = Ctx.getDiagnosticContext();
    Ctx.setDiagnosticHandler(PrintDiagnosticContext::PrintDiagnosticHandler,
                             DiagContext);
  }
  ~DiagRestore() {
    Ctx.setDiagnosticHandler(OrigHandler, OrigDiagContext);
  }
};

class DxcValidator : public IDxcValidator,
#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
                     public IDxcVersionInfo2
#else
                     public IDxcVersionInfo
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()

  HRESULT RunValidation(
    _In_ IDxcBlob *pShader,                       // Shader to validate.
    _In_ UINT32 Flags,                            // Validation flags.
    _In_ llvm::Module *pModule,                   // Module to validate, if available.
    _In_ llvm::Module *pDebugModule,              // Debug module to validate, if available
    _In_ AbstractMemoryStream *pDiagStream);

  HRESULT RunRootSignatureValidation(
    _In_ IDxcBlob *pShader,                       // Shader to validate.
    _In_ AbstractMemoryStream *pDiagStream);

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcValidator)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcValidator, IDxcVersionInfo>(this, iid, ppvObject);
  }

  // For internal use only.
  HRESULT ValidateWithOptModules(
    _In_ IDxcBlob *pShader,                       // Shader to validate.
    _In_ UINT32 Flags,                            // Validation flags.
    _In_ llvm::Module *pModule,                   // Module to validate, if available.
    _In_ llvm::Module *pDebugModule,              // Debug module to validate, if available
    _COM_Outptr_ IDxcOperationResult **ppResult   // Validation output status, buffer, and errors
  );

  // IDxcValidator
  HRESULT STDMETHODCALLTYPE Validate(
    _In_ IDxcBlob *pShader,                       // Shader to validate.
    _In_ UINT32 Flags,                            // Validation flags.
    _COM_Outptr_ IDxcOperationResult **ppResult   // Validation output status, buffer, and errors
    ) override;

  // IDxcVersionInfo
  HRESULT STDMETHODCALLTYPE GetVersion(_Out_ UINT32 *pMajor, _Out_ UINT32 *pMinor) override;
  HRESULT STDMETHODCALLTYPE GetFlags(_Out_ UINT32 *pFlags) override;

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
  // IDxcVersionInfo2
  HRESULT STDMETHODCALLTYPE GetCommitInfo(_Out_ UINT32 *pCommitCount, _Out_ char **pCommitHash) override;
#endif
};

// Compile a single entry point to the target shader model
HRESULT STDMETHODCALLTYPE DxcValidator::Validate(
  _In_ IDxcBlob *pShader,                       // Shader to validate.
  _In_ UINT32 Flags,                            // Validation flags.
  _COM_Outptr_ IDxcOperationResult **ppResult   // Validation output status, buffer, and errors
) {
  DxcThreadMalloc TM(m_pMalloc);
  if (pShader == nullptr || ppResult == nullptr || Flags & ~DxcValidatorFlags_ValidMask)
    return E_INVALIDARG;
  if ((Flags & DxcValidatorFlags_ModuleOnly) && (Flags & (DxcValidatorFlags_InPlaceEdit | DxcValidatorFlags_RootSignatureOnly)))
    return E_INVALIDARG;
  return ValidateWithOptModules(pShader, Flags, nullptr, nullptr, ppResult);
}

HRESULT DxcValidator::ValidateWithOptModules(
  _In_ IDxcBlob *pShader,                       // Shader to validate.
  _In_ UINT32 Flags,                            // Validation flags.
  _In_ llvm::Module *pModule,                   // Module to validate, if available.
  _In_ llvm::Module *pDebugModule,              // Debug module to validate, if available
  _COM_Outptr_ IDxcOperationResult **ppResult   // Validation output status, buffer, and errors
) {
  *ppResult = nullptr;
  HRESULT hr = S_OK;
  HRESULT validationStatus = S_OK;
  DxcEtw_DxcValidation_Start();
  DxcThreadMalloc TM(m_pMalloc);
  try {
    CComPtr<AbstractMemoryStream> pDiagStream;
    IFT(CreateMemoryStream(m_pMalloc, &pDiagStream));

    // Run validation may throw, but that indicates an inability to validate,
    // not that the validation failed (eg out of memory).
    if (Flags & DxcValidatorFlags_RootSignatureOnly) {
      validationStatus = RunRootSignatureValidation(pShader, pDiagStream);
    } else {
      validationStatus = RunValidation(pShader, Flags, pModule, pDebugModule, pDiagStream);
    }
    if (FAILED(validationStatus)) {
      std::string msg("Validation failed.\n");
      ULONG cbWritten;
      pDiagStream->Write(msg.c_str(), msg.size(), &cbWritten);
    }
    // Assemble the result object.
    CComPtr<IDxcBlob> pDiagBlob;
    CComPtr<IDxcBlobEncoding> pDiagBlobEnconding;
    hr = pDiagStream.QueryInterface(&pDiagBlob);
    DXASSERT_NOMSG(SUCCEEDED(hr));
    IFT(DxcCreateBlobWithEncodingSet(pDiagBlob, CP_UTF8, &pDiagBlobEnconding));
    IFT(DxcOperationResult::CreateFromResultErrorStatus(nullptr, pDiagBlobEnconding, validationStatus, ppResult));
  }
  CATCH_CPP_ASSIGN_HRESULT();

  DxcEtw_DxcValidation_Stop(SUCCEEDED(hr) ? validationStatus : hr);
  return hr;
}

HRESULT STDMETHODCALLTYPE DxcValidator::GetVersion(_Out_ UINT32 *pMajor, _Out_ UINT32 *pMinor) {
  if (pMajor == nullptr || pMinor == nullptr)
    return E_INVALIDARG;
  GetValidationVersion(pMajor, pMinor);
  return S_OK;
}

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
HRESULT STDMETHODCALLTYPE DxcValidator::GetCommitInfo(
    _Out_ UINT32 *pCommitCount, _Out_ char **pCommitHash) {
  if (pCommitCount == nullptr || pCommitHash == nullptr)
    return E_INVALIDARG;

  char *const hash = (char *)CoTaskMemAlloc(8 + 1); // 8 is guaranteed by utils/GetCommitInfo.py
  if (hash == nullptr)
    return E_OUTOFMEMORY;
  std::strcpy(hash, clang::getGitCommitHash());

  *pCommitHash = hash;
  *pCommitCount = clang::getGitCommitCount();

  return S_OK;
}
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO

HRESULT STDMETHODCALLTYPE DxcValidator::GetFlags(_Out_ UINT32 *pFlags) {
  if (pFlags == nullptr)
    return E_INVALIDARG;
  *pFlags = DxcVersionInfoFlags_None;
#ifdef _DEBUG
  *pFlags |= DxcVersionInfoFlags_Debug;
#endif
  *pFlags |= DxcVersionInfoFlags_Internal;
  return S_OK;
}

HRESULT DxcValidator::RunValidation(
  _In_ IDxcBlob *pShader,
  _In_ UINT32 Flags,                            // Validation flags.
  _In_ llvm::Module *pModule,                   // Module to validate, if available.
  _In_ llvm::Module *pDebugModule,              // Debug module to validate, if available
  _In_ AbstractMemoryStream *pDiagStream) {

  // Run validation may throw, but that indicates an inability to validate,
  // not that the validation failed (eg out of memory). That is indicated
  // by a failing HRESULT, and possibly error messages in the diagnostics stream.

  raw_stream_ostream DiagStream(pDiagStream);

  if (Flags & DxcValidatorFlags_ModuleOnly) {
    IFRBOOL(!IsDxilContainerLike(pShader->GetBufferPointer(), pShader->GetBufferSize()), E_INVALIDARG);
  } else {
    IFRBOOL(IsDxilContainerLike(pShader->GetBufferPointer(), pShader->GetBufferSize()), DXC_E_CONTAINER_INVALID);
  }

  if (!pModule) {
    DXASSERT_NOMSG(pDebugModule == nullptr);
    if (Flags & DxcValidatorFlags_ModuleOnly) {
      return ValidateDxilBitcode((const char*)pShader->GetBufferPointer(), (uint32_t)pShader->GetBufferSize(), DiagStream);
    } else {
      return ValidateDxilContainer(pShader->GetBufferPointer(), pShader->GetBufferSize(), DiagStream);
    }
  }

  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  DiagRestore DR(pModule->getContext(), &DiagContext);

  IFR(hlsl::ValidateDxilModule(pModule, pDebugModule));
  if (!(Flags & DxcValidatorFlags_ModuleOnly)) {
    IFR(ValidateDxilContainerParts(pModule, pDebugModule,
                      IsDxilContainerLike(pShader->GetBufferPointer(), pShader->GetBufferSize()),
                      (uint32_t)pShader->GetBufferSize()));
  }

  if (DiagContext.HasErrors() || DiagContext.HasWarnings()) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  return S_OK;
}

HRESULT DxcValidator::RunRootSignatureValidation(
  _In_ IDxcBlob *pShader,
  _In_ AbstractMemoryStream *pDiagStream) {

  const DxilContainerHeader *pDxilContainer = IsDxilContainerLike(
    pShader->GetBufferPointer(), pShader->GetBufferSize());
  if (!pDxilContainer) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  const DxilProgramHeader *pProgramHeader = GetDxilProgramHeader(pDxilContainer, DFCC_DXIL);
  const DxilPartHeader *pPSVPart = GetDxilPartByType(pDxilContainer, DFCC_PipelineStateValidation);
  const DxilPartHeader *pRSPart = GetDxilPartByType(pDxilContainer, DFCC_RootSignature);
  IFRBOOL(pPSVPart && pRSPart, DXC_E_MISSING_PART);
  try {
    RootSignatureHandle RSH;
    RSH.LoadSerialized((const uint8_t*)GetDxilPartData(pRSPart), pRSPart->PartSize);
    RSH.Deserialize();
    raw_stream_ostream DiagStream(pDiagStream);
    IFRBOOL(VerifyRootSignatureWithShaderPSV(RSH.GetDesc(),
                                             GetVersionShaderType(pProgramHeader->ProgramVersion),
                                             GetDxilPartData(pPSVPart),
                                             pPSVPart->PartSize,
                                             DiagStream),
      DXC_E_INCORRECT_ROOT_SIGNATURE);
  } catch(...) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////

HRESULT RunInternalValidator(_In_ IDxcValidator *pValidator,
                             _In_ llvm::Module *pModule,
                             _In_ llvm::Module *pDebugModule,
                             _In_ IDxcBlob *pShader, UINT32 Flags,
                             _COM_Outptr_ IDxcOperationResult **ppResult) {
  DXASSERT_NOMSG(pValidator != nullptr);
  DXASSERT_NOMSG(pModule != nullptr);
  DXASSERT_NOMSG(pShader != nullptr);
  DXASSERT_NOMSG(ppResult != nullptr);

  DxcValidator *pInternalValidator = (DxcValidator *)pValidator;
  return pInternalValidator->ValidateWithOptModules(pShader, Flags, pModule,
                                                    pDebugModule, ppResult);
}

HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID* ppv) {
  try {
      CComPtr<DxcValidator> result(DxcValidator::Alloc(DxcGetThreadMallocNoRef()));
      IFROOM(result.p);
      return result.p->QueryInterface(riid, ppv);
  }
  CATCH_CPP_RETURN_HRESULT();
}
