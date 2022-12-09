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

// For DxilRuntimeReflection.h:
#include "dxc/Support/WinIncludes.h"

#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilPipelineStateValidation.h"
#include "dxc/DxilContainer/DxilRuntimeReflection.h"
#include "dxc/DxilRootSignature/DxilRootSignature.h"
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

  TEST_METHOD(OptimizerWhenPassedContainerPreservesSubobjects)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesRootSig)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_HSDependentPCDependent)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_HSDependentPCNonDependent)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_HSNonDependentPCDependent)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_HSNonDependentPCNonDependent)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_MSDependent)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_MSNonDependent)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_DSDependent)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_DSNonDependent)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_VSDependent)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_VSNonDependent)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_GSDependent)
  TEST_METHOD(OptimizerWhenPassedContainerPreservesViewId_GSNonDependent)

  void OptimizerWhenSliceNThenOK(int optLevel);
  void OptimizerWhenSliceNThenOK(int optLevel, LPCSTR pText, LPCWSTR pTarget, llvm::ArrayRef<LPCWSTR> args = {});
  CComPtr<IDxcBlob> Compile(
      char const *source, wchar_t const *entry, wchar_t const *profile);
  CComPtr<IDxcBlob> RunHlslEmitAndReturnContainer(IDxcBlob *container);
  CComPtr<IDxcBlob> RetrievePartFromContainer(IDxcBlob *container,
                                                       UINT32 part);
  void ComparePSV0BeforeAndAfterOptimization(const char *source,
                                        const wchar_t *entry,
                                        const wchar_t *profile,
                                        bool usesViewId,
                                        int streamCount = 1);

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
  OptimizerWhenSliceNThenOK(optLevel, SampleProgram, L"ps_6_0",
    // Add -validator-version 1.4 to ensure it's not changed by DxcAssembler.
    {L"-validator-version", L"1.4"});
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

CComPtr<IDxcBlob> OptimizerTest::Compile(
    char const *source, wchar_t const *entry, wchar_t const *profile) {

  CComPtr<IDxcCompiler> pCompiler;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));

  CComPtr<IDxcBlobEncoding> pSource;
  Utf8ToBlob(m_dllSupport, source, &pSource);

  CComPtr<IDxcOperationResult> pResult;
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", entry, profile,
                                      nullptr, 0, nullptr, 0, nullptr,
                                      &pResult));
  VerifyOperationSucceeded(pResult);
  CComPtr<IDxcBlob> pProgram;
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  return pProgram;
}

CComPtr<IDxcBlob> OptimizerTest::RunHlslEmitAndReturnContainer(IDxcBlob *container) 
{
  CComPtr<IDxcOptimizer> pOptimizer;
  VERIFY_SUCCEEDED(
      m_dllSupport.CreateInstance(CLSID_DxcOptimizer, &pOptimizer));

  std::vector<LPCWSTR> Options;
  Options.push_back(L"-hlsl-dxilemit");

  CComPtr<IDxcBlob> pDxil;
  CComPtr<IDxcBlobEncoding> pText;
  VERIFY_SUCCEEDED(pOptimizer->RunOptimizer(container, Options.data(),
                                            Options.size(), &pDxil, &pText));

  CComPtr<IDxcAssembler> pAssembler;
  VERIFY_SUCCEEDED(
      m_dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));

  CComPtr<IDxcOperationResult> result;
  pAssembler->AssembleToContainer(pDxil, &result);

  CComPtr<IDxcBlob> pOptimizedContainer;
  result->GetResult(&pOptimizedContainer);

  return pOptimizedContainer;
}

CComPtr<IDxcBlob> OptimizerTest::RetrievePartFromContainer(IDxcBlob *container,
                                                     UINT32 part) {
  CComPtr<IDxcContainerReflection> pReflection;
  VERIFY_SUCCEEDED(
      m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
  VERIFY_SUCCEEDED(pReflection->Load(container));
  UINT32 dxilIndex;
  VERIFY_SUCCEEDED(pReflection->FindFirstPartKind(part, &dxilIndex));
  CComPtr<IDxcBlob> blob;
  VERIFY_SUCCEEDED(pReflection->GetPartContent(dxilIndex, & blob));
  return blob;
}

TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesSubobjects) {

    const char *source = R"x(
GlobalRootSignature so_GlobalRootSignature =
{
	"RootConstants(num32BitConstants=1, b8), "
};

StateObjectConfig so_StateObjectConfig = 
{ 
    STATE_OBJECT_FLAGS_ALLOW_LOCAL_DEPENDENCIES_ON_EXTERNAL_DEFINITONS
};

LocalRootSignature so_LocalRootSignature1 = 
{
	"RootConstants(num32BitConstants=3, b2), "
	"UAV(u6),RootFlags(LOCAL_ROOT_SIGNATURE)" 
};

LocalRootSignature so_LocalRootSignature2 = 
{
	"RootConstants(num32BitConstants=3, b2), "
	"UAV(u8, flags=DATA_STATIC), " 
	"RootFlags(LOCAL_ROOT_SIGNATURE)"
};

RaytracingShaderConfig  so_RaytracingShaderConfig =
{
    128, // max payload size
    32   // max attribute size
};

RaytracingPipelineConfig so_RaytracingPipelineConfig =
{
    2 // max trace recursion depth
};

TriangleHitGroup MyHitGroup =
{
    "MyAnyHit",       // AnyHit
    "MyClosestHit",   // ClosestHit
};

SubobjectToExportsAssociation so_Association1 =
{
	"so_LocalRootSignature1", // subobject name
	"MyRayGen"                // export association 
};

SubobjectToExportsAssociation so_Association2 =
{
	"so_LocalRootSignature2", // subobject name
	"MyAnyHit"                // export association 
};

struct MyPayload
{
    float4 color;
};

[shader("raygeneration")]
void MyRayGen()
{
}

[shader("closesthit")]
void MyClosestHit(inout MyPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{  
}

[shader("anyhit")]
void MyAnyHit(inout MyPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
}

[shader("miss")]
void MyMiss(inout MyPayload payload)
{
}

)x";

  auto pOptimizedContainer =
      RunHlslEmitAndReturnContainer(Compile(source, L"", L"lib_6_6"));

  auto runtimeDataPart = RetrievePartFromContainer(
      pOptimizedContainer, hlsl::DFCC_RuntimeData);

  hlsl::RDAT::DxilRuntimeData rdat(runtimeDataPart->GetBufferPointer(),
                                   static_cast<size_t>(runtimeDataPart->GetBufferSize()));

  auto const subObjectTableReader = rdat.GetSubobjectTable();

  // There are 9 subobjects in the HLSL above:
  VERIFY_ARE_EQUAL(subObjectTableReader.Count(), 9u);
  for (uint32_t i = 0; i < subObjectTableReader.Count(); ++i) {
    auto subObject = subObjectTableReader[i];
    hlsl::DXIL::SubobjectKind subobjectKind = subObject.getKind();
    switch (subobjectKind) {
    case hlsl::DXIL::SubobjectKind::StateObjectConfig:
      VERIFY_IS_TRUE(0 == strcmp(subObject.getName(), "so_StateObjectConfig"));
      break;
    case hlsl::DXIL::SubobjectKind::GlobalRootSignature:
      VERIFY_IS_TRUE(0 == strcmp(subObject.getName(), "so_GlobalRootSignature"));
      break;
    case hlsl::DXIL::SubobjectKind::LocalRootSignature:
      VERIFY_IS_TRUE(
          0 == strcmp(subObject.getName(), "so_LocalRootSignature1") || 
          0 == strcmp(subObject.getName(), "so_LocalRootSignature2"));
      break;
    case hlsl::DXIL::SubobjectKind::SubobjectToExportsAssociation:
      VERIFY_IS_TRUE(
          0 == strcmp(subObject.getName(), "so_Association1") ||
          0 == strcmp(subObject.getName(), "so_Association2"));
      break;
    case hlsl::DXIL::SubobjectKind::RaytracingShaderConfig:
      VERIFY_IS_TRUE(0 == strcmp(subObject.getName(), "so_RaytracingShaderConfig"));
      break;
    case hlsl::DXIL::SubobjectKind::RaytracingPipelineConfig:
      VERIFY_IS_TRUE(0 == strcmp(subObject.getName(), "so_RaytracingPipelineConfig"));
      break;
    case hlsl::DXIL::SubobjectKind::HitGroup:
      VERIFY_IS_TRUE(0 == strcmp(subObject.getName(), "MyHitGroup"));
      break;
      break;
    }
  }
}

TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesRootSig) {
  const char * source =
R"(
#define RootSig \
        "UAV(u3, space=12), " \
        "RootConstants(num32BitConstants=1, b7, space = 42) "
    [numthreads(1, 1, 1)]
    [RootSignature(RootSig)]
    void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
    {
    }
)";

  auto pOptimizedContainer =
      RunHlslEmitAndReturnContainer(Compile(source, L"CSMain", L"cs_6_6"));

  auto rootSigPart = RetrievePartFromContainer(
      pOptimizedContainer, hlsl::DFCC_RootSignature);

  hlsl::RootSignatureHandle RSH;
  RSH.LoadSerialized(reinterpret_cast<const uint8_t *>(rootSigPart->GetBufferPointer()),
                         static_cast<uint32_t>(rootSigPart->GetBufferSize()));

  RSH.Deserialize();

  auto const * desc = RSH.GetDesc();

  VERIFY_ARE_EQUAL(desc->Version, hlsl::DxilRootSignatureVersion::Version_1_1);
  VERIFY_ARE_EQUAL(desc->Desc_1_1.NumParameters, 2u);
  for (unsigned int i = 0; i < desc->Desc_1_1.NumParameters; ++i)
  {
    hlsl::DxilRootParameter1 const *param = desc->Desc_1_1.pParameters + i;
    switch (param->ParameterType) {
    case hlsl::DxilRootParameterType::Constants32Bit:
      VERIFY_ARE_EQUAL(param->Constants.Num32BitValues, 1u);
      VERIFY_ARE_EQUAL(param->Constants.RegisterSpace, 42u);
      VERIFY_ARE_EQUAL(param->Constants.ShaderRegister, 7u);
      break;
    case hlsl::DxilRootParameterType::UAV:
      VERIFY_ARE_EQUAL(param->Descriptor.RegisterSpace, 12u);
      VERIFY_ARE_EQUAL(param->Descriptor.ShaderRegister, 3u);
      break;
    default:
      VERIFY_FAIL(L"Unexpected root param type");
      break;
    }
  }
}

void OptimizerTest::ComparePSV0BeforeAndAfterOptimization(const char *source, const wchar_t *entry, const wchar_t *profile, bool usesViewId, int streamCount /*= 1*/)
{
  auto originalContainer = Compile(source, entry, profile);

  auto originalPsvPart = RetrievePartFromContainer(
      originalContainer, hlsl::DFCC_PipelineStateValidation);

  auto optimizedContainer = RunHlslEmitAndReturnContainer(originalContainer);

  auto optimizedPsvPart = RetrievePartFromContainer(optimizedContainer,
                                           hlsl::DFCC_PipelineStateValidation);

  VERIFY_ARE_EQUAL(originalPsvPart->GetBufferSize(), optimizedPsvPart->GetBufferSize());

  VERIFY_ARE_EQUAL(memcmp(originalPsvPart->GetBufferPointer(),
                          optimizedPsvPart->GetBufferPointer(),
                          originalPsvPart->GetBufferSize()),
                   0);

  DxilPipelineStateValidation originalPsv;
  originalPsv.InitFromPSV0(
      reinterpret_cast<const uint8_t *>(optimizedPsvPart->GetBufferPointer()),
      static_cast<uint32_t>(optimizedPsvPart->GetBufferSize()));

  const PSVRuntimeInfo1 *originalInfo1 = originalPsv.GetPSVRuntimeInfo1();
  if (usesViewId) {
    VERIFY_IS_TRUE(originalInfo1->UsesViewID);
  }

  DxilPipelineStateValidation optimizedPsv;
  optimizedPsv.InitFromPSV0(
      reinterpret_cast<const uint8_t *>(originalPsvPart->GetBufferPointer()),
      static_cast<uint32_t>(originalPsvPart->GetBufferSize()));

  const PSVRuntimeInfo1 *optimizedInfo1 = optimizedPsv.GetPSVRuntimeInfo1();

  VERIFY_ARE_EQUAL(originalInfo1->ShaderStage, optimizedInfo1->ShaderStage);
  VERIFY_ARE_EQUAL(originalInfo1->UsesViewID , optimizedInfo1->UsesViewID);
  VERIFY_ARE_EQUAL(originalInfo1->SigInputElements,
                   optimizedInfo1->SigInputElements);
  VERIFY_ARE_EQUAL(originalInfo1->SigOutputElements,
                   optimizedInfo1->SigOutputElements);
  VERIFY_ARE_EQUAL(originalInfo1->SigPatchConstOrPrimElements,
                   optimizedInfo1->SigPatchConstOrPrimElements);
  VERIFY_ARE_EQUAL(originalPsv.GetViewIDPCOutputMask().IsValid(),
                   optimizedPsv.GetViewIDPCOutputMask().IsValid());
  if (originalPsv.GetViewIDPCOutputMask().IsValid()) {
    VERIFY_ARE_EQUAL(*originalPsv.GetViewIDPCOutputMask().Mask,
                     *optimizedPsv.GetViewIDPCOutputMask().Mask);
    VERIFY_ARE_EQUAL(originalPsv.GetViewIDPCOutputMask().NumVectors,
                     optimizedPsv.GetViewIDPCOutputMask().NumVectors);
  }
  for (int stream = 0; stream < streamCount; ++stream) 
  {
    VERIFY_ARE_EQUAL(originalInfo1->SigOutputVectors[stream],
                     optimizedInfo1->SigOutputVectors[stream]);

    VERIFY_ARE_EQUAL(originalPsv.GetViewIDOutputMask(stream).IsValid(),
                     optimizedPsv.GetViewIDOutputMask(stream).IsValid());
    if (originalPsv.GetViewIDOutputMask(stream).IsValid())
    {
      VERIFY_ARE_EQUAL(*originalPsv.GetViewIDOutputMask(stream).Mask,
                       *optimizedPsv.GetViewIDOutputMask(stream).Mask);
      VERIFY_ARE_EQUAL(originalPsv.GetViewIDOutputMask(stream).NumVectors,
                       optimizedPsv.GetViewIDOutputMask(stream).NumVectors);

    }
  }
}


TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesViewId_HSDependentPCDependent)
{
  ComparePSV0BeforeAndAfterOptimization(
      R"(
#define NumOutPoints 2

struct HsCpIn {
    int foo : FOO;
};

struct HsCpOut {
    int bar : BAR;
};

struct HsPcfOut {
  float tessOuter[4] : SV_TessFactor;
  float tessInner[2] : SV_InsideTessFactor;
};

// Patch Constant Function
HsPcfOut pcf(uint viewid : SV_ViewID) {
  HsPcfOut output;
  output = (HsPcfOut)viewid;
  return output;
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(NumOutPoints)]
[patchconstantfunc("pcf")]
HsCpOut main(InputPatch<HsCpIn, NumOutPoints> patch,
             uint id : SV_OutputControlPointID,
             uint viewid : SV_ViewID) {
    HsCpOut output;
    output.bar = viewid;
    return output;
}

)", L"main", L"hs_6_6", true);

}

TEST_F(OptimizerTest,
       OptimizerWhenPassedContainerPreservesViewId_HSNonDependentPCDependent) {
  ComparePSV0BeforeAndAfterOptimization(
      R"(
#define NumOutPoints 2

struct HsCpIn {
    int foo : FOO;
};

struct HsCpOut {
    int bar : BAR;
};

struct HsPcfOut {
  float tessOuter[4] : SV_TessFactor;
  float tessInner[2] : SV_InsideTessFactor;
};

// Patch Constant Function
HsPcfOut pcf(uint viewid : SV_ViewID) {
  HsPcfOut output;
  output = (HsPcfOut)viewid;
  return output;
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(NumOutPoints)]
[patchconstantfunc("pcf")]
HsCpOut main(InputPatch<HsCpIn, NumOutPoints> patch,
             uint id : SV_OutputControlPointID,
             uint viewid : SV_ViewID) {
    HsCpOut output;
    output.bar = 0;
    return output;
}

)",
      L"main", L"hs_6_6", true);
}

TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesViewId_HSDependentPCNonDependent)
{
  ComparePSV0BeforeAndAfterOptimization(
      R"(
#define NumOutPoints 2

struct HsCpIn {
    int foo : FOO;
};

struct HsCpOut {
    int bar : BAR;
};

struct HsPcfOut {
  float tessOuter[4] : SV_TessFactor;
  float tessInner[2] : SV_InsideTessFactor;
};

// Patch Constant Function
HsPcfOut pcf(uint viewid : SV_ViewID) {
  HsPcfOut output;
  output = (HsPcfOut)0;
  return output;
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(NumOutPoints)]
[patchconstantfunc("pcf")]
HsCpOut main(InputPatch<HsCpIn, NumOutPoints> patch,
             uint id : SV_OutputControlPointID,
             uint viewid : SV_ViewID) {
    HsCpOut output;
    output.bar = viewid;
    return output;
}

)", L"main", L"hs_6_6", true);

}

TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesViewId_HSNonDependentPCNonDependent)
{
  ComparePSV0BeforeAndAfterOptimization(
      R"(
#define NumOutPoints 2

struct HsCpIn {
    int foo : FOO;
};

struct HsCpOut {
    int bar : BAR;
};

struct HsPcfOut {
  float tessOuter[4] : SV_TessFactor;
  float tessInner[2] : SV_InsideTessFactor;
};

// Patch Constant Function
HsPcfOut pcf(uint viewid : SV_ViewID) {
  HsPcfOut output;
  output = (HsPcfOut)0;
  return output;
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(NumOutPoints)]
[patchconstantfunc("pcf")]
HsCpOut main(InputPatch<HsCpIn, NumOutPoints> patch,
             uint id : SV_OutputControlPointID,
             uint viewid : SV_ViewID) {
    HsCpOut output;
    output.bar = 0;
    return output;
}

)",
      L"main", L"hs_6_6", false /*does not use view id*/);

}


TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesViewId_MSDependent)
{
  ComparePSV0BeforeAndAfterOptimization(
      R"(
#define MAX_VERT 32
#define MAX_PRIM 16
#define NUM_THREADS 32
struct MeshPerVertex {
    float4 position : SV_Position;
    float color[4] : COLOR;
};

struct MeshPerPrimitive {
    float normal : NORMAL;
    float malnor : MALNOR;
    float alnorm : ALNORM;
    float ormaln : ORMALN;
    int layer[6] : LAYER;
};

struct MeshPayload {
    float normal;
    float malnor;
    float alnorm;
    float ormaln;
    int layer[6];
};

groupshared float gsMem[MAX_PRIM];

[numthreads(NUM_THREADS, 1, 1)]
[outputtopology("triangle")]
void main(
            out indices uint3 primIndices[MAX_PRIM],
            out vertices MeshPerVertex verts[MAX_VERT],
            out primitives MeshPerPrimitive prims[MAX_PRIM],
            in payload MeshPayload mpl,
            in uint tig : SV_GroupIndex,
            in uint vid : SV_ViewID
         )
{
    SetMeshOutputCounts(MAX_VERT, MAX_PRIM);
    MeshPerVertex ov;
    if (vid % 2) {
        ov.position = float4(4.0,5.0,6.0,7.0);
        ov.color[0] = 4.0;
        ov.color[1] = 5.0;
        ov.color[2] = 6.0;
        ov.color[3] = 7.0;
    } else {
        ov.position = float4(14.0,15.0,16.0,17.0);
        ov.color[0] = 14.0;
        ov.color[1] = 15.0;
        ov.color[2] = 16.0;
        ov.color[3] = 17.0;
    }
    if (tig % 3) {
      primIndices[tig / 3] = uint3(tig, tig + 1, tig + 2);
      MeshPerPrimitive op;
      op.normal = mpl.normal;
      op.malnor = gsMem[tig / 3 + 1];
      op.alnorm = mpl.alnorm;
      op.ormaln = mpl.ormaln;
      op.layer[0] = mpl.layer[0];
      op.layer[1] = mpl.layer[1];
      op.layer[2] = mpl.layer[2];
      op.layer[3] = mpl.layer[3];
      op.layer[4] = mpl.layer[4];
      op.layer[5] = mpl.layer[5];
      gsMem[tig / 3] = op.normal;
      prims[tig / 3] = op;
    }
    verts[tig] = ov;
}
)",
      L"main", L"ms_6_5", true /*does use view id*/);

}

TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesViewId_MSNonDependent)
{
  ComparePSV0BeforeAndAfterOptimization(
      R"(
#define MAX_VERT 32
#define MAX_PRIM 16
#define NUM_THREADS 32
struct MeshPerVertex {
    float4 position : SV_Position;
    float color[4] : COLOR;
};

struct MeshPerPrimitive {
    float normal : NORMAL;
    float malnor : MALNOR;
    float alnorm : ALNORM;
    float ormaln : ORMALN;
    int layer[6] : LAYER;
};

struct MeshPayload {
    float normal;
    float malnor;
    float alnorm;
    float ormaln;
    int layer[6];
};

groupshared float gsMem[MAX_PRIM];

[numthreads(NUM_THREADS, 1, 1)]
[outputtopology("triangle")]
void main(
            out indices uint3 primIndices[MAX_PRIM],
            out vertices MeshPerVertex verts[MAX_VERT],
            out primitives MeshPerPrimitive prims[MAX_PRIM],
            in payload MeshPayload mpl,
            in uint tig : SV_GroupIndex,
            in uint vid : SV_ViewID
         )
{
    SetMeshOutputCounts(MAX_VERT, MAX_PRIM);
    MeshPerVertex ov;
    if (false) {
        ov.position = float4(4.0,5.0,6.0,7.0);
        ov.color[0] = 4.0;
        ov.color[1] = 5.0;
        ov.color[2] = 6.0;
        ov.color[3] = 7.0;
    } else {
        ov.position = float4(14.0,15.0,16.0,17.0);
        ov.color[0] = 14.0;
        ov.color[1] = 15.0;
        ov.color[2] = 16.0;
        ov.color[3] = 17.0;
    }
    if (tig % 3) {
      primIndices[tig / 3] = uint3(tig, tig + 1, tig + 2);
      MeshPerPrimitive op;
      op.normal = mpl.normal;
      op.malnor = gsMem[tig / 3 + 1];
      op.alnorm = mpl.alnorm;
      op.ormaln = mpl.ormaln;
      op.layer[0] = mpl.layer[0];
      op.layer[1] = mpl.layer[1];
      op.layer[2] = mpl.layer[2];
      op.layer[3] = mpl.layer[3];
      op.layer[4] = mpl.layer[4];
      op.layer[5] = mpl.layer[5];
      gsMem[tig / 3] = op.normal;
      prims[tig / 3] = op;
    }
    verts[tig] = ov;
}
)",
      L"main", L"ms_6_5", false /*does not use view id*/);

}


TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesViewId_DSDependent)
{
  ComparePSV0BeforeAndAfterOptimization(
      R"(
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

struct HS_CONSTANT_DATA_OUTPUT
{
    float Edges[3]        : SV_TessFactor;
    float Inside       : SV_InsideTessFactor;
};

struct BEZIER_CONTROL_POINT
{
    float3 vPosition    : BEZIERPOS;
    float4 color : COLOR;
};

[domain("tri")]
PSInput DSMain(HS_CONSTANT_DATA_OUTPUT input,
    float3 UV : SV_DomainLocation,
    uint viewID : SV_ViewID,
    const OutputPatch<BEZIER_CONTROL_POINT, 3> bezpatch)
{
    PSInput Output;

    Output.position = float4(
        bezpatch[0].vPosition * UV.x + viewID +
        bezpatch[1].vPosition * UV.y +
        bezpatch[2].vPosition * UV.z
        ,1);
    Output.color = float4(
        bezpatch[0].color * UV.x +
        bezpatch[1].color * UV.y +
        bezpatch[2].color * UV.z
        );
    return Output;
}

)",
      L"DSMain", L"ds_6_5", true /*does use view id*/);

}

TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesViewId_DSNonDependent)
{
  ComparePSV0BeforeAndAfterOptimization(
      R"(
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

struct HS_CONSTANT_DATA_OUTPUT
{
    float Edges[3]        : SV_TessFactor;
    float Inside       : SV_InsideTessFactor;
};

struct BEZIER_CONTROL_POINT
{
    float3 vPosition    : BEZIERPOS;
    float4 color : COLOR;
};

[domain("tri")]
PSInput DSMain(HS_CONSTANT_DATA_OUTPUT input,
    float3 UV : SV_DomainLocation,
    uint viewID : SV_ViewID,
    const OutputPatch<BEZIER_CONTROL_POINT, 3> bezpatch)
{
    PSInput Output;

    Output.position = float4(
        bezpatch[0].vPosition * UV.x + 
        bezpatch[1].vPosition * UV.y +
        bezpatch[2].vPosition * UV.z
        ,1);
    Output.color = float4(
        bezpatch[0].color * UV.x +
        bezpatch[1].color * UV.y +
        bezpatch[2].color * UV.z
        );
    return Output;
}

)",
      L"DSMain", L"ds_6_5", false /*does not use view id*/);

}


TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesViewId_VSDependent)
{
  ComparePSV0BeforeAndAfterOptimization(
      R"(
struct VertexShaderInput
{
	float3 pos : POSITION;
	float3 color : COLOR0;
	uint viewID : SV_ViewID;
};
struct VertexShaderOutput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	output.pos = float4(input.pos, 1.0f);
	if (input.viewID % 2) {
		output.color = float3(1.0f, 0.0f, 1.0f);
	} else {
		output.color = float3(0.0f, 1.0f, 0.0f);
	}
	return output;
}

)",
      L"main", L"vs_6_5", true /*does use view id*/);

}


TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesViewId_VSNonDependent)
{
  ComparePSV0BeforeAndAfterOptimization(
      R"(
struct VertexShaderInput
{
	float3 pos : POSITION;
	float3 color : COLOR0;
	uint viewID : SV_ViewID;
};
struct VertexShaderOutput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;
	output.pos = float4(input.pos, 1.0f);
	if (false) {
		output.color = float3(1.0f, 0.0f, 1.0f);
	} else {
		output.color = float3(0.0f, 1.0f, 0.0f);
	}
	return output;
}

)",
      L"main", L"vs_6_5", false /*does not use view id*/);

}

TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesViewId_GSNonDependent)
{
  ComparePSV0BeforeAndAfterOptimization(
      R"(


struct MyStructIn
{
//  float4 pos : SV_Position;
  float4 a : AAA;
  float2 b : BBB;
  float4 c[3] : CCC;
  //uint d : SV_RenderTargetIndex;
  float4 pos : SV_Position;
};

struct MyStructOut
{
  float4 pos : SV_Position;
  float2 out_a : OUT_AAA;
  uint d : SV_RenderTargetArrayIndex;
};

[maxvertexcount(18)]
void main(triangleadj MyStructIn array[6], inout TriangleStream<MyStructOut> OutputStream0)
{
  float4 r = array[1].a + array[2].b.x + array[3].pos;
  r += array[r.x].c[r.y].w;
  MyStructOut output = (MyStructOut)0;
  output.pos = array[r.x].a;
  output.out_a = array[r.y].b;
  output.d = array[r.x].a + 3;
  OutputStream0.Append(output);
  OutputStream0.RestartStrip();
}


)",
      L"main", L"gs_6_5", false /*does not use view id*/);

}

TEST_F(OptimizerTest, OptimizerWhenPassedContainerPreservesViewId_GSDependent)
{
  ComparePSV0BeforeAndAfterOptimization(
      R"(


struct MyStructIn
{
//  float4 pos : SV_Position;
  float4 a : AAA;
  float2 b : BBB;
  float4 c[3] : CCC;
  float4 pos : SV_Position;
};

struct MyStructOut
{
  float4 pos : SV_Position;
  float2 out_a : OUT_AAA;
  uint d : SV_RenderTargetArrayIndex;
};

[maxvertexcount(18)]
void main(triangleadj MyStructIn array[6], inout TriangleStream<MyStructOut> OutputStream0)
{
  float4 r = array[1].a + array[2].b.x + array[3].pos;
  r += array[r.x].c[r.y].w;
  MyStructOut output = (MyStructOut)0;
  output.pos = array[r.x].a;
  output.out_a = array[r.y].b;
  output.d = array[r.x].a + 3;
  OutputStream0.Append(output);
  OutputStream0.RestartStrip();
}


)",
      L"main", L"gs_6_5", false /*does not use view id*/, 4);

}

