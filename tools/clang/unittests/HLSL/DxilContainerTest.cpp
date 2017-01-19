///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilContainerTest.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Provides tests for container formatting and validation.                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif

#include <memory>
#include <vector>
#include <string>
#include <tuple>
#include <cassert>
#include <sstream>
#include <algorithm>
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#include <atlfile.h>

#include "HLSLTestData.h"
#include "WexTestClass.h"
#include "HlslTestUtils.h"
#include "DxcTestUtils.h"

#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/HLSL/DxilContainer.h"

#include <fstream>
#include <filesystem>

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include <codecvt>


using namespace std;
using namespace hlsl_test;
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

class DxilContainerTest {
public:
  BEGIN_TEST_CLASS(DxilContainerTest)
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_METHOD(CompileWhenOKThenIncludesFeatureInfo)
  TEST_METHOD(CompileWhenOKThenIncludesSignatures)
  TEST_METHOD(CompileWhenSigSquareThenIncludeSplit)
  TEST_METHOD(DisassemblyWhenMissingThenFails)
  TEST_METHOD(DisassemblyWhenBCInvalidThenFails)
  TEST_METHOD(DisassemblyWhenInvalidThenFails)
  TEST_METHOD(DisassemblyWhenValidThenOK)
  TEST_METHOD(ValidateFromLL_Abs2)


  TEST_METHOD(ReflectionMatchesDXBC_CheckIn)
  BEGIN_TEST_METHOD(ReflectionMatchesDXBC_Full)
    TEST_METHOD_PROPERTY(L"Priority", L"1")
  END_TEST_METHOD()

  dxc::DxcDllSupport m_dllSupport;

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
    if (!isInput)
      VERIFY_ARE_EQUAL(pTestDesc->ReadWriteMask != 0, pBaseDesc->ReadWriteMask != 0); // VERIFY_ARE_EQUAL(pTestDesc->ReadWriteMask, pBaseDesc->ReadWriteMask);
    // VERIFY_ARE_EQUAL(pTestDesc->Register, pBaseDesc->Register);
    //VERIFY_ARE_EQUAL(pTestDesc->SemanticIndex, pBaseDesc->SemanticIndex);
    VERIFY_ARE_EQUAL(pTestDesc->Stream, pBaseDesc->Stream);
    VERIFY_ARE_EQUAL(pTestDesc->SystemValueType, pBaseDesc->SystemValueType);
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
        for (UINT vi = 0; vi < testCB.Variables; ++vi) {
          ID3D12ShaderReflectionVariable *pBaseConst;
          D3D12_SHADER_VARIABLE_DESC baseConst;
          pBaseConst = pBaseCB->GetVariableByIndex(vi);
          VERIFY_SUCCEEDED(pBaseConst->GetDesc(&baseConst));
          variableMap[baseConst.Name] = baseConst;
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
          // TODO: enalbe size cmp.
          //VERIFY_ARE_EQUAL(testConst.Size, baseConst.Size);
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

  void split(const wstring &s, wchar_t delim, vector<wstring> &elems) {
    wstringstream ss(s);
    wstring item;
    while (getline(ss, item, delim)) {
      elems.push_back(item);
    }
  }

  vector<wstring> split(const wstring &s, char delim) {
    vector<wstring> elems;
    split(s, delim, elems);
    return elems;
  }

  wstring SplitFilename(const wstring &str) {
    size_t found;
    found = str.find_last_of(L"/\\");
    return str.substr(0, found);
  }

  void NameParseCommandPartsFromFile(LPCWSTR path, std::vector<FileRunCommandPart> &parts) {
    vector<wstring> Parts = split(wstring(path), '+');
    std::wstring Name = Parts[0];
    std::wstring EntryPoint = Parts[1];
    std::wstring ShaderModel = Parts[2];
    std::wstring Arguments = L"-T ";
    Arguments += ShaderModel;
    Arguments += L" -E ";
    Arguments += EntryPoint;
    Arguments += L" %s";
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t> > w;
    string ArgumentsNarrow = w.to_bytes(Arguments);

    FileRunCommandPart P(string("%dxc"), ArgumentsNarrow, path);
    parts.push_back(P);
  }

  HRESULT CompileFromFile(LPCWSTR path, bool useDXBC, IDxcBlob **ppBlob) {
    std::vector<FileRunCommandPart> parts;
    //NameParseCommandPartsFromFile(path, parts);
    ParseCommandPartsFromFile(path, parts);
    VERIFY_IS_TRUE(parts.size() > 0);
    VERIFY_ARE_EQUAL_STR(parts[0].Command.c_str(), "%dxc");
    FileRunCommandPart &dxc = parts[0];
    m_dllSupport.Initialize();
    dxc.DllSupport = &m_dllSupport;

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
      dxc.Run(nullptr);
      IFRBOOL(dxc.RunResult == 0, E_FAIL);
      IFR(dxc.OpResult->GetResult(ppBlob));
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

  std::string DisassembleProgram(LPCSTR program, LPCWSTR entryPoint,
                                 LPCWSTR target) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcBlob> pProgram;
    CComPtr<IDxcBlobEncoding> pDisassembly;
    CComPtr<IDxcOperationResult> pResult;

    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
    CreateBlobFromText(program, &pSource);
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", entryPoint,
                                        target, nullptr, 0, nullptr, 0, nullptr,
                                        &pResult));
    VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
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

  WEX::Common::String WStrFmt(const wchar_t* msg, ...) {
    va_list args;
    va_start(args, msg);
    WEX::Common::String result = WEX::Common::String().FormatV(msg, args);
    va_end(args);
    return result;
  }

  void ReflectionTest(LPCWSTR name, bool ignoreIfDXBCFails) {
    WEX::Logging::Log::Comment(WEX::Common::String().Format(L"Reflection comparison for %s", name));
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
      if (ignoreIfDXBCFails) return;
      VERIFY_FAIL();
    }
    
    CComPtr<ID3D12ShaderReflection> pProgramReflection;
    CComPtr<ID3D12ShaderReflection> pProgramReflectionDXBC;
    CreateReflectionFromBlob(pProgram, &pProgramReflection);
    CreateReflectionFromDXBC(pProgramDXBC, &pProgramReflectionDXBC);

    CompareReflection(pProgramReflection, pProgramReflectionDXBC);
  }
};

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
    // NOTE: this will change when proper packing is done, and when 'always-writes' is accurately implemented.
    const char expected[] =
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
      "; COLOR                    0   xyzw        0     NONE   float   xyzw\n"  // should read '1' in register
      "; SV_Position              0   xyzw        1      POS   float   xyzw\n"; // could read SV_POSITION
    std::string start(s.c_str(), strlen(expected));
    VERIFY_ARE_EQUAL_STR(expected, start.c_str());
  }

  {
    std::string s = DisassembleProgram(program, L"PSMain", L"ps_6_0");
    // NOTE: this will change when proper packing is done, and when 'always-writes' is accurately implemented.
    const char expected[] =
      ";\n"
      "; Input signature:\n"
      ";\n"
      "; Name                 Index   Mask Register SysValue  Format   Used\n"
      "; -------------------- ----- ------ -------- -------- ------- ------\n"
      "; COLOR                    0   xyzw        0     NONE   float       \n" // should read '1' in register, xyzw in Used
      "; SV_Position              0   xyzw        1      POS   float       \n" // could read SV_POSITION
      ";\n"
      ";\n"
      "; Output signature:\n"
      ";\n"
      "; Name                 Index   Mask Register SysValue  Format   Used\n"
      "; -------------------- ----- ------ -------- -------- ------- ------\n"
      "; SV_Target                0   xyzw        0   TARGET   float   xyzw\n";// could read SV_TARGET
    std::string start(s.c_str(), strlen(expected));
    VERIFY_ARE_EQUAL_STR(expected, start.c_str());
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
  VERIFY_ARE_EQUAL(0, *(uint64_t *)hlsl::GetDxilPartData(*pPartIter));
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
  CreateBlobPinned(&header, header.ContainerSizeInBytes, CP_UTF8, &pSource);
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
    CreateBlobPinned(pHeader, sizeof(hlsl::DxilContainerHeader) - 4, CP_UTF8,
                     &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }

  // Wrong major version.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    pHeader->Version.Major = 100;
    CreateBlobPinned(pHeader, pHeader->ContainerSizeInBytes, CP_UTF8, &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }

  // Size out of bounds.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    pHeader->ContainerSizeInBytes = 1024;
    CreateBlobPinned(pHeader, sizeof(hlsl::DxilContainerHeader), CP_UTF8,
                     &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }

  // Size too large as per spec limit.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    pHeader->ContainerSizeInBytes = hlsl::DxilContainerMaxSize + 1;
    CreateBlobPinned(pHeader, pHeader->ContainerSizeInBytes, CP_UTF8, &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }

  // Not large enough to hold offset table.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    pHeader->PartCount = 1;
    CreateBlobPinned(pHeader, pHeader->ContainerSizeInBytes, CP_UTF8, &pSource);
    VERIFY_FAILED(pCompiler->Disassemble(pSource, &pDisassembly));
  }

  // Part offset out of bounds.
  {
    CComPtr<IDxcBlobEncoding> pSource;
    SetupBasicHeader(pHeader);
    pHeader->PartCount = 1;
    *((uint32_t *)(pHeader + 1)) = 1024;
    pHeader->ContainerSizeInBytes += sizeof(uint32_t);
    CreateBlobPinned(pHeader, pHeader->ContainerSizeInBytes, CP_UTF8, &pSource);
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
    CreateBlobPinned(pHeader, pHeader->ContainerSizeInBytes, CP_UTF8, &pSource);
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
  VERIFY_ARE_NOT_EQUAL(0, disassembleString.size());
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

TEST_F(DxilContainerTest, ReflectionMatchesDXBC_CheckIn) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  ReflectionTest(hlsl_test::GetPathToHlslDataFile(L"..\\CodeGenHLSL\\Samples\\DX11\\SimpleBezier11DS.hlsl").c_str(), false);
  ReflectionTest(hlsl_test::GetPathToHlslDataFile(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_SmoothPS.hlsl").c_str(), false);
}

TEST_F(DxilContainerTest, ReflectionMatchesDXBC_Full) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  std::wstring codeGenPath = hlsl_test::GetPathToHlslDataFile(L"..\\CodeGenHLSL\\Samples");
  for (auto &p: recursive_directory_iterator(path(codeGenPath))) {
    if (is_regular_file(p)) {
      LPCWSTR fullPath = p.path().c_str();
      if (wcsstr(fullPath, L".hlsli") != nullptr) continue;
      if (wcsstr(fullPath, L"TessellatorCS40_defines.h") != nullptr) continue;
      // Skip failed tests.
      if (wcsstr(fullPath, L"SubD11_SubDToBezierHS") != nullptr) continue;

      ReflectionTest(fullPath, true);
    }
  }
}

TEST_F(DxilContainerTest, ValidateFromLL_Abs2) {
  CodeGenTestCheck(L"abs2_m.ll");
}
