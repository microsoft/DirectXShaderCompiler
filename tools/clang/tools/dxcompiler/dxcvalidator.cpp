///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcvalidator.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Implements the DirectX Validator object.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DiagnosticPrinter.h"

#include "dxc/Support/WinIncludes.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/HLSL/DxilValidation.h"

#include "dxc/Support/Global.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxcetw.h"

using namespace llvm;
using namespace hlsl;

class PrintDiagnosticContext {
private:
  DiagnosticPrinter &m_Printer;
  bool m_errorsFound;
  bool m_warningsFound;
public:
  PrintDiagnosticContext(DiagnosticPrinter &printer)
      : m_Printer(printer), m_errorsFound(false), m_warningsFound(false) {}

  bool HasErrors() const {
    return m_errorsFound;
  }
  bool HasWarnings() const {
    return m_warningsFound;
  }
  void Handle(const DiagnosticInfo &DI) {
    DI.print(m_Printer);
    switch (DI.getSeverity()) {
    case llvm::DiagnosticSeverity::DS_Error:
      m_errorsFound = true;
      break;
    case llvm::DiagnosticSeverity::DS_Warning:
      m_warningsFound = true;
      break;
    }
    m_Printer << "\n";
  }
};

static void PrintDiagnosticHandler(const DiagnosticInfo &DI, void *Context) {
  reinterpret_cast<PrintDiagnosticContext *>(Context)->Handle(DI);
}

class DxcValidator : public IDxcValidator, public IDxcVersionInfo {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)

  HRESULT RunValidation(
    _In_ IDxcBlob *pShader,                       // Shader to validate.
    _In_ llvm::Module *pModule,                   // Module to validate, if available.
    _In_ llvm::Module *pDiagModule,               // Diag module to validate, if available
    _In_ AbstractMemoryStream *pDiagStream);

public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  DxcValidator() : m_dwRef(0) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface2<IDxcValidator, IDxcVersionInfo>(this, iid, ppvObject);
  }

  // For internal use only.
  HRESULT ValidateWithOptModules(
    _In_ IDxcBlob *pShader,                       // Shader to validate.
    _In_ UINT32 Flags,                            // Validation flags.
    _In_ llvm::Module *pModule,                   // Module to validate, if available.
    _In_ llvm::Module *pDiagModule,               // Diag module to validate, if available
    _COM_Outptr_ IDxcOperationResult **ppResult   // Validation output status, buffer, and errors
  );

  // IDxcValidator
  __override HRESULT STDMETHODCALLTYPE Validate(
    _In_ IDxcBlob *pShader,                       // Shader to validate.
    _In_ UINT32 Flags,                            // Validation flags.
    _COM_Outptr_ IDxcOperationResult **ppResult   // Validation output status, buffer, and errors
    );

  // IDxcVersionInfo
  __override HRESULT STDMETHODCALLTYPE GetVersion(_Out_ UINT32 *pMajor, _Out_ UINT32 *pMinor);
  __override HRESULT STDMETHODCALLTYPE GetFlags(_Out_ UINT32 *pFlags);
};

// Compile a single entry point to the target shader model
HRESULT STDMETHODCALLTYPE DxcValidator::Validate(
  _In_ IDxcBlob *pShader,                       // Shader to validate.
  _In_ UINT32 Flags,                            // Validation flags.
  _COM_Outptr_ IDxcOperationResult **ppResult   // Validation output status, buffer, and errors
) {
  if (pShader == nullptr || ppResult == nullptr || Flags & ~DxcValidatorFlags_ValidMask)
    return E_INVALIDARG;
  return ValidateWithOptModules(pShader, Flags, nullptr, nullptr, ppResult);
}

HRESULT DxcValidator::ValidateWithOptModules(
  _In_ IDxcBlob *pShader,                       // Shader to validate.
  _In_ UINT32 Flags,                            // Validation flags.
  _In_ llvm::Module *pModule,                   // Module to validate, if available.
  _In_ llvm::Module *pDiagModule,               // Diag module to validate, if available
  _COM_Outptr_ IDxcOperationResult **ppResult   // Validation output status, buffer, and errors
) {
  *ppResult = nullptr;
  HRESULT hr = S_OK;
  HRESULT validationStatus = S_OK;
  DxcEtw_DxcValidation_Start();
  try {
    CComPtr<IMalloc> pMalloc;
    CComPtr<AbstractMemoryStream> pDiagStream;
    IFT(CoGetMalloc(1, &pMalloc));
    IFT(CreateMemoryStream(pMalloc, &pDiagStream));

    // Run validation may throw, but that indicates an inability to validate,
    // not that the validation failed (eg out of memory).
    validationStatus = RunValidation(pShader, pModule, pDiagModule, pDiagStream);

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

HRESULT STDMETHODCALLTYPE DxcValidator::GetFlags(_Out_ UINT32 *pFlags) {
  if (pFlags == nullptr)
    return E_INVALIDARG;
  *pFlags = DxcVersionInfoFlags_None;
#ifdef _NDEBUG
  *pFlags |= DxcVersionInfoFlags_Debug;
#endif
  return S_OK;
}

HRESULT DxcValidator::RunValidation(
  _In_ IDxcBlob *pShader,
  _In_ llvm::Module *pModule,                   // Module to validate, if available.
  _In_ llvm::Module *pDebugModule,              // Debug module to validate, if available
  _In_ AbstractMemoryStream *pDiagStream) {

  // Run validation may throw, but that indicates an inability to validate,
  // not that the validation failed (eg out of memory). That is indicated
  // by a failing HRESULT, and possibly error messages in the diagnostics stream.

  llvm::LLVMContext llvmContext;
  std::unique_ptr<llvm::MemoryBuffer> pBitcodeBuf;
  std::unique_ptr<llvm::Module> pLoadedModule;
  std::unique_ptr<llvm::MemoryBuffer> pDbgBitcodeBuf;
  std::unique_ptr<llvm::Module> pLoadedDbgModule;
  raw_stream_ostream DiagStream(pDiagStream);
  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  if (pModule == nullptr) {
    DXASSERT_NOMSG(pDebugModule == nullptr);
    // Accept a bitcode buffer or a DXIL container.
    const char *pIL = (const char*)pShader->GetBufferPointer();
    uint32_t pILLength = pShader->GetBufferSize();
    const char *pDbgIL = nullptr;
    uint32_t pDbgILLength = 0;
    if (const DxilContainerHeader *pContainer =
      IsDxilContainerLike(pIL, pILLength)) {
      if (!IsValidDxilContainer(pContainer, pILLength)) {
        IFR(DXC_E_CONTAINER_INVALID);
      }

      DxilPartIterator it = std::find_if(begin(pContainer), end(pContainer),
        DxilPartIsType(DFCC_DXIL));
      if (it == end(pContainer)) {
        IFR(DXC_E_CONTAINER_MISSING_DXIL);
      }

      const DxilProgramHeader *pProgramHeader =
        reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(*it));
      if (!IsValidDxilProgramHeader(pProgramHeader, (*it)->PartSize)) {
        IFR(DXC_E_CONTAINER_INVALID);
      }
      GetDxilProgramBitcode(pProgramHeader, &pIL, &pILLength);

      // Look for an optional debug version of the module. If it's there,
      // it should be valid.
      DxilPartIterator dbgit = std::find_if(begin(pContainer), end(pContainer),
        DxilPartIsType(DFCC_ShaderDebugInfoDXIL));
      if (dbgit != end(pContainer)) {
        const DxilProgramHeader *pDbgHeader =
          reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(*dbgit));
        if (!IsValidDxilProgramHeader(pDbgHeader, (*dbgit)->PartSize)) {
          IFR(DXC_E_CONTAINER_INVALID);
        }
        GetDxilProgramBitcode(pDbgHeader, &pDbgIL, &pDbgILLength);
      }
    }

    llvmContext.setDiagnosticHandler(PrintDiagnosticHandler, &DiagContext, true);
    pBitcodeBuf.reset(llvm::MemoryBuffer::getMemBuffer(
        llvm::StringRef(pIL, pILLength), "", false).release());
    ErrorOr<std::unique_ptr<llvm::Module>> loadedModuleResult(llvm::parseBitcodeFile(
      pBitcodeBuf->getMemBufferRef(), llvmContext));

    // DXIL disallows some LLVM bitcode constructs, like unaccounted-for sub-blocks.
    // These appear as warnings, which the validator should reject.
    if (DiagContext.HasErrors() || DiagContext.HasWarnings()) {
      IFR(DXC_E_IR_VERIFICATION_FAILED);
    }
    if (std::error_code ec = loadedModuleResult.getError()) {
      IFR(DXC_E_IR_VERIFICATION_FAILED);
    }
    pLoadedModule.swap(loadedModuleResult.get());

    if (pDbgIL != nullptr) {
      pDbgBitcodeBuf.reset(llvm::MemoryBuffer::getMemBuffer(
          llvm::StringRef(pDbgIL, pDbgILLength), "", false).release());
      ErrorOr<std::unique_ptr<llvm::Module>> loadedDbgModuleResult(
          llvm::parseBitcodeFile(pDbgBitcodeBuf->getMemBufferRef(), llvmContext));
      if (std::error_code ec = loadedDbgModuleResult.getError()) {
        IFR(DXC_E_IR_VERIFICATION_FAILED);
      }
      pLoadedDbgModule.swap(loadedDbgModuleResult.get());
    }
    pModule = pLoadedModule.get();
    pDebugModule = pLoadedDbgModule.get();
  }
  else {
    // Install the diagnostic handler on the
    pModule->getContext().setDiagnosticHandler(PrintDiagnosticHandler,
                                               &DiagContext, true);
  }

  if (std::error_code ec = hlsl::ValidateDxilModule(pModule, pDebugModule)) {
    IFR(DXC_E_IR_VERIFICATION_FAILED);
  }

  // TODO: run validation that cross-references other parts in the
  // DxilContainer if available.

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
  CComPtr<DxcValidator> result = new (std::nothrow) DxcValidator();
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }

  return result.p->QueryInterface(riid, ppv);
}
