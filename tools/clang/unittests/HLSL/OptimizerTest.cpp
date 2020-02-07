///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// OptimizerTest.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides tests for the optimizer API.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif

#include <memory>
#include <vector>
#include <string>
#include <map>
#include <cassert>
#include <sstream>
#include <algorithm>
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"

#include "dxc/Test/HLSLTestData.h"
#include "dxc/Test/HlslTestUtils.h"
#include "dxc/Test/DxcTestUtils.h"

#include "llvm/Support/raw_os_ostream.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Unicode.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringSwitch.h"

using namespace std;
using namespace hlsl_test;

///////////////////////////////////////////////////////////////////////////////
// Optimizer test cases.

#ifdef _WIN32
class OptimizerTest {
#else
class OptimizerTest : public ::testing::Test {
#endif
public:
  BEGIN_TEST_CLASS(OptimizerTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  // Split just so we can run them with some degree of concurrency.
  TEST_METHOD(OptimizerWhenSlice0ThenOK)
  TEST_METHOD(OptimizerWhenSlice1ThenOK)
  TEST_METHOD(OptimizerWhenSlice2ThenOK)
  TEST_METHOD(OptimizerWhenSlice3ThenOK)
  TEST_METHOD(OptimizerWhenSliceWithIntermediateOptionsThenOK)

  void OptimizerWhenSliceNThenOK(int optLevel);
  void OptimizerWhenSliceNThenOK(int optLevel, LPCSTR pText, LPCWSTR pTarget, llvm::ArrayRef<LPCWSTR> args = {});

  dxc::DxcDllSupport m_dllSupport;
  VersionSupportInfo m_ver;

  HRESULT CreateCompiler(IDxcCompiler **ppResult) {
    return m_dllSupport.CreateInstance(CLSID_DxcCompiler, ppResult);
  }

  HRESULT CreateContainerBuilder(IDxcContainerBuilder **ppResult) {
    return m_dllSupport.CreateInstance(CLSID_DxcContainerBuilder, ppResult);
  }

  void VerifyOperationSucceeded(IDxcOperationResult *pResult) {
    HRESULT result;
    VERIFY_SUCCEEDED(pResult->GetStatus(&result));
    if (FAILED(result)) {
      CComPtr<IDxcBlobEncoding> pErrors;
      VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
      CA2W errorsWide(BlobToUtf8(pErrors).c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(errorsWide);
    }
    VERIFY_SUCCEEDED(result);
  }
};

bool OptimizerTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    m_ver.Initialize(m_dllSupport);
  }
  return true;
}

TEST_F(OptimizerTest, OptimizerWhenSlice0ThenOK) { OptimizerWhenSliceNThenOK(0); }
TEST_F(OptimizerTest, OptimizerWhenSlice1ThenOK) { OptimizerWhenSliceNThenOK(1); }
TEST_F(OptimizerTest, OptimizerWhenSlice2ThenOK) { OptimizerWhenSliceNThenOK(2); }
TEST_F(OptimizerTest, OptimizerWhenSlice3ThenOK) { OptimizerWhenSliceNThenOK(3); }

TEST_F(OptimizerTest, OptimizerWhenSliceWithIntermediateOptionsThenOK) {
  // The program below working depends on the LegacyResourceReservation option being
  // carried through to the resource register allocator, even though it is not
  // preserved in the final shader.
  LPCSTR SampleProgram =
    "Texture2D tex0 : register(t0);\r\n"
    "Texture2D tex1;\r\n" // tex1 should get register t1
    "float4 main() : SV_Target {\r\n"
    "  return tex1.Load((int3)0);\r\n"
    "}";
  OptimizerWhenSliceNThenOK(1, SampleProgram, L"ps_6_0", { L"-flegacy-resource-reservation" });
}

void OptimizerTest::OptimizerWhenSliceNThenOK(int optLevel) {
  LPCSTR SampleProgram =
    "Texture2D g_Tex;\r\n"
    "SamplerState g_Sampler;\r\n"
    "void unused() { }\r\n"
    "float4 main(float4 pos : SV_Position, float4 user : USER, bool b : B) : SV_Target {\r\n"
    "  unused();\r\n"
    "  if (b) user = g_Tex.Sample(g_Sampler, pos.xy);\r\n"
    "  return user * pos;\r\n"
    "}";
  OptimizerWhenSliceNThenOK(optLevel, SampleProgram, L"ps_6_0");
}
static bool IsPassMarkerFunction(LPCWSTR pName) {
  return 0 == _wcsicmp(pName, L"-opt-fn-passes");
}
static bool IsPassMarkerNotFunction(LPCWSTR pName) {
  return 0 == _wcsnicmp(pName, L"-opt-", 5) && !IsPassMarkerFunction(pName);
}
static void ExtractFunctionPasses(std::vector<LPCWSTR> &passes, std::vector<LPCWSTR> &functionPasses) {
  // Assumption: contiguous range
  typedef std::vector<LPCWSTR>::iterator it;
  it firstPass = std::find_if(passes.begin(), passes.end(), IsPassMarkerFunction);
  if (firstPass == passes.end()) return;
  it lastPass = std::find_if(firstPass, passes.end(), IsPassMarkerNotFunction);
  it cursor = firstPass;
  while (cursor != lastPass) {
    functionPasses.push_back(*cursor);
    ++cursor;
  }
  passes.erase(firstPass, lastPass);
}

void OptimizerTest::OptimizerWhenSliceNThenOK(int optLevel, LPCSTR pText, LPCWSTR pTarget, llvm::ArrayRef<LPCWSTR> args) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOptimizer> pOptimizer;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  CComPtr<IDxcBlob> pProgramModule;
  CComPtr<IDxcBlob> pProgramDisassemble;
  CComPtr<IDxcBlob> pHighLevelBlob;
  CComPtr<IDxcBlob> pOptDump;
  std::string passes;
  std::vector<LPCWSTR> passList;
  std::vector<LPCWSTR> prefixPassList;

  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcOptimizer, &pOptimizer));

  // Set up compilation args vector
  wchar_t OptArg[4] = L"/O0";
  OptArg[2] = L'0' + optLevel;
  Utf8ToBlob(m_dllSupport, pText, &pSource);
  std::vector<LPCWSTR> highLevelArgs = { L"/Vd", OptArg };
  highLevelArgs.insert(highLevelArgs.end(), args.begin(), args.end());

  // Create the target program with a single invocation.
  highLevelArgs.emplace_back(L"/Qkeep_reflect_in_dxil");
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main", pTarget,
    highLevelArgs.data(), static_cast<UINT32>(highLevelArgs.size()), nullptr, 0, nullptr, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  pResult.Release();
  std::string originalAssembly = DisassembleProgram(m_dllSupport, pProgram);
  highLevelArgs.pop_back(); // Remove /keep_reflect_in_dxil

  // Get a list of passes for this configuration.
  highLevelArgs.emplace_back(L"/Odump");
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main", pTarget,
    highLevelArgs.data(), static_cast<UINT32>(highLevelArgs.size()), nullptr, 0, nullptr, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_SUCCEEDED(pResult->GetResult(&pOptDump));
  pResult.Release();
  passes = BlobToUtf8(pOptDump);
  CA2W passesW(passes.c_str(), CP_UTF8);

  // Get the high-level compile of the program.
  highLevelArgs.pop_back(); // Remove /Odump
  highLevelArgs.emplace_back(L"/fcgl");
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main", pTarget,
    highLevelArgs.data(), static_cast<UINT32>(highLevelArgs.size()), nullptr, 0, nullptr, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_SUCCEEDED(pResult->GetResult(&pHighLevelBlob));
  pResult.Release();

  // Create a list of passes.
  SplitPassList(passesW.m_psz, passList);
  ExtractFunctionPasses(passList, prefixPassList);

  // For each point in between the passes ...
  for (size_t i = 0; i <= passList.size(); ++i) {
    size_t secondPassIdx = i;
    size_t firstPassCount = i;
    size_t secondPassCount = passList.size() - i;

    // If we find an -hlsl-passes-nopause, pause/resume will not work past this.
    if (i > 0 && 0 == wcscmp(L"-hlsl-passes-nopause", passList[i - 1])) {
      break;
    }

    CComPtr<IDxcBlob> pFirstModule;
    CComPtr<IDxcBlob> pSecondModule;
    CComPtr<IDxcBlob> pAssembledBlob;
    std::vector<LPCWSTR> firstPassList, secondPassList;
    firstPassList = prefixPassList;
    firstPassList.push_back(L"-opt-mod-passes");
    secondPassList = firstPassList;
    firstPassList.insert(firstPassList.end(), passList.begin(), passList.begin() + firstPassCount);
    firstPassList.push_back(L"-hlsl-passes-pause");
    secondPassList.push_back(L"-hlsl-passes-resume");
    secondPassList.insert(secondPassList.end(), passList.begin() + secondPassIdx, passList.begin() + secondPassIdx + secondPassCount);

    // Run a first pass.
    VERIFY_SUCCEEDED(pOptimizer->RunOptimizer(pHighLevelBlob, 
      firstPassList.data(), (UINT32)firstPassList.size(),
      &pFirstModule, nullptr));

    // Run a second pass.
    VERIFY_SUCCEEDED(pOptimizer->RunOptimizer(pFirstModule,
      secondPassList.data(), (UINT32)secondPassList.size(),
      &pSecondModule, nullptr));

    // Assembly it into a container so the disassembler shows equivalent data.
    AssembleToContainer(m_dllSupport, pSecondModule, &pAssembledBlob);

    // Verify we get the same results as in the full version.
    std::string assembly = DisassembleProgram(m_dllSupport, pAssembledBlob);
    if (0 != strcmp(assembly.c_str(), originalAssembly.c_str())) {
      LogCommentFmt(L"Difference found in disassembly in iteration %u when breaking before '%s'", i, (i == passList.size()) ? L"(full list)" : passList[i]);
      LogCommentFmt(L"Original assembly\r\n%S", originalAssembly.c_str());
      LogCommentFmt(L"\r\nReassembled assembly\r\n%S", assembly.c_str());
      VERIFY_FAIL();
    }
  }
}
