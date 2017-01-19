///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ValidationTest.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <vector>
#include <string>
#include <algorithm>

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Regex.h"

#include <atlbase.h>

#include "WexTestClass.h"
#include "DxcTestUtils.h"
#include "HlslTestUtils.h"

using namespace std;

void CheckOperationSucceeded(IDxcOperationResult *pResult, IDxcBlob **ppBlob) {
  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
  VERIFY_SUCCEEDED(pResult->GetResult(ppBlob));
}

std::string DisassembleProgram(dxc::DxcDllSupport &dllSupport,
                               IDxcBlob *pProgram) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pDisassembly;

  if (!dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(dllSupport.Initialize());
  }

  VERIFY_SUCCEEDED(dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembly));
  return BlobToUtf8(pDisassembly);
}

class ValidationTest
{
public:
  BEGIN_TEST_CLASS(ValidationTest)
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_METHOD(WhenCorrectThenOK);
  TEST_METHOD(WhenMisalignedThenFail);
  TEST_METHOD(WhenEmptyFileThenFail);
  TEST_METHOD(WhenIncorrectMagicThenFail);
  TEST_METHOD(WhenIncorrectTargetTripleThenFail);
  TEST_METHOD(WhenIncorrectModelThenFail);
  TEST_METHOD(WhenIncorrectPSThenFail);

  TEST_METHOD(WhenWaveAffectsGradientThenFail);

  TEST_METHOD(WhenMultipleModulesThenFail);
  TEST_METHOD(WhenUnexpectedEOFThenFail);
  TEST_METHOD(WhenUnknownBlocksThenFail);

  TEST_METHOD(LoadOutputControlPointNotInPatchConstantFunction);
  TEST_METHOD(StorePatchControlNotInPatchConstantFunction);
  TEST_METHOD(OutputControlPointIDInPatchConstantFunction);
  TEST_METHOD(GsVertexIDOutOfBound)
  TEST_METHOD(StreamIDOutOfBound)
  TEST_METHOD(SignatureStreamIDForNonGS)
  TEST_METHOD(TypedUAVStoreFullMask0)
  TEST_METHOD(TypedUAVStoreFullMask1)
  TEST_METHOD(Recursive)
  TEST_METHOD(Recursive2)
  TEST_METHOD(UserDefineFunction)
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
  TEST_METHOD(AddrSpaceCast)
  TEST_METHOD(PtrBitCast)
  TEST_METHOD(MinPrecisionBitCast)
  TEST_METHOD(StructBitCast)
  TEST_METHOD(MultiDimArray)

  TEST_METHOD(ClipCullMaxComponents)
  TEST_METHOD(ClipCullMaxRows)
  TEST_METHOD(DuplicateSysValue)
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

  TEST_METHOD(WhenInstrDisallowedThenFail);
  TEST_METHOD(WhenDepthNotFloatThenFail);
  TEST_METHOD(BarrierFail);
  TEST_METHOD(CBufferLegacyOutOfBoundFail);
  TEST_METHOD(CBufferOutOfBoundFail);
  TEST_METHOD(CsThreadSizeFail);
  TEST_METHOD(DeadLoopFail);
  TEST_METHOD(EvalFail);
  TEST_METHOD(GetDimCalcLODFail);
  TEST_METHOD(HsAttributeFail);
  TEST_METHOD(InnerCoverageFail);
  TEST_METHOD(InterpChangeFail);
  TEST_METHOD(InterpOnIntFail);
  TEST_METHOD(InvalidSigCompTyFail);
  TEST_METHOD(MultiStream2Fail);
  TEST_METHOD(PhiTGSMFail);
  TEST_METHOD(ReducibleFail);
  TEST_METHOD(SampleBiasFail);
  TEST_METHOD(SamplerKindFail);
  TEST_METHOD(SemaOverlapFail);
  TEST_METHOD(SigOutOfRangeFail);
  TEST_METHOD(SigOverlapFail);
  TEST_METHOD(SimpleHs1Fail);
  TEST_METHOD(SimpleHs3Fail);
  TEST_METHOD(SimpleHs4Fail);
  TEST_METHOD(SimpleDs1Fail);
  TEST_METHOD(SimpleGs1Fail);
  TEST_METHOD(UavBarrierFail);
  TEST_METHOD(UndefValueFail);
  TEST_METHOD(UpdateCounterFail);

  TEST_METHOD(WhenSmUnknownThenFail);
  TEST_METHOD(WhenSmLegacyThenFail);

  TEST_METHOD(WhenMetaFlagsUsageDeclThenOK);
  TEST_METHOD(WhenMetaFlagsUsageThenFail);

  dxc::DxcDllSupport m_dllSupport;

  void TestCheck(LPCWSTR name) {
    std::wstring fullPath = hlsl_test::GetPathToHlslDataFile(name);
    FileRunTestResult t = FileRunTestResult::RunFromFileCommands(fullPath.c_str());
    if (t.RunResult != 0) {
      CA2W commentWide(t.ErrorMessage.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(commentWide);
      WEX::Logging::Log::Error(L"Run result is not zero");
    }
  }

  bool CheckOperationResultMsg(IDxcOperationResult *pResult,
                               const char *pErrorMsg, bool maySucceedAnyway,
                               bool bRegex) {
    HRESULT status;
    VERIFY_SUCCEEDED(pResult->GetStatus(&status));
    if (pErrorMsg == nullptr) {
      VERIFY_SUCCEEDED(status);
    }
    else {
      if (SUCCEEDED(status) && maySucceedAnyway) {
        return false;
      }
      //VERIFY_FAILED(status);
      CComPtr<IDxcBlobEncoding> text;
      VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&text));
      if (bRegex) {
        llvm::Regex RE(pErrorMsg);
        std::string reErrors;
        VERIFY_IS_TRUE(RE.isValid(reErrors));
        VERIFY_IS_TRUE(RE.match(llvm::StringRef((const char *)text->GetBufferPointer(), text->GetBufferSize())));
      } else {
        const char *pStart = (const char *)text->GetBufferPointer();
        const char *pEnd = pStart + text->GetBufferSize();
        const char *pMatch = std::search(pStart, pEnd, pErrorMsg, pErrorMsg + strlen(pErrorMsg));
        if (pEnd == pMatch) {
          WEX::Logging::Log::Comment(WEX::Common::String().Format(
              L"Unable to find '%S' in text:\r\n%.*S", pErrorMsg, (pEnd - pStart),
              pStart));
        }
        VERIFY_ARE_NOT_EQUAL(pEnd, pMatch);
      }
    }
    return true;
  }

  void CheckValidationMsg(IDxcBlob *pBlob, const char *pErrorMsg, bool bRegex = false) {
    CComPtr<IDxcValidator> pValidator;
    CComPtr<IDxcOperationResult> pResult;

    if (!m_dllSupport.IsEnabled()) {
      VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    }

    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcValidator, &pValidator));
    VERIFY_SUCCEEDED(pValidator->Validate(pBlob, DxcValidatorFlags_Default, &pResult));

    CheckOperationResultMsg(pResult, pErrorMsg, false, bRegex);
  }

  void CheckValidationMsg(const char *pBlob, size_t blobSize, const char *pErrorMsg, bool bRegex = false) {
    if (!m_dllSupport.IsEnabled()) {
      VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    }
    CComPtr<IDxcLibrary> pLibrary;
    CComPtr<IDxcBlobEncoding> pBlobEncoding; // Encoding doesn't actually matter, it's binary.
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    VERIFY_SUCCEEDED(pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)pBlob, blobSize, CP_UTF8, &pBlobEncoding));
    CheckValidationMsg(pBlobEncoding, pErrorMsg, bRegex);
  }

  void CompileSource(IDxcBlobEncoding *pSource, LPCSTR pShaderModel,
                     IDxcBlob **pResultBlob) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlob> pProgram;

    if (!m_dllSupport.IsEnabled()) {
      VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    }

    CA2W shWide(pShaderModel, CP_UTF8);
    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"main",
                                        shWide, nullptr, 0, nullptr, 0, nullptr,
                                        &pResult));
    VERIFY_SUCCEEDED(pResult->GetResult(pResultBlob));
  }

  void CompileSource(LPCSTR pSource, LPCSTR pShaderModel,
                     IDxcBlob **pResultBlob) {
    if (!m_dllSupport.IsEnabled()) {
      VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    }
    CComPtr<IDxcBlobEncoding> pSourceBlob;
    Utf8ToBlob(m_dllSupport, pSource, &pSourceBlob);
    CompileSource(pSourceBlob, pShaderModel, pResultBlob);
  }

  void DisassembleProgram(IDxcBlob *pProgram, std::string *text) {
    *text = ::DisassembleProgram(m_dllSupport, pProgram);
  }

  void RewriteAssemblyCheckMsg(LPCSTR pSource, LPCSTR pShaderModel,
                               LPCSTR pLookFor, LPCSTR pReplacement,
                               LPCSTR pErrorMsg, bool bRegex = false) {
    CComPtr<IDxcBlob> pText;
    CComPtr<IDxcBlobEncoding> pSourceBlob;
    
    if (!m_dllSupport.IsEnabled()) {
      VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    }

    Utf8ToBlob(m_dllSupport, pSource, &pSourceBlob);

    RewriteAssemblyToText(pSourceBlob, pShaderModel, pLookFor, pReplacement, &pText, bRegex);

    CComPtr<IDxcAssembler> pAssembler;
    CComPtr<IDxcOperationResult> pAssembleResult;
    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
    VERIFY_SUCCEEDED(pAssembler->AssembleToContainer(pText, &pAssembleResult));

    if (!CheckOperationResultMsg(pAssembleResult, pErrorMsg, true, bRegex)) {
      // Assembly succeeded, try validation.
      CComPtr<IDxcBlob> pBlob;
      VERIFY_SUCCEEDED(pAssembleResult->GetResult(&pBlob));
      CheckValidationMsg(pBlob, pErrorMsg, bRegex);
    }
  }

  void RewriteAssemblyToText(IDxcBlobEncoding *pSource, LPCSTR pShaderModel,
                             LPCSTR pLookFor, LPCSTR pReplacement,
                             IDxcBlob **pBlob, bool bRegex = false) {
    CComPtr<IDxcBlob> pProgram;
    std::string disassembly;
    CompileSource(pSource, pShaderModel, &pProgram);
    DisassembleProgram(pProgram, &disassembly);
    if (pLookFor && *pLookFor) {
      if (bRegex) {
        llvm::Regex RE(pLookFor);
        std::string reErrors;
        VERIFY_IS_TRUE(RE.isValid(reErrors));
        std::string replaced = RE.sub(pReplacement, disassembly, &reErrors);
        VERIFY_ARE_NOT_EQUAL(disassembly, replaced);
        VERIFY_IS_TRUE(reErrors.empty());
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
        VERIFY_IS_TRUE(found);
      }
    }
    Utf8ToBlob(m_dllSupport, disassembly.c_str(), pBlob);
  }
  
  void RewriteAssemblyCheckMsg(LPCWSTR name, LPCSTR pShaderModel,
                               LPCSTR pLookFor, LPCSTR pReplacement,
                               LPCSTR pErrorMsg, bool bRegex = false) {
    std::wstring fullPath = hlsl_test::GetPathToHlslDataFile(name);
    CComPtr<IDxcLibrary> pLibrary;
    CComPtr<IDxcBlobEncoding> pSource;
    if (!m_dllSupport.IsEnabled()) {
      VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    }
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    VERIFY_SUCCEEDED(
        pLibrary->CreateBlobFromFile(fullPath.c_str(), nullptr, &pSource));

    CComPtr<IDxcBlob> pText;

    RewriteAssemblyToText(pSource, pShaderModel, pLookFor, pReplacement, &pText);

    CComPtr<IDxcAssembler> pAssembler;
    CComPtr<IDxcOperationResult> pAssembleResult;
    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
    VERIFY_SUCCEEDED(pAssembler->AssembleToContainer(pText, &pAssembleResult));

    if (!CheckOperationResultMsg(pAssembleResult, pErrorMsg, true, bRegex)) {
      // Assembly succeeded, try validation.
      CComPtr<IDxcBlob> pBlob;
      VERIFY_SUCCEEDED(pAssembleResult->GetResult(&pBlob));
      CheckValidationMsg(pBlob, pErrorMsg, bRegex);
    }
  }
};

TEST_F(ValidationTest, WhenCorrectThenOK) {
  CComPtr<IDxcBlob> pProgram;
  CompileSource("float4 main() : SV_Target { return 1; }", "ps_6_0", &pProgram);
  CheckValidationMsg(pProgram, nullptr);
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
  CheckValidationMsg(blob, _countof(blob), "Invalid bitcode size");
}

TEST_F(ValidationTest, WhenEmptyFileThenFail) {
  // No blocks after signature.
  const char blob[] = {
    'B', 'C', 0xc0, 0xde
  };
  CheckValidationMsg(blob, _countof(blob), "Malformed IR file");
}

TEST_F(ValidationTest, WhenIncorrectMagicThenFail) {
  // Signature isn't 'B', 'C', 0xC0 0xDE
  const char blob[] = {
    'B', 'C', 0xc0, 0xdd
  };
  CheckValidationMsg(blob, _countof(blob), "Invalid bitcode signature");
}

TEST_F(ValidationTest, WhenIncorrectTargetTripleThenFail) {
  const char blob[] = {
    'B', 'C', 0xc0, 0xde
  };
  CheckValidationMsg(blob, _countof(blob), "Malformed IR file");
}

TEST_F(ValidationTest, WhenMultipleModulesThenFail) {
  const char blob[] = {
    'B', 'C', 0xc0, 0xde,
    0x21, 0x0c, 0x00, 0x00, // Enter sub-block, BlockID = 8, Code Size=3, padding x2
    0x00, 0x00, 0x00, 0x00, // NumWords = 0
    0x08, 0x00, 0x00, 0x00, // End-of-block, padding
    // At this point, this is valid bitcode (but missing required DXIL metadata)
    // Trigger the case we're looking for now
    0x21, 0x0c, 0x00, 0x00, // Enter sub-block, BlockID = 8, Code Size=3, padding x2
  };
  CheckValidationMsg(blob, _countof(blob), "Unused bits in buffer");
}

TEST_F(ValidationTest, WhenUnexpectedEOFThenFail) {
  // Importantly, this is testing the usage of report_fatal_error during deserialization.
  const char blob[] = {
    'B', 'C', 0xc0, 0xde,
    0x21, 0x0c, 0x00, 0x00, // Enter sub-block, BlockID = 8, Code Size=3, padding x2
    0x00, 0x00, 0x00, 0x00, // NumWords = 0
  };
  CheckValidationMsg(blob, _countof(blob), "Invalid record");
}

TEST_F(ValidationTest, WhenUnknownBlocksThenFail) {
  const char blob[] = {
    'B', 'C', 0xc0, 0xde,   // Signature
    0x31, 0x00, 0x00, 0x00  // Enter sub-block, BlockID != 8
  };
  CheckValidationMsg(blob, _countof(blob), "Unrecognized block found");
}

TEST_F(ValidationTest, WhenInstrDisallowedThenFail) {
  TestCheck(L"val-inst-disallowed.ll");
}

TEST_F(ValidationTest, WhenDepthNotFloatThenFail) {
  TestCheck(L"dxil_validation\\IntegerDepth.ll");
}

TEST_F(ValidationTest, BarrierFail) {
  TestCheck(L"dxil_validation\\barrier.ll");
}
TEST_F(ValidationTest, CBufferLegacyOutOfBoundFail) {
  TestCheck(L"dxil_validation\\cbuffer1.50_legacy.ll");
}
TEST_F(ValidationTest, CBufferOutOfBoundFail) {
  TestCheck(L"dxil_validation\\cbuffer1.50.ll");
}
TEST_F(ValidationTest, CsThreadSizeFail) {
  TestCheck(L"dxil_validation\\csThreadSize.ll");
}
TEST_F(ValidationTest, DeadLoopFail) {
  TestCheck(L"dxil_validation\\deadloop.ll");
}
TEST_F(ValidationTest, EvalFail) {
  TestCheck(L"dxil_validation\\Eval.ll");
}
TEST_F(ValidationTest, GetDimCalcLODFail) {
  TestCheck(L"dxil_validation\\GetDimCalcLOD.ll");
}
TEST_F(ValidationTest, HsAttributeFail) {
  TestCheck(L"dxil_validation\\hsAttribute.ll");
}
TEST_F(ValidationTest, InnerCoverageFail) {
  TestCheck(L"dxil_validation\\InnerCoverage.ll");
}
TEST_F(ValidationTest, InterpChangeFail) {
  TestCheck(L"dxil_validation\\interpChange.ll");
}
TEST_F(ValidationTest, InterpOnIntFail) {
  TestCheck(L"dxil_validation\\interpOnInt.ll");
}
TEST_F(ValidationTest, InvalidSigCompTyFail) {
  TestCheck(L"dxil_validation\\invalidSigCompTy.ll");
}
TEST_F(ValidationTest, MultiStream2Fail) {
  TestCheck(L"dxil_validation\\multiStream2.ll");
}
TEST_F(ValidationTest, PhiTGSMFail) {
  TestCheck(L"dxil_validation\\phiTGSM.ll");
}
TEST_F(ValidationTest, ReducibleFail) {
  TestCheck(L"dxil_validation\\reducible.ll");
}
TEST_F(ValidationTest, SampleBiasFail) {
  TestCheck(L"dxil_validation\\sampleBias.ll");
}
TEST_F(ValidationTest, SamplerKindFail) {
  TestCheck(L"dxil_validation\\samplerKind.ll");
}
TEST_F(ValidationTest, SemaOverlapFail) {
  TestCheck(L"dxil_validation\\semaOverlap.ll");
}
TEST_F(ValidationTest, SigOutOfRangeFail) {
  TestCheck(L"dxil_validation\\sigOutOfRange.ll");
}
TEST_F(ValidationTest, SigOverlapFail) {
  TestCheck(L"dxil_validation\\sigOverlap.ll");
}
TEST_F(ValidationTest, SimpleHs1Fail) {
  TestCheck(L"dxil_validation\\SimpleHs1.ll");
}
TEST_F(ValidationTest, SimpleHs3Fail) {
  TestCheck(L"dxil_validation\\SimpleHs3.ll");
}
TEST_F(ValidationTest, SimpleHs4Fail) {
  TestCheck(L"dxil_validation\\SimpleHs4.ll");
}
TEST_F(ValidationTest, SimpleDs1Fail) {
  TestCheck(L"dxil_validation\\SimpleDs1.ll");
}
TEST_F(ValidationTest, SimpleGs1Fail) {
  TestCheck(L"dxil_validation\\SimpleGs1.ll");
}
TEST_F(ValidationTest, UavBarrierFail) {
  TestCheck(L"dxil_validation\\uavBarrier.ll");
}
TEST_F(ValidationTest, UndefValueFail) {
  TestCheck(L"dxil_validation\\UndefValue.ll");
}
TEST_F(ValidationTest, UpdateCounterFail) {
  TestCheck(L"dxil_validation\\UpdateCounter.ll");
}

TEST_F(ValidationTest, WhenIncorrectModelThenFail) {
  TestCheck(L"val-failures.hlsl");
}

TEST_F(ValidationTest, WhenIncorrectPSThenFail) {
  TestCheck(L"val-failures-ps.hlsl");
}

TEST_F(ValidationTest, WhenSmUnknownThenFail) {
  RewriteAssemblyCheckMsg("float4 main() : SV_Target { return 1; }", "ps_6_0",
                          "{!\"ps\", i32 6, i32 0}", "{!\"ps\", i32 1, i32 2}",
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
      L"..\\CodeGenHLSL\\SimpleGs1.hlsl", "gs_6_0",
      "dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 0)",
      "dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 1)", 
      "expect VertexID between 0~1, got 1");
}

TEST_F(ValidationTest, StreamIDOutOfBound) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleGs1.hlsl", "gs_6_0",
      "dx.op.emitStream(i32 99, i8 0)",
      "dx.op.emitStream(i32 99, i8 1)", 
      "expect StreamID between 0 , got 1");
}

TEST_F(ValidationTest, SignatureStreamIDForNonGS) {
  RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\abs1.hlsl", "ps_6_0",
      ", i8 0, i32 1, i8 4, i32 0, i8 0, null}",
      ", i8 0, i32 1, i8 4, i32 0, i8 0, !19}\n!19 = !{i32 0, i32 1}", 
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
    TestCheck(L"..\\CodeGenHLSL\\recursive.hlsl");
}

TEST_F(ValidationTest, Recursive2) {
    TestCheck(L"..\\CodeGenHLSL\\recursive2.hlsl");
}

TEST_F(ValidationTest, UserDefineFunction) {
    TestCheck(L"..\\CodeGenHLSL\\recursive2.hlsl");
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
      "!7 = !{i32 1, i32 52}",
      "!7 = !{i32 1, i32 50}",
      "structured buffer element size must be a multiple of 4 bytes (actual size 50 bytes)");
}

TEST_F(ValidationTest, StructBufStrideOutOfBound) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\struct_buf1.hlsl", "ps_6_0",
      "!7 = !{i32 1, i32 52}",
      "!7 = !{i32 1, i32 2052}",
      "structured buffer elements cannot be larger than 2048 bytes (actual size 2052 bytes)");
}

TEST_F(ValidationTest, StructBufLoadCoordinates) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\struct_buf1.hlsl", "ps_6_0",
      "bufferLoad.f32(i32 69, %dx.types.Handle %buf1_texture_structbuf, i32 1, i32 8)",
      "bufferLoad.f32(i32 69, %dx.types.Handle %buf1_texture_structbuf, i32 1, i32 undef)",
      "structured buffer require 2 coordinates");
}

TEST_F(ValidationTest, StructBufStoreCoordinates) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\struct_buf1.hlsl", "ps_6_0",
      "bufferStore.f32(i32 70, %dx.types.Handle %buf2_UAV_structbuf, i32 0, i32 0",
      "bufferStore.f32(i32 70, %dx.types.Handle %buf2_UAV_structbuf, i32 0, i32 undef",
      "structured buffer require 2 coordinates");
}

TEST_F(ValidationTest, TypedBufRetType) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\sample5.hlsl", "ps_6_0",
      "%class.Texture2D = type { <4 x float>",
      "%class.Texture2D = type { <4 x double>",
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
      L"..\\CodeGenHLSL\\SimpleGs1.hlsl", "gs_6_0",
      "!\"POSSIZE\", i8 9, i8 0",
      "!\"VertexID\", i8 4, i8 1",
      "Semantic 'VertexID' is invalid as gs Input");
}

TEST_F(ValidationTest, GsOutputSemantic) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\SimpleGs1.hlsl", "gs_6_0",
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
      "i32 6, !\"SV_Target\", i8 9, i8 16, !36, i8 0, i32 1",
      "i32 6, !\"SV_Target\", i8 9, i8 16, !36, i8 0, i32 2",
      "Pixel shader output registers are not indexable.");
}

TEST_F(ValidationTest, InfiniteLog) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "op.unary.f32(i32 22, float %3)",
      "op.unary.f32(i32 22, float 0x7FF0000000000000)",
      "No indefinite logarithm");
}

TEST_F(ValidationTest, InfiniteAsin) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "op.unary.f32(i32 16, float %3)",
      "op.unary.f32(i32 16, float 0x7FF0000000000000)",
      "No indefinite arcsine");
}

TEST_F(ValidationTest, InfiniteAcos) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "op.unary.f32(i32 15, float %3)",
      "op.unary.f32(i32 15, float 0x7FF0000000000000)",
      "No indefinite arccosine");
}

TEST_F(ValidationTest, InfiniteDdxDdy) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "op.unary.f32(i32 86, float %3)",
      "op.unary.f32(i32 86, float 0x7FF0000000000000)",
      "No indefinite derivative calculation");
}

TEST_F(ValidationTest, IDivByZero) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "sdiv i32 %8, %9",
      "sdiv i32 %8, 0",
      "No signed integer division by zero");
}

TEST_F(ValidationTest, UDivByZero) {
    RewriteAssemblyCheckMsg(
      L"..\\CodeGenHLSL\\intrinsic_val_imm.hlsl", "ps_6_0",
      "udiv i32 %5, %6",
      "udiv i32 %5, 0",
      "No unsigned integer division by zero");
}

TEST_F(ValidationTest, UnusedMetadata) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\loop2.hlsl", "ps_6_0",
                          ", !llvm.loop ",
                          ", !llvm.loop2 ",
                          "All metadata must be used by dxil");
}

TEST_F(ValidationTest, MemoryOutOfBound) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\targetArray.hlsl", "ps_6_0",
                          "getelementptr [4 x float], [4 x float]* %12, i32 0, i32 3",
                          "getelementptr [4 x float], [4 x float]* %12, i32 0, i32 10",
                          "Access to out-of-bounds memory is disallowed");
}

TEST_F(ValidationTest, AddrSpaceCast) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\staticGlobals.hlsl", "ps_6_0",
                          "%12 = getelementptr [4 x float], [4 x float]* %1, i32 0, i32 0\n"
                          "  store float %11, float* %12, align 4",
                          "%12 = getelementptr [4 x float], [4 x float]* %1, i32 0, i32 0\n"
                          "  %X = addrspacecast float* %12 to float addrspace(1)*    \n"
                          "  store float %11, float addrspace(1)* %X, align 4",
                          "generic address space");
}

TEST_F(ValidationTest, PtrBitCast) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\staticGlobals.hlsl", "ps_6_0",
                          "%12 = getelementptr [4 x float], [4 x float]* %1, i32 0, i32 0\n"
                          "  store float %11, float* %12, align 4",
                          "%12 = getelementptr [4 x float], [4 x float]* %1, i32 0, i32 0\n"
                          "  %X = bitcast float* %12 to double*    \n"
                          "  store float %11, float* %12, align 4",
                          "Pointer type bitcast must be have same size");
}

TEST_F(ValidationTest, MinPrecisionBitCast) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\staticGlobals.hlsl", "ps_6_0",
                          "%12 = getelementptr [4 x float], [4 x float]* %1, i32 0, i32 0\n"
                          "  store float %11, float* %12, align 4",
                          "%12 = getelementptr [4 x float], [4 x float]* %1, i32 0, i32 0\n"
                          "  %X = bitcast float* %12 to [2 x half]*    \n"
                          "  store float %11, float* %12, align 4",
                          "Bitcast on minprecison types is not allowed");
}

TEST_F(ValidationTest, StructBitCast) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\staticGlobals.hlsl", "ps_6_0",
                          "%12 = getelementptr [4 x float], [4 x float]* %1, i32 0, i32 0\n"
                          "  store float %11, float* %12, align 4",
                          "%12 = getelementptr [4 x float], [4 x float]* %1, i32 0, i32 0\n"
                          "  %X = bitcast float* %12 to %dx.types.Handle*    \n"
                          "  store float %11, float* %12, align 4",
                          "Bitcast on struct types is not allowed");
}

TEST_F(ValidationTest, MultiDimArray) {
  RewriteAssemblyCheckMsg(L"..\\CodeGenHLSL\\staticGlobals.hlsl", "ps_6_0",
                          "%1 = alloca [4 x float]",
                          "%1 = alloca [4 x float]\n"
                          "  %md = alloca [2 x [4 x float]]",
                          "Array Type only allow one dimension");
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
      "dx.op.storePatchConstant.f32(i32 108",
      "opcode 'StorePatchConstant' should only used in 'PatchConstant function'");
}

TEST_F(ValidationTest, LoadOutputControlPointNotInPatchConstantFunction) {
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
      "dx.op.loadOutputControlPoint.f32(i32 105",
      "opcode 'LoadOutputControlPoint' should only used in 'PatchConstant function'");
}

TEST_F(ValidationTest, OutputControlPointIDInPatchConstantFunction) {
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
      "call i32 @dx.op.outputControlPointID.i32(i32 109)\n ret void",
      "opcode 'OutputControlPointID' should only used in 'hull function'");
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
    "!{i32 0, !\"SV_Target\", i8 9, i8 16, ![0-9]+, i8 0, i32 1, i8 4, i32 7, i8 0, null}",
    "!{i32 0, !\"SV_Target\", i8 9, i8 16, !101, i8 0, i32 1, i8 4, i32 8, i8 0, null}\n!101 = !{i32 8}",
    "SV_Target semantic index exceeds maximum \\(7\\)",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemTargetIndexMatchesRow) {
  RewriteAssemblyCheckMsg(" \
float4 main(float4 col : COLOR) : SV_Target7 { return col; } \
    ",
    "ps_6_0", 
    "!{i32 0, !\"SV_Target\", i8 9, i8 16, !([0-9]+), i8 0, i32 1, i8 4, i32 7, i8 0, null}",
    "!{i32 0, !\"SV_Target\", i8 9, i8 16, !\\1, i8 0, i32 1, i8 4, i32 6, i8 0, null}",
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
    "!{i32 1, !\"SV_InsideTessFactor\", i8 9, i8 26, !([0-9]+), i8 0, i32 1, i8 1, i32 3, i8 0, null}",
    "!{i32 1, !\"SV_InsideTessFactor\", i8 9, i8 26, !\\1, i8 0, i32 2, i8 1, i32 3, i8 0, null}",
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

    "= !{i32 1, !\"f2out\", i8 9, i8 0, !([0-9]+), i8 2, i32 1, i8 2, i32 2, i8 0, null}\n"
    "!([0-9]+) = !{i32 2, !\"f3out\", i8 9, i8 0, !([0-9]+), i8 2, i32 1, i8 3, i32 1, i8 0, null}\n"
    "!([0-9]+) = !{i32 3, !\"SV_ClipDistance\", i8 9, i8 6, !([0-9]+), i8 2, i32 1, i8 2, i32 3, i8 0, null}\n"
    "!([0-9]+) = !{i32 4, !\"SV_CullDistance\", i8 9, i8 7, !([0-9]+), i8 2, i32 1, i8 1, i32 3, i8 2, null}\n",

    "= !{i32 1, !\"f2out\", i8 9, i8 0, !\\1, i8 2, i32 1, i8 2, i32 2, i8 2, null}\n"
    "!\\2 = !{i32 2, !\"f3out\", i8 9, i8 0, !\\3, i8 2, i32 1, i8 3, i32 1, i8 1, null}\n"
    "!\\4 = !{i32 3, !\"SV_ClipDistance\", i8 9, i8 6, !\\5, i8 2, i32 1, i8 2, i32 2, i8 0, null}\n"
    "!\\6 = !{i32 4, !\"SV_CullDistance\", i8 9, i8 7, !\\7, i8 2, i32 1, i8 1, i32 1, i8 0, null}\n",

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
    "!([0-9]+) = !{i32 3, !\"SV_IsFrontFace\", i8 1, i8 13, !([0-9]+), i8 1, i32 1, i8 1, i32 1, i8 3, null}\n",

    "= !{i32 1, !\"Value\", i8 5, i8 0, !\\1, i8 1, i32 1, i8 2, i32 1, i8 2, null}\n"
    "!\\2 = !{i32 2, !\"SV_PrimitiveID\", i8 5, i8 10, !\\3, i8 1, i32 1, i8 1, i32 1, i8 0, null}\n"
    "!\\4 = !{i32 3, !\"SV_IsFrontFace\", i8 1, i8 13, !\\5, i8 1, i32 1, i8 1, i32 1, i8 1, null}\n",

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

    "= !{i32 1, !\"Value\", i8 5, i8 0, !([0-9]+), i8 1, i32 1, i8 1, i32 1, i8 0, null}\n"
    "!([0-9]+) = !{i32 2, !\"SV_PrimitiveID\", i8 5, i8 10, !([0-9]+), i8 1, i32 1, i8 1, i32 1, i8 2, null}\n"
    "!([0-9]+) = !{i32 3, !\"SV_IsFrontFace\", i8 1, i8 13, !([0-9]+), i8 1, i32 1, i8 1, i32 1, i8 3, null}\n"
    "!([0-9]+) = !{i32 4, !\"ViewPortArrayIndex\", i8 5, i8 0, !([0-9]+), i8 1, i32 1, i8 1, i32 1, i8 1, null}\n",

    "= !{i32 1, !\"Value\", i8 5, i8 0, !\\1, i8 1, i32 1, i8 1, i32 1, i8 1, null}\n"
    "!\\2 = !{i32 2, !\"SV_PrimitiveID\", i8 5, i8 10, !\\3, i8 1, i32 1, i8 1, i32 1, i8 0, null}\n"
    "!\\4 = !{i32 3, !\"SV_IsFrontFace\", i8 1, i8 13, !\\5, i8 1, i32 1, i8 1, i32 1, i8 2, null}\n"
    "!\\6 = !{i32 4, !\"ViewPortArrayIndex\", i8 5, i8 0, !\\7, i8 1, i32 1, i8 1, i32 1, i8 3, null}\n",

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
    "!{i32 1, !\"SV_InsideTessFactor\", i8 9, i8 26, !([0-9]+), i8 0, i32 2, i8 1, i32 4, i8 3, null}",
    "!{i32 1, !\"SV_InsideTessFactor\", i8 9, i8 26, !\\1, i8 0, i32 2, i8 1, i32 0, i8 2, null}",
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
    "!{i32 2, !\"Arb\", i8 9, i8 0, !([0-9]+), i8 0, i32 3, i8 1, i32 0, i8 0, null}",
    "!{i32 2, !\"Arb\", i8 9, i8 0, !\\1, i8 0, i32 3, i8 1, i32 31, i8 0, null}",
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

    "!{i32 1, !\"Array\", i8 5, i8 0, !([0-9]+), i8 1, i32 2, i8 1, i32 1, i8 0, null}\n"
    "!([0-9]+) = !{i32 2, !\"Value\", i8 5, i8 0, !([0-9]+), i8 1, i32 1, i8 3, i32 1, i8 1, null}",

    "!{i32 1, !\"Array\", i8 5, i8 0, !\\1, i8 1, i32 2, i8 1, i32 1, i8 1, null}\n"
    "!\\2 = !{i32 2, !\"Value\", i8 5, i8 0, !\\3, i8 1, i32 1, i8 3, i32 2, i8 0, null}",

    "signature element Value at location \\(2,0\\) size \\(1,3\\) overlaps another signature element.",
    /*bRegex*/true);
}

TEST_F(ValidationTest, SemMultiDepth) {
  RewriteAssemblyCheckMsg(" \
float4 main(float4 f4 : Input, out float d0 : SV_Depth, out float d1 : SV_Target1) : SV_Target \
{ d0 = f4.z; d1 = f4.w; return f4; } \
    ",
    "ps_6_0",

    "!{i32 1, !\"SV_Target\", i8 9, i8 16, !([0-9]+), i8 0, i32 1, i8 1, i32 1, i8 0, null}\n"
    "!([0-9]+) = !{i32 2, !\"SV_Target\", i8 9, i8 16, !([0-9]+), i8 0, i32 1, i8 4, i32 0, i8 0, null}",

    "!{i32 1, !\"SV_DepthGreaterEqual\", i8 9, i8 19, !\\3, i8 0, i32 1, i8 1, i32 -1, i8 -1, null}\n"
    "!\\2 = !{i32 2, !\"SV_Target\", i8 9, i8 16, !\\3, i8 0, i32 1, i8 4, i32 0, i8 0, null}",

    "Pixel Shader only allows one type of depth semantic to be declared",
    /*bRegex*/true);
}




// TODO: reject non-zero padding
