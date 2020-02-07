///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilContainerTest.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides tests for container formatting and validation.                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif

#ifdef __APPLE__
// Workaround for ambiguous wcsstr on Mac
#define _WCHAR_H_CPLUSPLUS_98_CONFORMANCE_
#endif

#include <memory>
#include <vector>
#include <string>
#include <tuple>
#include <cassert>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#ifdef _WIN32
#include <atlfile.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#if _MSC_VER >= 1920
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include  <experimental/filesystem>
#else
#include <filesystem>
#endif
#endif

#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include "dxc/Test/HLSLTestData.h"
#include "dxc/Test/HlslTestUtils.h"
#include "dxc/Test/DxcTestUtils.h"

#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilRuntimeReflection.h"
#include "dxc/DxilContainer/DxilPipelineStateValidation.h"
#include "dxc/DXIL/DxilShaderFlags.h"
#include "dxc/DXIL/DxilUtil.h"

#include <fstream>
#include <chrono>

#include <codecvt>


using namespace std;
using namespace hlsl_test;
#ifdef _WIN32

using namespace std::experimental::filesystem;

static uint8_t MaskCount(uint8_t V) {
  DXASSERT_NOMSG(0 <= V && V <= 0xF);
  static const uint8_t Count[16] = {
    0, 1, 1, 2,
    1, 2, 2, 3,
    1, 2, 2, 3,
    2, 3, 3, 4
  };
  return Count[V];
}
#endif

#ifdef _WIN32
class DxilContainerTest {
#else
class DxilContainerTest : public ::testing::Test {
#endif
public:
  BEGIN_TEST_CLASS(DxilContainerTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  TEST_METHOD(CompileWhenDebugSourceThenSourceMatters)
  TEST_METHOD(CompileAS_CheckPSV0)
  TEST_METHOD(CompileWhenOkThenCheckRDAT)
  TEST_METHOD(CompileWhenOkThenCheckRDAT2)
  TEST_METHOD(CompileWhenOkThenCheckReflection1)
  TEST_METHOD(DxcUtils_CreateReflection)
  TEST_METHOD(CompileWhenOKThenIncludesFeatureInfo)
  TEST_METHOD(CompileWhenOKThenIncludesSignatures)
  TEST_METHOD(CompileWhenSigSquareThenIncludeSplit)
  TEST_METHOD(DisassemblyWhenMissingThenFails)
  TEST_METHOD(DisassemblyWhenBCInvalidThenFails)
  TEST_METHOD(DisassemblyWhenInvalidThenFails)
  TEST_METHOD(DisassemblyWhenValidThenOK)
  TEST_METHOD(ValidateFromLL_Abs2)
  TEST_METHOD(DxilContainerUnitTest)

  TEST_METHOD(ReflectionMatchesDXBC_CheckIn)
  BEGIN_TEST_METHOD(ReflectionMatchesDXBC_Full)
    TEST_METHOD_PROPERTY(L"Priority", L"1")
  END_TEST_METHOD()

  dxc::DxcDllSupport m_dllSupport;
  VersionSupportInfo m_ver;

  void CreateBlobPinned(_In_bytecount_(size) LPCVOID data, SIZE_T size,
                        UINT32 codePage, _Outptr_ IDxcBlobEncoding **ppBlob) {
    CComPtr<IDxcLibrary> library;
    IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
    IFT(library->CreateBlobWithEncodingFromPinned((LPBYTE)data, size, codePage,
                                                  ppBlob));
  }

  void CreateBlobFromText(_In_z_ const char *pText,
                          _Outptr_ IDxcBlobEncoding **ppBlob) {
    CreateBlobPinned(pText, strlen(pText), CP_UTF8, ppBlob);
  }

  HRESULT CreateCompiler(IDxcCompiler **ppResult) {
    if (!m_dllSupport.IsEnabled()) {
      VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    }
    return m_dllSupport.CreateInstance(CLSID_DxcCompiler, ppResult);
  }

#ifdef _WIN32 // - Reflection Unsupported
  void CompareShaderInputBindDesc(D3D12_SHADER_INPUT_BIND_DESC *pTestDesc,
    D3D12_SHADER_INPUT_BIND_DESC *pBaseDesc) {
    VERIFY_ARE_EQUAL(pTestDesc->BindCount, pBaseDesc->BindCount);
    VERIFY_ARE_EQUAL(pTestDesc->BindPoint, pBaseDesc->BindPoint);
    VERIFY_ARE_EQUAL(pTestDesc->Dimension, pBaseDesc->Dimension);
    VERIFY_ARE_EQUAL_STR(pTestDesc->Name, pBaseDesc->Name);
    VERIFY_ARE_EQUAL(pTestDesc->NumSamples, pBaseDesc->NumSamples);
    VERIFY_ARE_EQUAL(pTestDesc->ReturnType, pBaseDesc->ReturnType);
    VERIFY_ARE_EQUAL(pTestDesc->Space, pBaseDesc->Space);
    if (pBaseDesc->Type == D3D_SIT_UAV_APPEND_STRUCTURED ||
        pBaseDesc->Type == D3D_SIT_UAV_CONSUME_STRUCTURED) {
      // dxil only have rw with counter.
      VERIFY_ARE_EQUAL(pTestDesc->Type, D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER);
    } else if (pTestDesc->Type == D3D_SIT_STRUCTURED &&
               pBaseDesc->Type != D3D_SIT_STRUCTURED) {
      VERIFY_ARE_EQUAL(pBaseDesc->Type, D3D_SIT_TEXTURE);
    } else
      VERIFY_ARE_EQUAL(pTestDesc->Type, pBaseDesc->Type);
    // D3D_SIF_USERPACKED is never set in dxil.
    UINT unusedFlag = D3D_SIF_USERPACKED;
    VERIFY_ARE_EQUAL(pTestDesc->uFlags, pBaseDesc->uFlags & ~unusedFlag);
    // VERIFY_ARE_EQUAL(pTestDesc->uID, pBaseDesc->uID); // Like register, this can vary.
  }

  void CompareParameterDesc(D3D12_SIGNATURE_PARAMETER_DESC *pTestDesc,
                            D3D12_SIGNATURE_PARAMETER_DESC *pBaseDesc, bool isInput) {
    VERIFY_ARE_EQUAL(0, _stricmp(pTestDesc->SemanticName, pBaseDesc->SemanticName));
    if (pTestDesc->SystemValueType == D3D_NAME_CLIP_DISTANCE) return; // currently generating multiple clip distance params, one per component
    VERIFY_ARE_EQUAL(pTestDesc->ComponentType, pBaseDesc->ComponentType);
    VERIFY_ARE_EQUAL(MaskCount(pTestDesc->Mask), MaskCount(pBaseDesc->Mask));
    VERIFY_ARE_EQUAL(pTestDesc->MinPrecision, pBaseDesc->MinPrecision);
    if (!isInput) {
      if (hlsl::DXIL::CompareVersions(m_ver.m_ValMajor, m_ver.m_ValMinor, 1, 5) < 0)
        VERIFY_ARE_EQUAL(pTestDesc->ReadWriteMask != 0, pBaseDesc->ReadWriteMask != 0);
      else
        VERIFY_ARE_EQUAL(MaskCount(pTestDesc->ReadWriteMask), MaskCount(pBaseDesc->ReadWriteMask));
    }
    // VERIFY_ARE_EQUAL(pTestDesc->Register, pBaseDesc->Register);
    //VERIFY_ARE_EQUAL(pTestDesc->SemanticIndex, pBaseDesc->SemanticIndex);
    VERIFY_ARE_EQUAL(pTestDesc->Stream, pBaseDesc->Stream);
    VERIFY_ARE_EQUAL(pTestDesc->SystemValueType, pBaseDesc->SystemValueType);
  }

  void CompareType(ID3D12ShaderReflectionType *pTest,
                   ID3D12ShaderReflectionType *pBase)
  {
    D3D12_SHADER_TYPE_DESC testDesc, baseDesc;
    VERIFY_SUCCEEDED(pTest->GetDesc(&testDesc));
    VERIFY_SUCCEEDED(pBase->GetDesc(&baseDesc));

    VERIFY_ARE_EQUAL(testDesc.Class,    baseDesc.Class);
    VERIFY_ARE_EQUAL(testDesc.Type,     baseDesc.Type);
    VERIFY_ARE_EQUAL(testDesc.Rows,     baseDesc.Rows);
    VERIFY_ARE_EQUAL(testDesc.Columns,  baseDesc.Columns);
    VERIFY_ARE_EQUAL(testDesc.Elements, baseDesc.Elements);
    VERIFY_ARE_EQUAL(testDesc.Members,  baseDesc.Members);

    VERIFY_ARE_EQUAL(testDesc.Offset,   baseDesc.Offset);

    VERIFY_ARE_EQUAL(0, strcmp(testDesc.Name, baseDesc.Name));

    for (UINT i = 0; i < baseDesc.Members; ++i) {
      ID3D12ShaderReflectionType* testMemberType = pTest->GetMemberTypeByIndex(i);
      ID3D12ShaderReflectionType* baseMemberType = pBase->GetMemberTypeByIndex(i);
      VERIFY_IS_NOT_NULL(testMemberType);
      VERIFY_IS_NOT_NULL(baseMemberType);

      CompareType(testMemberType, baseMemberType);

      LPCSTR testMemberName = pTest->GetMemberTypeName(i);
      LPCSTR baseMemberName = pBase->GetMemberTypeName(i);
      VERIFY_ARE_EQUAL(0, strcmp(testMemberName, baseMemberName));
    }
  }

  typedef HRESULT (__stdcall ID3D12ShaderReflection::*GetParameterDescFn)(UINT, D3D12_SIGNATURE_PARAMETER_DESC*);

  void SortNameIdxVector(std::vector<std::tuple<LPCSTR, UINT, UINT>> &value) {
    struct FirstPredT {
      bool operator()(std::tuple<LPCSTR, UINT, UINT> &l,
                      std::tuple<LPCSTR, UINT, UINT> &r) {
        int strResult = strcmp(std::get<0>(l), std::get<0>(r));
        return (strResult < 0 ) || (strResult == 0 && std::get<1>(l) < std::get<1>(r));
      }
    } FirstPred;
    std::sort(value.begin(), value.end(), FirstPred);
  }

  void CompareParameterDescs(UINT count, ID3D12ShaderReflection *pTest,
                             ID3D12ShaderReflection *pBase,
                             GetParameterDescFn Fn, bool isInput) {
    std::vector<std::tuple<LPCSTR, UINT, UINT>> testParams, baseParams;
    testParams.reserve(count);
    baseParams.reserve(count);
    for (UINT i = 0; i < count; ++i) {
      D3D12_SIGNATURE_PARAMETER_DESC D;
      VERIFY_SUCCEEDED((pTest->*Fn)(i, &D));
      testParams.push_back(std::make_tuple(D.SemanticName, D.SemanticIndex, i));
      VERIFY_SUCCEEDED((pBase->*Fn)(i, &D));
      baseParams.push_back(std::make_tuple(D.SemanticName, D.SemanticIndex, i));
    }
    SortNameIdxVector(testParams);
    SortNameIdxVector(baseParams);
    for (UINT i = 0; i < count; ++i) {
      D3D12_SIGNATURE_PARAMETER_DESC testParamDesc, baseParamDesc;
      VERIFY_SUCCEEDED(
          (pTest->*Fn)(std::get<2>(testParams[i]), &testParamDesc));
      VERIFY_SUCCEEDED(
          (pBase->*Fn)(std::get<2>(baseParams[i]), &baseParamDesc));
      CompareParameterDesc(&testParamDesc, &baseParamDesc, isInput);
    }
  }

  void CompareReflection(ID3D12ShaderReflection *pTest, ID3D12ShaderReflection *pBase) {
    D3D12_SHADER_DESC testDesc, baseDesc;
    VERIFY_SUCCEEDED(pTest->GetDesc(&testDesc));
    VERIFY_SUCCEEDED(pBase->GetDesc(&baseDesc));
    VERIFY_ARE_EQUAL(D3D12_SHVER_GET_TYPE(testDesc.Version), D3D12_SHVER_GET_TYPE(baseDesc.Version));
    VERIFY_ARE_EQUAL(testDesc.ConstantBuffers, baseDesc.ConstantBuffers);
    VERIFY_ARE_EQUAL(testDesc.BoundResources, baseDesc.BoundResources);
    VERIFY_ARE_EQUAL(testDesc.InputParameters, baseDesc.InputParameters);
    VERIFY_ARE_EQUAL(testDesc.OutputParameters, baseDesc.OutputParameters);
    VERIFY_ARE_EQUAL(testDesc.PatchConstantParameters, baseDesc.PatchConstantParameters);

    {
      for (UINT i = 0; i < testDesc.ConstantBuffers; ++i) {
        ID3D12ShaderReflectionConstantBuffer *pTestCB, *pBaseCB;
        D3D12_SHADER_BUFFER_DESC testCB, baseCB;
        pTestCB = pTest->GetConstantBufferByIndex(i);
        VERIFY_SUCCEEDED(pTestCB->GetDesc(&testCB));
        pBaseCB = pBase->GetConstantBufferByName(testCB.Name);
        VERIFY_SUCCEEDED(pBaseCB->GetDesc(&baseCB));
        VERIFY_ARE_EQUAL_STR(testCB.Name, baseCB.Name);
        VERIFY_ARE_EQUAL(testCB.Type, baseCB.Type);
        VERIFY_ARE_EQUAL(testCB.Variables, baseCB.Variables);
        VERIFY_ARE_EQUAL(testCB.Size, baseCB.Size);
        VERIFY_ARE_EQUAL(testCB.uFlags, baseCB.uFlags);

        llvm::StringMap<D3D12_SHADER_VARIABLE_DESC> variableMap;
        llvm::StringMap<ID3D12ShaderReflectionType*> variableTypeMap;
        for (UINT vi = 0; vi < testCB.Variables; ++vi) {
          ID3D12ShaderReflectionVariable *pBaseConst;
          D3D12_SHADER_VARIABLE_DESC baseConst;
          pBaseConst = pBaseCB->GetVariableByIndex(vi);
          VERIFY_SUCCEEDED(pBaseConst->GetDesc(&baseConst));
          variableMap[baseConst.Name] = baseConst;

          ID3D12ShaderReflectionType* pBaseType = pBaseConst->GetType();
          VERIFY_IS_NOT_NULL(pBaseType);
          variableTypeMap[baseConst.Name] = pBaseType;
        }
        for (UINT vi = 0; vi < testCB.Variables; ++vi) {
          ID3D12ShaderReflectionVariable *pTestConst;
          D3D12_SHADER_VARIABLE_DESC testConst;
          pTestConst = pTestCB->GetVariableByIndex(vi);
          VERIFY_SUCCEEDED(pTestConst->GetDesc(&testConst));
          VERIFY_ARE_EQUAL(variableMap.count(testConst.Name), 1);
          D3D12_SHADER_VARIABLE_DESC baseConst = variableMap[testConst.Name];
          VERIFY_ARE_EQUAL(testConst.uFlags, baseConst.uFlags);
          VERIFY_ARE_EQUAL(testConst.StartOffset, baseConst.StartOffset);

          VERIFY_ARE_EQUAL(testConst.Size, baseConst.Size);

          ID3D12ShaderReflectionType* pTestType = pTestConst->GetType();
          VERIFY_IS_NOT_NULL(pTestType);
          VERIFY_ARE_EQUAL(variableTypeMap.count(testConst.Name), 1);
          ID3D12ShaderReflectionType* pBaseType = variableTypeMap[testConst.Name];

          CompareType(pTestType, pBaseType);
        }
      }
    }

    for (UINT i = 0; i < testDesc.BoundResources; ++i) {
      D3D12_SHADER_INPUT_BIND_DESC testParamDesc, baseParamDesc;
      VERIFY_SUCCEEDED(pTest->GetResourceBindingDesc(i, &testParamDesc), WStrFmt(L"i=%u", i));
      VERIFY_SUCCEEDED(pBase->GetResourceBindingDescByName(testParamDesc.Name, &baseParamDesc));
      CompareShaderInputBindDesc(&testParamDesc, &baseParamDesc);
    }

    CompareParameterDescs(testDesc.InputParameters, pTest, pBase, &ID3D12ShaderReflection::GetInputParameterDesc, true);
    CompareParameterDescs(testDesc.OutputParameters, pTest, pBase,
                          &ID3D12ShaderReflection::GetOutputParameterDesc,
                          false);
    bool isHs = testDesc.HSPartitioning != D3D_TESSELLATOR_PARTITIONING::D3D11_TESSELLATOR_PARTITIONING_UNDEFINED;
    CompareParameterDescs(
        testDesc.PatchConstantParameters, pTest, pBase,
        &ID3D12ShaderReflection::GetPatchConstantParameterDesc, !isHs);

    {
      UINT32 testTotal, testX, testY, testZ;
      UINT32 baseTotal, baseX, baseY, baseZ;
      testTotal = pTest->GetThreadGroupSize(&testX, &testY, &testZ);
      baseTotal = pBase->GetThreadGroupSize(&baseX, &baseY, &baseZ);
      VERIFY_ARE_EQUAL(testTotal, baseTotal);
      VERIFY_ARE_EQUAL(testX, baseX);
      VERIFY_ARE_EQUAL(testY, baseY);
      VERIFY_ARE_EQUAL(testZ, baseZ);
    }
  }
#endif // _WIN32 - Reflection unsupported

#ifdef _WIN32  // - Reflection unsupported
  HRESULT CompileFromFile(LPCWSTR path, bool useDXBC, IDxcBlob **ppBlob) {
    std::vector<FileRunCommandPart> parts;
    ParseCommandPartsFromFile(path, parts);
    VERIFY_IS_TRUE(parts.size() > 0);
    unsigned partIdx = 0;
    if (parts[0].Command.compare("%dxilver") == 0) {
      partIdx = 1;
    }
    FileRunCommandPart &dxc = parts[partIdx];
    VERIFY_ARE_EQUAL_STR(dxc.Command.c_str(), "%dxc");
    m_dllSupport.Initialize();

    hlsl::options::MainArgs args;
    hlsl::options::DxcOpts opts;
    dxc.ReadOptsForDxc(args, opts);
    if (opts.CodeGenHighLevel) return E_FAIL; // skip for now
    if (useDXBC) {
      // Consider supporting defines and flags if/when needed.
      std::string TargetProfile(opts.TargetProfile.str());
      TargetProfile[3] = '5'; TargetProfile[5] = '1';
      CComPtr<ID3DBlob> pDxbcBlob;
      CComPtr<ID3DBlob> pDxbcErrors;
      UINT unboundDescTab = (1 << 20);
      IFR(D3DCompileFromFile(path, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        opts.EntryPoint.str().c_str(),
        TargetProfile.c_str(),
        unboundDescTab, 0, &pDxbcBlob, &pDxbcErrors));
      IFR(pDxbcBlob.QueryInterface(ppBlob));
    }
    else {
      FileRunCommandResult result = dxc.Run(m_dllSupport, nullptr);
      IFRBOOL(result.ExitCode == 0, E_FAIL);
      IFR(result.OpResult->GetResult(ppBlob));
    }
    return S_OK;
  }

  void CreateReflectionFromBlob(IDxcBlob *pBlob, ID3D12ShaderReflection **ppReflection) {
    CComPtr<IDxcContainerReflection> pReflection;
    UINT32 shaderIdx;
    m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection);
    VERIFY_SUCCEEDED(pReflection->Load(pBlob));
    VERIFY_SUCCEEDED(pReflection->FindFirstPartKind(hlsl::DFCC_DXIL, &shaderIdx));
    VERIFY_SUCCEEDED(pReflection->GetPartReflection(shaderIdx, __uuidof(ID3D12ShaderReflection), (void**)ppReflection));
  }

  void CreateReflectionFromDXBC(IDxcBlob *pBlob, ID3D12ShaderReflection **ppReflection) {
    VERIFY_SUCCEEDED(
        D3DReflect(pBlob->GetBufferPointer(), pBlob->GetBufferSize(),
                   __uuidof(ID3D12ShaderReflection), (void **)ppReflection));
  }
#endif // _WIN32 - Reflection unsupported

  void CompileToProgram(LPCSTR program, LPCWSTR entryPoint, LPCWSTR target,
                        LPCWSTR *pArguments, UINT32 argCount,
                        IDxcBlob **ppProgram,
                        IDxcOperationResult **ppResult = nullptr) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcBlob> pProgram;
    CComPtr<IDxcOperationResult> pResult;

    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
    CreateBlobFromText(program, &pSource);
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", entryPoint,
                                        target, pArguments, argCount, nullptr,
                                        0, nullptr, &pResult));
    VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
    *ppProgram = pProgram.Detach();
    if (ppResult)
      *ppResult = pResult.Detach();
  }

  bool DoesValidatorSupportDebugName() {
    CComPtr<IDxcVersionInfo> pVersionInfo;
    UINT Major, Minor;
    HRESULT hrVer = m_dllSupport.CreateInstance(CLSID_DxcValidator, &pVersionInfo);
    if (hrVer == E_NOINTERFACE) return false;
    VERIFY_SUCCEEDED(hrVer);
    VERIFY_SUCCEEDED(pVersionInfo->GetVersion(&Major, &Minor));
    return !(Major == 1 && Minor < 1);
  }

  bool DoesValidatorSupportShaderHash() {
    CComPtr<IDxcVersionInfo> pVersionInfo;
    UINT Major, Minor;
    HRESULT hrVer = m_dllSupport.CreateInstance(CLSID_DxcValidator, &pVersionInfo);
    if (hrVer == E_NOINTERFACE) return false;
    VERIFY_SUCCEEDED(hrVer);
    VERIFY_SUCCEEDED(pVersionInfo->GetVersion(&Major, &Minor));
    return !(Major == 1 && Minor < 5);
  }

  std::string CompileToDebugName(LPCSTR program, LPCWSTR entryPoint,
                                 LPCWSTR target, LPCWSTR *pArguments, UINT32 argCount) {
    CComPtr<IDxcBlob> pProgram;
    CComPtr<IDxcBlob> pNameBlob;
    CComPtr<IDxcContainerReflection> pContainer;
    UINT32 index;

    CompileToProgram(program, entryPoint, target, pArguments, argCount, &pProgram);
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pContainer));
    VERIFY_SUCCEEDED(pContainer->Load(pProgram));
    if (FAILED(pContainer->FindFirstPartKind(hlsl::DFCC_ShaderDebugName, &index))) {
      return std::string();
    }
    VERIFY_SUCCEEDED(pContainer->GetPartContent(index, &pNameBlob));
    const hlsl::DxilShaderDebugName *pDebugName = (hlsl::DxilShaderDebugName *)pNameBlob->GetBufferPointer();
    return std::string((const char *)(pDebugName + 1));
  }

  std::string RetrieveHashFromBlob(IDxcBlob* pHashBlob) {
    VERIFY_ARE_NOT_EQUAL(pHashBlob, nullptr);
    VERIFY_ARE_EQUAL(pHashBlob->GetBufferSize(), sizeof(DxcShaderHash));
    const hlsl::DxilShaderHash *pShaderHash = (hlsl::DxilShaderHash *)pHashBlob->GetBufferPointer();
    std::string result;
    llvm::raw_string_ostream os(result);
    for (int i = 0; i < 16; ++i)
      os << llvm::format("%.2x", pShaderHash->Digest[i]);
    return os.str();
  }

  std::string CompileToShaderHash(LPCSTR program, LPCWSTR entryPoint,
    LPCWSTR target, LPCWSTR *pArguments, UINT32 argCount) {
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlob> pProgram;
    CComPtr<IDxcBlob> pHashBlob;
    CComPtr<IDxcContainerReflection> pContainer;
    UINT32 index;

    CompileToProgram(program, entryPoint, target, pArguments, argCount, &pProgram, &pResult);
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pContainer));
    VERIFY_SUCCEEDED(pContainer->Load(pProgram));
    if (FAILED(pContainer->FindFirstPartKind(hlsl::DFCC_ShaderHash, &index))) {
      return std::string();
    }
    VERIFY_SUCCEEDED(pContainer->GetPartContent(index, &pHashBlob));
    std::string hashFromPart = RetrieveHashFromBlob(pHashBlob);

    CComPtr<IDxcResult> pDxcResult;
    if (SUCCEEDED(pResult->QueryInterface(&pDxcResult))) {
      // Make sure shader hash was returned in result
      VERIFY_IS_TRUE(pDxcResult->HasOutput(DXC_OUT_SHADER_HASH));
      pHashBlob.Release();
      VERIFY_SUCCEEDED(pDxcResult->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&pHashBlob), nullptr));
      std::string hashFromResult = RetrieveHashFromBlob(pHashBlob);
      VERIFY_ARE_EQUAL(hashFromPart, hashFromPart);
    }

    return hashFromPart;
  }

  std::string DisassembleProgram(LPCSTR program, LPCWSTR entryPoint,
                                 LPCWSTR target) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcBlob> pProgram;
    CComPtr<IDxcBlobEncoding> pDisassembly;

    CompileToProgram(program, entryPoint, target, nullptr, 0, &pProgram);
    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
    VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembly));
    return BlobToUtf8(pDisassembly);
  }

  void SetupBasicHeader(hlsl::DxilContainerHeader *pHeader,
                        uint32_t partCount = 0) {
    ZeroMemory(pHeader, sizeof(*pHeader));
    pHeader->HeaderFourCC = hlsl::DFCC_Container;
    pHeader->Version.Major = 1;
    pHeader->Version.Minor = 0;
    pHeader->PartCount = partCount;
    pHeader->ContainerSizeInBytes =
      sizeof(hlsl::DxilContainerHeader) +
      sizeof(uint32_t) * partCount +
      sizeof(hlsl::DxilPartHeader) * partCount;
  }

  void CodeGenTestCheck(LPCWSTR name) {
    std::wstring fullPath = hlsl_test::GetPathToHlslDataFile(name);
    FileRunTestResult t = FileRunTestResult::RunFromFileCommands(fullPath.c_str());
    if (t.RunResult != 0) {
      CA2W commentWide(t.ErrorMessage.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(commentWide);
      WEX::Logging::Log::Error(L"Run result is not zero");
    }
  }

#ifdef _WIN32 // Reflection unsupported
  WEX::Common::String WStrFmt(const wchar_t* msg, ...) {
    va_list args;
    va_start(args, msg);
    WEX::Common::String result = WEX::Common::String().FormatV(msg, args);
    va_end(args);
    return result;
  }

  void ReflectionTest(LPCWSTR name, bool ignoreIfDXBCFails) {
    WEX::Logging::Log::Comment(WEX::Common::String().Format(L"Reflection comparison for %s", name));

    // Skip if unsupported.
    std::vector<FileRunCommandPart> parts;
    ParseCommandPartsFromFile(name, parts);
    VERIFY_IS_TRUE(parts.size() > 0);
    if (parts[0].Command.compare("%dxilver") == 0) {
      VERIFY_IS_TRUE(parts.size() > 1);
      auto result = parts[0].Run(m_dllSupport, nullptr);
      if (result.ExitCode != 0)
        return;
    }

    CComPtr<IDxcBlob> pProgram;
    CComPtr<IDxcBlob> pProgramDXBC;
    HRESULT hrDXBC = CompileFromFile(name, true, &pProgramDXBC);
    if (FAILED(hrDXBC)) {
      WEX::Logging::Log::Comment(L"Failed to compile DXBC blob.");
      if (ignoreIfDXBCFails) return;
      VERIFY_FAIL();
    }
    if (FAILED(CompileFromFile(name, false, &pProgram))) {
      WEX::Logging::Log::Comment(L"Failed to compile DXIL blob.");
      VERIFY_FAIL();
    }
    
    CComPtr<ID3D12ShaderReflection> pProgramReflection;
    CComPtr<ID3D12ShaderReflection> pProgramReflectionDXBC;
    CreateReflectionFromBlob(pProgram, &pProgramReflection);
    CreateReflectionFromDXBC(pProgramDXBC, &pProgramReflectionDXBC);

    CompareReflection(pProgramReflection, pProgramReflectionDXBC);
  }
#endif // _WIN32 - Reflection unsupported
};

bool DxilContainerTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    m_ver.Initialize(m_dllSupport);
  }
  return true;
}

#ifdef _WIN32
TEST_F(DxilContainerTest, CompileWhenDebugSourceThenSourceMatters) {
  char program1[] = "float4 main() : SV_Target { return 0; }";
  char program2[] = "  float4 main() : SV_Target { return 0; }  ";
  LPCWSTR Zi[] = { L"/Zi", L"/Qembed_debug" };
  LPCWSTR ZiZss[] = { L"/Zi", L"/Qembed_debug", L"/Zss" };
  LPCWSTR ZiZsb[] = { L"/Zi", L"/Qembed_debug", L"/Zsb" };
  LPCWSTR Zsb[] = { L"/Zsb" };
  
  // No debug info, no debug name...
  std::string noName = CompileToDebugName(program1, L"main", L"ps_6_0", nullptr, 0);
  VERIFY_IS_TRUE(noName.empty());

  if (!DoesValidatorSupportDebugName())
    return;

  // Debug info, default to source name.
  std::string sourceName1 = CompileToDebugName(program1, L"main", L"ps_6_0", Zi, _countof(Zi));
  VERIFY_IS_FALSE(sourceName1.empty());

  // Deterministic naming.
  std::string sourceName1Again = CompileToDebugName(program1, L"main", L"ps_6_0", Zi, _countof(Zi));
  VERIFY_ARE_EQUAL_STR(sourceName1.c_str(), sourceName1Again.c_str());

  // Use program binary by default, so name should be the same.
  std::string sourceName2 = CompileToDebugName(program2, L"main", L"ps_6_0", Zi, _countof(Zi));
  VERIFY_IS_TRUE(0 == strcmp(sourceName2.c_str(), sourceName1.c_str()));

  // Source again, different because different switches are specified.
  std::string sourceName1Zss = CompileToDebugName(program1, L"main", L"ps_6_0", ZiZss, _countof(ZiZss));
  VERIFY_IS_FALSE(0 == strcmp(sourceName1Zss.c_str(), sourceName1.c_str()));

  // Binary program 1 and 2 should be different from source and equal to each other.
  std::string binName1 = CompileToDebugName(program1, L"main", L"ps_6_0", ZiZsb, _countof(ZiZsb));
  std::string binName2 = CompileToDebugName(program2, L"main", L"ps_6_0", ZiZsb, _countof(ZiZsb));
  VERIFY_ARE_EQUAL_STR(binName1.c_str(), binName2.c_str());
  VERIFY_IS_FALSE(0 == strcmp(sourceName1Zss.c_str(), binName1.c_str()));

  if (!DoesValidatorSupportShaderHash())
    return;

  // Verify source hash
  std::string binHash1Zss = CompileToShaderHash(program1, L"main", L"ps_6_0", ZiZss, _countof(ZiZss));
  VERIFY_IS_FALSE(binHash1Zss.empty());

  // bin hash when compiling with /Zi
  std::string binHash1 = CompileToShaderHash(program1, L"main", L"ps_6_0", ZiZsb, _countof(ZiZsb));
  VERIFY_IS_FALSE(binHash1.empty());

  // Without /Zi hash for /Zsb should be the same
  std::string binHash2 = CompileToShaderHash(program2, L"main", L"ps_6_0", Zsb, _countof(Zsb));
  VERIFY_IS_FALSE(binHash2.empty());
  VERIFY_ARE_EQUAL_STR(binHash1.c_str(), binHash2.c_str());

  // Source hash and bin hash should be different
  VERIFY_IS_FALSE(0 == strcmp(binHash1Zss.c_str(), binHash1.c_str()));
}
#endif // _WIN32

TEST_F(DxilContainerTest, CompileWhenOKThenIncludesSignatures) {
  char program[] =
    "struct PSInput {\r\n"
    " float4 position : SV_POSITION;\r\n"
    " float4 color : COLOR;\r\n"
    "};\r\n"
    "PSInput VSMain(float4 position : POSITION, float4 color : COLOR) {\r\n"
    " PSInput result;\r\n"
    " result.position = position;\r\n"
    " result.color = color;\r\n"
    " return result;\r\n"
    "}\r\n"
    "float4 PSMain(PSInput input) : SV_TARGET {\r\n"
    " return input.color;\r\n"
    "}";

  {
    std::string s = DisassembleProgram(program, L"VSMain", L"vs_6_0");
    // NOTE: this will change when proper packing is done, and when 'always-reads' is accurately implemented.
    const char expected_1_4[] =
      ";\n"
      "; Input signature:\n"
      ";\n"
      "; Name                 Index   Mask Register SysValue  Format   Used\n"
      "; -------------------- ----- ------ -------- -------- ------- ------\n"
      "; POSITION                 0   xyzw        0     NONE   float       \n" // should read 'xyzw' in Used
      "; COLOR                    0   xyzw        1     NONE   float       \n" // should read '1' in register
      ";\n"
      ";\n"
      "; Output signature:\n"
      ";\n"
      "; Name                 Index   Mask Register SysValue  Format   Used\n"
      "; -------------------- ----- ------ -------- -------- ------- ------\n"
      "; SV_Position              0   xyzw        0      POS   float   xyzw\n"  // could read SV_POSITION
      "; COLOR                    0   xyzw        1     NONE   float   xyzw\n"; // should read '1' in register
    const char expected[] =
      ";\n"
      "; Input signature:\n"
      ";\n"
      "; Name                 Index   Mask Register SysValue  Format   Used\n"
      "; -------------------- ----- ------ -------- -------- ------- ------\n"
      "; POSITION                 0   xyzw        0     NONE   float   xyzw\n" // should read 'xyzw' in Used
      "; COLOR                    0   xyzw        1     NONE   float   xyzw\n" // should read '1' in register
      ";\n"
      ";\n"
      "; Output signature:\n"
      ";\n"
      "; Name                 Index   Mask Register SysValue  Format   Used\n"
      "; -------------------- ----- ------ -------- -------- ------- ------\n"
      "; SV_Position              0   xyzw        0      POS   float   xyzw\n"  // could read SV_POSITION
      "; COLOR                    0   xyzw        1     NONE   float   xyzw\n"; // should read '1' in register
    if (hlsl::DXIL::CompareVersions(m_ver.m_ValMajor, m_ver.m_ValMinor, 1, 5) < 0) {
      std::string start(s.c_str(), strlen(expected_1_4));
      VERIFY_ARE_EQUAL_STR(expected_1_4, start.c_str());
    } else {
      std::string start(s.c_str(), strlen(expected));
      VERIFY_ARE_EQUAL_STR(expected, start.c_str());
    }
  }

  {
    std::string s = DisassembleProgram(program, L"PSMain", L"ps_6_0");
    // NOTE: this will change when proper packing is done, and when 'always-reads' is accurately implemented.
    const char expected_1_4[] =
      ";\n"
      "; Input signature:\n"
      ";\n"
      "; Name                 Index   Mask Register SysValue  Format   Used\n"
      "; -------------------- ----- ------ -------- -------- ------- ------\n"
      "; SV_Position              0   xyzw        0      POS   float       \n" // could read SV_POSITION
      "; COLOR                    0   xyzw        1     NONE   float       \n" // should read '1' in register, xyzw in Used
      ";\n"
      ";\n"
      "; Output signature:\n"
      ";\n"
      "; Name                 Index   Mask Register SysValue  Format   Used\n"
      "; -------------------- ----- ------ -------- -------- ------- ------\n"
      "; SV_Target                0   xyzw        0   TARGET   float   xyzw\n";// could read SV_TARGET
    const char expected[] =
      ";\n"
      "; Input signature:\n"
      ";\n"
      "; Name                 Index   Mask Register SysValue  Format   Used\n"
      "; -------------------- ----- ------ -------- -------- ------- ------\n"
      "; SV_Position              0   xyzw        0      POS   float       \n" // could read SV_POSITION
      "; COLOR                    0   xyzw        1     NONE   float   xyzw\n" // should read '1' in register, xyzw in Used
      ";\n"
      ";\n"
      "; Output signature:\n"
      ";\n"
      "; Name                 Index   Mask Register SysValue  Format   Used\n"
      "; -------------------- ----- ------ -------- -------- ------- ------\n"
      "; SV_Target                0   xyzw        0   TARGET   float   xyzw\n";// could read SV_TARGET
    if (hlsl::DXIL::CompareVersions(m_ver.m_ValMajor, m_ver.m_ValMinor, 1, 5) < 0) {
      std::string start(s.c_str(), strlen(expected_1_4));
      VERIFY_ARE_EQUAL_STR(expected_1_4, start.c_str());
    } else {
      std::string start(s.c_str(), strlen(expected));
      VERIFY_ARE_EQUAL_STR(expected, start.c_str());
    }
  }
}

TEST_F(DxilContainerTest, CompileWhenSigSquareThenIncludeSplit) {
#if 0 // TODO: reenable test when multiple rows are supported.
  const char program[] =
    "float main(float4x4 a : A, int4 b : B) : SV_Target {\n"
    " return a[b.x][b.y];\n"
    "}";
  std::string s = DisassembleProgram(program, L"main", L"ps_6_0");
  const char expected[] =
    "// Input signature:\n"
    "//\n"
    "// Name                 Index   Mask Register SysValue  Format   Used\n"
    "// -------------------- ----- ------ -------- -------- ------- ------\n"
    "// A                        0   xyzw        0     NONE   float   xyzw\n"
    "// A                        1   xyzw        1     NONE   float   xyzw\n"
    "// A                        2   xyzw        2     NONE   float   xyzw\n"
    "// A                        3   xyzw        3     NONE   float   xyzw\n"
    "// B                        0   xyzw        4     NONE     int   xy\n"
    "//\n"
    "//\n"
    "// Output signature:\n"
    "//\n"
    "// Name                 Index   Mask Register SysValue  Format   Used\n"
    "// -------------------- ----- ------ -------- -------- ------- ------\n"
    "// SV_Target                0   x           0   TARGET   float   x\n";
  std::string start(s.c_str(), strlen(expected));
  VERIFY_ARE_EQUAL_STR(expected, start.c_str());
#endif
}

TEST_F(DxilContainerTest, CompileAS_CheckPSV0) {
  if (m_ver.SkipDxilVersion(1, 5)) return;
  const char asSource[] =
    "struct PayloadType { uint a, b, c; };\n"
    "[shader(\"amplification\")]\n"
    "[numthreads(1,1,1)]\n"
    "void main(uint idx : SV_GroupIndex) {\n"
    " PayloadType p = { idx, 2, 3 };\n"
    " DispatchMesh(1,1,1, p);\n"
    "}";

  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  CComPtr<IDxcOperationResult> pResult;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(asSource, &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"main",
                                      L"as_6_5", nullptr, 0, nullptr, 0,
                                      nullptr, &pResult));
  HRESULT hrStatus;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hrStatus));
  VERIFY_SUCCEEDED(hrStatus);
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  CComPtr<IDxcContainerReflection> containerReflection;
  uint32_t partCount;
  IFT(m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &containerReflection));
  IFT(containerReflection->Load(pProgram));
  IFT(containerReflection->GetPartCount(&partCount));
  bool blobFound = false;
  for (uint32_t i = 0; i < partCount; ++i) {
    uint32_t kind;
    VERIFY_SUCCEEDED(containerReflection->GetPartKind(i, &kind));
    if (kind == (uint32_t)hlsl::DxilFourCC::DFCC_PipelineStateValidation) {
      blobFound = true;
      CComPtr<IDxcBlob> pBlob;
      VERIFY_SUCCEEDED(containerReflection->GetPartContent(i, &pBlob));
      DxilPipelineStateValidation PSV;
      PSV.InitFromPSV0(pBlob->GetBufferPointer(), pBlob->GetBufferSize());
      PSVShaderKind kind = PSV.GetShaderKind();
      VERIFY_ARE_EQUAL(PSVShaderKind::Amplification, kind);
      PSVRuntimeInfo0* pInfo = PSV.GetPSVRuntimeInfo0();
      VERIFY_IS_NOT_NULL(pInfo);
      VERIFY_ARE_EQUAL(12, pInfo->AS.PayloadSizeInBytes);
      break;
    }
  }
  VERIFY_IS_TRUE(blobFound);
}

TEST_F(DxilContainerTest, CompileWhenOkThenCheckRDAT) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  const char *shader = "float c_buf;"
    "RWTexture1D<int4> tex : register(u5);"
    "Texture1D<float4> tex2 : register(t0);"
    "RWByteAddressBuffer b_buf;"
    "struct Foo { float2 f2; int2 i2; };"
    "AppendStructuredBuffer<Foo> append_buf;"
    "ConsumeStructuredBuffer<Foo> consume_buf;"
    "RasterizerOrderedByteAddressBuffer rov_buf;"
    "globallycoherent RWByteAddressBuffer gc_buf;"
    "float function_import(float x);"
    "export float function0(min16float x) { "
    "  return x + 1 + tex[0].x; }"
    "export float function1(float x, min12int i) {"
    "  return x + c_buf + b_buf.Load(x) + tex2[i].x; }"
    "export float function2(float x) { return x + function_import(x); }"
    "export void function3(int i) {"
    "  Foo f = consume_buf.Consume();"
    "  f.f2 += 0.5; append_buf.Append(f);"
    "  rov_buf.Store(i, f.i2.x);"
    "  gc_buf.Store(i, f.i2.y);"
    "  b_buf.Store(i, f.i2.x + f.i2.y); }";
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  CComPtr<IDxcBlobEncoding> pDisassembly;
  CComPtr<IDxcOperationResult> pResult;

  struct CheckResFlagInfo { std::string name; hlsl::DXIL::ResourceKind kind; hlsl::RDAT::DxilResourceFlag flag; };
  const unsigned numResFlagCheck = 5;
  CheckResFlagInfo resFlags[numResFlagCheck] = {
    { "b_buf", hlsl::DXIL::ResourceKind::RawBuffer, hlsl::RDAT::DxilResourceFlag::None },
    { "append_buf", hlsl::DXIL::ResourceKind::StructuredBuffer, hlsl::RDAT::DxilResourceFlag::UAVCounter },
    { "consume_buf", hlsl::DXIL::ResourceKind::StructuredBuffer, hlsl::RDAT::DxilResourceFlag::UAVCounter },
    { "gc_buf", hlsl::DXIL::ResourceKind::RawBuffer, hlsl::RDAT::DxilResourceFlag::UAVGloballyCoherent },
    { "rov_buf", hlsl::DXIL::ResourceKind::RawBuffer, hlsl::RDAT::DxilResourceFlag::UAVRasterizerOrderedView }
  };

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(shader, &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"main",
                                      L"lib_6_3", nullptr, 0, nullptr, 0,
                                      nullptr, &pResult));
  HRESULT hrStatus;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hrStatus));
  VERIFY_SUCCEEDED(hrStatus);
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  CComPtr<IDxcContainerReflection> containerReflection;
  uint32_t partCount;
  IFT(m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &containerReflection));
  IFT(containerReflection->Load(pProgram));
  IFT(containerReflection->GetPartCount(&partCount));
  bool blobFound = false;
  for (uint32_t i = 0; i < partCount; ++i) {
    uint32_t kind;
    IFT(containerReflection->GetPartKind(i, &kind));
    if (kind == (uint32_t)hlsl::DxilFourCC::DFCC_RuntimeData) {
      blobFound = true;
      using namespace hlsl::RDAT;
      CComPtr<IDxcBlob> pBlob;
      IFT(containerReflection->GetPartContent(i, &pBlob));
      // Validate using DxilRuntimeData
      DxilRuntimeData context;
      context.InitFromRDAT((char *)pBlob->GetBufferPointer(), pBlob->GetBufferSize());
      FunctionTableReader *funcTableReader = context.GetFunctionTableReader();
      ResourceTableReader *resTableReader = context.GetResourceTableReader();
      VERIFY_ARE_EQUAL(funcTableReader->GetNumFunctions(), 4);
      std::string str("function");
      for (uint32_t j = 0; j < funcTableReader->GetNumFunctions(); ++j) {
        FunctionReader funcReader = funcTableReader->GetItem(j);
        std::string funcName(funcReader.GetUnmangledName());
        VERIFY_IS_TRUE(str.compare(funcName.substr(0,8)) == 0);
        std::string cur_str = str;
        cur_str.push_back('0' + j);
        if (cur_str.compare("function0") == 0) {
          VERIFY_ARE_EQUAL(funcReader.GetNumResources(), 1);
          hlsl::ShaderFlags flag;
          flag.SetUAVLoadAdditionalFormats(true);
          flag.SetLowPrecisionPresent(true);
          uint64_t rawFlag = flag.GetFeatureInfo();
          VERIFY_ARE_EQUAL(funcReader.GetFeatureFlag(), rawFlag);
          ResourceReader resReader = funcReader.GetResource(0);
          VERIFY_ARE_EQUAL(resReader.GetResourceClass(), hlsl::DXIL::ResourceClass::UAV);
          VERIFY_ARE_EQUAL(resReader.GetResourceKind(), hlsl::DXIL::ResourceKind::Texture1D);
        }
        else if (cur_str.compare("function1") == 0) {
          hlsl::ShaderFlags flag;
          flag.SetLowPrecisionPresent(true);
          uint64_t rawFlag = flag.GetFeatureInfo();
          VERIFY_ARE_EQUAL(funcReader.GetFeatureFlag(), rawFlag);
          VERIFY_ARE_EQUAL(funcReader.GetNumResources(), 3);
        }
        else if (cur_str.compare("function2") == 0) {
          VERIFY_ARE_EQUAL(funcReader.GetFeatureFlag() & 0xffffffffffffffff, 0);
          VERIFY_ARE_EQUAL(funcReader.GetNumResources(), 0);
          std::string dependency = funcReader.GetDependency(0);
          VERIFY_IS_TRUE(dependency.find("function_import") != std::string::npos);
        }
        else if (cur_str.compare("function3") == 0) {
          VERIFY_ARE_EQUAL(funcReader.GetFeatureFlag() & 0xffffffffffffffff, 0);
          VERIFY_ARE_EQUAL(funcReader.GetNumResources(), numResFlagCheck);
          for (unsigned i = 0; i < funcReader.GetNumResources(); ++i) {
            ResourceReader resReader = funcReader.GetResource(0);
            VERIFY_ARE_EQUAL(resReader.GetResourceClass(), hlsl::DXIL::ResourceClass::UAV);
            unsigned j = 0;
            for (; j < numResFlagCheck; ++j) {
              if (resFlags[j].name.compare(resReader.GetName()) == 0)
                break;
            }
            VERIFY_IS_LESS_THAN(j, numResFlagCheck);
            VERIFY_ARE_EQUAL(resReader.GetResourceKind(), resFlags[j].kind);
            VERIFY_ARE_EQUAL(resReader.GetFlags(), static_cast<uint32_t>(resFlags[j].flag));
          }
        }
        else {
          IFTBOOLMSG(false, E_FAIL, "unknown function name");
        }
      }
      VERIFY_ARE_EQUAL(resTableReader->GetNumResources(), 8);
      // This is validation test for DxilRuntimeReflection implemented on DxilRuntimeReflection.inl
      unique_ptr<DxilRuntimeReflection> pReflection(CreateDxilRuntimeReflection());
      VERIFY_IS_TRUE(pReflection->InitFromRDAT(pBlob->GetBufferPointer(), pBlob->GetBufferSize()));
      DxilLibraryDesc lib_reflection = pReflection->GetLibraryReflection();
      VERIFY_ARE_EQUAL(lib_reflection.NumFunctions, 4);
      for (uint32_t j = 0; j < 3; ++j) {
        DxilFunctionDesc function = lib_reflection.pFunction[j];
        std::string cur_str = str;
        cur_str.push_back('0' + j);
        if (cur_str.compare("function0") == 0) {
          hlsl::ShaderFlags flag;
          flag.SetUAVLoadAdditionalFormats(true);
          flag.SetLowPrecisionPresent(true);
          uint64_t rawFlag = flag.GetFeatureInfo();
          uint64_t featureFlag = static_cast<uint64_t>(function.FeatureInfo2) << 32;
          featureFlag |= static_cast<uint64_t>(function.FeatureInfo1);
          VERIFY_ARE_EQUAL(featureFlag, rawFlag);
          VERIFY_ARE_EQUAL(function.NumResources, 1);
          VERIFY_ARE_EQUAL(function.NumFunctionDependencies, 0);
          const DxilResourceDesc &resource = *function.Resources[0];
          VERIFY_ARE_EQUAL(resource.Class, (uint32_t)hlsl::DXIL::ResourceClass::UAV);
          VERIFY_ARE_EQUAL(resource.Kind, (uint32_t)hlsl::DXIL::ResourceKind::Texture1D);
          std::wstring wName = resource.Name;
          VERIFY_ARE_EQUAL(wName.compare(L"tex"), 0);
        }
        else if (cur_str.compare("function1") == 0) {
          hlsl::ShaderFlags flag;
          flag.SetLowPrecisionPresent(true);
          uint64_t rawFlag = flag.GetFeatureInfo();
          uint64_t featureFlag = static_cast<uint64_t>(function.FeatureInfo2) << 32;
          featureFlag |= static_cast<uint64_t>(function.FeatureInfo1);
          VERIFY_ARE_EQUAL(featureFlag, rawFlag);
          VERIFY_ARE_EQUAL(function.NumResources, 3);
          VERIFY_ARE_EQUAL(function.NumFunctionDependencies, 0);
          std::unordered_set<std::wstring> stringSet = { L"$Globals", L"b_buf", L"tex2" };
          for (uint32_t j = 0; j < 3; ++j) {
            const DxilResourceDesc &resource = *function.Resources[j];
            std::wstring compareName = resource.Name;
            VERIFY_IS_TRUE(stringSet.find(compareName) != stringSet.end());
          }
        }
        else if (cur_str.compare("function2") == 0) {
          VERIFY_ARE_EQUAL(function.FeatureInfo1, 0);
          VERIFY_ARE_EQUAL(function.FeatureInfo2, 0);
          VERIFY_ARE_EQUAL(function.NumResources, 0);
          VERIFY_ARE_EQUAL(function.NumFunctionDependencies, 1);
          std::wstring dependency = function.FunctionDependencies[0];
          VERIFY_IS_TRUE(dependency.find(L"function_import") != std::wstring::npos);
        }
        else if (cur_str.compare("function3") == 0) {
          VERIFY_ARE_EQUAL(function.FeatureInfo1, 0);
          VERIFY_ARE_EQUAL(function.FeatureInfo2, 0);
          VERIFY_ARE_EQUAL(function.NumResources, numResFlagCheck);
          VERIFY_ARE_EQUAL(function.NumFunctionDependencies, 0);
          for (unsigned i = 0; i < function.NumResources; ++i) {
            const DxilResourceDesc *res = function.Resources[i];
            VERIFY_ARE_EQUAL(res->Class, static_cast<uint32_t>(hlsl::DXIL::ResourceClass::UAV));
            unsigned j = 0;
            for (; j < numResFlagCheck; ++j) {
              CA2W WName(resFlags[j].name.c_str());
              std::wstring compareName(WName);
              if (compareName.compare(res->Name) == 0)
                break;
            }
            VERIFY_IS_LESS_THAN(j, numResFlagCheck);
            VERIFY_ARE_EQUAL(res->Kind, static_cast<uint32_t>(resFlags[j].kind));
            VERIFY_ARE_EQUAL(res->Flags, static_cast<uint32_t>(resFlags[j].flag));
          }
        }
        else {
          IFTBOOLMSG(false, E_FAIL, "unknown function name");
        }
      }
      VERIFY_IS_TRUE(lib_reflection.NumResources == 8);
    }
  }
  IFTBOOLMSG(blobFound, E_FAIL, "failed to find RDAT blob after compiling");
}

TEST_F(DxilContainerTest, CompileWhenOkThenCheckRDAT2) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  // This is a case when the user of resource is a constant, not instruction.
  // Compiler generates the following load instruction for texture.
  // load %class.Texture2D, %class.Texture2D* getelementptr inbounds ([3 x
  // %class.Texture2D], [3 x %class.Texture2D]*
  // @"\01?ThreeTextures@@3PAV?$Texture2D@M@@A", i32 0, i32 0), align 4
  const char *shader =
      "SamplerState Sampler : register(s0); RWBuffer<float> Uav : "
      "register(u0); Texture2D<float> ThreeTextures[3] : register(t0); "
      "float function1();"
      "[shader(\"raygeneration\")] void RayGenMain() { Uav[0] = "
      "ThreeTextures[0].SampleLevel(Sampler, float2(0, 0), 0) + "
      "ThreeTextures[2].SampleLevel(Sampler, float2(0, 0), 0) + function1(); }";
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  CComPtr<IDxcBlobEncoding> pDisassembly;
  CComPtr<IDxcOperationResult> pResult;
  HRESULT status;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(shader, &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"main",
                                      L"lib_6_3", nullptr, 0, nullptr, 0,
                                      nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
  CComPtr<IDxcContainerReflection> pReflection;
  uint32_t partCount;
  IFT(m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
  IFT(pReflection->Load(pProgram));
  IFT(pReflection->GetPartCount(&partCount));
  bool blobFound = false;
  for (uint32_t i = 0; i < partCount; ++i) {
    uint32_t kind;
    IFT(pReflection->GetPartKind(i, &kind));
    if (kind == (uint32_t)hlsl::DxilFourCC::DFCC_RuntimeData) {
      blobFound = true;
      using namespace hlsl::RDAT;
      CComPtr<IDxcBlob> pBlob;
      IFT(pReflection->GetPartContent(i, &pBlob));
      DxilRuntimeData context;
      context.InitFromRDAT((char *)pBlob->GetBufferPointer(), pBlob->GetBufferSize());
      FunctionTableReader *funcTableReader = context.GetFunctionTableReader();
      ResourceTableReader *resTableReader = context.GetResourceTableReader();
      VERIFY_IS_TRUE(funcTableReader->GetNumFunctions() == 1);
      VERIFY_IS_TRUE(resTableReader->GetNumResources() == 3);
      FunctionReader funcReader = funcTableReader->GetItem(0);
      llvm::StringRef name(funcReader.GetUnmangledName());
      VERIFY_IS_TRUE(name.compare("RayGenMain") == 0);
      VERIFY_IS_TRUE(funcReader.GetShaderKind() ==
                     hlsl::DXIL::ShaderKind::RayGeneration);
      VERIFY_IS_TRUE(funcReader.GetNumResources() == 3);
      VERIFY_IS_TRUE(funcReader.GetNumDependencies() == 1);
      llvm::StringRef dependencyName =
          hlsl::dxilutil::DemangleFunctionName(funcReader.GetDependency(0));
      VERIFY_IS_TRUE(dependencyName.compare("function1") == 0);
    }
  }
  IFTBOOLMSG(blobFound, E_FAIL, "failed to find RDAT blob after compiling");
}

static uint32_t EncodedVersion_lib_6_3 = hlsl::EncodeVersion(hlsl::DXIL::ShaderKind::Library, 6, 3);
static uint32_t EncodedVersion_vs_6_3 = hlsl::EncodeVersion(hlsl::DXIL::ShaderKind::Vertex, 6, 3);

static void Ref1_CheckCBuffer_Globals(ID3D12ShaderReflectionConstantBuffer *pCBReflection, D3D12_SHADER_BUFFER_DESC &cbDesc) {
  std::string cbName = cbDesc.Name;
  VERIFY_IS_TRUE(cbName.compare("$Globals") == 0);
  VERIFY_ARE_EQUAL(cbDesc.Size, 16);
  VERIFY_ARE_EQUAL(cbDesc.Type, D3D_CT_CBUFFER);
  VERIFY_ARE_EQUAL(cbDesc.Variables, 1);

  // cbval1
  ID3D12ShaderReflectionVariable *pVar = pCBReflection->GetVariableByIndex(0);
  D3D12_SHADER_VARIABLE_DESC varDesc;
  VERIFY_SUCCEEDED(pVar->GetDesc(&varDesc));
  VERIFY_ARE_EQUAL_STR(varDesc.Name, "cbval1");
  VERIFY_ARE_EQUAL(varDesc.StartOffset, 0);
  VERIFY_ARE_EQUAL(varDesc.Size, 4);
  // TODO: verify rest of variable
  ID3D12ShaderReflectionType *pType = pVar->GetType();
  D3D12_SHADER_TYPE_DESC tyDesc;
  VERIFY_SUCCEEDED(pType->GetDesc(&tyDesc));
  VERIFY_ARE_EQUAL(tyDesc.Class, D3D_SVC_SCALAR);
  VERIFY_ARE_EQUAL(tyDesc.Type, D3D_SVT_FLOAT);
  // TODO: verify rest of type
}

static void Ref1_CheckCBuffer_MyCB(ID3D12ShaderReflectionConstantBuffer *pCBReflection, D3D12_SHADER_BUFFER_DESC &cbDesc) {
  std::string cbName = cbDesc.Name;
  VERIFY_IS_TRUE(cbName.compare("MyCB") == 0);
  VERIFY_ARE_EQUAL(cbDesc.Size, 32);
  VERIFY_ARE_EQUAL(cbDesc.Type, D3D_CT_CBUFFER);
  VERIFY_ARE_EQUAL(cbDesc.Variables, 2);

  // cbval2
  {
    ID3D12ShaderReflectionVariable *pVar =  pCBReflection->GetVariableByIndex(0);
    D3D12_SHADER_VARIABLE_DESC varDesc;
    VERIFY_SUCCEEDED(pVar->GetDesc(&varDesc));
    VERIFY_ARE_EQUAL_STR(varDesc.Name, "cbval2");
    VERIFY_ARE_EQUAL(varDesc.StartOffset, 0);
    VERIFY_ARE_EQUAL(varDesc.Size, 16);
    // TODO: verify rest of variable
    ID3D12ShaderReflectionType *pType = pVar->GetType();
    D3D12_SHADER_TYPE_DESC tyDesc;
    VERIFY_SUCCEEDED(pType->GetDesc(&tyDesc));
    VERIFY_ARE_EQUAL(tyDesc.Class, D3D_SVC_VECTOR);
    VERIFY_ARE_EQUAL(tyDesc.Type, D3D_SVT_INT);
    // TODO: verify rest of type
  }

  // cbval3
  {
    ID3D12ShaderReflectionVariable *pVar = pCBReflection->GetVariableByIndex(1);
    D3D12_SHADER_VARIABLE_DESC varDesc;
    VERIFY_SUCCEEDED(pVar->GetDesc(&varDesc));
    VERIFY_ARE_EQUAL_STR(varDesc.Name, "cbval3");
    VERIFY_ARE_EQUAL(varDesc.StartOffset, 16);
    VERIFY_ARE_EQUAL(varDesc.Size, 16);
    // TODO: verify rest of variable
    ID3D12ShaderReflectionType *pType = pVar->GetType();
    D3D12_SHADER_TYPE_DESC tyDesc;
    VERIFY_SUCCEEDED(pType->GetDesc(&tyDesc));
    VERIFY_ARE_EQUAL(tyDesc.Class, D3D_SVC_VECTOR);
    VERIFY_ARE_EQUAL(tyDesc.Type, D3D_SVT_INT);
    // TODO: verify rest of type
  }
}

static void Ref1_CheckBinding_Globals(D3D12_SHADER_INPUT_BIND_DESC &resDesc) {
  std::string resName = resDesc.Name;
  VERIFY_IS_TRUE(resName.compare("$Globals") == 0);
  VERIFY_ARE_EQUAL(resDesc.Type, D3D_SIT_CBUFFER);
  // not explicitly bound:
  VERIFY_ARE_EQUAL(resDesc.BindPoint, 4294967295);
  VERIFY_ARE_EQUAL(resDesc.Space, 0);
  VERIFY_ARE_EQUAL(resDesc.BindCount, 1);
}

static void Ref1_CheckBinding_MyCB(D3D12_SHADER_INPUT_BIND_DESC &resDesc) {
  std::string resName = resDesc.Name;
  VERIFY_IS_TRUE(resName.compare("MyCB") == 0);
  VERIFY_ARE_EQUAL(resDesc.Type, D3D_SIT_CBUFFER);
  VERIFY_ARE_EQUAL(resDesc.BindPoint, 11);
  VERIFY_ARE_EQUAL(resDesc.Space, 2);
  VERIFY_ARE_EQUAL(resDesc.BindCount, 1);
}

static void Ref1_CheckBinding_tex(D3D12_SHADER_INPUT_BIND_DESC &resDesc) {
  std::string resName = resDesc.Name;
  VERIFY_IS_TRUE(resName.compare("tex") == 0);
  VERIFY_ARE_EQUAL(resDesc.Type, D3D_SIT_UAV_RWTYPED);
  VERIFY_ARE_EQUAL(resDesc.BindPoint, 5);
  VERIFY_ARE_EQUAL(resDesc.Space, 0);
  VERIFY_ARE_EQUAL(resDesc.BindCount, 1);
}

static void Ref1_CheckBinding_tex2(D3D12_SHADER_INPUT_BIND_DESC &resDesc) {
  std::string resName = resDesc.Name;
  VERIFY_IS_TRUE(resName.compare("tex2") == 0);
  VERIFY_ARE_EQUAL(resDesc.Type, D3D_SIT_TEXTURE);
  VERIFY_ARE_EQUAL(resDesc.BindPoint, 0);
  VERIFY_ARE_EQUAL(resDesc.Space, 0);
  VERIFY_ARE_EQUAL(resDesc.BindCount, 1);
}

static void Ref1_CheckBinding_samp(D3D12_SHADER_INPUT_BIND_DESC &resDesc) {
  std::string resName = resDesc.Name;
  VERIFY_IS_TRUE(resName.compare("samp") == 0);
  VERIFY_ARE_EQUAL(resDesc.Type, D3D_SIT_SAMPLER);
  VERIFY_ARE_EQUAL(resDesc.BindPoint, 7);
  VERIFY_ARE_EQUAL(resDesc.Space, 0);
  VERIFY_ARE_EQUAL(resDesc.BindCount, 1);
}

static void Ref1_CheckBinding_b_buf(D3D12_SHADER_INPUT_BIND_DESC &resDesc) {
  std::string resName = resDesc.Name;
  VERIFY_IS_TRUE(resName.compare("b_buf") == 0);
  VERIFY_ARE_EQUAL(resDesc.Type, D3D_SIT_UAV_RWBYTEADDRESS);
  // not explicitly bound:
  VERIFY_ARE_EQUAL(resDesc.BindPoint, 4294967295);
  VERIFY_ARE_EQUAL(resDesc.Space, 4294967295);
  VERIFY_ARE_EQUAL(resDesc.BindCount, 1);
}


const char *Ref1_Shader =
  "float cbval1;"
  "cbuffer MyCB : register(b11, space2) { int4 cbval2, cbval3; }"
  "RWTexture1D<int4> tex : register(u5);"
  "Texture1D<float4> tex2 : register(t0);"
  "SamplerState samp : register(s7);"
  "RWByteAddressBuffer b_buf;"
  "export float function0(min16float x) { "
  "  return x + cbval2.x + tex[0].x; }"
  "export float function1(float x, min12int i) {"
  "  return x + cbval1 + b_buf.Load(x) + tex2.Sample(samp, x).x; }"
  "[shader(\"vertex\")]"
  "float4 function2(float4 x : POSITION) : SV_Position { return x + cbval1 + cbval3.x; }";

TEST_F(DxilContainerTest, CompileWhenOkThenCheckReflection1) {
  if (m_ver.SkipDxilVersion(1, 3)) return;

  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  CComPtr<IDxcBlobEncoding> pDisassembly;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<ID3D12LibraryReflection> pLibraryReflection;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(Ref1_Shader, &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"",
    L"lib_6_3", nullptr, 0, nullptr, 0,
    nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  CComPtr<IDxcContainerReflection> containerReflection;
  uint32_t partCount;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &containerReflection));
  VERIFY_SUCCEEDED(containerReflection->Load(pProgram));
  VERIFY_SUCCEEDED(containerReflection->GetPartCount(&partCount));
  bool blobFound = false;

  for (uint32_t i = 0; i < partCount; ++i) {
    uint32_t kind;
    VERIFY_SUCCEEDED(containerReflection->GetPartKind(i, &kind));
    if (kind == (uint32_t)hlsl::DxilFourCC::DFCC_DXIL) {
      blobFound = true;
      VERIFY_SUCCEEDED(containerReflection->GetPartReflection(i, IID_PPV_ARGS(&pLibraryReflection)));
      D3D12_LIBRARY_DESC LibDesc;
      VERIFY_SUCCEEDED(pLibraryReflection->GetDesc(&LibDesc));
      VERIFY_ARE_EQUAL(LibDesc.FunctionCount, 3);
      for (INT iFn = 0; iFn < (INT)LibDesc.FunctionCount; iFn++) {
        ID3D12FunctionReflection *pFunctionReflection = pLibraryReflection->GetFunctionByIndex(iFn);
        D3D12_FUNCTION_DESC FnDesc;
        pFunctionReflection->GetDesc(&FnDesc);
        std::string Name = FnDesc.Name;
        if (Name.compare("\01?function0@@YAM$min16f@@Z") == 0) {
          VERIFY_ARE_EQUAL(FnDesc.Version, EncodedVersion_lib_6_3);
          VERIFY_ARE_EQUAL(FnDesc.ConstantBuffers, 1);
          VERIFY_ARE_EQUAL(FnDesc.BoundResources, 2);
          D3D12_SHADER_BUFFER_DESC cbDesc;
          ID3D12ShaderReflectionConstantBuffer *pCBReflection = pFunctionReflection->GetConstantBufferByIndex(0);
          VERIFY_SUCCEEDED(pCBReflection->GetDesc(&cbDesc));
          std::string cbName = cbDesc.Name;
          (void)(cbName);
          Ref1_CheckCBuffer_MyCB(pCBReflection, cbDesc);

          for (INT iRes = 0; iRes < (INT)FnDesc.BoundResources; iRes++) {
            D3D12_SHADER_INPUT_BIND_DESC resDesc;
            pFunctionReflection->GetResourceBindingDesc(iRes, &resDesc);
            std::string resName = resDesc.Name;
            if (resName.compare("$Globals") == 0) {
              Ref1_CheckBinding_Globals(resDesc);
            } else if (resName.compare("MyCB") == 0) {
              Ref1_CheckBinding_MyCB(resDesc);
            } else if (resName.compare("samp") == 0) {
              Ref1_CheckBinding_samp(resDesc);
            } else if (resName.compare("tex") == 0) {
              Ref1_CheckBinding_tex(resDesc);
            } else if (resName.compare("tex2") == 0) {
              Ref1_CheckBinding_tex2(resDesc);
            } else if (resName.compare("b_buf") == 0) {
              Ref1_CheckBinding_b_buf(resDesc);
            } else {
              VERIFY_FAIL(L"Unexpected resource used");
            }
          }
        } else if (Name.compare("\01?function1@@YAMM$min12i@@Z") == 0) {
          VERIFY_ARE_EQUAL(FnDesc.Version, EncodedVersion_lib_6_3);
          VERIFY_ARE_EQUAL(FnDesc.ConstantBuffers, 1);
          VERIFY_ARE_EQUAL(FnDesc.BoundResources, 4);
          D3D12_SHADER_BUFFER_DESC cbDesc;
          ID3D12ShaderReflectionConstantBuffer *pCBReflection = pFunctionReflection->GetConstantBufferByIndex(0);
          VERIFY_SUCCEEDED(pCBReflection->GetDesc(&cbDesc));
          std::string cbName = cbDesc.Name;
          (void)(cbName);
          Ref1_CheckCBuffer_Globals(pCBReflection, cbDesc);

          for (INT iRes = 0; iRes < (INT)FnDesc.BoundResources; iRes++) {
            D3D12_SHADER_INPUT_BIND_DESC resDesc;
            pFunctionReflection->GetResourceBindingDesc(iRes, &resDesc);
            std::string resName = resDesc.Name;
            if (resName.compare("$Globals") == 0) {
              Ref1_CheckBinding_Globals(resDesc);
            } else if (resName.compare("MyCB") == 0) {
              Ref1_CheckBinding_MyCB(resDesc);
            } else if (resName.compare("samp") == 0) {
              Ref1_CheckBinding_samp(resDesc);
            } else if (resName.compare("tex") == 0) {
              Ref1_CheckBinding_tex(resDesc);
            } else if (resName.compare("tex2") == 0) {
              Ref1_CheckBinding_tex2(resDesc);
            } else if (resName.compare("b_buf") == 0) {
              Ref1_CheckBinding_b_buf(resDesc);
            } else {
              VERIFY_FAIL(L"Unexpected resource used");
            }
          }
        } else if (Name.compare("function2") == 0) {
          // shader function with unmangled name
          VERIFY_ARE_EQUAL(FnDesc.Version, EncodedVersion_vs_6_3);
          VERIFY_ARE_EQUAL(FnDesc.ConstantBuffers, 2);
          VERIFY_ARE_EQUAL(FnDesc.BoundResources, 2);
          for (INT iCB = 0; iCB < (INT)FnDesc.BoundResources; iCB++) {
            D3D12_SHADER_BUFFER_DESC cbDesc;
            ID3D12ShaderReflectionConstantBuffer *pCBReflection = pFunctionReflection->GetConstantBufferByIndex(0);
            VERIFY_SUCCEEDED(pCBReflection->GetDesc(&cbDesc));
            std::string cbName = cbDesc.Name;
            if (cbName.compare("$Globals") == 0) {
              Ref1_CheckCBuffer_Globals(pCBReflection, cbDesc);
            } else if (cbName.compare("MyCB") == 0) {
              Ref1_CheckCBuffer_MyCB(pCBReflection, cbDesc);
            }
          }

          for (INT iRes = 0; iRes < (INT)FnDesc.BoundResources; iRes++) {
            D3D12_SHADER_INPUT_BIND_DESC resDesc;
            pFunctionReflection->GetResourceBindingDesc(iRes, &resDesc);
            std::string resName = resDesc.Name;
            if (resName.compare("$Globals") == 0) {
              Ref1_CheckBinding_Globals(resDesc);
            } else if (resName.compare("MyCB") == 0) {
              Ref1_CheckBinding_MyCB(resDesc);
            } else {
              VERIFY_FAIL(L"Unexpected resource used");
            }
          }
        } else {
          VERIFY_FAIL(L"Unexpected function");
        }
      }

      // TODO: FINISH THIS
    }
  }
  IFTBOOLMSG(blobFound, E_FAIL, "failed to find RDAT blob after compiling");
}

TEST_F(DxilContainerTest, DxcUtils_CreateReflection) {
  if (m_ver.SkipDxilVersion(1, 3)) return;

  CComPtr<IDxcUtils> pUtils;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcUtils, &pUtils));
  CComPtr<IDxcCompiler> pCompiler;
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CComPtr<IDxcBlobEncoding> pSource;
  CreateBlobFromText(Ref1_Shader, &pSource);

  LPCWSTR options[] = {
    L"-Qstrip_reflect_from_dxil",
    L"-Qstrip_reflect"
  };
  const UINT32 kStripFromDxilOnly = 1;  // just strip reflection from DXIL, not container
  const UINT32 kStripFromContainer = 2; // strip reflection from DXIL and container

  auto VerifyStripReflection = [&](IDxcBlob *pBlob, bool bShouldSucceed) {
    CComPtr<IDxcContainerReflection> pReflection;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
    VERIFY_SUCCEEDED(pReflection->Load(pBlob));
    UINT32 idxPart = (UINT32)-1;
    if (bShouldSucceed)
      VERIFY_SUCCEEDED(pReflection->FindFirstPartKind(DXC_PART_REFLECTION_DATA, &idxPart));
    else
      VERIFY_FAILED(pReflection->FindFirstPartKind(DXC_PART_REFLECTION_DATA, &idxPart));
    CComPtr<IDxcContainerBuilder> pBuilder;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcContainerBuilder, &pBuilder));
    VERIFY_SUCCEEDED(pBuilder->Load(pBlob));
    if (bShouldSucceed) {
      VERIFY_SUCCEEDED(pBuilder->RemovePart(DXC_PART_REFLECTION_DATA));
      CComPtr<IDxcOperationResult> pResult;
      VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));
      HRESULT hr = E_FAIL;
      VERIFY_SUCCEEDED(pResult->GetStatus(&hr));
      VERIFY_SUCCEEDED(hr);
      CComPtr<IDxcBlob> pStrippedBlob;
      pResult->GetResult(&pStrippedBlob);
      CComPtr<IDxcContainerReflection> pReflection2;
      VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection2));
      VERIFY_SUCCEEDED(pReflection2->Load(pStrippedBlob));
      idxPart = (UINT32)-1;
      VERIFY_FAILED(pReflection2->FindFirstPartKind(DXC_PART_REFLECTION_DATA, &idxPart));
    } else {
      VERIFY_FAILED(pBuilder->RemovePart(DXC_PART_REFLECTION_DATA));
    }
  };

  {
    // Test Shader path
    auto VerifyCreateReflectionShader = [&](IDxcBlob *pBlob, bool bValid)
    {
      DxcBuffer buffer = { pBlob->GetBufferPointer(), pBlob->GetBufferSize(), 0 };
      CComPtr<ID3D12ShaderReflection> pShaderReflection;
      VERIFY_SUCCEEDED(pUtils->CreateReflection(&buffer, IID_PPV_ARGS(&pShaderReflection)));
      D3D12_SHADER_DESC desc;
      VERIFY_SUCCEEDED(pShaderReflection->GetDesc(&desc));
      VERIFY_ARE_EQUAL(desc.Version, EncodedVersion_vs_6_3);
      if (bValid) {
        VERIFY_ARE_EQUAL(desc.ConstantBuffers, 2);
        VERIFY_ARE_EQUAL(desc.BoundResources, 2);
        // That should be good enough to check that IDxcUtils::CreateReflection worked
      }
    };

    {
      // Test Full container path
      CComPtr<IDxcOperationResult> pResult;
      VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"function2",
        L"vs_6_3", options, kStripFromDxilOnly,
        nullptr, 0, nullptr, &pResult));
      HRESULT hr;
      VERIFY_SUCCEEDED(pResult->GetStatus(&hr));
      VERIFY_SUCCEEDED(hr);

      CComPtr<IDxcBlob> pProgram;
      VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
      VerifyCreateReflectionShader(pProgram, true);

      // Verify reflection stripping
      VerifyStripReflection(pProgram, true);
    }

    {
      // From New IDxcResult API
      CComPtr<IDxcOperationResult> pResult;
      VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"function2",
        L"vs_6_3", options, kStripFromContainer,
        nullptr, 0, nullptr, &pResult));
      HRESULT hr;
      VERIFY_SUCCEEDED(pResult->GetStatus(&hr));
      VERIFY_SUCCEEDED(hr);

      // Test separate reflection result path
      CComPtr<IDxcResult> pResultV2;
      CComPtr<IDxcBlob> pReflectionPart;
      VERIFY_SUCCEEDED(pResult->QueryInterface(&pResultV2));
      VERIFY_SUCCEEDED(pResultV2->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionPart), nullptr));
      VerifyCreateReflectionShader(pReflectionPart, true);

      // Container should have limited reflection, and no reflection part
      CComPtr<IDxcBlob> pProgram;
      VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
      VerifyCreateReflectionShader(pProgram, false);
      VerifyStripReflection(pProgram, false);
    }
  }

  {
    // Test Library path
    auto VerifyCreateReflectionLibrary = [&](IDxcBlob *pBlob, bool bValid)
    {
      DxcBuffer buffer = { pBlob->GetBufferPointer(), pBlob->GetBufferSize(), 0 };
      CComPtr<ID3D12LibraryReflection> pLibraryReflection;
      VERIFY_SUCCEEDED(pUtils->CreateReflection(&buffer, IID_PPV_ARGS(&pLibraryReflection)));
      D3D12_LIBRARY_DESC desc;
      VERIFY_SUCCEEDED(pLibraryReflection->GetDesc(&desc));
      if (bValid) {
        VERIFY_ARE_EQUAL(desc.FunctionCount, 3);
      // That should be good enough to check that IDxcUtils::CreateReflection worked
      }
    };

    {
      // Test Full container path
      CComPtr<IDxcOperationResult> pResult;
      VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"",
        L"lib_6_3", options, kStripFromDxilOnly,
        nullptr, 0, nullptr, &pResult));
      HRESULT hr;
      VERIFY_SUCCEEDED(pResult->GetStatus(&hr));
      VERIFY_SUCCEEDED(hr);

      CComPtr<IDxcBlob> pProgram;
      VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
      VerifyCreateReflectionLibrary(pProgram, true);

      // Verify reflection stripping
      VerifyStripReflection(pProgram, true);
    }

    {
      // From New IDxcResult API
      CComPtr<IDxcOperationResult> pResult;
      VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"",
        L"lib_6_3", options, kStripFromContainer,
        nullptr, 0, nullptr, &pResult));
      HRESULT hr;
      VERIFY_SUCCEEDED(pResult->GetStatus(&hr));
      VERIFY_SUCCEEDED(hr);

      // Test separate reflection result path
      CComPtr<IDxcResult> pResultV2;
      CComPtr<IDxcBlob> pReflectionPart;
      VERIFY_SUCCEEDED(pResult->QueryInterface(&pResultV2));
      VERIFY_SUCCEEDED(pResultV2->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionPart), nullptr));
      // Test Reflection part path
      VerifyCreateReflectionLibrary(pReflectionPart, true);

      // Container should have limited reflection, and no reflection part
      CComPtr<IDxcBlob> pProgram;
      VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
      VerifyCreateReflectionLibrary(pProgram, false);
      VerifyStripReflection(pProgram, false);
    }
  }
}

TEST_F(DxilContainerTest, CompileWhenOKThenIncludesFeatureInfo) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  CComPtr<IDxcBlobEncoding> pDisassembly;
  CComPtr<IDxcOperationResult> pResult;
  hlsl::DxilContainerHeader *pHeader;
  hlsl::DxilPartIterator pPartIter(nullptr, 0);

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"main", L"ps_6_0",
    nullptr, 0, nullptr, 0, nullptr,
    &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));

  // Now mess with the program bitcode.
  pHeader = (hlsl::DxilContainerHeader *)pProgram->GetBufferPointer();
  pPartIter = std::find_if(hlsl::begin(pHeader), hlsl::end(pHeader),
                           hlsl::DxilPartIsType(hlsl::DFCC_FeatureInfo));
  VERIFY_ARE_NOT_EQUAL(hlsl::end(pHeader), pPartIter);
  VERIFY_ARE_EQUAL(sizeof(uint64_t), (*pPartIter)->PartSize);
  VERIFY_ARE_EQUAL(0U, *(const uint64_t *)hlsl::GetDxilPartData(*pPartIter));
}

TEST_F(DxilContainerTest, DisassemblyWhenBCInvalidThenFails) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  CComPtr<IDxcBlobEncoding> pDisassembly;
  CComPtr<IDxcOperationResult> pResult;
  hlsl::DxilContainerHeader *pHeader;
  hlsl::DxilPartHeader *pPart;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"main", L"ps_6_0",
    nullptr, 0, nullptr, 0, nullptr,
    &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));

  // Now mess with the program bitcode.
  pHeader = (hlsl::DxilContainerHeader *)pProgram->GetBufferPointer();
  pPart = const_cast<hlsl::DxilPartHeader *>(
      *std::find_if(hlsl::begin(pHeader), hlsl::end(pHeader),
                    hlsl::DxilPartIsType(hlsl::DFCC_DXIL)));
  strcpy_s(hlsl::GetDxilPartData(pPart), pPart->PartSize, "corruption");
  VERIFY_FAILED(pCompiler->Disassemble(pProgram, &pDisassembly));
}

TEST_F(DxilContainerTest, DisassemblyWhenMissingThenFails) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pDisassembly;
  hlsl::DxilContainerHeader header;

  SetupBasicHeader(&header);
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobPinned(&header, header.ContainerSizeInBytes, 0, &pSource);
  VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
}

TEST_F(DxilContainerTest, DisassemblyWhenInvalidThenFails) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pDisassembly;
  uint8_t scratch[1024];
  hlsl::DxilContainerHeader *pHeader = (hlsl::DxilContainerHeader *)scratch;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));

  // Too small to contain header.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    CreateBlobPinned(pHeader, sizeof(hlsl::DxilContainerHeader) - 4, 0,
                     &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }

  // Wrong major version.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    pHeader->Version.Major = 100;
    CreateBlobPinned(pHeader, pHeader->ContainerSizeInBytes, 0, &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }

  // Size out of bounds.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    pHeader->ContainerSizeInBytes = 1024;
    CreateBlobPinned(pHeader, sizeof(hlsl::DxilContainerHeader), 0,
                     &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }

  // Size too large as per spec limit.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    pHeader->ContainerSizeInBytes = hlsl::DxilContainerMaxSize + 1;
    CreateBlobPinned(pHeader, pHeader->ContainerSizeInBytes, 0, &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }

  // Not large enough to hold offset table.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    pHeader->PartCount = 1;
    CreateBlobPinned(pHeader, pHeader->ContainerSizeInBytes, 0, &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }

  // Part offset out of bounds.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    pHeader->PartCount = 1;
    *((uint32_t *)(pHeader + 1)) = 1024;
    pHeader->ContainerSizeInBytes += sizeof(uint32_t);
    CreateBlobPinned(pHeader, pHeader->ContainerSizeInBytes, 0, &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }

  // Part size out of bounds.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    pHeader->PartCount = 1;
    *((uint32_t *)(pHeader + 1)) = sizeof(*pHeader) + sizeof(uint32_t);
    pHeader->ContainerSizeInBytes += sizeof(uint32_t);
    hlsl::GetDxilContainerPart(pHeader, 0)->PartSize = 1024;
    pHeader->ContainerSizeInBytes += sizeof(hlsl::DxilPartHeader);
    CreateBlobPinned(pHeader, pHeader->ContainerSizeInBytes, 0, &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }
}

TEST_F(DxilContainerTest, DisassemblyWhenValidThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  CComPtr<IDxcBlobEncoding> pDisassembly;
  CComPtr<IDxcOperationResult> pResult;
  hlsl::DxilContainerHeader header;

  SetupBasicHeader(&header);
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"main", L"ps_6_0",
                                      nullptr, 0, nullptr, 0, nullptr,
                                      &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembly));
  std::string disassembleString(BlobToUtf8(pDisassembly));
  VERIFY_ARE_NOT_EQUAL(0U, disassembleString.size());
}

class HlslFileVariables {
private:
  std::wstring m_Entry;
  std::wstring m_Mode;
  std::wstring m_Target;
  std::vector<std::wstring> m_Arguments;
  std::vector<LPCWSTR> m_ArgumentPtrs;
public:
  HlslFileVariables(HlslFileVariables &other) = delete;
  const LPCWSTR *GetArguments() const { return m_ArgumentPtrs.data(); }
  UINT32 GetArgumentCount() const { return m_ArgumentPtrs.size(); }
  LPCWSTR GetEntry() const { return m_Entry.c_str(); }
  LPCWSTR GetMode() const { return m_Mode.c_str(); }
  LPCWSTR GetTarget() const { return m_Target.c_str(); }
  void Reset();
  HRESULT SetFromText(_In_count_(len) const char *pText, size_t len);
};

void HlslFileVariables::Reset() {
  m_Entry.resize(0);
  m_Mode.resize(0);
  m_Target.resize(0);
  m_Arguments.resize(0);
  m_ArgumentPtrs.resize(0);
}

#include <codecvt>

static bool wcsneq(const wchar_t *pValue, const wchar_t *pCheck) {
  return 0 == wcsncmp(pValue, pCheck, wcslen(pCheck));
}

HRESULT HlslFileVariables::SetFromText(_In_count_(len) const char *pText, size_t len) {
  // Look for the line of interest.
  const char *pEnd = pText + len;
  const char *pLineEnd = pText;
  while (pLineEnd < pEnd && *pLineEnd != '\n') pLineEnd++;

  // Create a UTF-16-backing store.
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t> > w;
  std::wstring line = w.from_bytes(pText, pLineEnd);

  // Find starting and ending '-*-' delimiters.
  const wchar_t *pWText = line.c_str();
  const wchar_t *pVarStart = wcsstr(pWText, L"-*-");
  if (!pVarStart) return E_INVALIDARG;
  pVarStart += 3;
  const wchar_t *pVarEnd = wcsstr(pVarStart, L"-*-");
  if (!pVarEnd) return E_INVALIDARG;

  for (;;) {
    // Find 'name' ':' 'value' ';'
    const wchar_t *pVarNameStart = pVarStart;
    while (pVarNameStart < pVarEnd && L' ' == *pVarNameStart) ++pVarNameStart;
    if (pVarNameStart == pVarEnd) break;
    const wchar_t *pVarValDelim = pVarNameStart;
    while (pVarValDelim < pVarEnd && L':' != *pVarValDelim) ++pVarValDelim;
    if (pVarValDelim == pVarEnd) break;
    const wchar_t *pVarValStart = pVarValDelim + 1;
    while (pVarValStart < pVarEnd && L' ' == *pVarValStart) ++pVarValStart;
    if (pVarValStart == pVarEnd) break;
    const wchar_t *pVarValEnd = pVarValStart;
    while (pVarValEnd < pVarEnd && L';' != *pVarValEnd) ++pVarValEnd;

    if (wcsneq(pVarNameStart, L"mode")) {
      m_Mode.assign(pVarValStart, pVarValEnd - pVarValStart - 1);
    }
    else if (wcsneq(pVarNameStart, L"hlsl-entry")) {
      m_Entry.assign(pVarValStart, pVarValEnd - pVarValStart - 1);
    }
    else if (wcsneq(pVarNameStart, L"hlsl-target")) {
      m_Target.assign(pVarValStart, pVarValEnd - pVarValStart - 1);
    }
    else if (wcsneq(pVarNameStart, L"hlsl-args")) {
      // skip for now
    }
  }

  return S_OK;
}

#ifdef _WIN32 // Reflection unsupported
TEST_F(DxilContainerTest, ReflectionMatchesDXBC_CheckIn) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  ReflectionTest(hlsl_test::GetPathToHlslDataFile(L"..\\CodeGenHLSL\\container\\SimpleBezier11DS.hlsl").c_str(), false);
  ReflectionTest(hlsl_test::GetPathToHlslDataFile(L"..\\CodeGenHLSL\\container\\SubD11_SmoothPS.hlsl").c_str(), false);
  ReflectionTest(hlsl_test::GetPathToHlslDataFile(L"..\\HLSLFileCheck\\d3dreflect\\structured_buffer_layout.hlsl").c_str(), false);
}

TEST_F(DxilContainerTest, ReflectionMatchesDXBC_Full) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  std::wstring codeGenPath = hlsl_test::GetPathToHlslDataFile(L"..\\CodeGenHLSL\\Samples");
  // This test was running at about three minutes; that can be enabled with TestAll=True,
  // otherwise the much shorter list is used.
  const bool TestAll = false;
  LPCWSTR PreApprovedPaths[] = {
    L"2DQuadShaders_VS.hlsl",
    L"BC6HEncode_TryModeLE10CS.hlsl",
    L"DepthViewerVS.hlsl",
    L"DetailTessellation11_DS.hlsl",
    L"GenerateHistogramCS.hlsl",
    L"OIT_PS.hlsl",
    L"PNTriangles11_DS.hlsl",
    L"PerfGraphPS.hlsl",
    L"PerfGraphVS.hlsl",
    L"ScreenQuadVS.hlsl",
    L"SimpleBezier11HS.hlsl"
  };
  for (auto &p: recursive_directory_iterator(path(codeGenPath))) {
    if (is_regular_file(p)) {
      LPCWSTR fullPath = p.path().c_str();
      if (wcsstr(fullPath, L".hlsli") != nullptr) continue;
      if (wcsstr(fullPath, L"TessellatorCS40_defines.h") != nullptr) continue;
      // Skip failed tests.
      if (wcsstr(fullPath, L"SubD11_SubDToBezierHS") != nullptr) continue;
      if (!TestAll) {
        bool shouldTest = false;
        LPCWSTR *PreApprovedEnd = PreApprovedPaths + _countof(PreApprovedPaths);
        shouldTest = PreApprovedEnd == std::find_if(PreApprovedPaths, PreApprovedEnd,
          [&](LPCWSTR candidate) { return nullptr != wcsstr(fullPath, candidate); });
        if (!shouldTest) {
          break;
        }
      }
      auto start = std::chrono::system_clock::now();
      ReflectionTest(fullPath, true);
      if (TestAll) {
        // If testing all cases, print out their timing.
        auto end = std::chrono::system_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        LogCommentFmt(L"%s,%u", fullPath, (unsigned)dur.count());
      }
    }
  }
}
#endif // _WIN32 - Reflection unsupported

TEST_F(DxilContainerTest, ValidateFromLL_Abs2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\container\\abs2_m.ll");
}

TEST_F(DxilContainerTest, DxilContainerUnitTest) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  CComPtr<IDxcBlobEncoding> pDisassembly;
  CComPtr<IDxcOperationResult> pResult;
  std::vector<LPCWSTR> arguments;
  arguments.emplace_back(L"/Zi");
  arguments.emplace_back(L"/Qembed_debug");
  
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);
  // Test DxilContainer with ShaderDebugInfoDXIL
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"main", L"ps_6_0", arguments.data(), arguments.size(), nullptr, 0, nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  
  const hlsl::DxilContainerHeader *pHeader = hlsl::IsDxilContainerLike(pProgram->GetBufferPointer(), pProgram->GetBufferSize());
  VERIFY_IS_TRUE(hlsl::IsValidDxilContainer(pHeader, pProgram->GetBufferSize()));
  VERIFY_IS_NOT_NULL(hlsl::IsDxilContainerLike(pHeader, pProgram->GetBufferSize()));
  VERIFY_IS_NOT_NULL(hlsl::GetDxilProgramHeader(pHeader, hlsl::DxilFourCC::DFCC_DXIL));
  VERIFY_IS_NOT_NULL(hlsl::GetDxilProgramHeader(pHeader, hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL));
  VERIFY_IS_NOT_NULL(hlsl::GetDxilPartByType(pHeader, hlsl::DxilFourCC::DFCC_DXIL));
  VERIFY_IS_NOT_NULL(hlsl::GetDxilPartByType(pHeader, hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL));
  
  pResult.Release();
  pProgram.Release();

  // Test DxilContainer without ShaderDebugInfoDXIL
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"main", L"ps_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  
  pHeader = hlsl::IsDxilContainerLike(pProgram->GetBufferPointer(), pProgram->GetBufferSize());
  VERIFY_IS_TRUE(hlsl::IsValidDxilContainer(pHeader, pProgram->GetBufferSize()));
  VERIFY_IS_NOT_NULL(hlsl::IsDxilContainerLike(pHeader, pProgram->GetBufferSize()));
  VERIFY_IS_NOT_NULL(hlsl::GetDxilProgramHeader(pHeader, hlsl::DxilFourCC::DFCC_DXIL));
  VERIFY_IS_NULL(hlsl::GetDxilProgramHeader(pHeader, hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL));
  VERIFY_IS_NOT_NULL(hlsl::GetDxilPartByType(pHeader, hlsl::DxilFourCC::DFCC_DXIL));
  VERIFY_IS_NULL(hlsl::GetDxilPartByType(pHeader, hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL));

  // Test Empty DxilContainer
  hlsl::DxilContainerHeader header;
  SetupBasicHeader(&header);
  VERIFY_IS_TRUE(hlsl::IsValidDxilContainer(&header, header.ContainerSizeInBytes));
  VERIFY_IS_NOT_NULL(hlsl::IsDxilContainerLike(&header, header.ContainerSizeInBytes));
  VERIFY_IS_NULL(hlsl::GetDxilProgramHeader(&header, hlsl::DxilFourCC::DFCC_DXIL));
  VERIFY_IS_NULL(hlsl::GetDxilPartByType(&header, hlsl::DxilFourCC::DFCC_DXIL));

}
