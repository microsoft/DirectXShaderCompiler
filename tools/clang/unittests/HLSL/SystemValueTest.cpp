///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SystemValueTest.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Test system values at various signature points                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <vector>
#include <string>
#include <cassert>
#include <sstream>
#include <algorithm>
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"

#include "WexTestClass.h"
#include "HlslTestUtils.h"
#include "DxcTestUtils.h"

#include "llvm/Support/raw_os_ostream.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Unicode.h"

#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/DxilSemantic.h"
#include "dxc/HLSL/DxilSigPoint.h"
#include "dxc/HLSL/DxilShaderModel.h"

#include <fstream>

using namespace std;
using namespace hlsl_test;
using namespace hlsl;

class SystemValueTest {
public:
  BEGIN_TEST_CLASS(SystemValueTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  TEST_METHOD(VerifyArbitrarySupport)
  TEST_METHOD(VerifyNotAvailableFail)
  TEST_METHOD(VerifySVAsArbitrary)
  TEST_METHOD(VerifySVAsSV)
  TEST_METHOD(VerifySGV)
  TEST_METHOD(VerifySVNotPacked)
  TEST_METHOD(VerifySVNotInSig)
  TEST_METHOD(VerifyVertexPacking)
  TEST_METHOD(VerifyPatchConstantPacking)
  TEST_METHOD(VerifyTargetPacking)
  TEST_METHOD(VerifyTessFactors)
  TEST_METHOD(VerifyShadowEntries)
  TEST_METHOD(VerifyVersionedSemantics)
  TEST_METHOD(VerifyMissingSemanticFailure)

  void CompileHLSLTemplate(CComPtr<IDxcOperationResult> &pResult, DXIL::SigPointKind sigPointKind, DXIL::SemanticKind semKind, bool addArb, unsigned Major = 6, unsigned Minor = 0) {
    const Semantic *sem = Semantic::Get(semKind);
    const char* pSemName = sem->GetName();
    std::wstring sigDefValue(L"");
    if(semKind < DXIL::SemanticKind::Invalid && pSemName) {
      if(Semantic::HasSVPrefix(pSemName))
        pSemName += 3;
      CA2W semNameW(pSemName, CP_UTF8);
      sigDefValue = L"Def_";
      sigDefValue += semNameW;
    }
    if (addArb) {
      if (!sigDefValue.empty())
        sigDefValue += L" ";
      sigDefValue += L"Def_Arb(uint, arb0, ARB0)";
    }
    return CompileHLSLTemplate(pResult, sigPointKind, sigDefValue, Major, Minor);
  }

  void CompileHLSLTemplate(CComPtr<IDxcOperationResult> &pResult, DXIL::SigPointKind sigPointKind, const std::wstring &sigDefValue, unsigned Major = 6, unsigned Minor = 0) {
    const SigPoint * sigPoint = SigPoint::GetSigPoint(sigPointKind);
    DXIL::ShaderKind shaderKind = sigPoint->GetShaderKind();

    std::wstring path = hlsl_test::GetPathToHlslDataFile(L"system-values.hlsl");

    CComPtr<IDxcCompiler> pCompiler;

    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));

    if (!m_pSource) {
      CComPtr<IDxcLibrary> library;
      IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
      IFT(library->CreateBlobFromFile(path.c_str(), nullptr, &m_pSource));
    }

    std::string entry, profile;
    switch(shaderKind) {
      case DXIL::ShaderKind::Vertex: entry = "VSMain"; profile = "vs_6_0"; break;
      case DXIL::ShaderKind::Pixel: entry = "PSMain"; profile = "ps_6_0"; break;
      case DXIL::ShaderKind::Geometry: entry = "GSMain"; profile = "gs_6_0"; break;
      case DXIL::ShaderKind::Hull: entry = "HSMain"; profile = "hs_6_0"; break;
      case DXIL::ShaderKind::Domain: entry = "DSMain"; profile = "ds_6_0"; break;
      case DXIL::ShaderKind::Compute: entry = "CSMain"; profile = "cs_6_0"; break;
    }
    if (Major != 6 || Minor != 0) {
      llvm::Twine p = llvm::Twine(profile.substr(0, 3)) + llvm::Twine(Major) + " " + llvm::Twine(Minor);
      profile = p.str();
    }

    CA2W entryW(entry.c_str(), CP_UTF8);
    CA2W profileW(profile.c_str(), CP_UTF8);

    CA2W sigPointNameW(sigPoint->GetName(), CP_UTF8);
    // Strip SV_ from semantic name
    std::wstring sigDefName(sigPointNameW);
    sigDefName += L"_Defs";
    DxcDefine define;
    define.Name = sigDefName.c_str();
    define.Value = sigDefValue.c_str();

    VERIFY_SUCCEEDED(pCompiler->Compile(m_pSource, path.c_str(), entryW,
                                        profileW, nullptr, 0, &define, 1,
                                        nullptr, &pResult));
  }

  void CheckAnyOperationResultMsg(IDxcOperationResult *pResult,
                                  const char **pErrorMsgArray = nullptr,
                                  unsigned ErrorMsgCount = 0) {
    HRESULT status;
    VERIFY_SUCCEEDED(pResult->GetStatus(&status));
    if (pErrorMsgArray == nullptr || ErrorMsgCount == 0) {
      VERIFY_SUCCEEDED(status);
      return;
    }
    VERIFY_FAILED(status);
    CComPtr<IDxcBlobEncoding> text;
    VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&text));
    const char *pStart = (const char *)text->GetBufferPointer();
    const char *pEnd = pStart + text->GetBufferSize();
    bool bMessageFound = false;
    for (unsigned i = 0; i < ErrorMsgCount; i++) {
      const char *pErrorMsg = pErrorMsgArray[i];
      const char *pMatch = std::search(pStart, pEnd, pErrorMsg, pErrorMsg + strlen(pErrorMsg));
      if (pEnd != pMatch)
        bMessageFound = true;
    }
    VERIFY_IS_TRUE(bMessageFound);
  }

  dxc::DxcDllSupport m_dllSupport;
  CComPtr<IDxcBlobEncoding> m_pSource;
};

bool SystemValueTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
  }
  return true;
}

static bool ArbAllowed(DXIL::SigPointKind sp) {
  switch (sp) {
  case DXIL::SigPointKind::VSIn:
  case DXIL::SigPointKind::VSOut:
  case DXIL::SigPointKind::GSVIn:
  case DXIL::SigPointKind::GSOut:
  case DXIL::SigPointKind::HSCPIn:
  case DXIL::SigPointKind::HSCPOut:
  case DXIL::SigPointKind::PCOut:
  case DXIL::SigPointKind::DSCPIn:
  case DXIL::SigPointKind::DSIn:
  case DXIL::SigPointKind::DSOut:
  case DXIL::SigPointKind::PSIn:
    return true;
  }
  return false;
}

TEST_F(SystemValueTest, VerifyArbitrarySupport) {
  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    CComPtr<IDxcOperationResult> pResult;
    CompileHLSLTemplate(pResult, sp, DXIL::SemanticKind::Invalid, true);
    HRESULT result;
    VERIFY_SUCCEEDED(pResult->GetStatus(&result));
    if (ArbAllowed(sp)) {
      CheckAnyOperationResultMsg(pResult);
    } else {
      // TODO: We should probably improve this error message since it pertains to a parameter
      // at a particular signature point, not necessarily the whole shader model.
      // These are a couple of possible errors:
      // error: invalid semantic 'ARB' for <sm>
      // error: Semantic ARB is invalid for shader model <sm>
      // error: invalid semantic found in <sm>
      const char *Errors[] = {
        "error: Semantic ARB is invalid for shader model",
        "error: invalid semantic 'ARB' for",
        "error: invalid semantic found in CS",
      };
      CheckAnyOperationResultMsg(pResult, Errors, _countof(Errors));
    }
  }
}

TEST_F(SystemValueTest, VerifyNotAvailableFail) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    for (DXIL::SemanticKind sv = (DXIL::SemanticKind)((unsigned)DXIL::SemanticKind::Arbitrary + 1); sv < DXIL::SemanticKind::Invalid; sv = (DXIL::SemanticKind)((unsigned)sv + 1)) {
      DXIL::SemanticInterpretationKind interpretation = hlsl::SigPoint::GetInterpretation(sv, sp, 6, 0);
      if (interpretation == DXIL::SemanticInterpretationKind::NA) {
        CComPtr<IDxcOperationResult> pResult;
        CompileHLSLTemplate(pResult, sp, sv, false);
        // error: Semantic SV_SampleIndex is invalid for shader model: vs
        // error: invalid semantic 'SV_VertexID' for gs
        // error: invalid semantic found in CS
        const Semantic *pSemantic = Semantic::Get(sv);
        const char *SemName = pSemantic->GetName();
        std::string ErrorStrs[] = {
          std::string("error: Semantic ") + SemName + " is invalid for shader model:",
          std::string("error: invalid semantic '") + SemName + "' for",
          "error: invalid semantic found in CS",
        };
        const char *Errors[_countof(ErrorStrs)];
        for (unsigned i = 0; i < _countof(ErrorStrs); i++)
          Errors[i] = ErrorStrs[i].c_str();
        CheckAnyOperationResultMsg(pResult, Errors, _countof(Errors));
      }
    }
  }
}

TEST_F(SystemValueTest, VerifySVAsArbitrary) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    for (DXIL::SemanticKind sv = (DXIL::SemanticKind)((unsigned)DXIL::SemanticKind::Arbitrary + 1); sv < DXIL::SemanticKind::Invalid; sv = (DXIL::SemanticKind)((unsigned)sv + 1)) {
      DXIL::SemanticInterpretationKind interpretation = hlsl::SigPoint::GetInterpretation(sv, sp, 6, 0);
      if (interpretation == DXIL::SemanticInterpretationKind::Arb) {
        CComPtr<IDxcOperationResult> pResult;
        CompileHLSLTemplate(pResult, sp, sv, false);
        HRESULT result;
        VERIFY_SUCCEEDED(pResult->GetStatus(&result));
        VERIFY_SUCCEEDED(result);
        // TODO: Verify system value item is included in signature, treated as arbitrary,
        //  and that the element id is used in load input instruction.
      }
    }
  }
}

TEST_F(SystemValueTest, VerifySVAsSV) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    for (DXIL::SemanticKind sv = (DXIL::SemanticKind)((unsigned)DXIL::SemanticKind::Arbitrary + 1); sv < DXIL::SemanticKind::Invalid; sv = (DXIL::SemanticKind)((unsigned)sv + 1)) {
      DXIL::SemanticInterpretationKind interpretation = hlsl::SigPoint::GetInterpretation(sv, sp, 6, 0);
      if (interpretation == DXIL::SemanticInterpretationKind::SV || interpretation == DXIL::SemanticInterpretationKind::SGV) {
        CComPtr<IDxcOperationResult> pResult;
        CompileHLSLTemplate(pResult, sp, sv, false);
        HRESULT result;
        VERIFY_SUCCEEDED(pResult->GetStatus(&result));
        VERIFY_SUCCEEDED(result);
        // TODO: Verify system value is included in signature, system value enum is appropriately set,
        //  and that the element id is used in load input instruction.
      }
    }
  }
}

TEST_F(SystemValueTest, VerifySGV) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    for (DXIL::SemanticKind sv = (DXIL::SemanticKind)((unsigned)DXIL::SemanticKind::Arbitrary + 1); sv < DXIL::SemanticKind::Invalid; sv = (DXIL::SemanticKind)((unsigned)sv + 1)) {
      DXIL::SemanticInterpretationKind interpretation = hlsl::SigPoint::GetInterpretation(sv, sp, 6, 0);
      if (interpretation == DXIL::SemanticInterpretationKind::SGV) {
        CComPtr<IDxcOperationResult> pResult;
        CompileHLSLTemplate(pResult, sp, sv, true);
        HRESULT result;
        VERIFY_SUCCEEDED(pResult->GetStatus(&result));
        VERIFY_SUCCEEDED(result);
        // TODO: Verify system value is included in signature and arbitrary is packed before system value
        //  Or: verify failed when using greedy signature packing
        // TODO: Verify warning about declaring the system value last for fxc HLSL compatibility.
      }
    }
  }
}

TEST_F(SystemValueTest, VerifySVNotPacked) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    for (DXIL::SemanticKind sv = (DXIL::SemanticKind)((unsigned)DXIL::SemanticKind::Arbitrary + 1); sv < DXIL::SemanticKind::Invalid; sv = (DXIL::SemanticKind)((unsigned)sv + 1)) {
      DXIL::SemanticInterpretationKind interpretation = hlsl::SigPoint::GetInterpretation(sv, sp, 6, 0);
      if (interpretation == DXIL::SemanticInterpretationKind::NotPacked) {
        CComPtr<IDxcOperationResult> pResult;
        CompileHLSLTemplate(pResult, sp, sv, false);
        HRESULT result;
        VERIFY_SUCCEEDED(pResult->GetStatus(&result));
        VERIFY_SUCCEEDED(result);
        // TODO: Verify system value is included in signature and has packing location (-1, -1),
        //  and that the element id is used in load input instruction.
      }
    }
  }
}

TEST_F(SystemValueTest, VerifySVNotInSig) {
  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    for (DXIL::SemanticKind sv = (DXIL::SemanticKind)((unsigned)DXIL::SemanticKind::Arbitrary + 1); sv < DXIL::SemanticKind::Invalid; sv = (DXIL::SemanticKind)((unsigned)sv + 1)) {
      DXIL::SemanticInterpretationKind interpretation = hlsl::SigPoint::GetInterpretation(sv, sp, 6, 0);
      if (interpretation == DXIL::SemanticInterpretationKind::NotInSig) {
        CComPtr<IDxcOperationResult> pResult;
        CompileHLSLTemplate(pResult, sp, sv, false);
        HRESULT result;
        VERIFY_SUCCEEDED(pResult->GetStatus(&result));
        VERIFY_SUCCEEDED(result);
        // TODO: Verify system value is not included in signature,
        //  that intrinsic function is used, and that the element id is not used in load input instruction.
      }
    }
  }
}

TEST_F(SystemValueTest, VerifyVertexPacking) {
  // TODO: Implement
  VERIFY_IS_TRUE("Not Implemented");

  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    DXIL::PackingKind pk = SigPoint::GetSigPoint(sp)->GetPackingKind();
    if (pk == DXIL::PackingKind::Vertex) {
      // TBD: Test constraints here, or add constraints to validator and just generate cases to pack here, expecting success?
    }
  }
}

TEST_F(SystemValueTest, VerifyPatchConstantPacking) {
  // TODO: Implement
  VERIFY_IS_TRUE("Not Implemented");

  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    DXIL::PackingKind pk = SigPoint::GetSigPoint(sp)->GetPackingKind();
    if (pk == DXIL::PackingKind::PatchConstant) {
      // TBD: Test constraints here, or add constraints to validator and just generate cases to pack here, expecting success?
    }
  }
}

TEST_F(SystemValueTest, VerifyTargetPacking) {
  // TODO: Implement
  VERIFY_IS_TRUE("Not Implemented");

  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    DXIL::PackingKind pk = SigPoint::GetSigPoint(sp)->GetPackingKind();
    if (pk == DXIL::PackingKind::Target) {
      // TBD: Test constraints here, or add constraints to validator and just generate cases to pack here, expecting success?
    }
  }
}

TEST_F(SystemValueTest, VerifyTessFactors) {
  // TODO: Implement
  VERIFY_IS_TRUE("Not Implemented");
  // TBD: Split between return and out params?
}

TEST_F(SystemValueTest, VerifyShadowEntries) {
  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    for (DXIL::SemanticKind sv = (DXIL::SemanticKind)((unsigned)DXIL::SemanticKind::Arbitrary + 1); sv < DXIL::SemanticKind::Invalid; sv = (DXIL::SemanticKind)((unsigned)sv + 1)) {
      DXIL::SemanticInterpretationKind interpretation = hlsl::SigPoint::GetInterpretation(sv, sp, 6, 0);
      if (interpretation == DXIL::SemanticInterpretationKind::Shadow) {
        CComPtr<IDxcOperationResult> pResult;
        CompileHLSLTemplate(pResult, sp, sv, false);
        HRESULT result;
        VERIFY_SUCCEEDED(pResult->GetStatus(&result));
        VERIFY_SUCCEEDED(result);
        // TODO: Verify system value is included in corresponding signature (with fallback),
        //  that intrinsic function is used, and that the element id is not used in load input instruction.
      }
    }
  }
}

TEST_F(SystemValueTest, VerifyVersionedSemantics) {
  struct TestInfo {
    DXIL::SigPointKind sp;
    DXIL::SemanticKind sv;
    unsigned Major, Minor;
  };
  const unsigned kNumTests = 7;
  TestInfo info[kNumTests] = {
    {DXIL::SigPointKind::PSIn, DXIL::SemanticKind::SampleIndex, 4, 1 },
    {DXIL::SigPointKind::PSIn, DXIL::SemanticKind::Coverage, 5, 0 },
    {DXIL::SigPointKind::PSOut, DXIL::SemanticKind::Coverage, 4, 1 },
    {DXIL::SigPointKind::PSIn, DXIL::SemanticKind::InnerCoverage, 5, 0 },
    {DXIL::SigPointKind::PSOut, DXIL::SemanticKind::DepthLessEqual, 5, 0 },
    {DXIL::SigPointKind::PSOut, DXIL::SemanticKind::DepthGreaterEqual, 5, 0 },
    {DXIL::SigPointKind::PSOut, DXIL::SemanticKind::StencilRef, 5, 0 },
  };

  for (unsigned i = 0; i < kNumTests; i++) {
    TestInfo &test = info[i];
    unsigned MajorLower = test.Major, MinorLower = test.Minor;
    if (MinorLower > 0)
      MinorLower--;
    else {
      MajorLower--;
      MinorLower = 1;
    }
    DXIL::SemanticInterpretationKind SI = hlsl::SigPoint::GetInterpretation(test.sv, test.sp, test.Major, test.Minor);
    VERIFY_IS_TRUE(SI != DXIL::SemanticInterpretationKind::NA);
    DXIL::SemanticInterpretationKind SILower = hlsl::SigPoint::GetInterpretation(test.sv, test.sp, MajorLower, MinorLower);
    VERIFY_IS_TRUE(SILower == DXIL::SemanticInterpretationKind::NA);
#ifdef _VERIFY_VERSIONED_SEMANTICS_
    {
      CComPtr<IDxcOperationResult> pResult;
      CompileHLSLTemplate(pResult, test.sp, test.sv, false, test.Major, test.Minor);
      HRESULT result;
      VERIFY_SUCCEEDED(pResult->GetStatus(&result));
      VERIFY_SUCCEEDED(result);
    }
    {
      CComPtr<IDxcOperationResult> pResult;
      CompileHLSLTemplate(pResult, test.sp, test.sv, false, test.Major, test.Minor);
      HRESULT result;
      VERIFY_SUCCEEDED(pResult->GetStatus(&result));
      VERIFY_FAILED(result);
      // TODO: Verify error message
      const char *Errors[] = {
        "TODO: fill this in."
      };
      CheckAnyOperationResultMsg(pResult, Errors, _countof(Errors));
    }
#endif //_VERIFY_VERSIONED_SEMANTICS_
  }
}

TEST_F(SystemValueTest, VerifyMissingSemanticFailure) {
  for (DXIL::SigPointKind sp = (DXIL::SigPointKind)0; sp < DXIL::SigPointKind::Invalid; sp = (DXIL::SigPointKind)((unsigned)sp + 1)) {
    std::wstring sigDefValue(L"Def_Arb_NoSem(uint, arb0)");
    CComPtr<IDxcOperationResult> pResult;
    CompileHLSLTemplate(pResult, sp, sigDefValue);
    const char *Errors[] = {
      "error: Semantic must be defined for all parameters of an entry function or patch constant function",
    };
    CheckAnyOperationResultMsg(pResult, Errors, _countof(Errors));
  }
}
