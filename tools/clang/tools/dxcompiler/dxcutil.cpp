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

#include "dxcutil.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilPDB.h"
#include "dxc/DxilContainer/DxilContainerAssembler.h"
#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/dxcapi.h"
#include "dxillib.h"
#include "clang/Basic/Diagnostic.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "llvm/Support/Path.h"

using namespace llvm;
using namespace hlsl;

// This declaration is used for the locally-linked validator.
HRESULT CreateDxcValidator(REFIID riid, LPVOID *ppv);
// This internal call allows the validator to avoid having to re-deserialize
// the module. It trusts that the caller didn't make any changes and is
// kept internal because the layout of the module class may change based
// on changes across modules, or picking a different compiler version or CRT.
HRESULT RunInternalValidator(IDxcValidator *pValidator, llvm::Module *pModule,
                             llvm::Module *pDebugModule, IDxcBlob *pShader,
                             UINT32 Flags, IDxcOperationResult **ppResult);

namespace {
// AssembleToContainer helper functions.

bool CreateValidator(CComPtr<IDxcValidator> &pValidator,
                     hlsl::options::ValidatorSelection SelectValidator =
                         hlsl::options::ValidatorSelection::Auto) {
  bool bInternal =
      SelectValidator == hlsl::options::ValidatorSelection::Internal;
  bool bExternal =
      SelectValidator == hlsl::options::ValidatorSelection::External;
  if (!bInternal && DxilLibIsEnabled())
    DxilLibCreateInstance(CLSID_DxcValidator, &pValidator);

  bool bInternalValidator = false;
  if (pValidator == nullptr) {
    IFTBOOL(!bExternal, DXC_E_VALIDATOR_MISSING);
    IFT(CreateDxcValidator(IID_PPV_ARGS(&pValidator)));
    bInternalValidator = true;
  }
  return bInternalValidator;
}

} // namespace

namespace dxcutil {

AssembleInputs::AssembleInputs(
    std::unique_ptr<llvm::Module> &&pM, CComPtr<IDxcBlob> &pOutputContainerBlob,
    IMalloc *pMalloc, hlsl::SerializeDxilFlags SerializeFlags,
    CComPtr<hlsl::AbstractMemoryStream> &pModuleBitcode,
    llvm::StringRef DebugName, clang::DiagnosticsEngine *pDiag,
    hlsl::DxilShaderHash *pShaderHashOut, AbstractMemoryStream *pReflectionOut,
    AbstractMemoryStream *pRootSigOut, CComPtr<IDxcBlob> pRootSigBlob,
    CComPtr<IDxcBlob> pPrivateBlob,
    hlsl::options::ValidatorSelection SelectValidator)
    : pM(std::move(pM)), pOutputContainerBlob(pOutputContainerBlob),
      pMalloc(pMalloc), SerializeFlags(SerializeFlags),
      pModuleBitcode(pModuleBitcode), DebugName(DebugName), pDiag(pDiag),
      pShaderHashOut(pShaderHashOut), pReflectionOut(pReflectionOut),
      pRootSigOut(pRootSigOut), pRootSigBlob(pRootSigBlob),
      pPrivateBlob(pPrivateBlob), SelectValidator(SelectValidator) {}

void GetValidatorVersion(unsigned *pMajor, unsigned *pMinor,
                         hlsl::options::ValidatorSelection SelectValidator) {
  if (pMajor == nullptr || pMinor == nullptr)
    return;

  CComPtr<IDxcValidator> pValidator;
  CreateValidator(pValidator, SelectValidator);

  CComPtr<IDxcVersionInfo> pVersionInfo;
  if (SUCCEEDED(pValidator.QueryInterface(&pVersionInfo))) {
    IFT(pVersionInfo->GetVersion(pMajor, pMinor));
  } else {
    // Default to 1.0
    *pMajor = 1;
    *pMinor = 0;
  }
}

void AssembleToContainer(AssembleInputs &inputs) {
  CComPtr<AbstractMemoryStream> pContainerStream;
  IFT(CreateMemoryStream(inputs.pMalloc, &pContainerStream));
  if (!(inputs.SerializeFlags & SerializeDxilFlags::StripRootSignature) &&
      inputs.pRootSigBlob) {
    IFT(SetRootSignature(&inputs.pM->GetOrCreateDxilModule(),
                         inputs.pRootSigBlob));
  } // Update the module root signature from file
  if (inputs.pPrivateBlob) {
    SerializeDxilContainerForModule(
        &inputs.pM->GetOrCreateDxilModule(), inputs.pModuleBitcode,
        inputs.pVersionInfo, pContainerStream, inputs.DebugName,
        inputs.SerializeFlags, inputs.pShaderHashOut, inputs.pReflectionOut,
        inputs.pRootSigOut, inputs.pPrivateBlob->GetBufferPointer(),
        inputs.pPrivateBlob->GetBufferSize());
  } else {
    SerializeDxilContainerForModule(
        &inputs.pM->GetOrCreateDxilModule(), inputs.pModuleBitcode,
        inputs.pVersionInfo, pContainerStream, inputs.DebugName,
        inputs.SerializeFlags, inputs.pShaderHashOut, inputs.pReflectionOut,
        inputs.pRootSigOut);
  }
  inputs.pOutputContainerBlob.Release();
  IFT(pContainerStream.QueryInterface(&inputs.pOutputContainerBlob));
}

void ReadOptsAndValidate(hlsl::options::MainArgs &mainArgs,
                         hlsl::options::DxcOpts &opts,
                         AbstractMemoryStream *pOutputStream,
                         IDxcOperationResult **ppResult, bool &finished) {
  const llvm::opt::OptTable *table = ::options::getHlslOptTable();
  raw_stream_ostream outStream(pOutputStream);
  if (0 != hlsl::options::ReadDxcOpts(table, hlsl::options::CompilerFlags,
                                      mainArgs, opts, outStream)) {
    CComPtr<IDxcBlob> pErrorBlob;
    IFT(pOutputStream->QueryInterface(&pErrorBlob));
    outStream.flush();
    IFT(DxcResult::Create(
        E_INVALIDARG, DXC_OUT_NONE,
        {DxcOutputObject::ErrorOutput(opts.DefaultTextCodePage,
                                      (LPCSTR)pErrorBlob->GetBufferPointer(),
                                      pErrorBlob->GetBufferSize())},
        ppResult));
    finished = true;
    return;
  }
  DXASSERT(opts.HLSLVersion > hlsl::LangStd::v2015,
           "else ReadDxcOpts didn't fail for non-isense");
  finished = false;
}

HRESULT ValidateAndAssembleToContainer(AssembleInputs &inputs) {
  HRESULT valHR = S_OK;

  // If we have debug info, this will be a clone of the module before debug info
  // is stripped. This is used with internal validator to provide more useful
  // error messages.
  std::unique_ptr<llvm::Module> llvmModuleWithDebugInfo;

  CComPtr<IDxcValidator> pValidator;
  bool bInternalValidator = CreateValidator(pValidator, inputs.SelectValidator);
  // Warning on internal Validator

  CComPtr<IDxcValidator2> pValidator2;
  if (!bInternalValidator) {
    pValidator.QueryInterface(&pValidator2);
  }

  if (bInternalValidator &&
      inputs.SelectValidator != hlsl::options::ValidatorSelection::Internal) {
    if (inputs.pDiag) {
      unsigned diagID = inputs.pDiag->getCustomDiagID(
          clang::DiagnosticsEngine::Level::Warning,
          "DXIL signing library (dxil.dll,libdxil.so) not found.  Resulting "
          "DXIL will not be "
          "signed for use in release environments.\r\n");
      inputs.pDiag->Report(diagID);
    }
  }

  if (bInternalValidator || pValidator2) {
    // If using the internal validator or external validator supports
    // IDxcValidator2, we'll use the modules directly. In this case, we'll want
    // to make a clone to avoid SerializeDxilContainerForModule stripping all
    // the debug info. The debug info will be stripped from the orginal module,
    // but preserved in the cloned module.
    if (llvm::getDebugMetadataVersionFromModule(*inputs.pM) != 0) {
      llvmModuleWithDebugInfo.reset(llvm::CloneModule(inputs.pM.get()));
    }
  }

  // Verify validator version can validate this module
  CComPtr<IDxcVersionInfo> pValidatorVersion;
  IFT(pValidator->QueryInterface(&pValidatorVersion));
  UINT32 ValMajor, ValMinor;
  IFT(pValidatorVersion->GetVersion(&ValMajor, &ValMinor));
  DxilModule &DM = inputs.pM.get()->GetOrCreateDxilModule();
  unsigned ReqValMajor, ReqValMinor;
  DM.GetValidatorVersion(ReqValMajor, ReqValMinor);
  if (DXIL::CompareVersions(ValMajor, ValMinor, ReqValMajor, ReqValMinor) < 0) {
    // Module is expecting to be validated by a newer validator.
    if (inputs.pDiag) {
      unsigned diagID = inputs.pDiag->getCustomDiagID(
          clang::DiagnosticsEngine::Level::Error,
          "The module cannot be validated by the version of the validator "
          "currently attached.");
      inputs.pDiag->Report(diagID);
    }
    return E_FAIL;
  }

  AssembleToContainer(inputs);

  CComPtr<IDxcOperationResult> pValResult;
  // Important: in-place edit is required so the blob is reused and thus
  // dxil.dll can be released.
  if (bInternalValidator) {
    IFT(RunInternalValidator(pValidator, inputs.pM.get(),
                             llvmModuleWithDebugInfo.get(),
                             inputs.pOutputContainerBlob,
                             DxcValidatorFlags_InPlaceEdit, &pValResult));
  } else {
    if (pValidator2 && llvmModuleWithDebugInfo) {

      // If metadata was stripped, re-serialize the input module.
      CComPtr<AbstractMemoryStream> pDebugModuleStream;
      IFT(CreateMemoryStream(DxcGetThreadMallocNoRef(), &pDebugModuleStream));
      raw_stream_ostream outStream(pDebugModuleStream.p);
      WriteBitcodeToFile(llvmModuleWithDebugInfo.get(), outStream, true);
      outStream.flush();

      DxcBuffer debugModule = {};
      debugModule.Ptr = pDebugModuleStream->GetPtr();
      debugModule.Size = pDebugModuleStream->GetPtrSize();

      IFT(pValidator2->ValidateWithDebug(inputs.pOutputContainerBlob,
                                         DxcValidatorFlags_InPlaceEdit,
                                         &debugModule, &pValResult));
    } else {
      IFT(pValidator->Validate(inputs.pOutputContainerBlob,
                               DxcValidatorFlags_InPlaceEdit, &pValResult));
    }
  }
  IFT(pValResult->GetStatus(&valHR));
  if (inputs.pDiag) {
    if (FAILED(valHR)) {
      CComPtr<IDxcBlobEncoding> pErrors;
      CComPtr<IDxcBlobUtf8> pErrorsUtf8;
      IFT(pValResult->GetErrorBuffer(&pErrors));
      IFT(hlsl::DxcGetBlobAsUtf8(pErrors, inputs.pMalloc, &pErrorsUtf8));
      StringRef errRef(pErrorsUtf8->GetStringPointer(),
                       pErrorsUtf8->GetStringLength());
      unsigned DiagID = inputs.pDiag->getCustomDiagID(
          clang::DiagnosticsEngine::Error, "validation errors\r\n%0");
      inputs.pDiag->Report(DiagID) << errRef;
    }
  }
  CComPtr<IDxcBlob> pValidatedBlob;
  IFT(pValResult->GetResult(&pValidatedBlob));
  if (pValidatedBlob != nullptr) {
    std::swap(inputs.pOutputContainerBlob, pValidatedBlob);
  }
  pValidator.Release();

  return valHR;
}

HRESULT ValidateRootSignatureInContainer(
    IDxcBlob *pRootSigContainer, clang::DiagnosticsEngine *pDiag,
    hlsl::options::ValidatorSelection SelectValidator) {
  HRESULT valHR = S_OK;
  CComPtr<IDxcValidator> pValidator;
  CComPtr<IDxcOperationResult> pValResult;
  CreateValidator(pValidator);
  IFT(pValidator->Validate(pRootSigContainer,
                           DxcValidatorFlags_RootSignatureOnly |
                               DxcValidatorFlags_InPlaceEdit,
                           &pValResult));
  IFT(pValResult->GetStatus(&valHR));
  if (pDiag) {
    if (FAILED(valHR)) {
      CComPtr<IDxcBlobEncoding> pErrors;
      CComPtr<IDxcBlobUtf8> pErrorsUtf8;
      IFT(pValResult->GetErrorBuffer(&pErrors));
      IFT(hlsl::DxcGetBlobAsUtf8(pErrors, nullptr, &pErrorsUtf8));
      StringRef errRef(pErrorsUtf8->GetStringPointer(),
                       pErrorsUtf8->GetStringLength());
      unsigned DiagID =
          pDiag->getCustomDiagID(clang::DiagnosticsEngine::Error,
                                 "root signature validation errors\r\n%0");
      pDiag->Report(DiagID) << errRef;
    }
  }
  return valHR;
}

HRESULT SetRootSignature(hlsl::DxilModule *pModule, CComPtr<IDxcBlob> pSource) {
  const char *pName = "RTS0";
  const UINT32 partKind = ((UINT32)pName[0] | ((UINT32)pName[1] << 8) |
                           ((UINT32)pName[2] << 16) | ((UINT32)pName[3] << 24));

  CComPtr<IDxcContainerReflection> pReflection;
  UINT32 partCount;
  IFT(DxcCreateInstance(CLSID_DxcContainerReflection,
                        __uuidof(IDxcContainerReflection),
                        (void **)&pReflection));
  IFT(pReflection->Load(pSource));
  IFT(pReflection->GetPartCount(&partCount));

  CComPtr<IDxcBlob> pContent;
  for (UINT32 i = 0; i < partCount; ++i) {
    UINT32 curPartKind;
    IFT(pReflection->GetPartKind(i, &curPartKind));
    if (curPartKind == partKind) {
      CComPtr<IDxcBlob> pContent;
      IFT(pReflection->GetPartContent(i, &pContent));

      try {
        const void *serializedData = pContent->GetBufferPointer();
        uint32_t serializedSize = pContent->GetBufferSize();
        hlsl::RootSignatureHandle rootSig;
        rootSig.LoadSerialized(static_cast<const uint8_t *>(serializedData),
                               serializedSize);
        std::vector<uint8_t> serializedRootSignature;
        serializedRootSignature.assign(rootSig.GetSerializedBytes(),
                                       rootSig.GetSerializedBytes() +
                                           rootSig.GetSerializedSize());
        pModule->ResetSerializedRootSignature(serializedRootSignature);
      } catch (...) {
        return DXC_E_INCORRECT_ROOT_SIGNATURE;
      }
    }
  }
  return S_OK;
}

void CreateOperationResultFromOutputs(
    DXC_OUT_KIND resultKind, UINT32 textEncoding, IDxcBlob *pResultBlob,
    CComPtr<IStream> &pErrorStream, const std::string &warnings,
    bool hasErrorOccurred, IDxcOperationResult **ppResult) {
  CComPtr<DxcResult> pResult = DxcResult::Alloc(DxcGetThreadMallocNoRef());
  IFT(pResult->SetEncoding(textEncoding));
  IFT(pResult->SetStatusAndPrimaryResult(hasErrorOccurred ? E_FAIL : S_OK,
                                         resultKind));
  IFT(pResult->SetOutputObject(resultKind, pResultBlob));
  CComPtr<IDxcBlob> pErrorBlob;
  IFT(pErrorStream.QueryInterface(&pErrorBlob));
  if (IsBlobNullOrEmpty(pErrorBlob)) {
    IFT(pResult->SetOutputString(DXC_OUT_ERRORS, warnings.c_str(),
                                 warnings.size()));
  } else {
    IFT(pResult->SetOutputObject(DXC_OUT_ERRORS, pErrorBlob));
  }
  IFT(pResult.QueryInterface(ppResult));
}

void CreateOperationResultFromOutputs(IDxcBlob *pResultBlob,
                                      CComPtr<IStream> &pErrorStream,
                                      const std::string &warnings,
                                      bool hasErrorOccurred,
                                      IDxcOperationResult **ppResult) {
  CreateOperationResultFromOutputs(DXC_OUT_OBJECT, DXC_CP_UTF8, pResultBlob,
                                   pErrorStream, warnings, hasErrorOccurred,
                                   ppResult);
}

bool IsAbsoluteOrCurDirRelative(const llvm::Twine &T) {
  if (llvm::sys::path::is_absolute(T)) {
    return true;
  }
  if (T.isSingleStringRef()) {
    StringRef r = T.getSingleStringRef();
    if (r.size() < 2)
      return false;
    const char *pData = r.data();
    return pData[0] == '.' && (pData[1] == '\\' || pData[1] == '/');
  }
  DXASSERT(false, "twine kind not supported");
  return false;
}

static bool ShouldBeCopiedIntoPDB(UINT32 FourCC) {
  switch (FourCC) {
  case hlsl::DFCC_ShaderDebugName:
  case hlsl::DFCC_ShaderHash:
    return true;
  }
  return false;
}

HRESULT CreateContainerForPDB(IMalloc *pMalloc, IDxcBlob *pOldContainer,
                              IDxcBlob *pDebugBlob,
                              IDxcVersionInfo *pVersionInfo,
                              const hlsl::DxilSourceInfo *pSourceInfo,
                              AbstractMemoryStream *pReflectionStream,
                              IDxcBlob **ppNewContainer) {
  // If the pContainer is not a valid container, give up.
  if (!hlsl::IsValidDxilContainer(
          (hlsl::DxilContainerHeader *)pOldContainer->GetBufferPointer(),
          pOldContainer->GetBufferSize()))
    return E_FAIL;

  hlsl::DxilContainerHeader *DxilHeader =
      (hlsl::DxilContainerHeader *)pOldContainer->GetBufferPointer();
  hlsl::DxilProgramHeader *ProgramHeader = nullptr;

  std::unique_ptr<DxilContainerWriter> containerWriter(
      NewDxilContainerWriter(false));
  std::unique_ptr<DxilPartWriter> pDxilVersionWriter(
      NewVersionWriter(pVersionInfo));

  for (unsigned i = 0; i < DxilHeader->PartCount; i++) {
    hlsl::DxilPartHeader *PartHeader = GetDxilContainerPart(DxilHeader, i);
    if (ShouldBeCopiedIntoPDB(PartHeader->PartFourCC)) {
      UINT32 uSize = PartHeader->PartSize;
      const void *pPartData = PartHeader + 1;
      containerWriter->AddPart(
          PartHeader->PartFourCC, uSize,
          [pPartData, uSize](AbstractMemoryStream *pStream) {
            ULONG uBytesWritten = 0;
            IFR(pStream->Write(pPartData, uSize, &uBytesWritten));
            return S_OK;
          });
    }

    // Could use any of these. We're mostly after the header version and all
    // that.
    if (PartHeader->PartFourCC == hlsl::DFCC_DXIL ||
        PartHeader->PartFourCC == hlsl::DFCC_ShaderDebugInfoDXIL) {
      ProgramHeader = (hlsl::DxilProgramHeader *)(PartHeader + 1);
    }
  }

  if (!ProgramHeader)
    return E_FAIL;

  if (pSourceInfo) {
    const UINT32 uPartSize = pSourceInfo->AlignedSizeInBytes;

    containerWriter->AddPart(hlsl::DFCC_ShaderSourceInfo, uPartSize,
                             [pSourceInfo](IStream *pStream) {
                               ULONG uBytesWritten = 0;
                               pStream->Write(pSourceInfo,
                                              pSourceInfo->AlignedSizeInBytes,
                                              &uBytesWritten);
                               return S_OK;
                             });
  }

  if (pReflectionStream) {
    const hlsl::DxilPartHeader *pReflectionPartHeader =
        (const hlsl::DxilPartHeader *)pReflectionStream->GetPtr();

    containerWriter->AddPart(
        hlsl::DFCC_ShaderStatistics, pReflectionPartHeader->PartSize,
        [pReflectionPartHeader](IStream *pStream) {
          ULONG uBytesWritten = 0;
          pStream->Write(pReflectionPartHeader + 1,
                         pReflectionPartHeader->PartSize, &uBytesWritten);
          return S_OK;
        });
  }

  if (pVersionInfo) {
    containerWriter->AddPart(
        hlsl::DFCC_CompilerVersion, pDxilVersionWriter->size(),
        [&pDxilVersionWriter](AbstractMemoryStream *pStream) {
          pDxilVersionWriter->write(pStream);
          return S_OK;
        });
  }

  if (pDebugBlob) {
    static auto AlignByDword = [](UINT32 uSize, UINT32 *pPaddingBytes) {
      UINT32 uRem = uSize % sizeof(UINT32);
      UINT32 uResult =
          (uSize / sizeof(UINT32) + (uRem ? 1 : 0)) * sizeof(UINT32);
      *pPaddingBytes = uRem ? (sizeof(UINT32) - uRem) : 0;
      return uResult;
    };

    UINT32 uPaddingSize = 0;
    UINT32 uPartSize = AlignByDword(sizeof(hlsl::DxilProgramHeader) +
                                        pDebugBlob->GetBufferSize(),
                                    &uPaddingSize);

    containerWriter->AddPart(
        hlsl::DFCC_ShaderDebugInfoDXIL, uPartSize,
        [uPartSize, ProgramHeader, pDebugBlob, uPaddingSize](IStream *pStream) {
          hlsl::DxilProgramHeader Header = *ProgramHeader;
          Header.BitcodeHeader.BitcodeSize = pDebugBlob->GetBufferSize();
          Header.BitcodeHeader.BitcodeOffset = sizeof(hlsl::DxilBitcodeHeader);
          Header.SizeInUint32 = uPartSize / sizeof(UINT32);

          ULONG uBytesWritten = 0;
          IFR(pStream->Write(&Header, sizeof(Header), &uBytesWritten));
          IFR(pStream->Write(pDebugBlob->GetBufferPointer(),
                             pDebugBlob->GetBufferSize(), &uBytesWritten));
          if (uPaddingSize) {
            UINT32 uPadding = 0;
            assert(uPaddingSize <= sizeof(uPadding) &&
                   "Padding size calculation is wrong.");
            IFR(pStream->Write(&uPadding, uPaddingSize, &uBytesWritten));
          }
          return S_OK;
        });
  }

  CComPtr<hlsl::AbstractMemoryStream> pStrippedContainerStream;
  IFR(hlsl::CreateMemoryStream(pMalloc, &pStrippedContainerStream));

  containerWriter->write(pStrippedContainerStream);
  IFR(pStrippedContainerStream.QueryInterface(ppNewContainer));

  return S_OK;
}

HRESULT CreatePDBContainerFromModule(
    IMalloc *pMalloc, hlsl::options::DxcOpts &opts, llvm::Module *pDebugModule,
    IDxcBlob *pOldContainer, hlsl::AbstractMemoryStream *pReflectionStream,
    IDxcVersionInfo *pVersionInfo, const hlsl::DxilSourceInfo *pSourceInfo,
    llvm::ArrayRef<BYTE> HashData, IDxcBlob **ppNewContainer) {
  CComPtr<IDxcBlob> pStrippedContainer;
  {
    CComPtr<IDxcBlob> pDebugProgramBlob;
    CComPtr<AbstractMemoryStream> pReflectionInPdb;
    // Don't include the debug part if using source only PDB
    if (opts.SourceOnlyDebug) {
      assert(pSourceInfo);
      pReflectionInPdb = pReflectionStream;
    } else {
      if (!opts.SourceInDebugModule) {
        // Strip out the source related metadata
        pDebugModule->GetOrCreateDxilModule()
            .StripShaderSourcesAndCompileOptions(
                /* bReplaceWithDummyData */ true);
      }
      CComPtr<AbstractMemoryStream> pDebugBlobStorage;
      IFT(CreateMemoryStream(DxcGetThreadMallocNoRef(), &pDebugBlobStorage));
      raw_stream_ostream outStream(pDebugBlobStorage.p);
      WriteBitcodeToFile(pDebugModule, outStream, true);
      outStream.flush();
      IFT(pDebugBlobStorage.QueryInterface(&pDebugProgramBlob));
    }

    IFT(dxcutil::CreateContainerForPDB(
        pMalloc, pOldContainer, pDebugProgramBlob, pVersionInfo, pSourceInfo,
        pReflectionInPdb, &pStrippedContainer));
  }

  // Create the final PDB Blob
  IFT(hlsl::pdb::WriteDxilPDB(pMalloc, pStrippedContainer, HashData,
                              ppNewContainer));

  return S_OK;
}

} // namespace dxcutil
