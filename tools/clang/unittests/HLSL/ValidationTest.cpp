///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ValidationTest.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <vector>
#include <string>
#include <algorithm>

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Regex.h"
#include "llvm/ADT/ArrayRef.h"
#include "dxc/HLSL/DxilContainer.h"

#ifdef _WIN32
#include <atlbase.h>
#endif
#include "dxc/Support/Global.h"
#include "dxc/Support/FileIOHelper.h"

#include "DxcTestUtils.h"
#include "HlslTestUtils.h"

using namespace std;
using namespace hlsl;

#ifdef _WIN32
class ValidationTest {
#else
class ValidationTest : public ::testing::Test {
#endif
public:
  BEGIN_TEST_CLASS(ValidationTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  TEST_METHOD(WhenCorrectThenOK)
  TEST_METHOD(WhenMisalignedThenFail)
  TEST_METHOD(WhenEmptyFileThenFail)
  TEST_METHOD(WhenIncorrectMagicThenFail)
  TEST_METHOD(WhenIncorrectTargetTripleThenFail)
  TEST_METHOD(WhenIncorrectModelThenFail)
  TEST_METHOD(WhenIncorrectPSThenFail)

  TEST_METHOD(WhenWaveAffectsGradientThenFail)

  TEST_METHOD(WhenMultipleModulesThenFail)
  TEST_METHOD(WhenUnexpectedEOFThenFail)
  TEST_METHOD(WhenUnknownBlocksThenFail)
  TEST_METHOD(WhenZeroInputPatchCountWithInputThenFail)

  TEST_METHOD(Float32DenormModeAttribute)
  TEST_METHOD(LoadOutputControlPointNotInPatchConstantFunction)
  TEST_METHOD(StorePatchControlNotInPatchConstantFunction)
  TEST_METHOD(OutputControlPointIDInPatchConstantFunction)
  TEST_METHOD(GsVertexIDOutOfBound)
  TEST_METHOD(StreamIDOutOfBound)
  TEST_METHOD(SignatureDataWidth)
  TEST_METHOD(SignatureStreamIDForNonGS)
  TEST_METHOD(TypedUAVStoreFullMask0)
  TEST_METHOD(TypedUAVStoreFullMask1)
  TEST_METHOD(Recursive)
  TEST_METHOD(Recursive2)
  TEST_METHOD(Recursive3)
  TEST_METHOD(ResourceRangeOverlap0)
  TEST_METHOD(ResourceRangeOverlap1)
  TEST_METHOD(ResourceRangeOverlap2)
  TEST_METHOD(ResourceRangeOverlap3)
  TEST_METHOD(CBufferOverlap0)
  TEST_METHOD(CBufferOverlap1)
  TEST_METHOD(ControlFlowHint)
  TEST_METHOD(ControlFlowHint1)
  TEST_METHOD(ControlFlowHint2)
  TEST_METHOD(SemanticLength1)
  TEST_METHOD(SemanticLength64)
  TEST_METHOD(PullModelPosition)
  TEST_METHOD(StructBufStrideAlign)
  TEST_METHOD(StructBufStrideOutOfBound)
  TEST_METHOD(StructBufGlobalCoherentAndCounter)
  TEST_METHOD(StructBufLoadCoordinates)
  TEST_METHOD(StructBufStoreCoordinates)
  TEST_METHOD(TypedBufRetType)
  TEST_METHOD(VsInputSemantic)
  TEST_METHOD(VsOutputSemantic)
  TEST_METHOD(HsInputSemantic)
  TEST_METHOD(HsOutputSemantic)
  TEST_METHOD(PatchConstSemantic)
  TEST_METHOD(DsInputSemantic)
  TEST_METHOD(DsOutputSemantic)
  TEST_METHOD(GsInputSemantic)
  TEST_METHOD(GsOutputSemantic)
  TEST_METHOD(PsInputSemantic)
  TEST_METHOD(PsOutputSemantic)
  TEST_METHOD(ArrayOfSVTarget)
  TEST_METHOD(InfiniteLog)
  TEST_METHOD(InfiniteAsin)
  TEST_METHOD(InfiniteAcos)
  TEST_METHOD(InfiniteDdxDdy)
  TEST_METHOD(IDivByZero)
  TEST_METHOD(UDivByZero)
  TEST_METHOD(UnusedMetadata)
  TEST_METHOD(MemoryOutOfBound)
  TEST_METHOD(LocalRes2)
  TEST_METHOD(LocalRes3)
  TEST_METHOD(LocalRes5)
  TEST_METHOD(LocalRes5Dbg)
  TEST_METHOD(LocalRes6)
  TEST_METHOD(LocalRes6Dbg)
  TEST_METHOD(AddrSpaceCast)
  TEST_METHOD(PtrBitCast)
  TEST_METHOD(MinPrecisionBitCast)
  TEST_METHOD(StructBitCast)
  TEST_METHOD(MultiDimArray)
  TEST_METHOD(SimpleGs8)
  TEST_METHOD(SimpleGs9)
  TEST_METHOD(SimpleGs10)
  TEST_METHOD(IllegalSampleOffset3)
  TEST_METHOD(IllegalSampleOffset4)
  TEST_METHOD(NoFunctionParam)
  TEST_METHOD(I8Type)
  TEST_METHOD(EmptyStructInBuffer)
  TEST_METHOD(BigStructInBuffer)
  TEST_METHOD(GloballyCoherent2)
  TEST_METHOD(GloballyCoherent3)
  // TODO: enable this.
  //TEST_METHOD(TGSMRaceCond)
  //TEST_METHOD(TGSMRaceCond2)
  TEST_METHOD(AddUint64Odd)

  TEST_METHOD(BarycentricFloat4Fail)
  TEST_METHOD(BarycentricMaxIndexFail)
  TEST_METHOD(BarycentricNoInterpolationFail)
  TEST_METHOD(BarycentricSamePerspectiveFail)
  TEST_METHOD(ClipCullMaxComponents)
  TEST_METHOD(ClipCullMaxRows)
  TEST_METHOD(DuplicateSysValue)
  TEST_METHOD(FunctionAttributes)
  TEST_METHOD(GSMainMissingAttributeFail)
  TEST_METHOD(GSOtherMissingAttributeFail)
  TEST_METHOD(GSMissingMaxVertexCountFail)
  TEST_METHOD(HSMissingPCFFail)
  TEST_METHOD(GetAttributeAtVertexInVSFail)
  TEST_METHOD(GetAttributeAtVertexIn60Fail)
  TEST_METHOD(GetAttributeAtVertexInterpFail)
  TEST_METHOD(SemTargetMax)
  TEST_METHOD(SemTargetIndexMatchesRow)
  TEST_METHOD(SemTargetCol0)
  TEST_METHOD(SemIndexMax)
  TEST_METHOD(SemTessFactorIndexMax)
  TEST_METHOD(SemInsideTessFactorIndexMax)
  TEST_METHOD(SemShouldBeAllocated)
  TEST_METHOD(SemShouldNotBeAllocated)
  TEST_METHOD(SemComponentOrder)
  TEST_METHOD(SemComponentOrder2)
  TEST_METHOD(SemComponentOrder3)
  TEST_METHOD(SemIndexConflictArbSV)
  TEST_METHOD(SemIndexConflictTessfactors)
  TEST_METHOD(SemIndexConflictTessfactors2)
  TEST_METHOD(SemRowOutOfRange)
  TEST_METHOD(SemPackOverlap)
  TEST_METHOD(SemPackOverlap2)
  TEST_METHOD(SemMultiDepth)

  TEST_METHOD(WhenInstrDisallowedThenFail)
  TEST_METHOD(WhenDepthNotFloatThenFail)
  TEST_METHOD(BarrierFail)
  TEST_METHOD(CBufferLegacyOutOfBoundFail)
  TEST_METHOD(CsThreadSizeFail)
  TEST_METHOD(DeadLoopFail)
  TEST_METHOD(EvalFail)
  TEST_METHOD(GetDimCalcLODFail)
  TEST_METHOD(HsAttributeFail)
  TEST_METHOD(InnerCoverageFail)
  TEST_METHOD(InterpChangeFail)
  TEST_METHOD(InterpOnIntFail)
  TEST_METHOD(InvalidSigCompTyFail)
  TEST_METHOD(MultiStream2Fail)
  TEST_METHOD(PhiTGSMFail)
  TEST_METHOD(QuadOpInCS)
  TEST_METHOD(ReducibleFail)
  TEST_METHOD(SampleBiasFail)
  TEST_METHOD(SamplerKindFail)
  TEST_METHOD(SemaOverlapFail)
  TEST_METHOD(SigOutOfRangeFail)
  TEST_METHOD(SigOverlapFail)
  TEST_METHOD(SimpleHs1Fail)
  TEST_METHOD(SimpleHs3Fail)
  TEST_METHOD(SimpleHs4Fail)
  TEST_METHOD(SimpleDs1Fail)
  TEST_METHOD(SimpleGs1Fail)
  TEST_METHOD(UavBarrierFail)
  TEST_METHOD(UndefValueFail)
  TEST_METHOD(UpdateCounterFail)
  TEST_METHOD(LocalResCopy)
  TEST_METHOD(ResCounter)

  TEST_METHOD(WhenSmUnknownThenFail)
  TEST_METHOD(WhenSmLegacyThenFail)

  TEST_METHOD(WhenMetaFlagsUsageDeclThenOK)
  TEST_METHOD(WhenMetaFlagsUsageThenFail)

  TEST_METHOD(WhenRootSigMismatchThenFail)
  TEST_METHOD(WhenRootSigCompatThenSucceed)
  TEST_METHOD(WhenRootSigMatchShaderSucceed_RootConstVis)
  TEST_METHOD(WhenRootSigMatchShaderFail_RootConstVis)
  TEST_METHOD(WhenRootSigMatchShaderSucceed_RootCBV)
  TEST_METHOD(WhenRootSigMatchShaderFail_RootCBV_Range)
  TEST_METHOD(WhenRootSigMatchShaderFail_RootCBV_Space)
  TEST_METHOD(WhenRootSigMatchShaderSucceed_RootSRV)
  TEST_METHOD(WhenRootSigMatchShaderFail_RootSRV_ResType)
  TEST_METHOD(WhenRootSigMatchShaderSucceed_RootUAV)
  TEST_METHOD(WhenRootSigMatchShaderSucceed_DescTable)
  TEST_METHOD(WhenRootSigMatchShaderSucceed_DescTable_GoodRange)
  TEST_METHOD(WhenRootSigMatchShaderSucceed_DescTable_Unbounded)
  TEST_METHOD(WhenRootSigMatchShaderFail_DescTable_Range1)
  TEST_METHOD(WhenRootSigMatchShaderFail_DescTable_Range2)
  TEST_METHOD(WhenRootSigMatchShaderFail_DescTable_Range3)
  TEST_METHOD(WhenRootSigMatchShaderFail_DescTable_Space)
  TEST_METHOD(WhenRootSigMatchShaderSucceed_Unbounded)
  TEST_METHOD(WhenRootSigMatchShaderFail_Unbounded1)
  TEST_METHOD(WhenRootSigMatchShaderFail_Unbounded2)
  TEST_METHOD(WhenRootSigMatchShaderFail_Unbounded3)
  TEST_METHOD(WhenProgramOutSigMissingThenFail)
  TEST_METHOD(WhenProgramOutSigUnexpectedThenFail)
  TEST_METHOD(WhenProgramSigMismatchThenFail)
  TEST_METHOD(WhenProgramInSigMissingThenFail)
  TEST_METHOD(WhenProgramSigMismatchThenFail2)
  TEST_METHOD(WhenProgramPCSigMissingThenFail)
  TEST_METHOD(WhenPSVMismatchThenFail)
  TEST_METHOD(WhenRDATMismatchThenFail)
  TEST_METHOD(WhenFeatureInfoMismatchThenFail)
  TEST_METHOD(RayShaderWithSignaturesFail)

  TEST_METHOD(ViewIDInCSFail)
  TEST_METHOD(ViewIDIn60Fail)
  TEST_METHOD(ViewIDNoSpaceFail)

  TEST_METHOD(LibFunctionResInSig)
  TEST_METHOD(RayPayloadIsStruct)
  TEST_METHOD(RayAttrIsStruct)
  TEST_METHOD(CallableParamIsStruct)
  TEST_METHOD(RayShaderExtraArg)
  TEST_METHOD(ResInShaderStruct)
  TEST_METHOD(WhenPayloadSizeTooSmallThenFail)
  TEST_METHOD(WhenMissingPayloadThenFail)
  TEST_METHOD(ShaderFunctionReturnTypeVoid)

  dxc::DxcDllSupport m_dllSupport;
  VersionSupportInfo m_ver;

  void TestCheck(LPCWSTR name) {
    std::wstring fullPath = hlsl_test::GetPathToHlslDataFile(name);
    FileRunTestResult t = FileRunTestResult::RunFromFileCommands(fullPath.c_str());
    if (t.RunResult != 0) {
      CA2W commentWide(t.ErrorMessage.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(commentWide);
      WEX::Logging::Log::Error(L"Run result is not zero");
    }
  }

  void CheckValidationMsgs(IDxcBlob *pBlob, llvm::ArrayRef<LPCSTR> pErrorMsgs, bool bRegex = false) {
    CComPtr<IDxcValidator> pValidator;
    CComPtr<IDxcOperationResult> pResult;

    UINT32 Flags = DxcValidatorFlags_Default;
    if (!IsDxilContainerLike(pBlob->GetBufferPointer(), pBlob->GetBufferSize())) {
      // Validation of raw bitcode as opposed to DxilContainer is not supported through DXIL.dll
      if (!m_ver.m_InternalValidator) {
        WEX::Logging::Log::Comment(L"Test skipped due to validation of raw bitcode without container and use of external DXIL.dll validator.");
        return;
      }
      Flags |= DxcValidatorFlags_ModuleOnly;
    }

    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcValidator, &pValidator));
    VERIFY_SUCCEEDED(pValidator->Validate(pBlob, Flags, &pResult));

    CheckOperationResultMsgs(pResult, pErrorMsgs, false, bRegex);
  }

  void CheckValidationMsgs(const char *pBlob, size_t blobSize, llvm::ArrayRef<LPCSTR> pErrorMsgs, bool bRegex = false) {
    CComPtr<IDxcLibrary> pLibrary;
    CComPtr<IDxcBlobEncoding> pBlobEncoding; // Encoding doesn't actually matter, it's binary.
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    VERIFY_SUCCEEDED(pLibrary->CreateBlobWithEncodingFromPinned(pBlob, blobSize, CP_UTF8, &pBlobEncoding));
    CheckValidationMsgs(pBlobEncoding, pErrorMsgs, bRegex);
  }

  void CompileSource(IDxcBlobEncoding *pSource, LPCSTR pShaderModel,
                     LPCWSTR *pArguments, UINT32 argCount, const DxcDefine *pDefines,
                     UINT32 defineCount, IDxcBlob **pResultBlob) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlob> pProgram;

    CA2W shWide(pShaderModel, CP_UTF8);

    wchar_t *pEntryName = L"main";
    if (llvm::StringRef(pShaderModel).startswith("lib_"))
      pEntryName = L"";

    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", pEntryName, shWide,
                                        pArguments, argCount, pDefines,
                                        defineCount, nullptr, &pResult));
    CheckOperationResultMsgs(pResult, nullptr, false, false);
    VERIFY_SUCCEEDED(pResult->GetResult(pResultBlob));
  }

  void CompileSource(IDxcBlobEncoding *pSource, LPCSTR pShaderModel,
                     IDxcBlob **pResultBlob) {
    CompileSource(pSource, pShaderModel, nullptr, 0, nullptr, 0, pResultBlob);
  }

  void CompileSource(LPCSTR pSource, LPCSTR pShaderModel,
                     IDxcBlob **pResultBlob) {
    CComPtr<IDxcBlobEncoding> pSourceBlob;
    Utf8ToBlob(m_dllSupport, pSource, &pSourceBlob);
    CompileSource(pSourceBlob, pShaderModel, nullptr, 0, nullptr, 0, pResultBlob);
  }

  void DisassembleProgram(IDxcBlob *pProgram, std::string *text) {
    *text = ::DisassembleProgram(m_dllSupport, pProgram);
  }

  void RewriteAssemblyCheckMsg(IDxcBlobEncoding *pSource, LPCSTR pShaderModel,
    LPCWSTR *pArguments, UINT32 argCount,
    const DxcDefine *pDefines, UINT32 defineCount,
    llvm::ArrayRef<LPCSTR> pLookFors,
    llvm::ArrayRef<LPCSTR> pReplacements,
    llvm::ArrayRef<LPCSTR> pErrorMsgs,
    bool bRegex = false) {
    CComPtr<IDxcBlob> pText;
    RewriteAssemblyToText(pSource, pShaderModel, pArguments, argCount, pDefines, defineCount, pLookFors, pReplacements, &pText, bRegex);
    CComPtr<IDxcAssembler> pAssembler;
    CComPtr<IDxcOperationResult> pAssembleResult;
    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
    VERIFY_SUCCEEDED(pAssembler->AssembleToContainer(pText, &pAssembleResult));

    if (!CheckOperationResultMsgs(pAssembleResult, pErrorMsgs, true, bRegex)) {
      // Assembly succeeded, try validation.
      CComPtr<IDxcBlob> pBlob;
      VERIFY_SUCCEEDED(pAssembleResult->GetResult(&pBlob));
      CheckValidationMsgs(pBlob, pErrorMsgs, bRegex);
    }
  }

  void RewriteAssemblyCheckMsg(LPCSTR pSource, LPCSTR pShaderModel,
                               LPCWSTR *pArguments, UINT32 argCount,
                               const DxcDefine *pDefines, UINT32 defineCount,
                               llvm::ArrayRef<LPCSTR> pLookFors,
                               llvm::ArrayRef<LPCSTR> pReplacements,
                               llvm::ArrayRef<LPCSTR> pErrorMsgs,
                               bool bRegex = false) {
    CComPtr<IDxcBlobEncoding> pSourceBlob;
    Utf8ToBlob(m_dllSupport, pSource, &pSourceBlob);
    RewriteAssemblyCheckMsg(pSourceBlob, pShaderModel, pArguments, argCount,
                            pDefines, defineCount, pLookFors, pReplacements,
                            pErrorMsgs, bRegex);
  }

  void RewriteAssemblyCheckMsg(LPCSTR pSource, LPCSTR pShaderModel,
    llvm::ArrayRef<LPCSTR> pLookFors, llvm::ArrayRef<LPCSTR> pReplacements,
    llvm::ArrayRef<LPCSTR> pErrorMsgs, bool bRegex = false) {
    RewriteAssemblyCheckMsg(pSource, pShaderModel, nullptr, 0, nullptr, 0, pLookFors, pReplacements, pErrorMsgs, bRegex);
  }

  void RewriteAssemblyCheckMsg(LPCWSTR name, LPCSTR pShaderModel,
    LPCWSTR *pArguments, UINT32 argCount,
    const DxcDefine *pDefines, UINT32 defCount,
    llvm::ArrayRef<LPCSTR> pLookFors,
    llvm::ArrayRef<LPCSTR> pReplacements,
    llvm::ArrayRef<LPCSTR> pErrorMsgs,
    bool bRegex = false) {
    std::wstring fullPath = hlsl_test::GetPathToHlslDataFile(name);
    CComPtr<IDxcLibrary> pLibrary;
    CComPtr<IDxcBlobEncoding> pSource;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    VERIFY_SUCCEEDED(
      pLibrary->CreateBlobFromFile(fullPath.c_str(), nullptr, &pSource));
    RewriteAssemblyCheckMsg(pSource, pShaderModel,
      pArguments, argCount, pDefines, defCount, pLookFors,
      pReplacements, pErrorMsgs, bRegex);
  }

  void RewriteAssemblyCheckMsg(LPCWSTR name, LPCSTR pShaderModel,
    llvm::ArrayRef<LPCSTR> pLookFors,
    llvm::ArrayRef<LPCSTR> pReplacements,
    llvm::ArrayRef<LPCSTR> pErrorMsgs,
    bool bRegex = false) {
    RewriteAssemblyCheckMsg(name, pShaderModel, nullptr, 0, nullptr, 0,
      pLookFors, pReplacements, pErrorMsgs, bRegex);
  }

  void RewriteAssemblyToText(IDxcBlobEncoding *pSource, LPCSTR pShaderModel,
                             LPCWSTR *pArguments, UINT32 argCount,
                             const DxcDefine *pDefines, UINT32 defineCount,
                             llvm::ArrayRef<LPCSTR> pLookFors,
                             llvm::ArrayRef<LPCSTR> pReplacements,
                             IDxcBlob **pBlob, bool bRegex = false) {
    CComPtr<IDxcBlob> pProgram;
    std::string disassembly;
    CompileSource(pSource, pShaderModel, pArguments, argCount, pDefines, defineCount, &pProgram);
    DisassembleProgram(pProgram, &disassembly);
    for (unsigned i = 0; i < pLookFors.size(); ++i) {
      LPCSTR pLookFor = pLookFors[i];
      bool bOptional = false;
      if (pLookFor[0] == '?') {
        bOptional = true;
        pLookFor++;
      }
      LPCSTR pReplacement = pReplacements[i];
      if (pLookFor && *pLookFor) {
        if (bRegex) {
          llvm::Regex RE(pLookFor);
          std::string reErrors;
          VERIFY_IS_TRUE(RE.isValid(reErrors));
          std::string replaced = RE.sub(pReplacement, disassembly, &reErrors);
          if (!bOptional) {
            VERIFY_ARE_NOT_EQUAL(disassembly, replaced);
            VERIFY_IS_TRUE(reErrors.empty());
          }
          disassembly = std::move(replaced);
        } else {
          bool found = false;
          size_t pos = 0;
          size_t lookForLen = strlen(pLookFor);
          size_t replaceLen = strlen(pReplacement);
          for (;;) {
            pos = disassembly.find(pLookFor, pos);
            if (pos == std::string::npos)
              break;
            found = true; // at least once
            disassembly.replace(pos, lookForLen, pReplacement);
            pos += replaceLen;
          }
          if (!bOptional) {
            VERIFY_IS_TRUE(found);
          }
        }
      }
    }
    Utf8ToBlob(m_dllSupport, disassembly.c_str(), pBlob);
  }


  // compile one or two sources, validate module from 1 with container parts from 2, check messages
  void ReplaceContainerPartsCheckMsgs(LPCSTR pSource1, LPCSTR pSource2, LPCSTR pShaderModel,
                                     llvm::ArrayRef<DxilFourCC> PartsToReplace,
                                     llvm::ArrayRef<LPCSTR> pErrorMsgs) {
    CComPtr<IDxcBlob> pProgram1, pProgram2;
    CompileSource(pSource1, pShaderModel, &pProgram1);
    VERIFY_IS_NOT_NULL(pProgram1);
    if (pSource2) {
      CompileSource(pSource2, pShaderModel, &pProgram2);
      VERIFY_IS_NOT_NULL(pProgram2);
    } else {
      pProgram2 = pProgram1;
    }

    // construct container with module from pProgram1 with other parts from pProgram2:
    const DxilContainerHeader *pHeader1 = IsDxilContainerLike(pProgram1->GetBufferPointer(), pProgram1->GetBufferSize());
    VERIFY_IS_NOT_NULL(pHeader1);
    const DxilContainerHeader *pHeader2 = IsDxilContainerLike(pProgram2->GetBufferPointer(), pProgram2->GetBufferSize());
    VERIFY_IS_NOT_NULL(pHeader2);

    unique_ptr<DxilContainerWriter> pContainerWriter(NewDxilContainerWriter());

    // Add desired parts from first container
    for (auto pPart : pHeader1) {
      for (auto dfcc : PartsToReplace) {
        if (dfcc == pPart->PartFourCC) {
          pPart = nullptr;
          break;
        }
      }
      if (!pPart)
        continue;
      pContainerWriter->AddPart(pPart->PartFourCC, pPart->PartSize, [=](AbstractMemoryStream *pStream) {
        ULONG cbWritten = 0;
        pStream->Write(GetDxilPartData(pPart), pPart->PartSize, &cbWritten);
      });
    }

    // Add desired parts from second container
    for (auto pPart : pHeader2) {
      for (auto dfcc : PartsToReplace) {
        if (dfcc == pPart->PartFourCC) {
          pContainerWriter->AddPart(pPart->PartFourCC, pPart->PartSize, [=](AbstractMemoryStream *pStream) {
            ULONG cbWritten = 0;
            pStream->Write(GetDxilPartData(pPart), pPart->PartSize, &cbWritten);
          });
          break;
        }
      }
    }

    // Write the container
    CComPtr<IMalloc> pMalloc;
    VERIFY_SUCCEEDED(CoGetMalloc(1, &pMalloc));
    CComPtr<AbstractMemoryStream> pOutputStream;
    VERIFY_SUCCEEDED(CreateMemoryStream(pMalloc, &pOutputStream));
    pOutputStream->Reserve(pContainerWriter->size());
    pContainerWriter->write(pOutputStream);

    CheckValidationMsgs((const char *)pOutputStream->GetPtr(), pOutputStream->GetPtrSize(), pErrorMsgs, /*bRegex*/false);
  }
};

bool ValidationTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    m_ver.Initialize(m_dllSupport);
  }
  return true;
}

TEST_F(ValidationTest, WhenCorrectThenOK) {
  CComPtr<IDxcBlob> pProgram;
  CompileSource("float4 main() : SV_Target { return 1; }", "ps_6_0", &pProgram);
  CheckValidationMsgs(pProgram, nullptr);
}

// Lots of these going on below for simplicity in setting up payloads.
//
// warning C4838: conversion from 'int' to 'const char' requires a narrowing conversion
// warning C4309: 'initializing': truncation of constant value
#pragma warning(disable: 4838)
#pragma warning(disable: 4309)

TEST_F(ValidationTest, WhenMisalignedThenFail) {
  // Bitcode size must 4-byte aligned
  const char blob[] = {
    'B', 'C',
  };
  CheckValidationMsgs(blob, _countof(blob), "Invalid bitcode size");
}

TEST_F(ValidationTest, WhenEmptyFileThenFail) {
  // No blocks after signature.
  const char blob[] = {
    'B', 'C', (char)0xc0, (char)0xde
  };
  CheckValidationMsgs(blob, _countof(blob), "Malformed IR file");
}

TEST_F(ValidationTest, WhenIncorrectMagicThenFail) {
  // Signature isn't 'B', 'C', 0xC0 0xDE
  const char blob[] = {
    'B', 'C', (char)0xc0, (char)0xdd
  };
  CheckValidationMsgs(blob, _countof(blob), "Invalid bitcode signature");
}

TEST_F(ValidationTest, WhenIncorrectTargetTripleThenFail) {
  const char blob[] = {
    'B', 'C', (char)0xc0, (char)0xde
  };
  CheckValidationMsgs(blob, _countof(blob), "Malformed IR file");
}

TEST_F(ValidationTest, WhenMultipleModulesThenFail) {
  const char blob[] = {
    'B', 'C', (char)0xc0, (char)0xde,
    0x21, 0x0c, 0x00, 0x00, // Enter sub-block, BlockID = 8, Code Size=3, padding x2
    0x00, 0x00, 0x00, 0x00, // NumWords = 0
    0x08, 0x00, 0x00, 0x00, // End-of-block, padding
    // At this point, this is valid bitcode (but missing required DXIL metadata)
    // Trigger the case we're looking for now
    0x21, 0x0c, 0x00, 0x00, // Enter sub-block, BlockID = 8, Code Size=3, padding x2
  };
  CheckValidationMsgs(blob, _countof(blob), "Unused bits in buffer");
}

TEST_F(ValidationTest, WhenUnexpectedEOFThenFail) {
  // Importantly, this is testing the usage of report_fatal_error during deserialization.
  const char blob[] = {
    'B', 'C', (char)0xc0, (char)0xde,
    0x21, 0x0c, 0x00, 0x00, // Enter sub-block, BlockID = 8, Code Size=3, padding x2
    0x00, 0x00, 0x00, 0x00, // NumWords = 0
  };
  CheckValidationMsgs(blob, _countof(blob), "Invalid record");
}

TEST_F(ValidationTest, WhenUnknownBlocksThenFail) {
  const char blob[] = {
    'B', 'C', (char)0xc0, (char)0xde,   // Signature
    0x31, 0x00, 0x00, 0x00  // Enter sub-block, BlockID != 8
  };
  CheckValidationMsgs(blob, _countof(blob), "Unrecognized block found");
}

TEST_F(ValidationTest, WhenZeroInputPatchCountWithInputThenFail) {
	RewriteAssemblyCheckMsg(
		L"..\\CodeGenHLSL\\SimpleHs1.hlsl", "hs_6_0",
		"void ()* @\"\\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@V?$InputPatch@UPSSceneIn@@$02@@@Z\", i32 3, i32 3",
		"void ()* @\"\\01?HSPerPatchFunc@@YA?AUHSPerPatchData@@V?$InputPatch@UPSSceneIn@@$02@@@Z\", i32 0, i32 3",
		"When HS input control point count is 0, no input signature should exist");
}

TEST_F(ValidationTest, WhenInstrDisallowedThenFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\abs2.hlsl", "ps_6_0",
      {
          "target triple = \"dxil-ms-dx\"",
          "ret void",
          "dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 3, i32 undef)",
          "!\"ps\", i32 6, i32 0",
      },
      {
          "target triple = \"dxil-ms-dx\"\n%dx.types.wave_t = type { i8* }",
          "unreachable",
          "dx.op.loadInput.i32(i32 4, i32 0, i32 0, i8 3, i32 undef)\n%wave_local = alloca %dx.types.wave_t",
          "!\"vs\", i32 6, i32 0",
      },
      {"Semantic 'SV_Target' is invalid as vs Output",
       "Declaration '%dx.types.wave_t = type { i8* }' uses a reserved prefix",
       "Instructions must be of an allowed type",
      }
  );
}

TEST_F(ValidationTest, WhenDepthNotFloatThenFail) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\IntegerDepth2.hlsl", "ps_6_0",
                          {
                              "!\"SV_Depth\", i8 9",
                          },
                          {
                              "!\"SV_Depth\", i8 4",
                          },
                          {
                              "SV_Depth must be float",
                          });
}

TEST_F(ValidationTest, BarrierFail) {
  if (m_ver.SkipIRSensitiveTest()) return;
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\barrier.hlsl", "cs_6_0",
      {"dx.op.barrier(i32 80, i32 8)",
        "dx.op.barrier(i32 80, i32 9)",
        "dx.op.barrier(i32 80, i32 11)",
        "%\"class.RWStructuredBuffer<matrix<float, 2, 2> >\" = type { %class.matrix.float.2.2 }\n",
        "call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)",
      },
      {"dx.op.barrier(i32 80, i32 15)",
        "dx.op.barrier(i32 80, i32 0)",
        "dx.op.barrier(i32 80, i32 %rem)",
        "%\"class.RWStructuredBuffer<matrix<float, 2, 2> >\" = type { %class.matrix.float.2.2 }\n"
        "@dx.typevar.8 = external addrspace(1) constant %\"class.RWStructuredBuffer<matrix<float, 2, 2> >\"\n"
        "@\"internalGV\" = internal global [64 x <4 x float>] undef\n",
        "call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)\n"
        "%load = load %\"class.RWStructuredBuffer<matrix<float, 2, 2> >\", %\"class.RWStructuredBuffer<matrix<float, 2, 2> >\" addrspace(1)* @dx.typevar.8",
      },
      {"Internal declaration 'internalGV' is unused",
       "External declaration 'dx.typevar.8' is unused",
       "Vector type '<4 x float>' is not allowed",
       "Mode of Barrier must be an immediate constant",
       "sync must include some form of memory barrier - _u (UAV) and/or _g (Thread Group Shared Memory)",
       "sync can't specify both _ugroup and _uglobal. If both are needed, just specify _uglobal"
      });
}
TEST_F(ValidationTest, CBufferLegacyOutOfBoundFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\cbuffer1.50.hlsl", "ps_6_0",
      "cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %Foo2_cbuffer, i32 0)",
      "cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %Foo2_cbuffer, i32 6)",
      "Cbuffer access out of bound");
}

TEST_F(ValidationTest, CsThreadSizeFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\share_mem1.hlsl", "cs_6_0",
      {"!{i32 8, i32 8, i32 1",
       "[256 x float]"},
      {"!{i32 1025, i32 1025, i32 1025",
       "[64000000 x float]"},
      {"Declared Thread Group X size 1025 outside valid range",
       "Declared Thread Group Y size 1025 outside valid range",
       "Declared Thread Group Z size 1025 outside valid range",
       "Declared Thread Group Count 1076890625 (X*Y*Z) is beyond the valid maximum",
       "Total Thread Group Shared Memory storage is 256000000, exceeded 32768",
      });
}
TEST_F(ValidationTest, DeadLoopFail) {
  if (m_ver.SkipIRSensitiveTest()) return;
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\loop1.hlsl", "ps_6_0",
      {"br i1 %exitcond, label %for.end.loopexit, label %for.body, !llvm.loop !([0-9]+)",
       "?%add(\\.lcssa)? = phi float \\[ %add, %for.body \\]",
       "!dx.entryPoints = !\\{!([0-9]+)\\}",
       "\\[ %add(\\.lcssa)?, %for.end.loopexit \\]"
      },
      {"br label %for.body",
       "",
       "!dx.entryPoints = !\\{!\\1\\}\n!dx.unused = !\\{!\\1\\}",
       "[ 0.000000e+00, %for.end.loopexit ]"
      },
      {"Loop must have break",
       "Named metadata 'dx.unused' is unknown",
      },
      /*bRegex*/true);
}
TEST_F(ValidationTest, EvalFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\eval.hlsl", "ps_6_0",
      "!\"A\", i8 9, i8 0, !([0-9]+), i8 2, i32 1, i8 4",
      "!\"A\", i8 9, i8 0, !\\1, i8 0, i32 1, i8 4",
      "Interpolation mode on A used with eval_\\* instruction must be ",
      /*bRegex*/true);
}
TEST_F(ValidationTest, GetDimCalcLODFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\GetDimCalcLOD.hlsl", "ps_6_0",
      {"extractvalue %dx.types.Dimensions %([0-9]+), 1",
       "float 1.000000e\\+00, i1 true"
      },
      {"extractvalue %dx.types.Dimensions %\\1, 2",
       "float undef, i1 true"
      },
      {"GetDimensions used undef dimension z on TextureCube",
       "coord uninitialized"},
      /*bRegex*/true);
}
TEST_F(ValidationTest, HsAttributeFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\hsAttribute.hlsl", "hs_6_0",
      {"i32 3, i32 3, i32 2, i32 3, i32 3, float 6.400000e+01"
      },
      {"i32 36, i32 36, i32 0, i32 0, i32 0, float 6.500000e+01"
      },
      {"HS input control point count must be [0..32].  36 specified",
       "Invalid Tessellator Domain specified. Must be isoline, tri or quad",
       "Invalid Tessellator Partitioning specified",
       "Invalid Tessellator Output Primitive specified",
       "Hull Shader MaxTessFactor must be [1.000000..64.000000].  65.000000 specified",
       "output control point count must be [0..32].  36 specified"});
}
TEST_F(ValidationTest, InnerCoverageFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\InnerCoverage2.hlsl", "ps_6_0",
      {"dx.op.coverage.i32(i32 91)",
       "declare i32 @dx.op.coverage.i32(i32)"
      },
      {"dx.op.coverage.i32(i32 91)\n  %inner = call i32 @dx.op.innerCoverage.i32(i32 92)",
       "declare i32 @dx.op.coverage.i32(i32)\n"
       "declare i32 @dx.op.innerCoverage.i32(i32)"
      },
      "InnerCoverage and Coverage are mutually exclusive.");
}
TEST_F(ValidationTest, InterpChangeFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\interpChange.hlsl", "ps_6_0",
      { "i32 1, i8 0, null}",
        "?!dx.viewIdState ="},
      { "i32 0, i8 2, null}",
        "!1012 ="},
      "interpolation mode that differs from another element packed",
      /*bRegex*/true);
}
TEST_F(ValidationTest, InterpOnIntFail) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\interpOnInt2.hlsl", "ps_6_0",
      "!\"A\", i8 5, i8 0, !([0-9]+), i8 1",
      "!\"A\", i8 5, i8 0, !\\1, i8 2",
      "signature element A specifies invalid interpolation mode for integer component type",
      /*bRegex*/true);
}
TEST_F(ValidationTest, InvalidSigCompTyFail) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\abs2.hlsl", "ps_6_0",
      "!\"A\", i8 4",
      "!\"A\", i8 0",
      "A specifies unrecognized or invalid component type");
}
TEST_F(ValidationTest, MultiStream2Fail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\multiStreamGS.hlsl", "gs_6_0",
      "i32 1, i32 12, i32 7, i32 1, i32 1",
      "i32 1, i32 12, i32 7, i32 2, i32 1",
      "Multiple GS output streams are used but 'XXX' is not pointlist");
}
TEST_F(ValidationTest, PhiTGSMFail) {
  if (m_ver.SkipIRSensitiveTest()) return;
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\phiTGSM.hlsl", "cs_6_0",
      "ret void",
      "%arrayPhi = phi i32 addrspace(3)* [ %arrayidx, %if.then ], [ %arrayidx2, %if.else ]\n"
      "%phiAtom = atomicrmw add i32 addrspace(3)* %arrayPhi, i32 1 seq_cst\n"
      "ret void",
      "TGSM pointers must originate from an unambiguous TGSM global variable");
}

TEST_F(ValidationTest, QuadOpInCS) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
      "struct PerThreadData { int "
      "input; int output; }; RWStructuredBuffer<PerThreadData> g_sb; "
      "[numthreads(8, 12, 1)] void main(uint GI : SV_GroupIndex) "
      "{ PerThreadData pts = g_sb[GI]; pts.output = "
      "WaveActiveSum(pts.input); g_sb[GI] = pts; }; ",
      "cs_6_0", {"@dx.op.waveActiveOp.i32(i32 119", "declare i32 @dx.op.waveActiveOp.i32(i32, i32, i8, i8)"},
      {"@dx.op.quadOp.i32(i32 123", "declare i32 @dx.op.quadOp.i32(i32, i32, i8, i8)"},
      "'QuadReadAcross' should only be used in 'Pixel Shader'"
      );
}

TEST_F(ValidationTest, ReducibleFail) {
  if (m_ver.SkipIRSensitiveTest()) return;
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\reducible.hlsl", "ps_6_0",
      {"%conv\n"
       "  br label %if.end",
       "to float\n"
       "  br label %if.end"
      },
      {"%conv\n"
      "  br i1 %cmp, label %if.else, label %if.end",
       "to float\n"
       "  br i1 %cmp, label %if.then, label %if.end"
      },
      "Execution flow must be reducible");
}
TEST_F(ValidationTest, SampleBiasFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\sampleBias.hlsl", "ps_6_0",
      {"float -1.600000e+01"
      },
      {"float 1.800000e+01"
      },
      "bias amount for sample_b must be in the range [-16.000000,15.990000]");
}
TEST_F(ValidationTest, SamplerKindFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SamplerKind.hlsl", "ps_6_0",
      {"uav1_UAV_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1",
       "g_txDiffuse_texture_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0",
       "\"g_samLinear\", i32 0, i32 0, i32 1, i32 0",
       "\"g_samLinearC\", i32 0, i32 1, i32 1, i32 1",
      },
      {"uav1_UAV_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0",
       "g_txDiffuse_texture_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1",
       "\"g_samLinear\", i32 0, i32 0, i32 1, i32 3",
       "\"g_samLinearC\", i32 0, i32 1, i32 1, i32 3",
      },
      {"Invalid sampler mode",
       "require sampler declared in comparison mode",
       "requires sampler declared in default mode",
       "should on srv resource"});
}
TEST_F(ValidationTest, SemaOverlapFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\semaOverlap1.hlsl", "ps_6_0",
      {"!([0-9]+) = !\\{i32 0, !\"A\", i8 9, i8 0, !([0-9]+), i8 2, i32 1, i8 4, i32 0, i8 0, null\\}\n"
      "!([0-9]+) = !\\{i32 0\\}\n"
      "!([0-9]+) = !\\{i32 1, !\"A\", i8 9, i8 0, !([0-9]+)",
      },
      {"!\\1 = !\\{i32 0, !\"A\", i8 9, i8 0, !\\2, i8 2, i32 1, i8 4, i32 0, i8 0, null\\}\n"
      "!\\3 = !\\{i32 0\\}\n"
      "!\\4 = !\\{i32 1, !\"A\", i8 9, i8 0, !\\2",
      },
      {"Semantic 'A' overlap at 0"},
      /*bRegex*/true);
}
TEST_F(ValidationTest, SigOutOfRangeFail) {
  return;   // Skip for now since this fails AssembleToContainer in PSV creation due to out of range start row
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\semaOverlap1.hlsl", "ps_6_0",
      {"i32 1, i8 0, null}",
      },
      {"i32 8000, i8 0, null}",
      },
      {"signature element A at location (8000,0) size (1,4) is out of range"});
}
TEST_F(ValidationTest, SigOverlapFail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\semaOverlap1.hlsl", "ps_6_0",
      { "i32 1, i8 0, null}",
        "?!dx.viewIdState =" },
      { "i32 0, i8 0, null}",
        "!1012 =" },
      {"signature element A at location (0,0) size (1,4) overlaps another signature element"});
}
TEST_F(ValidationTest, SimpleHs1Fail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleHs1.hlsl", "hs_6_0",
      {"i32 3, i32 3, i32 2, i32 3, i32 3, float 6.400000e+01}",
       "\"SV_TessFactor\", i8 9, i8 25",
       "\"SV_InsideTessFactor\", i8 9, i8 26",
      },
      {"i32 3, i32 3000, i32 2, i32 3, i32 3, float 6.400000e+01}",
       "\"TessFactor\", i8 9, i8 0",
       "\"InsideTessFactor\", i8 9, i8 0",
      },
      {"output control point count must be [0..32].  3000 specified",
       "Required TessFactor for domain not found declared anywhere in Patch Constant data",
       // TODO: enable this after support pass thru hull shader.
       //"For pass thru hull shader, input control point count must match output control point count",
       //"Total number of scalars across all HS output control points must not exceed",
      });
}
TEST_F(ValidationTest, SimpleHs3Fail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleHs3.hlsl", "hs_6_0",
      {
          "i32 3, i32 3, i32 2, i32 3, i32 3, float 6.400000e+01}",
      },
      {
          "i32 3, i32 3, i32 2, i32 3, i32 2, float 6.400000e+01}",
      },
      {"Hull Shader declared with Tri Domain must specify output primitive "
       "point, triangle_cw or triangle_ccw. Line output is not compatible with "
       "the Tri domain"});
}
TEST_F(ValidationTest, SimpleHs4Fail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleHs4.hlsl", "hs_6_0",
      {
          "i32 2, i32 2, i32 1, i32 3, i32 2, float 6.400000e+01}",
      },
      {
          "i32 2, i32 2, i32 1, i32 3, i32 3, float 6.400000e+01}",
      },
      {"Hull Shader declared with IsoLine Domain must specify output primitive "
       "point or line. Triangle_cw or triangle_ccw output are not compatible "
       "with the IsoLine Domain"});
}
TEST_F(ValidationTest, SimpleDs1Fail) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleDs1.hlsl", "ds_6_0",
      {"!{i32 2, i32 3}"
      },
      {"!{i32 4, i32 36}"
      },
      {"DS input control point count must be [0..32].  36 specified",
       "Invalid Tessellator Domain specified. Must be isoline, tri or quad",
       "DomainLocation component index out of bounds for the domain"});
}
TEST_F(ValidationTest, SimpleGs1Fail) {
  return;   // Skip for now since this fails AssembleToContainer in PSV creation due to out of range stream index
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleGS1.hlsl", "gs_6_0",
      {"!{i32 1, i32 3, i32 1, i32 5, i32 1}",
       "i8 4, i32 1, i8 4, i32 2, i8 0, null}"
      },
      {"!{i32 5, i32 1025, i32 1, i32 0, i32 33}",
      "i8 4, i32 1, i8 4, i32 2, i8 0, !100}\n"
      "!100 = !{i32 0, i32 5}"
      },
      {"GS output vertex count must be [0..1024].  1025 specified",
       "GS instance count must be [1..32].  33 specified",
       "GS output primitive topology unrecognized",
       "GS input primitive unrecognized",
       "Stream index (5) must between 0 and 3"});
}
TEST_F(ValidationTest, UavBarrierFail) {
  if (m_ver.SkipIRSensitiveTest()) return;
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\uavBarrier.hlsl", "ps_6_0",
      {"dx.op.barrier(i32 80, i32 2)",
       "textureLoad.f32(i32 66, %dx.types.Handle %uav1_UAV_2d, i32 undef",
       "i32 undef, i32 undef, i32 undef, i32 undef)",
       "float %add9.i3, i8 15)",
      },
      {"dx.op.barrier(i32 80, i32 9)",
       "textureLoad.f32(i32 66, %dx.types.Handle %uav1_UAV_2d, i32 1",
       "i32 1, i32 2, i32 undef, i32 undef)",
       "float undef, i8 7)",
      },
      {"uav load don't support offset",
       "uav load don't support mipLevel/sampleIndex",
       "store on typed uav must write to all four components of the UAV",
       "sync in a non-Compute Shader must only sync UAV (sync_uglobal)"});
}
TEST_F(ValidationTest, UndefValueFail) {
  TestCheck(L"..\\CodeGenHLSL\\UndefValue.hlsl");
}
TEST_F(ValidationTest, UpdateCounterFail) {
  if (m_ver.SkipIRSensitiveTest()) return;
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\updateCounter2.hlsl", "ps_6_0",
      {"%2 = call i32 @dx.op.bufferUpdateCounter(i32 70, %dx.types.Handle %buf2_UAV_structbuf, i8 1)",
       "%3 = call i32 @dx.op.bufferUpdateCounter(i32 70, %dx.types.Handle %buf2_UAV_structbuf, i8 1)"
      },
      {"%2 = call i32 @dx.op.bufferUpdateCounter(i32 70, %dx.types.Handle %buf2_UAV_structbuf, i8 -1)",
       "%3 = call i32 @dx.op.bufferUpdateCounter(i32 70, %dx.types.Handle %buf2_UAV_structbuf, i8 1)\n"
       "%srvUpdate = call i32 @dx.op.bufferUpdateCounter(i32 70, %dx.types.Handle %buf1_texture_buf, i8 undef)"
      },
      {"BufferUpdateCounter valid only on UAV",
       "BufferUpdateCounter valid only on structured buffers",
       "inc of BufferUpdateCounter must be an immediate constant",
       "RWStructuredBuffers may increment or decrement their counters, but not both"});
}

TEST_F(ValidationTest, LocalResCopy) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\resCopy.hlsl", "cs_6_0", {"ret void"},
      {"%H = alloca %dx.types.ResRet.i32\n"
       "ret void"},
      {"Dxil struct types should only used by ExtractValue"});
}

TEST_F(ValidationTest, WhenIncorrectModelThenFail) {
  TestCheck(L"val-failures.hlsl");
}

TEST_F(ValidationTest, WhenIncorrectPSThenFail) {
  TestCheck(L"val-failures-ps.hlsl");
}

TEST_F(ValidationTest, WhenSmUnknownThenFail) {
  RewriteAssemblyCheckMsg("float4 main() : SV_Target { return 1; }", "ps_6_0",
                          {"{!\"ps\", i32 6, i32 0}"},
                          {"{!\"ps\", i32 1, i32 2}"},
                          "Unknown shader model 'ps_1_2'");
}

TEST_F(ValidationTest, WhenSmLegacyThenFail) {
  RewriteAssemblyCheckMsg("float4 main() : SV_Target { return 1; }", "ps_6_0",
                          "{!\"ps\", i32 6, i32 0}", "{!\"ps\", i32 5, i32 1}",
                          "Unknown shader model 'ps_5_1'");
}

TEST_F(ValidationTest, WhenMetaFlagsUsageDeclThenOK) {
  RewriteAssemblyCheckMsg(
    "uint u; float4 main() : SV_Target { uint64_t n = u; n *= u; return (uint)(n >> 32); }", "ps_6_0",
    "1048576", "1048577", // inhibit optimization, which should work fine
    nullptr);
}

TEST_F(ValidationTest, GsVertexIDOutOfBound) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleGS1.hlsl", "gs_6_0",
      "dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 0)",
      "dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 1)", 
      "expect VertexID between 0~1, got 1");
}

TEST_F(ValidationTest, StreamIDOutOfBound) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleGS1.hlsl", "gs_6_0",
      "dx.op.emitStream(i32 97, i8 0)",
      "dx.op.emitStream(i32 97, i8 1)", 
      "expect StreamID between 0 , got 1");
}

TEST_F(ValidationTest, SignatureDataWidth) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  std::vector<LPCWSTR> pArguments = { L"-enable-16bit-types", L"-HV", L"2018" };
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\signature_packing_by_width.hlsl", "ps_6_2",
      pArguments.data(), 3, nullptr, 0,
      {"i8 8, i8 0, (![0-9]+), i8 2, i32 1, i8 2, i32 0, i8 0, null}"},
      {"i8 9, i8 0, \\1, i8 2, i32 1, i8 2, i32 0, i8 0, null}"},
      "signature element F at location \\(0, 2\\) size \\(1, 2\\) has data "
      "width that differs from another element packed into the same row.",
      true);
}

TEST_F(ValidationTest, SignatureStreamIDForNonGS) {
  RewriteAssemblyCheckMsg(
    L"..\\CodeGenHLSL\\abs1.hlsl", "ps_6_0",
    { ", i8 0, i32 1, i8 4, i32 0, i8 0, null}",
      "?!dx.viewIdState ="},
    { ", i8 0, i32 1, i8 4, i32 0, i8 0, !19}\n!19 = !{i32 0, i32 1}",
      "!1012 =" },
    "Stream index (1) must between 0 and 0");
}

TEST_F(ValidationTest, TypedUAVStoreFullMask0) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\uav_typed_store.hlsl", "ps_6_0",
      "float 2.000000e+00, i8 15)",
      "float 2.000000e+00, i8 undef)",
      "Mask of TextureStore must be an immediate constant");
}

TEST_F(ValidationTest, TypedUAVStoreFullMask1) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\uav_typed_store.hlsl", "ps_6_0",
      "float 3.000000e+00, i8 15)",
      "float 3.000000e+00, i8 undef)",
      "Mask of BufferStore must be an immediate constant");
}

TEST_F(ValidationTest, Recursive) {
  // Includes coverage for user-defined functions.
  TestCheck(L"..\\CodeGenHLSL\\recursive.ll");
}

TEST_F(ValidationTest, Recursive2) {
    TestCheck(L"..\\CodeGenHLSL\\recursive2.hlsl");
}

TEST_F(ValidationTest, Recursive3) {
    TestCheck(L"..\\CodeGenHLSL\\recursive3.hlsl");
}

TEST_F(ValidationTest, ResourceRangeOverlap0) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\resource_overlap.hlsl", "ps_6_0",
      "!\"B\", i32 0, i32 1",
      "!\"B\", i32 0, i32 0",
      "Resource B with base 0 size 1 overlap");
}

TEST_F(ValidationTest, ResourceRangeOverlap1) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\resource_overlap.hlsl", "ps_6_0",
      "!\"s1\", i32 0, i32 1",
      "!\"s1\", i32 0, i32 0",
      "Resource s1 with base 0 size 1 overlap");
}

TEST_F(ValidationTest, ResourceRangeOverlap2) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\resource_overlap.hlsl", "ps_6_0",
      "!\"uav2\", i32 0, i32 0",
      "!\"uav2\", i32 0, i32 3",
      "Resource uav2 with base 3 size 1 overlap");
}

TEST_F(ValidationTest, ResourceRangeOverlap3) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\resource_overlap.hlsl", "ps_6_0",
      "!\"srv2\", i32 0, i32 1",
      "!\"srv2\", i32 0, i32 0",
      "Resource srv2 with base 0 size 1 overlap");
}

TEST_F(ValidationTest, CBufferOverlap0) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\cbufferOffset.hlsl", "ps_6_0",
      "i32 6, !\"g2\", i32 3, i32 0",
      "i32 6, !\"g2\", i32 3, i32 8",
      "CBuffer Foo1 has offset overlaps at 16");
}

TEST_F(ValidationTest, CBufferOverlap1) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\cbufferOffset.hlsl", "ps_6_0",
      " = !{i32 32, !",
      " = !{i32 16, !",
      "CBuffer Foo1 size insufficient for element at offset 16");
}

TEST_F(ValidationTest, ControlFlowHint) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\if1.hlsl", "ps_6_0",
      "!\"dx.controlflow.hints\", i32 1",
      "!\"dx.controlflow.hints\", i32 5",
      "Attribute forcecase only works for switch");
}

TEST_F(ValidationTest, ControlFlowHint1) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\if1.hlsl", "ps_6_0",
      "!\"dx.controlflow.hints\", i32 1",
      "!\"dx.controlflow.hints\", i32 1, i32 2",
      "Can't use branch and flatten attributes together");
}

TEST_F(ValidationTest, ControlFlowHint2) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\if1.hlsl", "ps_6_0",
      "!\"dx.controlflow.hints\", i32 1",
      "!\"dx.controlflow.hints\", i32 3",
      "Invalid control flow hint");
}

TEST_F(ValidationTest, SemanticLength1) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\binary1.hlsl", "ps_6_0",
      "!\"C\"",
      "!\"\"",
      "Semantic length must be at least 1 and at most 64");
}

TEST_F(ValidationTest, SemanticLength64) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\binary1.hlsl", "ps_6_0",
      "!\"C\"",
      "!\"CSESESESESESESESESESESESESESESESESESESESESESESESESESESESESESESESE\"",
      "Semantic length must be at least 1 and at most 64");
}

TEST_F(ValidationTest, PullModelPosition) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\eval.hlsl", "ps_6_0",
      "!\"A\", i8 9, i8 0",
      "!\"SV_Position\", i8 9, i8 3",
      "does not support pull-model evaluation of position");
}

TEST_F(ValidationTest, StructBufGlobalCoherentAndCounter) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\struct_buf1.hlsl", "ps_6_0",
      "!\"buf2\", i32 0, i32 0, i32 1, i32 12, i1 false, i1 false",
      "!\"buf2\", i32 0, i32 0, i32 1, i32 12, i1 true, i1 true",
      "globallycoherent cannot be used with append/consume buffers'buf2'");
}

TEST_F(ValidationTest, StructBufStrideAlign) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\struct_buf1.hlsl", "ps_6_0",
      "= !{i32 1, i32 52}",
      "= !{i32 1, i32 50}",
      "structured buffer element size must be a multiple of 4 bytes (actual size 50 bytes)");
}

TEST_F(ValidationTest, StructBufStrideOutOfBound) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\struct_buf1.hlsl", "ps_6_0",
      "= !{i32 1, i32 52}",
      "= !{i32 1, i32 2052}",
      "structured buffer elements cannot be larger than 2048 bytes (actual size 2052 bytes)");
}

TEST_F(ValidationTest, StructBufLoadCoordinates) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\struct_buf1.hlsl", "ps_6_0",
      "bufferLoad.f32(i32 68, %dx.types.Handle %buf1_texture_structbuf, i32 1, i32 8)",
      "bufferLoad.f32(i32 68, %dx.types.Handle %buf1_texture_structbuf, i32 1, i32 undef)",
      "structured buffer require 2 coordinates");
}

TEST_F(ValidationTest, StructBufStoreCoordinates) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\struct_buf1.hlsl", "ps_6_0",
      "bufferStore.f32(i32 69, %dx.types.Handle %buf2_UAV_structbuf, i32 0, i32 0",
      "bufferStore.f32(i32 69, %dx.types.Handle %buf2_UAV_structbuf, i32 0, i32 undef",
      "structured buffer require 2 coordinates");
}

TEST_F(ValidationTest, TypedBufRetType) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\sample5.hlsl", "ps_6_0",
      " = type { <4 x float>",
      " = type { <4 x double>",
      "elements of typed buffers and textures must fit in four 32-bit quantities");
}

TEST_F(ValidationTest, VsInputSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\clip_planes.hlsl", "vs_6_0",
      "!\"POSITION\", i8 9, i8 0",
      "!\"SV_Target\", i8 9, i8 16",
      "Semantic 'SV_Target' is invalid as vs Input");
}

TEST_F(ValidationTest, VsOutputSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\clip_planes.hlsl", "vs_6_0",
      "!\"NORMAL\", i8 9, i8 0",
      "!\"SV_Target\", i8 9, i8 16",
      "Semantic 'SV_Target' is invalid as vs Output");
}

TEST_F(ValidationTest, HsInputSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleHs1.hlsl", "hs_6_0",
      "!\"TEXCOORD\", i8 9, i8 0",
      "!\"VertexID\", i8 4, i8 1",
      "Semantic 'VertexID' is invalid as hs Input");
}

TEST_F(ValidationTest, HsOutputSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleHs1.hlsl", "hs_6_0",
      "!\"TEXCOORD\", i8 9, i8 0",
      "!\"VertexID\", i8 4, i8 1",
      "Semantic 'VertexID' is invalid as hs Output");
}

TEST_F(ValidationTest, PatchConstSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleHs1.hlsl", "hs_6_0",
      "!\"SV_TessFactor\", i8 9, i8 25",
      "!\"VertexID\", i8 4, i8 1",
      "Semantic 'VertexID' is invalid as hs PatchConstant");
}

TEST_F(ValidationTest, DsInputSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleDs1.hlsl", "ds_6_0",
      "!\"TEXCOORD\", i8 9, i8 0",
      "!\"VertexID\", i8 4, i8 1",
      "Semantic 'VertexID' is invalid as ds Input");
}

TEST_F(ValidationTest, DsOutputSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleDs1.hlsl", "ds_6_0",
      "!\"TEXCOORD\", i8 9, i8 0",
      "!\"VertexID\", i8 4, i8 1",
      "Semantic 'VertexID' is invalid as ds Output");
}

TEST_F(ValidationTest, GsInputSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleGS1.hlsl", "gs_6_0",
      "!\"POSSIZE\", i8 9, i8 0",
      "!\"VertexID\", i8 4, i8 1",
      "Semantic 'VertexID' is invalid as gs Input");
}

TEST_F(ValidationTest, GsOutputSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleGS1.hlsl", "gs_6_0",
      "!\"TEXCOORD\", i8 9, i8 0",
      "!\"VertexID\", i8 4, i8 1",
      "Semantic 'VertexID' is invalid as gs Output");
}

TEST_F(ValidationTest, PsInputSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\abs2.hlsl", "ps_6_0",
      "!\"A\", i8 4, i8 0",
      "!\"VertexID\", i8 4, i8 1",
      "Semantic 'VertexID' is invalid as ps Input");
}

TEST_F(ValidationTest, PsOutputSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\abs2.hlsl", "ps_6_0",
      "!\"SV_Target\", i8 9, i8 16",
      "!\"VertexID\", i8 4, i8 1",
      "Semantic 'VertexID' is invalid as ps Output");
}

TEST_F(ValidationTest, ArrayOfSVTarget) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\targetArray.hlsl", "ps_6_0",
      "i32 2, !\"SV_Target\", i8 9, i8 16, !([0-9]+), i8 0, i32 1, i8 4, i32 5, i8 0, null}",
      "i32 2, !\"SV_Target\", i8 9, i8 16, !101, i8 0, i32 2, i8 4, i32 5, i8 0, null}\n!101 = !{i32 5, i32 6}",
      "Pixel shader output registers are not indexable.",
      /*bRegex*/true);
}

TEST_F(ValidationTest, InfiniteLog) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "op.unary.f32\\(i32 23, float %[0-9+]\\)",
      "op.unary.f32(i32 23, float 0x7FF0000000000000)",
      "No indefinite logarithm",
      /*bRegex*/true);
}

TEST_F(ValidationTest, InfiniteAsin) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "op.unary.f32\\(i32 16, float %[0-9]+\\)",
      "op.unary.f32(i32 16, float 0x7FF0000000000000)",
      "No indefinite arcsine",
      /*bRegex*/true);
}

TEST_F(ValidationTest, InfiniteAcos) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "op.unary.f32\\(i32 15, float %[0-9]+\\)",
      "op.unary.f32(i32 15, float 0x7FF0000000000000)",
      "No indefinite arccosine",
      /*bRegex*/true);
}

TEST_F(ValidationTest, InfiniteDdxDdy) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "op.unary.f32\\(i32 85, float %[0-9]+\\)",
      "op.unary.f32(i32 85, float 0x7FF0000000000000)",
      "No indefinite derivative calculation",
      /*bRegex*/true);
}

TEST_F(ValidationTest, IDivByZero) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "sdiv i32 %([0-9]+), %[0-9]+",
      "sdiv i32 %\\1, 0",
      "No signed integer division by zero",
      /*bRegex*/true);
}

TEST_F(ValidationTest, UDivByZero) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "udiv i32 %([0-9]+), %[0-9]+",
      "udiv i32 %\\1, 0",
      "No unsigned integer division by zero",
      /*bRegex*/true);
}

TEST_F(ValidationTest, UnusedMetadata) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\loop2.hlsl", "ps_6_0",
                          ", !llvm.loop ",
                          ", !llvm.loop2 ",
                          "All metadata must be used by dxil");
}

TEST_F(ValidationTest, MemoryOutOfBound) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\targetArray.hlsl", "ps_6_0",
                          "getelementptr [4 x float], [4 x float]* %7, i32 0, i32 3",
                          "getelementptr [4 x float], [4 x float]* %7, i32 0, i32 10",
                          "Access to out-of-bounds memory is disallowed");
}

TEST_F(ValidationTest, LocalRes2) {
  TestCheck(L"..\\CodeGenHLSL\\local_resource2.hlsl");
}

TEST_F(ValidationTest, LocalRes3) {
  TestCheck(L"..\\CodeGenHLSL\\local_resource3.hlsl");
}

TEST_F(ValidationTest, LocalRes5) {
  TestCheck(L"..\\CodeGenHLSL\\local_resource5.hlsl");
}

TEST_F(ValidationTest, LocalRes5Dbg) {
  TestCheck(L"..\\CodeGenHLSL\\local_resource5_dbg.hlsl");
}

TEST_F(ValidationTest, LocalRes6) {
  TestCheck(L"..\\CodeGenHLSL\\local_resource6.hlsl");
}

TEST_F(ValidationTest, LocalRes6Dbg) {
  TestCheck(L"..\\CodeGenHLSL\\local_resource6_dbg.hlsl");
}

TEST_F(ValidationTest, AddrSpaceCast) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\staticGlobals.hlsl", "ps_6_0",
                          "%([0-9]+) = getelementptr \\[4 x i32\\], \\[4 x i32\\]\\* %([0-9]+), i32 0, i32 0\n"
                          "  store i32 %([0-9]+), i32\\* %\\1, align 4",
                          "%\\1 = getelementptr [4 x i32], [4 x i32]* %\\2, i32 0, i32 0\n"
                          "  %X = addrspacecast i32* %\\1 to i32 addrspace(1)*    \n"
                          "  store i32 %\\3, i32 addrspace(1)* %X, align 4",
                          "generic address space",
                          /*bRegex*/true);
}

TEST_F(ValidationTest, PtrBitCast) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\staticGlobals.hlsl", "ps_6_0",
                          "%([0-9]+) = getelementptr \\[4 x i32\\], \\[4 x i32\\]\\* %([0-9]+), i32 0, i32 0\n"
                          "  store i32 %([0-9]+), i32\\* %\\1, align 4",
                          "%\\1 = getelementptr [4 x i32], [4 x i32]* %\\2, i32 0, i32 0\n"
                          "  %X = bitcast i32* %\\1 to double*    \n"
                          "  store i32 %\\3, i32* %\\1, align 4",
                          "Pointer type bitcast must be have same size",
                          /*bRegex*/true);
}

TEST_F(ValidationTest, MinPrecisionBitCast) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\staticGlobals.hlsl", "ps_6_0",
                          "%([0-9]+) = getelementptr \\[4 x i32\\], \\[4 x i32\\]\\* %([0-9]+), i32 0, i32 0\n"
                          "  store i32 %([0-9]+), i32\\* %\\1, align 4",
                          "%\\1 = getelementptr [4 x i32], [4 x i32]* %\\2, i32 0, i32 0\n"
                          "  %X = bitcast i32* %\\1 to half* \n"
                          "  store i32 %\\3, i32* %\\1, align 4",
                          "Bitcast on minprecison types is not allowed",
                          /*bRegex*/true);
}

TEST_F(ValidationTest, StructBitCast) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\staticGlobals.hlsl", "ps_6_0",
                          "%([0-9]+) = getelementptr \\[4 x i32\\], \\[4 x i32\\]\\* %([0-9]+), i32 0, i32 0\n"
                          "  store i32 %([0-9]+), i32\\* %\\1, align 4",
                          "%\\1 = getelementptr [4 x i32], [4 x i32]* %\\2, i32 0, i32 0\n"
                          "  %X = bitcast i32* %\\1 to %dx.types.Handle*    \n"
                          "  store i32 %\\3, i32* %\\1, align 4",
                          "Bitcast on struct types is not allowed",
                          /*bRegex*/true);
}

TEST_F(ValidationTest, MultiDimArray) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\staticGlobals.hlsl", "ps_6_0",
                          "= alloca [4 x i32]",
                          "= alloca [4 x i32]\n"
                          "  %md = alloca [2 x [4 x float]]",
                          "Only one dimension allowed for array type");
}

TEST_F(ValidationTest, SimpleGs8) {
  TestCheck(L"..\\CodeGenHLSL\\SimpleGS8.hlsl");
}

TEST_F(ValidationTest, SimpleGs9) {
  TestCheck(L"..\\CodeGenHLSL\\SimpleGS9.hlsl");
}

TEST_F(ValidationTest, SimpleGs10) {
  TestCheck(L"..\\CodeGenHLSL\\SimpleGS10.hlsl");
}

TEST_F(ValidationTest, IllegalSampleOffset3) {
  TestCheck(L"..\\CodeGenHLSL\\optForNoOpt3.hlsl");
}

TEST_F(ValidationTest, IllegalSampleOffset4) {
  TestCheck(L"..\\CodeGenHLSL\\optForNoOpt4.hlsl");
}

TEST_F(ValidationTest, NoFunctionParam) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\abs2.hlsl", "ps_6_0",
    {"define void @main\\(\\)",               "void \\(\\)\\* @main, !([0-9]+)\\}(.*)!\\1 = !\\{!([0-9]+)\\}",  "void \\(\\)\\* @main"},
    {"define void @main(<4 x i32> %mainArg)", "void (<4 x i32>)* @main, !\\1}\\2!\\1 = !{!\\3, !\\3}",          "void (<4 x i32>)* @main"},
    "with parameter is not permitted",
    /*bRegex*/true);
}

TEST_F(ValidationTest, I8Type) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\staticGlobals.hlsl", "ps_6_0",
                          "%([0-9]+) = alloca \\[4 x i32\\]",
                          "%\\1 = alloca [4 x i32]\n"
                          "  %m8 = alloca i8",
                          "I8 can only used as immediate value for intrinsic",
    /*bRegex*/true);
}

TEST_F(ValidationTest, EmptyStructInBuffer) {
  TestCheck(L"..\\CodeGenHLSL\\EmptyStructInBuffer.hlsl");
}

TEST_F(ValidationTest, BigStructInBuffer) {
  TestCheck(L"..\\CodeGenHLSL\\BigStructInBuffer.hlsl");
}

TEST_F(ValidationTest, GloballyCoherent2) {
  TestCheck(L"..\\CodeGenHLSL\\globallycoherent2.hlsl");
}

TEST_F(ValidationTest, GloballyCoherent3) {
  TestCheck(L"..\\CodeGenHLSL\\globallycoherent3.hlsl");
}

// TODO: enable this.
//TEST_F(ValidationTest, TGSMRaceCond) {
//  TestCheck(L"..\\CodeGenHLSL\\RaceCond.hlsl");
//}
//
//TEST_F(ValidationTest, TGSMRaceCond2) {
//    RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\structInBuffer.hlsl", "cs_6_0",
//        "ret void",
//        "%TID = call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)\n"
//        "store i32 %TID, i32 addrspace(3)* @\"\\01?sharedData@@3UFoo@@A.3\", align 4\n"
//        "ret void",
//        "Race condition writing to shared memory detected, consider making this write conditional");
//}

TEST_F(ValidationTest, AddUint64Odd) {
  TestCheck(L"..\\CodeGenHLSL\\AddUint64Odd.hlsl");
}

TEST_F(ValidationTest, WhenWaveAffectsGradientThenFail) {
  TestCheck(L"val-wave-failures-ps.hlsl");
}

TEST_F(ValidationTest, WhenMetaFlagsUsageThenFail) {
  RewriteAssemblyCheckMsg(
    "uint u; float4 main() : SV_Target { uint64_t n = u; n *= u; return (uint)(n >> 32); }", "ps_6_0",
    "1048576", "0", // remove the int64 flag
    "Flags must match usage");
}

TEST_F(ValidationTest, StorePatchControlNotInPatchConstantFunction) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
      "struct PSSceneIn \
    { \
    float4 pos  : SV_Position; \
    float2 tex  : TEXCOORD0; \
    float3 norm : NORMAL; \
    }; \
       \
    struct HSPerVertexData  \
    { \
    PSSceneIn v; \
    }; \
    struct HSPerPatchData  \
{  \
	float	edges[ 3 ]	: SV_TessFactor; \
	float	inside		: SV_InsideTessFactor; \
};  \
HSPerPatchData HSPerPatchFunc( const InputPatch< PSSceneIn, 3 > points, \
     OutputPatch<HSPerVertexData, 3> outpoints) \
{ \
    HSPerPatchData d; \
     \
    d.edges[ 0 ] = points[0].tex.x + outpoints[0].v.tex.x; \
    d.edges[ 1 ] = 1; \
    d.edges[ 2 ] = 1; \
    d.inside = 1; \
    \
    return d; \
}\
[domain(\"tri\")]\
[partitioning(\"fractional_odd\")]\
[outputtopology(\"triangle_cw\")]\
[patchconstantfunc(\"HSPerPatchFunc\")]\
[outputcontrolpoints(3)]\
HSPerVertexData main( const uint id : SV_OutputControlPointID,\
                               const InputPatch< PSSceneIn, 3 > points )\
{\
    HSPerVertexData v;\
    \
    v.v = points[ id ];\
    \
	return v;\
}\
    ",
      "hs_6_0", 
      "dx.op.storeOutput.f32(i32 5",
      "dx.op.storePatchConstant.f32(i32 106",
      "opcode 'StorePatchConstant' should only be used in 'PatchConstant function'");
}

TEST_F(ValidationTest, LoadOutputControlPointNotInPatchConstantFunction) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
      "struct PSSceneIn \
    { \
    float4 pos  : SV_Position; \
    float2 tex  : TEXCOORD0; \
    float3 norm : NORMAL; \
    }; \
       \
    struct HSPerVertexData  \
    { \
    PSSceneIn v; \
    }; \
    struct HSPerPatchData  \
{  \
	float	edges[ 3 ]	: SV_TessFactor; \
	float	inside		: SV_InsideTessFactor; \
};  \
HSPerPatchData HSPerPatchFunc( const InputPatch< PSSceneIn, 3 > points, \
     OutputPatch<HSPerVertexData, 3> outpoints) \
{ \
    HSPerPatchData d; \
     \
    d.edges[ 0 ] = points[0].tex.x + outpoints[0].v.tex.x; \
    d.edges[ 1 ] = 1; \
    d.edges[ 2 ] = 1; \
    d.inside = 1; \
    \
    return d; \
}\
[domain(\"tri\")]\
[partitioning(\"fractional_odd\")]\
[outputtopology(\"triangle_cw\")]\
[patchconstantfunc(\"HSPerPatchFunc\")]\
[outputcontrolpoints(3)]\
HSPerVertexData main( const uint id : SV_OutputControlPointID,\
                               const InputPatch< PSSceneIn, 3 > points )\
{\
    HSPerVertexData v;\
    \
    v.v = points[ id ];\
    \
	return v;\
}\
    ",
      "hs_6_0",
      "dx.op.loadInput.f32(i32 4",
      "dx.op.loadOutputControlPoint.f32(i32 103",
      "opcode 'LoadOutputControlPoint' should only be used in 'PatchConstant function'");
}

TEST_F(ValidationTest, OutputControlPointIDInPatchConstantFunction) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
      "struct PSSceneIn \
    { \
    float4 pos  : SV_Position; \
    float2 tex  : TEXCOORD0; \
    float3 norm : NORMAL; \
    }; \
       \
    struct HSPerVertexData  \
    { \
    PSSceneIn v; \
    }; \
    struct HSPerPatchData  \
{  \
	float	edges[ 3 ]	: SV_TessFactor; \
	float	inside		: SV_InsideTessFactor; \
};  \
HSPerPatchData HSPerPatchFunc( const InputPatch< PSSceneIn, 3 > points, \
     OutputPatch<HSPerVertexData, 3> outpoints) \
{ \
    HSPerPatchData d; \
     \
    d.edges[ 0 ] = points[0].tex.x + outpoints[0].v.tex.x; \
    d.edges[ 1 ] = 1; \
    d.edges[ 2 ] = 1; \
    d.inside = 1; \
    \
    return d; \
}\
[domain(\"tri\")]\
[partitioning(\"fractional_odd\")]\
[outputtopology(\"triangle_cw\")]\
[patchconstantfunc(\"HSPerPatchFunc\")]\
[outputcontrolpoints(3)]\
HSPerVertexData main( const uint id : SV_OutputControlPointID,\
                               const InputPatch< PSSceneIn, 3 > points )\
{\
    HSPerVertexData v;\
    \
    v.v = points[ id ];\
    \
	return v;\
}\
    ",
      "hs_6_0",
      "ret void",
      "call i32 @dx.op.outputControlPointID.i32(i32 107)\n ret void",
      "opcode 'OutputControlPointID' should only be used in 'hull function'");
}

TEST_F(ValidationTest, ClipCullMaxComponents) {
  RewriteAssemblyCheckMsg(" \
struct VSOut { \
  float3 clip0 : SV_ClipDistance; \
  float3 clip1 : SV_ClipDistance1; \
  float cull0 : SV_CullDistance; \
  float cull1 : SV_CullDistance1; \
  float cull2 : CullDistance2; \
}; \
VSOut main() { \
  VSOut Out; \
  Out.clip0 = 0.1; \
  Out.clip1 = 0.2; \
  Out.cull0 = 0.3; \
  Out.cull1 = 0.4; \
  Out.cull2 = 0.5; \
  return Out; \
} \
    ",
    "vs_6_0", 
    "!{i32 4, !\"CullDistance\", i8 9, i8 0,",
    "!{i32 4, !\"SV_CullDistance\", i8 9, i8 7,",
    "ClipDistance and CullDistance use more than the maximum of 8 components combined.");
}

TEST_F(ValidationTest, ClipCullMaxRows) {
  RewriteAssemblyCheckMsg(" \
struct VSOut { \
  float3 clip0 : SV_ClipDistance; \
  float3 clip1 : SV_ClipDistance1; \
  float2 cull0 : CullDistance; \
}; \
VSOut main() { \
  VSOut Out; \
  Out.clip0 = 0.1; \
  Out.clip1 = 0.2; \
  Out.cull0 = 0.3; \
  return Out; \
} \
    ",
    "vs_6_0", 
    "!{i32 2, !\"CullDistance\", i8 9, i8 0,",
    "!{i32 2, !\"SV_CullDistance\", i8 9, i8 7,",
    "ClipDistance and CullDistance occupy more than the maximum of 2 rows combined.");
}

TEST_F(ValidationTest, DuplicateSysValue) {
  RewriteAssemblyCheckMsg(" \
float4 main(uint vid : SV_VertexID, uint iid : SV_InstanceID) : SV_Position { \
  return (float4)0 + vid + iid; \
} \
    ",
    "vs_6_0", 
    "!{i32 1, !\"SV_InstanceID\", i8 5, i8 2,",
    "!{i32 1, !\"\", i8 5, i8 1,",
    //"System value SV_VertexID appears more than once in the same signature.");
    "Semantic 'SV_VertexID' overlap at 0");
}

TEST_F(ValidationTest, SemTargetMax) {
  RewriteAssemblyCheckMsg(" \
float4 main(float4 col : COLOR) : SV_Target7 { return col; } \
    ",
    "ps_6_0", 
    { "!{i32 0, !\"SV_Target\", i8 9, i8 16, ![0-9]+, i8 0, i32 1, i8 4, i32 7, i8 0, null}",
      "?!dx.viewIdState ="},
    { "!{i32 0, !\"SV_Target\", i8 9, i8 16, !101, i8 0, i32 1, i8 4, i32 8, i8 0, null}\n!101 = !{i32 8}",
      "!1012 ="},
    "SV_Target semantic index exceeds maximum \\(7\\)",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemTargetIndexMatchesRow) {
  RewriteAssemblyCheckMsg(" \
float4 main(float4 col : COLOR) : SV_Target7 { return col; } \
    ",
    "ps_6_0", 
    { "!{i32 0, !\"SV_Target\", i8 9, i8 16, !([0-9]+), i8 0, i32 1, i8 4, i32 7, i8 0, null}",
      "?!dx.viewIdState ="},
    { "!{i32 0, !\"SV_Target\", i8 9, i8 16, !\\1, i8 0, i32 1, i8 4, i32 6, i8 0, null}",
      "!1012 ="},
    "SV_Target semantic index must match packed row location",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemTargetCol0) {
  RewriteAssemblyCheckMsg(" \
float3 main(float4 col : COLOR) : SV_Target7 { return col.xyz; } \
    ",
    "ps_6_0", 
    "!{i32 0, !\"SV_Target\", i8 9, i8 16, !([0-9]+), i8 0, i32 1, i8 3, i32 7, i8 0, null}",
    "!{i32 0, !\"SV_Target\", i8 9, i8 16, !\\1, i8 0, i32 1, i8 3, i32 7, i8 1, null}",
    "SV_Target packed location must start at column 0",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemIndexMax) {
  RewriteAssemblyCheckMsg(" \
float4 main(uint vid : SV_VertexID, uint iid : SV_InstanceID) : SV_Position { \
  return (float4)0 + vid + iid; \
} \
    ",
    "vs_6_0", 
    "!{i32 0, !\"SV_VertexID\", i8 5, i8 1, ![0-9]+, i8 0, i32 1, i8 1, i32 0, i8 0, null}",
    "!{i32 0, !\"SV_VertexID\", i8 5, i8 1, !101, i8 0, i32 1, i8 1, i32 0, i8 0, null}\n!101 = !{i32 1}",
    "SV_VertexID semantic index exceeds maximum \\(0\\)",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemTessFactorIndexMax) {
  RewriteAssemblyCheckMsg(" \
struct Vertex { \
  float4 pos : SV_Position; \
}; \
struct PatchConstant { \
  float edges[ 3 ]  : SV_TessFactor; \
  float inside    : SV_InsideTessFactor; \
}; \
PatchConstant PCMain( InputPatch<Vertex, 3> patch) { \
  PatchConstant PC; \
  PC.edges = (float[3])patch[1].pos.xyz; \
  PC.inside = patch[1].pos.w; \
  return PC; \
} \
[domain(\"tri\")] \
[partitioning(\"fractional_odd\")] \
[outputtopology(\"triangle_cw\")] \
[patchconstantfunc(\"PCMain\")] \
[outputcontrolpoints(3)] \
Vertex main(uint id : SV_OutputControlPointID, InputPatch< Vertex, 3 > patch) { \
  Vertex Out = patch[id]; \
  Out.pos.w += 0.25; \
  return Out; \
} \
    ",
    "hs_6_0",
    "!{i32 0, !\"SV_TessFactor\", i8 9, i8 25, ![0-9]+, i8 0, i32 3, i8 1, i32 0, i8 3, null}",
    "!{i32 0, !\"SV_TessFactor\", i8 9, i8 25, !101, i8 0, i32 2, i8 1, i32 0, i8 3, null}\n!101 = !{i32 0, i32 1}",
    "TessFactor rows, columns \\(2, 1\\) invalid for domain Tri.  Expected 3 rows and 1 column.",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemInsideTessFactorIndexMax) {
  RewriteAssemblyCheckMsg(" \
struct Vertex { \
  float4 pos : SV_Position; \
}; \
struct PatchConstant { \
  float edges[ 3 ]  : SV_TessFactor; \
  float inside    : SV_InsideTessFactor; \
}; \
PatchConstant PCMain( InputPatch<Vertex, 3> patch) { \
  PatchConstant PC; \
  PC.edges = (float[3])patch[1].pos.xyz; \
  PC.inside = patch[1].pos.w; \
  return PC; \
} \
[domain(\"tri\")] \
[partitioning(\"fractional_odd\")] \
[outputtopology(\"triangle_cw\")] \
[patchconstantfunc(\"PCMain\")] \
[outputcontrolpoints(3)] \
Vertex main(uint id : SV_OutputControlPointID, InputPatch< Vertex, 3 > patch) { \
  Vertex Out = patch[id]; \
  Out.pos.w += 0.25; \
  return Out; \
} \
    ",
    "hs_6_0",
    { "!{i32 1, !\"SV_InsideTessFactor\", i8 9, i8 26, !([0-9]+), i8 0, i32 1, i8 1, i32 3, i8 0, null}",
      "?!dx.viewIdState =" },
    { "!{i32 1, !\"SV_InsideTessFactor\", i8 9, i8 26, !101, i8 0, i32 2, i8 1, i32 3, i8 0, null}\n!101 = !{i32 0, i32 1}",
      "!1012 =" },
    "InsideTessFactor rows, columns \\(2, 1\\) invalid for domain Tri.  Expected 1 rows and 1 column.",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemShouldBeAllocated) {
  RewriteAssemblyCheckMsg(" \
struct Vertex { \
  float4 pos : SV_Position; \
}; \
struct PatchConstant { \
  float edges[ 3 ]  : SV_TessFactor; \
  float inside    : SV_InsideTessFactor; \
}; \
PatchConstant PCMain( InputPatch<Vertex, 3> patch) { \
  PatchConstant PC; \
  PC.edges = (float[3])patch[1].pos.xyz; \
  PC.inside = patch[1].pos.w; \
  return PC; \
} \
[domain(\"tri\")] \
[partitioning(\"fractional_odd\")] \
[outputtopology(\"triangle_cw\")] \
[patchconstantfunc(\"PCMain\")] \
[outputcontrolpoints(3)] \
Vertex main(uint id : SV_OutputControlPointID, InputPatch< Vertex, 3 > patch) { \
  Vertex Out = patch[id]; \
  Out.pos.w += 0.25; \
  return Out; \
} \
    ",
    "hs_6_0",
    "!{i32 0, !\"SV_TessFactor\", i8 9, i8 25, !([0-9]+), i8 0, i32 3, i8 1, i32 0, i8 3, null}",
    "!{i32 0, !\"SV_TessFactor\", i8 9, i8 25, !\\1, i8 0, i32 3, i8 1, i32 -1, i8 -1, null}",
    "PatchConstant Semantic 'SV_TessFactor' should have a valid packing location",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemShouldNotBeAllocated) {
  RewriteAssemblyCheckMsg(" \
float4 main(float4 col : COLOR, out uint coverage : SV_Coverage) : SV_Target7 { coverage = 7; return col; } \
    ",
    "ps_6_0",
    "!\"SV_Coverage\", i8 5, i8 14, !([0-9]+), i8 0, i32 1, i8 1, i32 -1, i8 -1, null}",
    "!\"SV_Coverage\", i8 5, i8 14, !\\1, i8 0, i32 1, i8 1, i32 2, i8 0, null}",
    "Output Semantic 'SV_Coverage' should have a packing location of -1",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemComponentOrder) {
  RewriteAssemblyCheckMsg(" \
void main( \
  float2 f2in : f2in, \
  float3 f3in : f3in, \
  uint vid : SV_VertexID, \
  uint iid : SV_InstanceID, \
  out float4 pos : SV_Position, \
  out float2 f2out : f2out, \
  out float3 f3out : f3out, \
  out float2 ClipDistance : SV_ClipDistance, \
  out float CullDistance : SV_CullDistance) \
{ \
  pos = float4(f3in, f2in.x); \
  ClipDistance = f2in.x; \
  CullDistance = f2in.y; \
} \
    ",
    "vs_6_0",

    { "= !{i32 1, !\"f2out\", i8 9, i8 0, !([0-9]+), i8 2, i32 1, i8 2, i32 1, i8 0, null}\n"
      "!([0-9]+) = !{i32 2, !\"f3out\", i8 9, i8 0, !([0-9]+), i8 2, i32 1, i8 3, i32 2, i8 0, null}\n"
      "!([0-9]+) = !{i32 3, !\"SV_ClipDistance\", i8 9, i8 6, !([0-9]+), i8 2, i32 1, i8 2, i32 3, i8 0, null}\n"
      "!([0-9]+) = !{i32 4, !\"SV_CullDistance\", i8 9, i8 7, !([0-9]+), i8 2, i32 1, i8 1, i32 3, i8 2, null}\n",
      "?!dx.viewIdState =" },

    { "= !{i32 1, !\"f2out\", i8 9, i8 0, !\\1, i8 2, i32 1, i8 2, i32 1, i8 2, null}\n"
      "!\\2 = !{i32 2, !\"f3out\", i8 9, i8 0, !\\3, i8 2, i32 1, i8 3, i32 2, i8 1, null}\n"
      "!\\4 = !{i32 3, !\"SV_ClipDistance\", i8 9, i8 6, !\\5, i8 2, i32 1, i8 2, i32 2, i8 0, null}\n"
      "!\\6 = !{i32 4, !\"SV_CullDistance\", i8 9, i8 7, !\\7, i8 2, i32 1, i8 1, i32 1, i8 0, null}\n",
      "!1012 =" },

    "signature element SV_ClipDistance at location \\(2,0\\) size \\(1,2\\) violates component ordering rule \\(arb < sv < sgv\\).\n"
    "signature element SV_CullDistance at location \\(1,0\\) size \\(1,1\\) violates component ordering rule \\(arb < sv < sgv\\).",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemComponentOrder2) {
  RewriteAssemblyCheckMsg(" \
float4 main( \
  float4 col : Color, \
  uint2 val : Value, \
  uint pid : SV_PrimitiveID, \
  bool ff : SV_IsFrontFace) : SV_Target \
{ \
  return col; \
} \
    ",
    "ps_6_0",

    "= !{i32 1, !\"Value\", i8 5, i8 0, !([0-9]+), i8 1, i32 1, i8 2, i32 1, i8 0, null}\n"
    "!([0-9]+) = !{i32 2, !\"SV_PrimitiveID\", i8 5, i8 10, !([0-9]+), i8 1, i32 1, i8 1, i32 1, i8 2, null}\n"
    "!([0-9]+) = !{i32 3, !\"SV_IsFrontFace\", i8 ([15]), i8 13, !([0-9]+), i8 1, i32 1, i8 1, i32 1, i8 3, null}\n",

    "= !{i32 1, !\"Value\", i8 5, i8 0, !\\1, i8 1, i32 1, i8 2, i32 1, i8 2, null}\n"
    "!\\2 = !{i32 2, !\"SV_PrimitiveID\", i8 5, i8 10, !\\3, i8 1, i32 1, i8 1, i32 1, i8 0, null}\n"
    "!\\4 = !{i32 3, !\"SV_IsFrontFace\", i8 \\5, i8 13, !\\6, i8 1, i32 1, i8 1, i32 1, i8 1, null}\n",

    "signature element SV_PrimitiveID at location \\(1,0\\) size \\(1,1\\) violates component ordering rule \\(arb < sv < sgv\\).\n"
    "signature element SV_IsFrontFace at location \\(1,1\\) size \\(1,1\\) violates component ordering rule \\(arb < sv < sgv\\).",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemComponentOrder3) {
  RewriteAssemblyCheckMsg(" \
float4 main( \
  float4 col : Color, \
  uint val : Value, \
  uint pid : SV_PrimitiveID, \
  bool ff : SV_IsFrontFace, \
  uint vpid : ViewPortArrayIndex) : SV_Target \
{ \
  return col; \
} \
    ",
    "ps_6_0",

    { "= !{i32 1, !\"Value\", i8 5, i8 0, !([0-9]+), i8 1, i32 1, i8 1, i32 1, i8 0, null}\n"
      "!([0-9]+) = !{i32 2, !\"SV_PrimitiveID\", i8 5, i8 10, !([0-9]+), i8 1, i32 1, i8 1, i32 1, i8 1, null}\n"
      "!([0-9]+) = !{i32 3, !\"SV_IsFrontFace\", i8 ([15]), i8 13, !([0-9]+), i8 1, i32 1, i8 1, i32 1, i8 2, null}\n"
      "!([0-9]+) = !{i32 4, !\"ViewPortArrayIndex\", i8 5, i8 0, !([0-9]+), i8 1, i32 1, i8 1, i32 2, i8 0, null}\n",
      "?!dx.viewIdState ="},

    { "= !{i32 1, !\"Value\", i8 5, i8 0, !\\1, i8 1, i32 1, i8 1, i32 1, i8 1, null}\n"
      "!\\2 = !{i32 2, !\"SV_PrimitiveID\", i8 5, i8 10, !\\3, i8 1, i32 1, i8 1, i32 1, i8 0, null}\n"
      "!\\4 = !{i32 3, !\"SV_IsFrontFace\", i8 \\5, i8 13, !\\6, i8 1, i32 1, i8 1, i32 1, i8 2, null}\n"
      "!\\7 = !{i32 4, !\"ViewPortArrayIndex\", i8 5, i8 0, !\\8, i8 1, i32 1, i8 1, i32 1, i8 3, null}\n",
      "!1012 ="},

    "signature element SV_PrimitiveID at location \\(1,0\\) size \\(1,1\\) violates component ordering rule \\(arb < sv < sgv\\).\n"
    "signature element ViewPortArrayIndex at location \\(1,3\\) size \\(1,1\\) violates component ordering rule \\(arb < sv < sgv\\).",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemIndexConflictArbSV) {
  RewriteAssemblyCheckMsg(" \
void main( \
  float4 inpos : Position, \
  uint iid : SV_InstanceID, \
  out float4 pos : SV_Position, \
  out uint id[2] : Array, \
  out uint vpid : SV_ViewPortArrayIndex, \
  out float2 ClipDistance : SV_ClipDistance, \
  out float CullDistance : SV_CullDistance) \
{ \
  pos = inpos; \
  ClipDistance = inpos.x; \
  CullDistance = inpos.y; \
  vpid = iid; \
  id[0] = iid; \
  id[1] = iid + 1; \
} \
    ",
    "vs_6_0",

    "!{i32 2, !\"SV_ViewportArrayIndex\", i8 5, i8 5, !([0-9]+), i8 1, i32 1, i8 1, i32 3, i8 0, null}",
    "!{i32 2, !\"SV_ViewportArrayIndex\", i8 5, i8 5, !\\1, i8 1, i32 1, i8 1, i32 1, i8 3, null}",

    "signature element SV_ViewportArrayIndex at location \\(1,3\\) size \\(1,1\\) has an indexing conflict with another signature element packed into the same row.",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemIndexConflictTessfactors) {
  RewriteAssemblyCheckMsg(" \
struct Vertex { \
  float4 pos : SV_Position; \
}; \
struct PatchConstant { \
  float edges[ 4 ]  : SV_TessFactor; \
  float inside[ 2 ] : SV_InsideTessFactor; \
}; \
PatchConstant PCMain( InputPatch<Vertex, 4> patch) { \
  PatchConstant PC; \
  PC.edges = (float[4])patch[1].pos; \
  PC.inside = (float[2])patch[1].pos.xy; \
  return PC; \
} \
[domain(\"quad\")] \
[partitioning(\"fractional_odd\")] \
[outputtopology(\"triangle_cw\")] \
[patchconstantfunc(\"PCMain\")] \
[outputcontrolpoints(4)] \
Vertex main(uint id : SV_OutputControlPointID, InputPatch< Vertex, 4 > patch) { \
  Vertex Out = patch[id]; \
  Out.pos.w += 0.25; \
  return Out; \
} \
    ",
    "hs_6_0",
    //!{i32 0, !"SV_TessFactor", i8 9, i8 25, !23, i8 0, i32 4, i8 1, i32 0, i8 3, null}
    { "!{i32 1, !\"SV_InsideTessFactor\", i8 9, i8 26, !([0-9]+), i8 0, i32 2, i8 1, i32 4, i8 3, null}",
      "?!dx.viewIdState =" },
    { "!{i32 1, !\"SV_InsideTessFactor\", i8 9, i8 26, !\\1, i8 0, i32 2, i8 1, i32 0, i8 2, null}",
      "!1012 =" },
    "signature element SV_InsideTessFactor at location \\(0,2\\) size \\(2,1\\) has an indexing conflict with another signature element packed into the same row.",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemIndexConflictTessfactors2) {
  RewriteAssemblyCheckMsg(" \
struct Vertex { \
  float4 pos : SV_Position; \
}; \
struct PatchConstant { \
  float edges[ 4 ]  : SV_TessFactor; \
  float inside[ 2 ] : SV_InsideTessFactor; \
  float arb [ 3 ] : Arb; \
}; \
PatchConstant PCMain( InputPatch<Vertex, 4> patch) { \
  PatchConstant PC; \
  PC.edges = (float[4])patch[1].pos; \
  PC.inside = (float[2])patch[1].pos.xy; \
  PC.arb[0] = 1; PC.arb[1] = 2; PC.arb[2] = 3; \
  return PC; \
} \
[domain(\"quad\")] \
[partitioning(\"fractional_odd\")] \
[outputtopology(\"triangle_cw\")] \
[patchconstantfunc(\"PCMain\")] \
[outputcontrolpoints(4)] \
Vertex main(uint id : SV_OutputControlPointID, InputPatch< Vertex, 4 > patch) { \
  Vertex Out = patch[id]; \
  Out.pos.w += 0.25; \
  return Out; \
} \
    ",
    "hs_6_0",
    "!{i32 2, !\"Arb\", i8 9, i8 0, !([0-9]+), i8 0, i32 3, i8 1, i32 0, i8 0, null}",
    "!{i32 2, !\"Arb\", i8 9, i8 0, !\\1, i8 0, i32 3, i8 1, i32 2, i8 0, null}",
    "signature element Arb at location \\(2,0\\) size \\(3,1\\) has an indexing conflict with another signature element packed into the same row.",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemRowOutOfRange) {
  RewriteAssemblyCheckMsg(" \
struct Vertex { \
  float4 pos : SV_Position; \
}; \
struct PatchConstant { \
  float edges[ 4 ]  : SV_TessFactor; \
  float inside[ 2 ] : SV_InsideTessFactor; \
  float arb [ 3 ] : Arb; \
}; \
PatchConstant PCMain( InputPatch<Vertex, 4> patch) { \
  PatchConstant PC; \
  PC.edges = (float[4])patch[1].pos; \
  PC.inside = (float[2])patch[1].pos.xy; \
  PC.arb[0] = 1; PC.arb[1] = 2; PC.arb[2] = 3; \
  return PC; \
} \
[domain(\"quad\")] \
[partitioning(\"fractional_odd\")] \
[outputtopology(\"triangle_cw\")] \
[patchconstantfunc(\"PCMain\")] \
[outputcontrolpoints(4)] \
Vertex main(uint id : SV_OutputControlPointID, InputPatch< Vertex, 4 > patch) { \
  Vertex Out = patch[id]; \
  Out.pos.w += 0.25; \
  return Out; \
} \
    ",
    "hs_6_0",
    { "!{i32 2, !\"Arb\", i8 9, i8 0, !([0-9]+), i8 0, i32 3, i8 1, i32 0, i8 0, null}",
      "?!dx.viewIdState =" },
    { "!{i32 2, !\"Arb\", i8 9, i8 0, !\\1, i8 0, i32 3, i8 1, i32 31, i8 0, null}",
      "!1012 =" },
    "signature element Arb at location \\(31,0\\) size \\(3,1\\) is out of range.",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemPackOverlap) {
  RewriteAssemblyCheckMsg(" \
struct Vertex { \
  float4 pos : SV_Position; \
}; \
struct PatchConstant { \
  float edges[ 4 ]  : SV_TessFactor; \
  float inside[ 2 ] : SV_InsideTessFactor; \
  float arb [ 3 ] : Arb; \
}; \
PatchConstant PCMain( InputPatch<Vertex, 4> patch) { \
  PatchConstant PC; \
  PC.edges = (float[4])patch[1].pos; \
  PC.inside = (float[2])patch[1].pos.xy; \
  PC.arb[0] = 1; PC.arb[1] = 2; PC.arb[2] = 3; \
  return PC; \
} \
[domain(\"quad\")] \
[partitioning(\"fractional_odd\")] \
[outputtopology(\"triangle_cw\")] \
[patchconstantfunc(\"PCMain\")] \
[outputcontrolpoints(4)] \
Vertex main(uint id : SV_OutputControlPointID, InputPatch< Vertex, 4 > patch) { \
  Vertex Out = patch[id]; \
  Out.pos.w += 0.25; \
  return Out; \
} \
    ",
    "hs_6_0",
    "!{i32 2, !\"Arb\", i8 9, i8 0, !([0-9]+), i8 0, i32 3, i8 1, i32 0, i8 0, null}",
    "!{i32 2, !\"Arb\", i8 9, i8 0, !\\1, i8 0, i32 3, i8 1, i32 1, i8 3, null}",
    "signature element Arb at location \\(1,3\\) size \\(3,1\\) overlaps another signature element.",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemPackOverlap2) {
  RewriteAssemblyCheckMsg(" \
void main( \
  float4 inpos : Position, \
  uint iid : SV_InstanceID, \
  out float4 pos : SV_Position, \
  out uint id[2] : Array, \
  out uint3 value : Value, \
  out float2 ClipDistance : SV_ClipDistance, \
  out float CullDistance : SV_CullDistance) \
{ \
  pos = inpos; \
  ClipDistance = inpos.x; \
  CullDistance = inpos.y; \
  value = iid; \
  id[0] = iid; \
  id[1] = iid + 1; \
} \
    ",
    "vs_6_0",

    {"!{i32 1, !\"Array\", i8 5, i8 0, !([0-9]+), i8 1, i32 2, i8 1, i32 1, i8 0, null}(.*)"
    "!\\1 = !{i32 0, i32 1}\n",
    "= !{i32 2, !\"Value\", i8 5, i8 0, !([0-9]+), i8 1, i32 1, i8 3, i32 1, i8 1, null}"},

    {"!{i32 1, !\"Array\", i8 5, i8 0, !\\1, i8 1, i32 2, i8 1, i32 1, i8 1, null}\\2"
    "!\\1 = !{i32 0, i32 1}\n",
    "= !{i32 2, !\"Value\", i8 5, i8 0, !\\1, i8 1, i32 1, i8 3, i32 2, i8 0, null}"},

    "signature element Value at location \\(2,0\\) size \\(1,3\\) overlaps another signature element.",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemMultiDepth) {
  RewriteAssemblyCheckMsg(" \
float4 main(float4 f4 : Input, out float d0 : SV_Depth, out float d1 : SV_Target) : SV_Target1 \
{ d0 = f4.z; d1 = f4.w; return f4; } \
    ",
    "ps_6_0",
    {"!{i32 1, !\"SV_Target\", i8 9, i8 16, !([0-9]+), i8 0, i32 1, i8 1, i32 0, i8 0, null}"},
    {"!{i32 1, !\"SV_DepthGreaterEqual\", i8 9, i8 19, !\\1, i8 0, i32 1, i8 1, i32 -1, i8 -1, null}"},
    "Pixel Shader only allows one type of depth semantic to be declared",
    /*bRegex*/true);
}

TEST_F(ValidationTest, WhenRootSigMismatchThenFail) {
  ReplaceContainerPartsCheckMsgs(
    "float c; [RootSignature ( \"RootConstants(b0, num32BitConstants = 1)\" )] float4 main() : semantic { return c; }",
    "[RootSignature ( \"\" )] float4 main() : semantic { return 0; }",
    "vs_6_0",
    {DFCC_RootSignature},
    {
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}
TEST_F(ValidationTest, WhenRootSigCompatThenSucceed) {
  ReplaceContainerPartsCheckMsgs(
    "[RootSignature ( \"\" )] float4 main() : semantic { return 0; }",
    "float c; [RootSignature ( \"RootConstants(b0, num32BitConstants = 1)\" )] float4 main() : semantic { return c; }",
    "vs_6_0",
    {DFCC_RootSignature},
    {}
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderSucceed_RootConstVis) {
  ReplaceContainerPartsCheckMsgs(
    "float c; float4 main() : semantic { return c; }",
    "[RootSignature ( \"RootConstants(b0, visibility = SHADER_VISIBILITY_VERTEX, num32BitConstants = 1)\" )]"
    "  float4 main() : semantic { return 0; }",
    "vs_6_0",
    {DFCC_RootSignature},
    {}
  );
}
TEST_F(ValidationTest, WhenRootSigMatchShaderFail_RootConstVis) {
  ReplaceContainerPartsCheckMsgs(
    "float c; float4 main() : semantic { return c; }",
    "[RootSignature ( \"RootConstants(b0, visibility = SHADER_VISIBILITY_PIXEL, num32BitConstants = 1)\" )]"
    "  float4 main() : semantic { return 0; }",
    "vs_6_0",
    {DFCC_RootSignature},
    {
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderSucceed_RootCBV) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { float a; int4 b; }; "
    "ConstantBuffer<Foo> cb1 : register(b2, space5); "
    "float4 main() : semantic { return cb1.b.x; }",
    "[RootSignature ( \"CBV(b2, space = 5)\" )]"
    "  float4 main() : semantic { return 0; }",
    "vs_6_0",
    {DFCC_RootSignature},
    {}
  );
}
TEST_F(ValidationTest, WhenRootSigMatchShaderFail_RootCBV_Range) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { float a; int4 b; }; "
    "ConstantBuffer<Foo> cb1 : register(b0, space5); "
    "float4 main() : semantic { return cb1.b.x; }",
    "[RootSignature ( \"CBV(b2, space = 5)\" )]"
    "  float4 main() : semantic { return 0; }",
    "vs_6_0",
    {DFCC_RootSignature},
    {
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}
TEST_F(ValidationTest, WhenRootSigMatchShaderFail_RootCBV_Space) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { float a; int4 b; }; "
    "ConstantBuffer<Foo> cb1 : register(b2, space7); "
    "float4 main() : semantic { return cb1.b.x; }",
    "[RootSignature ( \"CBV(b2, space = 5)\" )]"
    "  float4 main() : semantic { return 0; }",
    "vs_6_0",
    {DFCC_RootSignature},
    {
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderSucceed_RootSRV) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { float4 a; }; "
    "StructuredBuffer<Foo> buf1 : register(t1, space3); "
    "float4 main(float4 a : AAA) : SV_Target { return buf1[a.x].a; }",
    "[RootSignature ( \"SRV(t1, space = 3)\" )]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {}
  );
}
TEST_F(ValidationTest, WhenRootSigMatchShaderFail_RootSRV_ResType) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { float4 a; }; "
    "StructuredBuffer<Foo> buf1 : register(t1, space3); "
    "float4 main(float4 a : AAA) : SV_Target { return buf1[a.x].a; }",
    "[RootSignature ( \"UAV(u1, space = 3)\" )]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderSucceed_RootUAV) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { float4 a; }; "
    "RWStructuredBuffer<Foo> buf1 : register(u1, space3); "
    "float4 main(float4 a : AAA) : SV_Target { return buf1[a.x].a; }",
    "[RootSignature ( \"UAV(u1, space = 3)\" )]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {}
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderSucceed_DescTable) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { int a; float4 b; };"
    ""
    "ConstantBuffer<Foo> cb1[4] : register(b2, space5);"
    "Texture2D<float4> tex1[8]  : register(t1, space3);"
    "RWBuffer<float4> buf1[6]   : register(u33, space17);"
    "SamplerState sampler1[5]   : register(s0, space0);"
    ""
    "float4 main(float4 a : AAA) : SV_TARGET"
    "{"
    "  return buf1[a.x][a.y] + cb1[a.x].b + tex1[a.x].Sample(sampler1[a.x], a.xy);"
    "}",
    "[RootSignature(\"DescriptorTable( SRV(t1,space=3,numDescriptors=8), "
                                      "CBV(b2,space=5,numDescriptors=4), "
                                      "UAV(u33,space=17,numDescriptors=6)), "
                     "DescriptorTable(Sampler(s0, numDescriptors=5))\")]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {}
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderSucceed_DescTable_GoodRange) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { int a; float4 b; };"
    ""
    "ConstantBuffer<Foo> cb1[4] : register(b2, space5);"
    "Texture2D<float4> tex1[8]  : register(t1, space3);"
    "RWBuffer<float4> buf1[6]   : register(u33, space17);"
    "SamplerState sampler1[5]   : register(s0, space0);"
    ""
    "float4 main(float4 a : AAA) : SV_TARGET"
    "{"
    "  return buf1[a.x][a.y] + cb1[a.x].b + tex1[a.x].Sample(sampler1[a.x], a.xy);"
    "}",
    "[RootSignature(\"DescriptorTable( SRV(t0,space=3,numDescriptors=20), "
                                      "CBV(b2,space=5,numDescriptors=4), "
                                      "UAV(u33,space=17,numDescriptors=6)), "
                     "DescriptorTable(Sampler(s0, numDescriptors=5))\")]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {}
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderSucceed_DescTable_Unbounded) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { int a; float4 b; };"
    ""
    "ConstantBuffer<Foo> cb1[4] : register(b2, space5);"
    "Texture2D<float4> tex1[8]  : register(t1, space3);"
    "RWBuffer<float4> buf1[6]   : register(u33, space17);"
    "SamplerState sampler1[5]   : register(s0, space0);"
    ""
    "float4 main(float4 a : AAA) : SV_TARGET"
    "{"
    "  return buf1[a.x][a.y] + cb1[a.x].b + tex1[a.x].Sample(sampler1[a.x], a.xy);"
    "}",
    "[RootSignature(\"DescriptorTable( CBV(b2,space=5,numDescriptors=4), "
                                      "SRV(t1,space=3,numDescriptors=8), "
                                      "UAV(u10,space=17,numDescriptors=unbounded)), "
                     "DescriptorTable(Sampler(s0, numDescriptors=5))\")]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {}
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderFail_DescTable_Range1) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { int a; float4 b; };"
    ""
    "ConstantBuffer<Foo> cb1[4] : register(b2, space5);"
    "Texture2D<float4> tex1[8]  : register(t1, space3);"
    "RWBuffer<float4> buf1[6]   : register(u33, space17);"
    "SamplerState sampler1[5]   : register(s0, space0);"
    ""
    "float4 main(float4 a : AAA) : SV_TARGET"
    "{"
    "  return buf1[a.x][a.y] + cb1[a.x].b + tex1[a.x].Sample(sampler1[a.x], a.xy);"
    "}",
    "[RootSignature(\"DescriptorTable( CBV(b2,space=5,numDescriptors=4), "
                                      "SRV(t2,space=3,numDescriptors=8), "
                                      "UAV(u33,space=17,numDescriptors=6)), "
                     "DescriptorTable(Sampler(s0, numDescriptors=5))\")]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {
      "Shader SRV descriptor range (RegisterSpace=3, NumDescriptors=8, BaseShaderRegister=1) is not fully bound in root signature.",
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderFail_DescTable_Range2) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { int a; float4 b; };"
    ""
    "ConstantBuffer<Foo> cb1[4] : register(b2, space5);"
    "Texture2D<float4> tex1[8]  : register(t1, space3);"
    "RWBuffer<float4> buf1[6]   : register(u33, space17);"
    "SamplerState sampler1[5]   : register(s0, space0);"
    ""
    "float4 main(float4 a : AAA) : SV_TARGET"
    "{"
    "  return buf1[a.x][a.y] + cb1[a.x].b + tex1[a.x].Sample(sampler1[a.x], a.xy);"
    "}",
    "[RootSignature(\"DescriptorTable( SRV(t2,space=3,numDescriptors=8), "
                                      "CBV(b20,space=5,numDescriptors=4), "
                                      "UAV(u33,space=17,numDescriptors=6)), "
                     "DescriptorTable(Sampler(s0, numDescriptors=5))\")]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderFail_DescTable_Range3) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { int a; float4 b; };"
    ""
    "ConstantBuffer<Foo> cb1[4] : register(b2, space5);"
    "Texture2D<float4> tex1[8]  : register(t1, space3);"
    "RWBuffer<float4> buf1[6]   : register(u33, space17);"
    "SamplerState sampler1[5]   : register(s0, space0);"
    ""
    "float4 main(float4 a : AAA) : SV_TARGET"
    "{"
    "  return buf1[a.x][a.y] + cb1[a.x].b + tex1[a.x].Sample(sampler1[a.x], a.xy);"
    "}",
    "[RootSignature(\"DescriptorTable( CBV(b2,space=5,numDescriptors=4), "
                                      "SRV(t1,space=3,numDescriptors=8), "
                                      "UAV(u33,space=17,numDescriptors=5)), "
                     "DescriptorTable(Sampler(s0, numDescriptors=5))\")]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderFail_DescTable_Space) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { int a; float4 b; };"
    ""
    "ConstantBuffer<Foo> cb1[4] : register(b2, space5);"
    "Texture2D<float4> tex1[8]  : register(t1, space3);"
    "RWBuffer<float4> buf1[6]   : register(u33, space17);"
    "SamplerState sampler1[5]   : register(s0, space0);"
    ""
    "float4 main(float4 a : AAA) : SV_TARGET"
    "{"
    "  return buf1[a.x][a.y] + cb1[a.x].b + tex1[a.x].Sample(sampler1[a.x], a.xy);"
    "}",
    "[RootSignature(\"DescriptorTable( SRV(t2,space=3,numDescriptors=8), "
                                      "CBV(b2,space=5,numDescriptors=4), "
                                      "UAV(u33,space=0,numDescriptors=6)), "
                     "DescriptorTable(Sampler(s0, numDescriptors=5))\")]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderSucceed_Unbounded) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { int a; float4 b; };"
    ""
    "ConstantBuffer<Foo> cb1[]  : register(b2, space5);"
    "Texture2D<float4> tex1[]   : register(t1, space3);"
    "RWBuffer<float4> buf1[]    : register(u33, space17);"
    "SamplerState sampler1[]    : register(s0, space0);"
    ""
    "float4 main(float4 a : AAA) : SV_TARGET"
    "{"
    "  return buf1[a.x][a.y] + cb1[a.x].b + tex1[a.x].Sample(sampler1[a.x], a.xy);"
    "}",
    "[RootSignature(\"DescriptorTable( CBV(b2,space=5,numDescriptors=1)), "
                     "DescriptorTable( SRV(t1,space=3,numDescriptors=unbounded)), "
                     "DescriptorTable( UAV(u10,space=17,numDescriptors=100)), "
                     "DescriptorTable(Sampler(s0, numDescriptors=5))\")]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {}
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderFail_Unbounded1) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { int a; float4 b; };"
    ""
    "ConstantBuffer<Foo> cb1[]  : register(b2, space5);"
    "Texture2D<float4> tex1[]   : register(t1, space3);"
    "RWBuffer<float4> buf1[]    : register(u33, space17);"
    "SamplerState sampler1[]    : register(s0, space0);"
    ""
    "float4 main(float4 a : AAA) : SV_TARGET"
    "{"
    "  return buf1[a.x][a.y] + cb1[a.x].b + tex1[a.x].Sample(sampler1[a.x], a.xy);"
    "}",
    "[RootSignature(\"DescriptorTable( CBV(b3,space=5,numDescriptors=1)), "
                     "DescriptorTable( SRV(t1,space=3,numDescriptors=unbounded)), "
                     "DescriptorTable( UAV(u10,space=17,numDescriptors=unbounded)), "
                     "DescriptorTable( Sampler(s0, numDescriptors=5))\")]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderFail_Unbounded2) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { int a; float4 b; };"
    ""
    "ConstantBuffer<Foo> cb1[]  : register(b2, space5);"
    "Texture2D<float4> tex1[]   : register(t1, space3);"
    "RWBuffer<float4> buf1[]    : register(u33, space17);"
    "SamplerState sampler1[]    : register(s0, space0);"
    ""
    "float4 main(float4 a : AAA) : SV_TARGET"
    "{"
    "  return buf1[a.x][a.y] + cb1[a.x].b + tex1[a.x].Sample(sampler1[a.x], a.xy);"
    "}",
    "[RootSignature(\"DescriptorTable( CBV(b2,space=5,numDescriptors=1)), "
                     "DescriptorTable( SRV(t1,space=3,numDescriptors=unbounded)), "
                     "DescriptorTable( UAV(u10,space=17,numDescriptors=unbounded)), "
                     "DescriptorTable( Sampler(s5, numDescriptors=unbounded))\")]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenRootSigMatchShaderFail_Unbounded3) {
  ReplaceContainerPartsCheckMsgs(
    "struct Foo { int a; float4 b; };"
    ""
    "ConstantBuffer<Foo> cb1[]  : register(b2, space5);"
    "Texture2D<float4> tex1[]   : register(t1, space3);"
    "RWBuffer<float4> buf1[]    : register(u33, space17);"
    "SamplerState sampler1[]    : register(s0, space0);"
    ""
    "float4 main(float4 a : AAA) : SV_TARGET"
    "{"
    "  return buf1[a.x][a.y] + cb1[a.x].b + tex1[a.x].Sample(sampler1[a.x], a.xy);"
    "}",
    "[RootSignature(\"DescriptorTable( CBV(b2,space=5,numDescriptors=1)), "
                     "DescriptorTable( SRV(t1,space=3,numDescriptors=unbounded)), "
                     "DescriptorTable( UAV(u10,space=17,numDescriptors=7)), "
                     "DescriptorTable(Sampler(s0, numDescriptors=5))\")]"
    "  float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_RootSignature},
    {
      "Root Signature in DXIL container is not compatible with shader.",
      "Validation failed."
    }
  );
}

#define VERTEX_STRUCT1 \
    "struct PSSceneIn \n\
    { \n\
      float4 pos  : SV_Position; \n\
      float2 tex  : TEXCOORD0; \n\
      float3 norm : NORMAL; \n\
    }; \n"
#define VERTEX_STRUCT2 \
    "struct PSSceneIn \n\
    { \n\
      float4 pos  : SV_Position; \n\
      float2 tex  : TEXCOORD0; \n\
    }; \n"
#define PC_STRUCT1 "struct HSPerPatchData {  \n\
      float edges[ 3 ] : SV_TessFactor; \n\
      float inside : SV_InsideTessFactor; \n\
      float foo : FOO; \n\
    }; \n"
#define PC_STRUCT2 "struct HSPerPatchData {  \n\
      float edges[ 3 ] : SV_TessFactor; \n\
      float inside : SV_InsideTessFactor; \n\
    }; \n"
#define PC_FUNC "HSPerPatchData HSPerPatchFunc( InputPatch< PSSceneIn, 3 > points, \n\
      OutputPatch<PSSceneIn, 3> outpoints) { \n\
      HSPerPatchData d = (HSPerPatchData)0; \n\
      d.edges[ 0 ] = points[0].tex.x + outpoints[0].tex.x; \n\
      d.edges[ 1 ] = 1; \n\
      d.edges[ 2 ] = 1; \n\
      d.inside = 1; \n\
      return d; \n\
    } \n"
#define PC_FUNC_NOOUT "HSPerPatchData HSPerPatchFunc( InputPatch< PSSceneIn, 3 > points ) { \n\
      HSPerPatchData d = (HSPerPatchData)0; \n\
      d.edges[ 0 ] = points[0].tex.x; \n\
      d.edges[ 1 ] = 1; \n\
      d.edges[ 2 ] = 1; \n\
      d.inside = 1; \n\
      return d; \n\
    } \n"
#define PC_FUNC_NOIN "HSPerPatchData HSPerPatchFunc( OutputPatch<PSSceneIn, 3> outpoints) { \n\
      HSPerPatchData d = (HSPerPatchData)0; \n\
      d.edges[ 0 ] = outpoints[0].tex.x; \n\
      d.edges[ 1 ] = 1; \n\
      d.edges[ 2 ] = 1; \n\
      d.inside = 1; \n\
      return d; \n\
    } \n"
#define HS_ATTR "[domain(\"tri\")] \n\
    [partitioning(\"fractional_odd\")] \n\
    [outputtopology(\"triangle_cw\")] \n\
    [patchconstantfunc(\"HSPerPatchFunc\")] \n\
    [outputcontrolpoints(3)] \n"
#define HS_FUNC \
    "PSSceneIn main(const uint id : SV_OutputControlPointID, \n\
                    const InputPatch< PSSceneIn, 3 > points ) { \n\
      return points[ id ]; \n\
    } \n"
#define HS_FUNC_NOOUT \
    "void main(const uint id : SV_OutputControlPointID, \n\
               const InputPatch< PSSceneIn, 3 > points ) { \n\
    } \n"
#define HS_FUNC_NOIN \
    "PSSceneIn main( const uint id : SV_OutputControlPointID ) { \n\
      return (PSSceneIn)0; \n\
    } \n"
#define DS_FUNC \
    "[domain(\"tri\")] PSSceneIn main(const float3 bary : SV_DomainLocation, \n\
                                      const OutputPatch<PSSceneIn, 3> patch, \n\
                                      const HSPerPatchData perPatchData) { \n\
      PSSceneIn v = patch[0]; \n\
      v.pos = patch[0].pos * bary.x; \n\
      v.pos += patch[1].pos * bary.y; \n\
      v.pos += patch[2].pos * bary.z; \n\
      return v; \n\
    } \n"
#define DS_FUNC_NOPC \
    "[domain(\"tri\")] PSSceneIn main(const float3 bary : SV_DomainLocation, \n\
                                      const OutputPatch<PSSceneIn, 3> patch) { \n\
      PSSceneIn v = patch[0]; \n\
      v.pos = patch[0].pos * bary.x; \n\
      v.pos += patch[1].pos * bary.y; \n\
      v.pos += patch[2].pos * bary.z; \n\
      return v; \n\
    } \n"

TEST_F(ValidationTest, WhenProgramOutSigMissingThenFail) {
  ReplaceContainerPartsCheckMsgs(
    VERTEX_STRUCT1
    PC_STRUCT1
    PC_FUNC
    HS_ATTR
    HS_FUNC,

    VERTEX_STRUCT1
    PC_STRUCT1
    PC_FUNC_NOOUT
    HS_ATTR
    HS_FUNC_NOOUT,

    "hs_6_0",
    {DFCC_InputSignature, DFCC_OutputSignature, DFCC_PatchConstantSignature},
    {
      "Container part 'Program Output Signature' does not match expected for module.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenProgramOutSigUnexpectedThenFail) {
  ReplaceContainerPartsCheckMsgs(
    VERTEX_STRUCT1
    PC_STRUCT1
    PC_FUNC_NOOUT
    HS_ATTR
    HS_FUNC_NOOUT,

    VERTEX_STRUCT1
    PC_STRUCT1
    PC_FUNC
    HS_ATTR
    HS_FUNC,

    "hs_6_0",
    {DFCC_InputSignature, DFCC_OutputSignature, DFCC_PatchConstantSignature},
    {
      "Container part 'Program Output Signature' does not match expected for module.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenProgramSigMismatchThenFail) {
  ReplaceContainerPartsCheckMsgs(
    VERTEX_STRUCT1
    PC_STRUCT1
    PC_FUNC
    HS_ATTR
    HS_FUNC,

    VERTEX_STRUCT2
    PC_STRUCT2
    PC_FUNC
    HS_ATTR
    HS_FUNC,

    "hs_6_0",
    {DFCC_InputSignature, DFCC_OutputSignature, DFCC_PatchConstantSignature},
    {
      "Container part 'Program Input Signature' does not match expected for module.",
      "Container part 'Program Output Signature' does not match expected for module.",
      "Container part 'Program Patch Constant Signature' does not match expected for module.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenProgramInSigMissingThenFail) {
  ReplaceContainerPartsCheckMsgs(
    VERTEX_STRUCT1
    PC_STRUCT1
    PC_FUNC
    HS_ATTR
    HS_FUNC,

    // Compiling the HS_FUNC_NOIN produces the following error
    //error: validation errors
    //HS input control point count must be [1..32].  0 specified
    VERTEX_STRUCT1
    PC_STRUCT1
    PC_FUNC_NOIN
    HS_ATTR
    HS_FUNC_NOIN,
    "hs_6_0",
    {DFCC_InputSignature, DFCC_OutputSignature, DFCC_PatchConstantSignature},
    {
      "Container part 'Program Input Signature' does not match expected for module.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenProgramSigMismatchThenFail2) {
  ReplaceContainerPartsCheckMsgs(
    VERTEX_STRUCT1
    PC_STRUCT1
    DS_FUNC,

    VERTEX_STRUCT2
    PC_STRUCT2
    DS_FUNC,

    "ds_6_0",
    {DFCC_InputSignature, DFCC_OutputSignature, DFCC_PatchConstantSignature},
    {
      "Container part 'Program Input Signature' does not match expected for module.",
      "Container part 'Program Output Signature' does not match expected for module.",
      "Container part 'Program Patch Constant Signature' does not match expected for module.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenProgramPCSigMissingThenFail) {
  ReplaceContainerPartsCheckMsgs(
    VERTEX_STRUCT1
    PC_STRUCT1
    DS_FUNC,

    VERTEX_STRUCT2
    PC_STRUCT2
    DS_FUNC_NOPC,

    "ds_6_0",
    {DFCC_InputSignature, DFCC_OutputSignature, DFCC_PatchConstantSignature},
    {
      "Container part 'Program Input Signature' does not match expected for module.",
      "Container part 'Program Output Signature' does not match expected for module.",
      "Missing part 'Program Patch Constant Signature' required by module.",
      "Validation failed."
    }
  );
}

#undef VERTEX_STRUCT1
#undef VERTEX_STRUCT2
#undef PC_STRUCT1
#undef PC_STRUCT2
#undef PC_FUNC
#undef PC_FUNC_NOOUT
#undef PC_FUNC_NOIN
#undef HS_ATTR
#undef HS_FUNC
#undef HS_FUNC_NOOUT
#undef HS_FUNC_NOIN
#undef DS_FUNC
#undef DS_FUNC_NOPC

TEST_F(ValidationTest, WhenPSVMismatchThenFail) {
  ReplaceContainerPartsCheckMsgs(
    "float c; [RootSignature ( \"RootConstants(b0, num32BitConstants = 1)\" )] float4 main() : semantic { return c; }",
    "[RootSignature ( \"\" )] float4 main() : semantic { return 0; }",
    "vs_6_0",
    {DFCC_PipelineStateValidation},
    {
      "Container part 'Pipeline State Validation' does not match expected for module.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, WhenRDATMismatchThenFail) {
  ReplaceContainerPartsCheckMsgs(
    "export float4 main(float f) : semantic { return f; }",
    "export float4 main() : semantic { return 0; }",
    "lib_6_3",
    { DFCC_RuntimeData },
    {
      "Container part 'Runtime Data (RDAT)' does not match expected for module.",
      "Validation failed."
    }
    );
}

TEST_F(ValidationTest, WhenFeatureInfoMismatchThenFail) {
  ReplaceContainerPartsCheckMsgs(
    "float4 main(uint2 foo : FOO) : SV_Target { return asdouble(foo.x, foo.y) * 2.0; }",
    "float4 main() : SV_Target { return 0; }",
    "ps_6_0",
    {DFCC_FeatureInfo},
    {
      "Container part 'Feature Info' does not match expected for module.",
      "Validation failed."
    }
  );
}

TEST_F(ValidationTest, RayShaderWithSignaturesFail) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
    "struct Payload { float f; }; struct Attributes { float2 b; };\n"
    "struct Param { float f; };\n"
    "[shader(\"raygeneration\")] void RayGenProto() { return; }\n"
    "[shader(\"intersection\")] void IntersectionProto() { return; }\n"
    "[shader(\"anyhit\")] void AnyHitProto(inout Payload p, in Attributes a) { p.f += a.b.x; }\n"
    "[shader(\"closesthit\")] void ClosestHitProto(inout Payload p, in Attributes a) { p.f += a.b.y; }\n"
    "[shader(\"miss\")] void MissProto(inout Payload p) { p.f += 1.0; }\n"
    "[shader(\"callable\")] void CallableProto(inout Param p) { p.f += 1.0; }\n"
    "[shader(\"vertex\")] float VSOutOnly() : OUTPUT { return 1; }\n"
    "[shader(\"vertex\")] void VSInOnly(float f : INPUT) : OUTPUT {}\n"
    "[shader(\"vertex\")] float VSInOut(float f : INPUT) : OUTPUT { return f; }\n"
    , "lib_6_3",
    {  "!{void \\(\\)\\* @VSInOnly, !\"VSInOnly\", !([0-9]+), null,(.*)!\\1 = "
      ,"!{void \\(\\)\\* @VSOutOnly, !\"VSOutOnly\", !([0-9]+), null,(.*)!\\1 = "
      ,"!{void \\(\\)\\* @VSInOut, !\"VSInOut\", !([0-9]+), null,(.*)!\\1 = "
      ,"!{void \\(\\)\\* @\"\\\\01\\?RayGenProto@@YAXXZ\", !\"\\\\01\\?RayGenProto@@YAXXZ\", null, null,"
      ,"!{void \\(\\)\\* @\"\\\\01\\?IntersectionProto@@YAXXZ\", !\"\\\\01\\?IntersectionProto@@YAXXZ\", null, null,"
      ,"!{void \\(%struct.Payload\\*, %struct.Attributes\\*\\)\\* @\"\\\\01\\?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\", !\"\\\\01\\?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\", null, null,"
      ,"!{void \\(%struct.Payload\\*, %struct.Attributes\\*\\)\\* @\"\\\\01\\?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\", !\"\\\\01\\?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\", null, null,"
      ,"!{void \\(%struct.Payload\\*\\)\\* @\"\\\\01\\?MissProto@@YAXUPayload@@@Z\", !\"\\\\01\\?MissProto@@YAXUPayload@@@Z\", null, null,"
      ,"!{void \\(%struct.Param\\*\\)\\* @\"\\\\01\\?CallableProto@@YAXUParam@@@Z\", !\"\\\\01\\?CallableProto@@YAXUParam@@@Z\", null, null,"
    },
    {  "!{void ()* @VSInOnly, !\"VSInOnly\", !1001, null,\\2!1001 = "
      ,"!{void ()* @VSOutOnly, !\"VSOutOnly\", !1002, null,\\2!1002 = "
      ,"!{void ()* @VSInOut, !\"VSInOut\", !1003, null,\\2!1003 = "
      ,"!{void ()* @\"\\\\01?RayGenProto@@YAXXZ\", !\"\\\\01?RayGenProto@@YAXXZ\", !1001, null,"
      ,"!{void ()* @\"\\\\01?IntersectionProto@@YAXXZ\", !\"\\\\01?IntersectionProto@@YAXXZ\", !1002, null,"
      ,"!{void (%struct.Payload*, %struct.Attributes*)* @\"\\\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\", !\"\\\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\", !1003, null,"
      ,"!{void (%struct.Payload*, %struct.Attributes*)* @\"\\\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\", !\"\\\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\", !1001, null,"
      ,"!{void (%struct.Payload*)* @\"\\\\01?MissProto@@YAXUPayload@@@Z\", !\"\\\\01?MissProto@@YAXUPayload@@@Z\", !1002, null,"
      ,"!{void (%struct.Param*)* @\"\\\\01?CallableProto@@YAXUParam@@@Z\", !\"\\\\01?CallableProto@@YAXUParam@@@Z\", !1003, null,"
    },
    {  "Ray tracing shader '\\\\01\\?RayGenProto@@YAXXZ' should not have any shader signatures"
      ,"Ray tracing shader '\\\\01\\?IntersectionProto@@YAXXZ' should not have any shader signatures"
      ,"Ray tracing shader '\\\\01\\?AnyHitProto@@YAXUPayload@@UAttributes@@@Z' should not have any shader signatures"
      ,"Ray tracing shader '\\\\01\\?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z' should not have any shader signatures"
      ,"Ray tracing shader '\\\\01\\?MissProto@@YAXUPayload@@@Z' should not have any shader signatures"
      ,"Ray tracing shader '\\\\01\\?CallableProto@@YAXUParam@@@Z' should not have any shader signatures"
    },
    /*bRegex*/true);
}

TEST_F(ValidationTest, ViewIDInCSFail) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  RewriteAssemblyCheckMsg(" \
RWStructuredBuffer<uint> Buf; \
[numthreads(1,1,1)] \
void main(uint id : SV_GroupIndex) \
{ Buf[id] = 0; } \
    ",
    "cs_6_1",
    {"dx.op.flattenedThreadIdInGroup.i32(i32 96",
     "declare i32 @dx.op.flattenedThreadIdInGroup.i32(i32)"},
    {"dx.op.viewID.i32(i32 138",
     "declare i32 @dx.op.viewID.i32(i32)"},
    "Opcode ViewID not valid in shader model cs_6_1",
    /*bRegex*/false);
}

TEST_F(ValidationTest, ViewIDIn60Fail) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  RewriteAssemblyCheckMsg(" \
[domain(\"tri\")] \
float4 main(float3 pos : Position, uint id : SV_PrimitiveID) : SV_Position \
{ return float4(pos, id); } \
    ",
    "ds_6_0",
    {"dx.op.primitiveID.i32(i32 108",
     "declare i32 @dx.op.primitiveID.i32(i32)"},
    {"dx.op.viewID.i32(i32 138",
     "declare i32 @dx.op.viewID.i32(i32)"},
    "Opcode ViewID not valid in shader model ds_6_0",
    /*bRegex*/false);
}

TEST_F(ValidationTest, ViewIDNoSpaceFail) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  RewriteAssemblyCheckMsg(" \
float4 main(uint vid : SV_ViewID, float3 In[31] : INPUT) : SV_Target \
{ return float4(In[vid], 1); } \
    ",
    "ps_6_1",
    { "!{i32 0, !\"INPUT\", i8 9, i8 0, !([0-9]+), i8 2, i32 31",
      "!{i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17, i32 18, i32 19, i32 20, i32 21, i32 22, i32 23, i32 24, i32 25, i32 26, i32 27, i32 28, i32 29, i32 30}",
      "?!dx.viewIdState =" },
    { "!{i32 0, !\"INPUT\", i8 9, i8 0, !\\1, i8 2, i32 32",
      "!{i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9, i32 10, i32 11, i32 12, i32 13, i32 14, i32 15, i32 16, i32 17, i32 18, i32 19, i32 20, i32 21, i32 22, i32 23, i32 24, i32 25, i32 26, i32 27, i32 28, i32 29, i32 30, i32 31}",
      "!1012 =" },
    "Pixel shader input signature lacks available space for ViewID",
    /*bRegex*/true);
}

TEST_F(ValidationTest, GSMainMissingAttributeFail) {
  TestCheck(L"..\\CodeGenHLSL\\attributes-gs-no-inout-main.hlsl");
}

TEST_F(ValidationTest, GSOtherMissingAttributeFail) {
  TestCheck(L"..\\CodeGenHLSL\\attributes-gs-no-inout-other.hlsl");
}

TEST_F(ValidationTest, GSMissingMaxVertexCountFail) {
  TestCheck(L"..\\CodeGenHLSL\\attributes-gs-no-maxvertexcount.hlsl");
}

TEST_F(ValidationTest, HSMissingPCFFail) {
  TestCheck(L"..\\CodeGenHLSL\\attributes-hs-no-pcf.hlsl");
}

TEST_F(ValidationTest, GetAttributeAtVertexInVSFail) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  RewriteAssemblyCheckMsg(
    "float4 main(float4 pos: POSITION) : SV_POSITION { return pos.x; }",
    "vs_6_1",
    { "call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)",
    "declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32)" },
    { "call float @dx.op.attributeAtVertex.f32(i32 137, i32 0, i32 0, i8 0, i8 0)",
    "declare float @dx.op.attributeAtVertex.f32(i32, i32, i32, i8, i8)" },
    "Opcode AttributeAtVertex not valid in shader model vs_6_1",
    /*bRegex*/ false);
}

TEST_F(ValidationTest, GetAttributeAtVertexIn60Fail) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  RewriteAssemblyCheckMsg(
    "float4 main(float4 col : COLOR) : "
    "SV_Target { return EvaluateAttributeCentroid(col).x; }",
    "ps_6_0",
    { "call float @dx.op.evalCentroid.f32(i32 89, i32 0, i32 0, i8 0)",
    "declare float @dx.op.evalCentroid.f32(i32, i32, i32, i8)"
    },
    { "call float @dx.op.attributeAtVertex.f32(i32 137, i32 0, i32 0, i8 0, i8 0)",
    "declare float @dx.op.attributeAtVertex.f32(i32, i32, i32, i8, i8)" },
    "Opcode AttributeAtVertex not valid in shader model ps_6_0", /*bRegex*/ false);
}

TEST_F(ValidationTest, GetAttributeAtVertexInterpFail) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  RewriteAssemblyCheckMsg("float4 main(nointerpolation float4 col : COLOR) : "
                          "SV_Target { return GetAttributeAtVertex(col, 0); }",
                          "ps_6_1", {"!\"COLOR\", i8 9, i8 0, (![0-9]+), i8 1"},
                          {"!\"COLOR\", i8 9, i8 0, \\1, i8 2"},
                          "Attribute COLOR must have nointerpolation mode in "
                          "order to use GetAttributeAtVertex function.",
                          /*bRegex*/ true);
}

TEST_F(ValidationTest, BarycentricMaxIndexFail) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  RewriteAssemblyCheckMsg(
      "float4 main(float3 bary : SV_Barycentrics, noperspective float3 bary1 : "
      "SV_Barycentrics1) : SV_Target { return 1; }",
      "ps_6_1",
      {"!([0-9]+) = !{i32 0, !\"SV_Barycentrics\", i8 9, i8 28, !([0-9]+), i8 "
       "2, i32 1, i8 3, i32 -1, i8 -1, null}\n"
       "!([0-9]+) = !{i32 0}"},
      {"!\\1 = !{i32 0, !\"SV_Barycentrics\", i8 9, i8 28, !\\2, i8 2, i32 1, "
       "i8 3, i32 -1, i8 -1, null}\n"
       "!\\3 = !{i32 2}"},
      "SV_Barycentrics semantic index exceeds maximum", /*bRegex*/ true);
}

TEST_F(ValidationTest, BarycentricNoInterpolationFail) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  RewriteAssemblyCheckMsg(
      "float4 main(float3 bary : SV_Barycentrics) : "
      "SV_Target { return bary.x * float4(1,0,0,0) + bary.y * float4(0,1,0,0) "
      "+ bary.z * float4(0,0,1,0); }",
      "ps_6_1", {"!\"SV_Barycentrics\", i8 9, i8 28, (![0-9]+), i8 2"},
      {"!\"SV_Barycentrics\", i8 9, i8 28, \\1, i8 1"},
      "SV_Barycentrics cannot be used with 'nointerpolation' type",
      /*bRegex*/ true);
}

TEST_F(ValidationTest, BarycentricFloat4Fail) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  RewriteAssemblyCheckMsg(
      "float4 main(float4 col : COLOR) : SV_Target { return col; }", "ps_6_1",
      {"!\"COLOR\", i8 9, i8 0"}, {"!\"SV_Barycentrics\", i8 9, i8 28"},
      "only 'float3' type is allowed for SV_Barycentrics.", false);
}

TEST_F(ValidationTest, BarycentricSamePerspectiveFail) {
  if (m_ver.SkipDxilVersion(1,1)) return;
  RewriteAssemblyCheckMsg(
      "float4 main(float3 bary : SV_Barycentrics, noperspective float3 bary1 : "
      "SV_Barycentrics1) : SV_Target { return 1; }",
      "ps_6_1", {"!\"SV_Barycentrics\", i8 9, i8 28, (![0-9]+), i8 4"},
      {"!\"SV_Barycentrics\", i8 9, i8 28, \\1, i8 2"},
      "There can only be up to two input attributes of SV_Barycentrics with "
      "different perspective interpolation mode.",
      true);
}

TEST_F(ValidationTest, Float32DenormModeAttribute) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  std::vector<LPCWSTR> pArguments = { L"-denorm", L"ftz" };
  RewriteAssemblyCheckMsg(
    "float4 main(float4 col: COL) : SV_Target { return col; }", "ps_6_2",
    pArguments.data(), 2, nullptr, 0,
    { "\"fp32-denorm-mode\"=\"ftz\"" },
    { "\"fp32-denorm-mode\"=\"invalid_mode\"" },
    "contains invalid attribute 'fp32-denorm-mode' with value 'invalid_mode'",
    false);
}

TEST_F(ValidationTest, ResCounter) {
    if (m_ver.SkipDxilVersion(1, 3)) return;
    RewriteAssemblyCheckMsg(
        "RWStructuredBuffer<float4> buf; export float GetCounter() {return buf.IncrementCounter();}",
        "lib_6_3",
        { "!\"buf\", i32 0, i32 -1, i32 1, i32 12, i1 false, i1 true, i1 false, !" },
        { "!\"buf\", i32 0, i32 -1, i32 1, i32 12, i1 false, i1 false, i1 false, !" },
        "BufferUpdateCounter valid only when HasCounter is true",
        true);
}

TEST_F(ValidationTest, FunctionAttributes) {
  if (m_ver.SkipDxilVersion(1, 2)) return;
  std::vector<LPCWSTR> pArguments = { L"-denorm", L"ftz" };
  RewriteAssemblyCheckMsg(
    "float4 main(float4 col: COL) : SV_Target { return col; }", "ps_6_2",
    pArguments.data(), 2, nullptr, 0,
    { "\"fp32-denorm-mode\"=\"ftz\"" },
    { "\"dummy_attribute\"=\"invalid_mode\"" },
    "contains invalid attribute 'dummy_attribute' with value 'invalid_mode'",
    false);
}// TODO: reject non-zero padding

TEST_F(ValidationTest, LibFunctionResInSig) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
    "Texture2D<float4> T1;\n"
    "struct ResInStruct { float f; Texture2D<float4> T; };\n"
    "struct ResStructInStruct { float f; ResInStruct S; };\n"
    "ResStructInStruct fnResInReturn(float f) : SV_Target {\n"
    "  ResStructInStruct S1; S1.f = S1.S.f = f; S1.S.T = T1;\n"
    "  return S1; }\n"
    "float fnResInArg(ResStructInStruct S1) : SV_Target {\n"
    "  return S1.f; }\n"
    "struct Data { float f; };\n"
    "float fnStreamInArg(float f, inout PointStream<Data> S1) : SV_Target {\n"
    "  S1.Append((Data)f); return 1.0; }\n"
    , "lib_6_x",
    { "!{!\"lib\", i32 6, i32 15}", "!dx.valver = !{!2}" },
    { "!{!\"lib\", i32 6, i32 3}", "!dx.valver = !{!1002}\n!1002 = !{i32 1, i32 3}" },
    {  "Function '\\01?fnResInReturn@@YA?AUResStructInStruct@@M@Z' uses resource in function signature"
      ,"Function '\\01?fnResInArg@@YAMUResStructInStruct@@@Z' uses resource in function signature"
      ,"Function '\\01?fnStreamInArg@@YAMMV?$PointStream@UData@@@@@Z' uses resource in function signature"
      // TODO: Unable to lower stream append, since it's used in a non-GS function.
      // Should we fail to compile earlier (even on lib_6_x), or add lowering to linker?
      ,"Function 'dx.hl.op..void (i32, %\"class.PointStream<Data>\"*, float*)' uses resource in function signature"
    },
    false);
}

TEST_F(ValidationTest, RayPayloadIsStruct) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
    "struct Payload { float f; }; struct Attributes { float2 b; };\n"
    "[shader(\"anyhit\")] void AnyHitProto(inout Payload p, in Attributes a) { p.f += a.b.x; }\n"
    "export void BadAnyHit(inout float f, in Attributes a) { f += a.b.x; }\n"
    "[shader(\"closesthit\")] void ClosestHitProto(inout Payload p, in Attributes a) { p.f += a.b.y; }\n"
    "export void BadClosestHit(inout float f, in Attributes a) { f += a.b.y; }\n"
    "[shader(\"miss\")] void MissProto(inout Payload p) { p.f += 1.0; }\n"
    "export void BadMiss(inout float f) { f += 1.0; }\n"
    , "lib_6_3",
    { "!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\", "
        "!\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\",",
      "!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\", "
        "!\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\",",
      "!{void (%struct.Payload*)* @\"\\01?MissProto@@YAXUPayload@@@Z\", "
        "!\"\\01?MissProto@@YAXUPayload@@@Z\","
    },
    { "!{void (float*, %struct.Attributes*)* @\"\\01?BadAnyHit@@YAXAIAMUAttributes@@@Z\", "
        "!\"\\01?BadAnyHit@@YAXAIAMUAttributes@@@Z\",",
      "!{void (float*, %struct.Attributes*)* @\"\\01?BadClosestHit@@YAXAIAMUAttributes@@@Z\", "
        "!\"\\01?BadClosestHit@@YAXAIAMUAttributes@@@Z\",",
      "!{void (float*)* @\"\\01?BadMiss@@YAXAIAM@Z\", "
        "!\"\\01?BadMiss@@YAXAIAM@Z\","
    },
    {  "Argument 'f' must be a struct type for payload in shader function '\\01?BadAnyHit@@YAXAIAMUAttributes@@@Z'"
      ,"Argument 'f' must be a struct type for payload in shader function '\\01?BadClosestHit@@YAXAIAMUAttributes@@@Z'"
      ,"Argument 'f' must be a struct type for payload in shader function '\\01?BadMiss@@YAXAIAM@Z'"
    },
    false);
}

TEST_F(ValidationTest, RayAttrIsStruct) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
    "struct Payload { float f; }; struct Attributes { float2 b; };\n"
    "[shader(\"anyhit\")] void AnyHitProto(inout Payload p, in Attributes a) { p.f += a.b.x; }\n"
    "export void BadAnyHit(inout Payload p, in float a) { p.f += a; }\n"
    "[shader(\"closesthit\")] void ClosestHitProto(inout Payload p, in Attributes a) { p.f += a.b.y; }\n"
    "export void BadClosestHit(inout Payload p, in float a) { p.f += a; }\n"
    , "lib_6_3",
    { "!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\", "
        "!\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\",",
      "!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\", "
        "!\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\","
    },
    { "!{void (%struct.Payload*, float)* @\"\\01?BadAnyHit@@YAXUPayload@@M@Z\", "
        "!\"\\01?BadAnyHit@@YAXUPayload@@M@Z\",",
      "!{void (%struct.Payload*, float)* @\"\\01?BadClosestHit@@YAXUPayload@@M@Z\", "
        "!\"\\01?BadClosestHit@@YAXUPayload@@M@Z\","
    },
    {  "Argument 'a' must be a struct type for attributes in shader function '\\01?BadAnyHit@@YAXUPayload@@M@Z'"
      ,"Argument 'a' must be a struct type for attributes in shader function '\\01?BadClosestHit@@YAXUPayload@@M@Z'"
    },
    false);
}

TEST_F(ValidationTest, CallableParamIsStruct) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
    "struct Param { float f; };\n"
    "[shader(\"callable\")] void CallableProto(inout Param p) { p.f += 1.0; }\n"
    "export void BadCallable(inout float f) { f += 1.0; }\n"
    , "lib_6_3",
    { "!{void (%struct.Param*)* @\"\\01?CallableProto@@YAXUParam@@@Z\", "
        "!\"\\01?CallableProto@@YAXUParam@@@Z\","
    },
    { "!{void (float*)* @\"\\01?BadCallable@@YAXAIAM@Z\", "
        "!\"\\01?BadCallable@@YAXAIAM@Z\","
    },
    {  "Argument 'f' must be a struct type for callable shader function '\\01?BadCallable@@YAXAIAM@Z'"
    },
    false);
}

TEST_F(ValidationTest, RayShaderExtraArg) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
    "struct Payload { float f; }; struct Attributes { float2 b; };\n"
    "struct Param { float f; };\n"
    "[shader(\"anyhit\")] void AnyHitProto(inout Payload p, in Attributes a) { p.f += a.b.x; }\n"
    "[shader(\"closesthit\")] void ClosestHitProto(inout Payload p, in Attributes a) { p.f += a.b.y; }\n"
    "[shader(\"miss\")] void MissProto(inout Payload p) { p.f += 1.0; }\n"
    "[shader(\"callable\")] void CallableProto(inout Param p) { p.f += 1.0; }\n"
    "export void BadAnyHit(inout Payload p, in Attributes a, float f) { p.f += f; }\n"
    "export void BadClosestHit(inout Payload p, in Attributes a, float f) { p.f += f; }\n"
    "export void BadMiss(inout Payload p, float f) { p.f += f; }\n"
    "export void BadCallable(inout Param p, float f) { p.f += f; }\n"
    , "lib_6_3",
    { "!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\", "
        "!\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\"",
      "!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\", "
        "!\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\"",
      "!{void (%struct.Payload*)* @\"\\01?MissProto@@YAXUPayload@@@Z\", "
        "!\"\\01?MissProto@@YAXUPayload@@@Z\"",
      "!{void (%struct.Param*)* @\"\\01?CallableProto@@YAXUParam@@@Z\", "
        "!\"\\01?CallableProto@@YAXUParam@@@Z\""
    },
    { "!{void (%struct.Payload*, %struct.Attributes*, float)* @\"\\01?BadAnyHit@@YAXUPayload@@UAttributes@@M@Z\", "
        "!\"\\01?BadAnyHit@@YAXUPayload@@UAttributes@@M@Z\"",
      "!{void (%struct.Payload*, %struct.Attributes*, float)* @\"\\01?BadClosestHit@@YAXUPayload@@UAttributes@@M@Z\", "
        "!\"\\01?BadClosestHit@@YAXUPayload@@UAttributes@@M@Z\"",
      "!{void (%struct.Payload*, float)* @\"\\01?BadMiss@@YAXUPayload@@M@Z\", "
        "!\"\\01?BadMiss@@YAXUPayload@@M@Z\"",
      "!{void (%struct.Param*, float)* @\"\\01?BadCallable@@YAXUParam@@M@Z\", "
        "!\"\\01?BadCallable@@YAXUParam@@M@Z\""
    },
    {  "Extra argument 'f' not allowed for shader function '\\01?BadAnyHit@@YAXUPayload@@UAttributes@@M@Z'"
      ,"Extra argument 'f' not allowed for shader function '\\01?BadClosestHit@@YAXUPayload@@UAttributes@@M@Z'"
      ,"Extra argument 'f' not allowed for shader function '\\01?BadMiss@@YAXUPayload@@M@Z'"
      ,"Extra argument 'f' not allowed for shader function '\\01?BadCallable@@YAXUParam@@M@Z'"
    },
    false);
}

TEST_F(ValidationTest, ResInShaderStruct) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  // Verify resource not used in shader argument structure
  RewriteAssemblyCheckMsg(
    "struct ResInStruct { float f; Texture2D<float4> T; };\n"
    "struct ResStructInStruct { float f; ResInStruct S; };\n"
    "struct Payload { float f; }; struct Attributes { float2 b; };\n"
    "[shader(\"anyhit\")] void AnyHitProto(inout Payload p, in Attributes a) { p.f += a.b.x; }\n"
    "export void BadAnyHit(inout ResStructInStruct p, in Attributes a) { p.f += a.b.x; }\n"
    "[shader(\"closesthit\")] void ClosestHitProto(inout Payload p, in Attributes a) { p.f += a.b.y; }\n"
    "export void BadClosestHit(inout ResStructInStruct p, in Attributes a) { p.f += a.b.x; }\n"
    "[shader(\"miss\")] void MissProto(inout Payload p) { p.f += 1.0; }\n"
    "export void BadMiss(inout ResStructInStruct p) { p.f += 1.0; }\n"
    "struct Param { float f; };\n"
    "[shader(\"callable\")] void CallableProto(inout Param p) { p.f += 1.0; }\n"
    "export void BadCallable(inout ResStructInStruct p) { p.f += 1.0; }\n"
    , "lib_6_x",
    { "!{!\"lib\", i32 6, i32 15}", "!dx.valver = !{!2}",
      "!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\", "
        "!\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\",",
      "!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\", "
        "!\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\",",
      "!{void (%struct.Payload*)* @\"\\01?MissProto@@YAXUPayload@@@Z\", "
        "!\"\\01?MissProto@@YAXUPayload@@@Z\",",
      "!{void (%struct.Param*)* @\"\\01?CallableProto@@YAXUParam@@@Z\", "
        "!\"\\01?CallableProto@@YAXUParam@@@Z\","
    },
    { "!{!\"lib\", i32 6, i32 3}", "!dx.valver = !{!1002}\n!1002 = !{i32 1, i32 3}",
      "!{void (%struct.ResStructInStruct*, %struct.Attributes*)* @\"\\01?BadAnyHit@@YAXUResStructInStruct@@UAttributes@@@Z\", "
        "!\"\\01?BadAnyHit@@YAXUResStructInStruct@@UAttributes@@@Z\",",
      "!{void (%struct.ResStructInStruct*, %struct.Attributes*)* @\"\\01?BadClosestHit@@YAXUResStructInStruct@@UAttributes@@@Z\", "
        "!\"\\01?BadClosestHit@@YAXUResStructInStruct@@UAttributes@@@Z\",",
      "!{void (%struct.ResStructInStruct*)* @\"\\01?BadMiss@@YAXUResStructInStruct@@@Z\", "
        "!\"\\01?BadMiss@@YAXUResStructInStruct@@@Z\",",
      "!{void (%struct.ResStructInStruct*)* @\"\\01?BadCallable@@YAXUResStructInStruct@@@Z\", "
        "!\"\\01?BadCallable@@YAXUResStructInStruct@@@Z\",",
    },
    {  "Function '\\01?BadAnyHit@@YAXUResStructInStruct@@UAttributes@@@Z' uses resource in function signature"
      ,"Function '\\01?BadClosestHit@@YAXUResStructInStruct@@UAttributes@@@Z' uses resource in function signature"
      ,"Function '\\01?BadMiss@@YAXUResStructInStruct@@@Z' uses resource in function signature"
      ,"Function '\\01?BadCallable@@YAXUResStructInStruct@@@Z' uses resource in function signature"
    },
    false);
}

TEST_F(ValidationTest, WhenPayloadSizeTooSmallThenFail) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
    "struct Payload { float f; }; struct Attributes { float2 b; };\n"
    "struct Param { float f; };\n"
    "[shader(\"raygeneration\")] void RayGenProto() { return; }\n"
    "[shader(\"intersection\")] void IntersectionProto() { return; }\n"
    "[shader(\"anyhit\")] void AnyHitProto(inout Payload p, in Attributes a) { p.f += a.b.x; }\n"
    "[shader(\"closesthit\")] void ClosestHitProto(inout Payload p, in Attributes a) { p.f += a.b.y; }\n"
    "[shader(\"miss\")] void MissProto(inout Payload p) { p.f += 1.0; }\n"
    "[shader(\"callable\")] void CallableProto(inout Param p) { p.f += 1.0; }\n"
    "\n"
    "struct BadPayload { float2 f; }; struct BadAttributes { float3 b; };\n"
    "struct BadParam { float2 f; };\n"
    "export void BadRayGen() { return; }\n"
    "export void BadIntersection() { return; }\n"
    "export void BadAnyHit(inout BadPayload p, in BadAttributes a) { p.f += a.b.x; }\n"
    "export void BadClosestHit(inout BadPayload p, in BadAttributes a) { p.f += a.b.y; }\n"
    "export void BadMiss(inout BadPayload p) { p.f += 1.0; }\n"
    "export void BadCallable(inout BadParam p) { p.f += 1.0; }\n"
    , "lib_6_3",
    {  "!{void ()* @\"\\01?RayGenProto@@YAXXZ\", !\"\\01?RayGenProto@@YAXXZ\","
      ,"!{void ()* @\"\\01?IntersectionProto@@YAXXZ\", !\"\\01?IntersectionProto@@YAXXZ\","
      ,"!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\", !\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\","
      ,"!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\", !\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\","
      ,"!{void (%struct.Payload*)* @\"\\01?MissProto@@YAXUPayload@@@Z\", !\"\\01?MissProto@@YAXUPayload@@@Z\","
      ,"!{void (%struct.Param*)* @\"\\01?CallableProto@@YAXUParam@@@Z\", !\"\\01?CallableProto@@YAXUParam@@@Z\","
    },
    {  "!{void ()* @\"\\01?BadRayGen@@YAXXZ\", !\"\\01?BadRayGen@@YAXXZ\","
      ,"!{void ()* @\"\\01?BadIntersection@@YAXXZ\", !\"\\01?BadIntersection@@YAXXZ\","
      ,"!{void (%struct.BadPayload*, %struct.BadAttributes*)* @\"\\01?BadAnyHit@@YAXUBadPayload@@UBadAttributes@@@Z\", !\"\\01?BadAnyHit@@YAXUBadPayload@@UBadAttributes@@@Z\","
      ,"!{void (%struct.BadPayload*, %struct.BadAttributes*)* @\"\\01?BadClosestHit@@YAXUBadPayload@@UBadAttributes@@@Z\", !\"\\01?BadClosestHit@@YAXUBadPayload@@UBadAttributes@@@Z\","
      ,"!{void (%struct.BadPayload*)* @\"\\01?BadMiss@@YAXUBadPayload@@@Z\", !\"\\01?BadMiss@@YAXUBadPayload@@@Z\","
      ,"!{void (%struct.BadParam*)* @\"\\01?BadCallable@@YAXUBadParam@@@Z\", !\"\\01?BadCallable@@YAXUBadParam@@@Z\","
    },
    {  "For shader '\\01?BadAnyHit@@YAXUBadPayload@@UBadAttributes@@@Z', payload size is smaller than argument's allocation size"
      ,"For shader '\\01?BadAnyHit@@YAXUBadPayload@@UBadAttributes@@@Z', attribute size is smaller than argument's allocation size"
      ,"For shader '\\01?BadClosestHit@@YAXUBadPayload@@UBadAttributes@@@Z', payload size is smaller than argument's allocation size"
      ,"For shader '\\01?BadClosestHit@@YAXUBadPayload@@UBadAttributes@@@Z', attribute size is smaller than argument's allocation size"
      ,"For shader '\\01?BadMiss@@YAXUBadPayload@@@Z', payload size is smaller than argument's allocation size"
      ,"For shader '\\01?BadCallable@@YAXUBadParam@@@Z', params size is smaller than argument's allocation size"
    },
    false);
}

TEST_F(ValidationTest, WhenMissingPayloadThenFail) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  RewriteAssemblyCheckMsg(
    "struct Payload { float f; }; struct Attributes { float2 b; };\n"
    "struct Param { float f; };\n"
    "[shader(\"anyhit\")] void AnyHitProto(inout Payload p, in Attributes a) { p.f += a.b.x; }\n"
    "[shader(\"closesthit\")] void ClosestHitProto(inout Payload p, in Attributes a) { p.f += a.b.y; }\n"
    "[shader(\"miss\")] void MissProto(inout Payload p) { p.f += 1.0; }\n"
    "[shader(\"callable\")] void CallableProto(inout Param p) { p.f += 1.0; }\n"
    "export void BadAnyHit(inout Payload p) { p.f += 1.0; }\n"
    "export void BadClosestHit() {}\n"
    "export void BadMiss() {}\n"
    "export void BadCallable() {}\n"
    , "lib_6_3",
    {  "!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\", !\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\","
      ,"!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\", !\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\","
      ,"!{void (%struct.Payload*)* @\"\\01?MissProto@@YAXUPayload@@@Z\", !\"\\01?MissProto@@YAXUPayload@@@Z\","
      ,"!{void (%struct.Param*)* @\"\\01?CallableProto@@YAXUParam@@@Z\", !\"\\01?CallableProto@@YAXUParam@@@Z\","
    },
    {  "!{void (%struct.Payload*)* @\"\\01?BadAnyHit@@YAXUPayload@@@Z\", !\"\\01?BadAnyHit@@YAXUPayload@@@Z\","
      ,"!{void ()* @\"\\01?BadClosestHit@@YAXXZ\", !\"\\01?BadClosestHit@@YAXXZ\","
      ,"!{void ()* @\"\\01?BadMiss@@YAXXZ\", !\"\\01?BadMiss@@YAXXZ\","
      ,"!{void ()* @\"\\01?BadCallable@@YAXXZ\", !\"\\01?BadCallable@@YAXXZ\","
    },
    {  "anyhit shader '\\01?BadAnyHit@@YAXUPayload@@@Z' missing required attributes parameter"
      ,"closesthit shader '\\01?BadClosestHit@@YAXXZ' missing required payload parameter"
      ,"closesthit shader '\\01?BadClosestHit@@YAXXZ' missing required attributes parameter"
      ,"miss shader '\\01?BadMiss@@YAXXZ' missing required payload parameter"
      ,"callable shader '\\01?BadCallable@@YAXXZ' missing required params parameter"
    },
    false);
}

TEST_F(ValidationTest, ShaderFunctionReturnTypeVoid) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  // Verify resource not used in shader argument structure
  RewriteAssemblyCheckMsg(
    "struct Payload { float f; }; struct Attributes { float2 b; };\n"
    "struct Param { float f; };\n"
    "[shader(\"raygeneration\")] void RayGenProto() { return; }\n"
    "[shader(\"anyhit\")] void AnyHitProto(inout Payload p, in Attributes a) { p.f += a.b.x; }\n"
    "[shader(\"closesthit\")] void ClosestHitProto(inout Payload p, in Attributes a) { p.f += a.b.y; }\n"
    "[shader(\"miss\")] void MissProto(inout Payload p) { p.f += 1.0; }\n"
    "[shader(\"callable\")] void CallableProto(inout Param p) { p.f += 1.0; }\n"
    "export float BadRayGen() { return 1; }\n"
    "export float BadAnyHit(inout Payload p, in Attributes a) { return p.f; }\n"
    "export float BadClosestHit(inout Payload p, in Attributes a) { return p.f; }\n"
    "export float BadMiss(inout Payload p) { return p.f; }\n"
    "export float BadCallable(inout Param p) { return p.f; }\n"
    , "lib_6_3",
    { "!{void ()* @\"\\01?RayGenProto@@YAXXZ\", "
        "!\"\\01?RayGenProto@@YAXXZ\",",
      "!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\", "
        "!\"\\01?AnyHitProto@@YAXUPayload@@UAttributes@@@Z\",",
      "!{void (%struct.Payload*, %struct.Attributes*)* @\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\", "
        "!\"\\01?ClosestHitProto@@YAXUPayload@@UAttributes@@@Z\",",
      "!{void (%struct.Payload*)* @\"\\01?MissProto@@YAXUPayload@@@Z\", "
        "!\"\\01?MissProto@@YAXUPayload@@@Z\",",
      "!{void (%struct.Param*)* @\"\\01?CallableProto@@YAXUParam@@@Z\", "
        "!\"\\01?CallableProto@@YAXUParam@@@Z\","
    },
    { "!{float ()* @\"\\01?BadRayGen@@YAMXZ\", "
        "!\"\\01?BadRayGen@@YAMXZ\",",
      "!{float (%struct.Payload*, %struct.Attributes*)* @\"\\01?BadAnyHit@@YAMUPayload@@UAttributes@@@Z\", "
        "!\"\\01?BadAnyHit@@YAMUPayload@@UAttributes@@@Z\",",
      "!{float (%struct.Payload*, %struct.Attributes*)* @\"\\01?BadClosestHit@@YAMUPayload@@UAttributes@@@Z\", "
        "!\"\\01?BadClosestHit@@YAMUPayload@@UAttributes@@@Z\",",
      "!{float (%struct.Payload*)* @\"\\01?BadMiss@@YAMUPayload@@@Z\", "
        "!\"\\01?BadMiss@@YAMUPayload@@@Z\",",
      "!{float (%struct.Param*)* @\"\\01?BadCallable@@YAMUParam@@@Z\", "
        "!\"\\01?BadCallable@@YAMUParam@@@Z\","
    },
    {  "Shader function '\\01?BadRayGen@@YAMXZ' must have void return type"
      ,"Shader function '\\01?BadAnyHit@@YAMUPayload@@UAttributes@@@Z' must have void return type"
      ,"Shader function '\\01?BadClosestHit@@YAMUPayload@@UAttributes@@@Z' must have void return type"
      ,"Shader function '\\01?BadMiss@@YAMUPayload@@@Z' must have void return type"
      ,"Shader function '\\01?BadCallable@@YAMUParam@@@Z' must have void return type"
    },
    false);
}
