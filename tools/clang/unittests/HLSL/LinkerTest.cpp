///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LinkerTest.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <vector>
#include <string>
#include "llvm/ADT/ArrayRef.h"
#include "dxc/Test/CompilationResult.h"
#include "dxc/Test/HLSLTestData.h"
#include "llvm/Support/ManagedStatic.h"

#include <fstream>

#include "WexTestClass.h"
#include "dxc/Test/HlslTestUtils.h"
#include "dxc/Test/DxcTestUtils.h"
#include "dxc/dxcapi.h"

using namespace std;
using namespace hlsl;
using namespace llvm;

// The test fixture.
class LinkerTest
{
public:
  BEGIN_TEST_CLASS(LinkerTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  TEST_METHOD(RunLinkResource);
  TEST_METHOD(RunLinkResourceWithBinding);
  TEST_METHOD(RunLinkAllProfiles);
  TEST_METHOD(RunLinkFailNoDefine);
  TEST_METHOD(RunLinkFailReDefine);
  TEST_METHOD(RunLinkGlobalInit);
  TEST_METHOD(RunLinkNoAlloca);
  TEST_METHOD(RunLinkMatArrayParam);
  TEST_METHOD(RunLinkMatParam);
  TEST_METHOD(RunLinkMatParamToLib);
  TEST_METHOD(RunLinkResRet);
  TEST_METHOD(RunLinkToLib);
  TEST_METHOD(RunLinkToLibExport);
  TEST_METHOD(RunLinkFailReDefineGlobal);
  TEST_METHOD(RunLinkFailProfileMismatch);
  TEST_METHOD(RunLinkFailEntryNoProps);
  TEST_METHOD(RunLinkFailSelectRes);
  TEST_METHOD(RunLinkToLibWithUnresolvedFunctions);
  TEST_METHOD(RunLinkToLibWithUnresolvedFunctionsExports);
  TEST_METHOD(RunLinkToLibWithExportNamesSwapped);
  TEST_METHOD(RunLinkToLibWithExportCollision);
  TEST_METHOD(RunLinkToLibWithUnusedExport);
  TEST_METHOD(RunLinkToLibWithNoExports);
  TEST_METHOD(RunLinkWithPotentialIntrinsicNameCollisions);
  TEST_METHOD(RunLinkWithValidatorVersion);
  TEST_METHOD(RunLinkWithTempReg);
  TEST_METHOD(RunLinkToLibWithGlobalCtor);


  dxc::DxcDllSupport m_dllSupport;
  VersionSupportInfo m_ver;

  void CreateLinker(IDxcLinker **pResultLinker) {
    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcLinker, pResultLinker));
  }

  void CompileLib(LPCWSTR filename, IDxcBlob **pResultBlob,
                  llvm::ArrayRef<LPCWSTR> pArguments = {},
                  LPCWSTR pShaderTarget = L"lib_6_x") {
    std::wstring fullPath = hlsl_test::GetPathToHlslDataFile(filename);
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcLibrary> pLibrary;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));

    VERIFY_SUCCEEDED(
        pLibrary->CreateBlobFromFile(fullPath.c_str(), nullptr, &pSource));

    CComPtr<IDxcIncludeHandler> pIncludeHandler;
    VERIFY_SUCCEEDED(pLibrary->CreateIncludeHandler(&pIncludeHandler));

    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlob> pProgram;

    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, fullPath.c_str(), L"", pShaderTarget,
                                        const_cast<LPCWSTR*>(pArguments.data()), pArguments.size(),
                                        nullptr, 0,
                                        pIncludeHandler, &pResult));
    CheckOperationSucceeded(pResult, pResultBlob);
  }

  void AssembleLib(LPCWSTR filename, IDxcBlob **pResultBlob) {
    std::wstring fullPath = hlsl_test::GetPathToHlslDataFile(filename);
    CComPtr<IDxcLibrary> pLibrary;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    CComPtr<IDxcBlobEncoding> pSource;
    VERIFY_SUCCEEDED(pLibrary->CreateBlobFromFile(fullPath.c_str(), nullptr, &pSource));
    CComPtr<IDxcAssembler> pAssembler;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
    CComPtr<IDxcOperationResult> pResult;
    VERIFY_SUCCEEDED(pAssembler->AssembleToContainer(pSource, &pResult));
    CheckOperationSucceeded(pResult, pResultBlob);
  }

  void RegisterDxcModule(LPCWSTR pLibName, IDxcBlob *pBlob,
                         IDxcLinker *pLinker) {
    VERIFY_SUCCEEDED(pLinker->RegisterLibrary(pLibName, pBlob));
  }

  void Link(LPCWSTR pEntryName, LPCWSTR pShaderModel, IDxcLinker *pLinker,
            ArrayRef<LPCWSTR> libNames, llvm::ArrayRef<LPCSTR> pCheckMsgs,
            llvm::ArrayRef<LPCSTR> pCheckNotMsgs,
            llvm::ArrayRef<LPCWSTR> pArguments = {},
            bool bRegEx = false) {
    CComPtr<IDxcOperationResult> pResult;
    VERIFY_SUCCEEDED(pLinker->Link(pEntryName, pShaderModel, libNames.data(),
                                   libNames.size(),
                                   pArguments.data(), pArguments.size(),
                                   &pResult));
    CComPtr<IDxcBlob> pProgram;
    CheckOperationSucceeded(pResult, &pProgram);

    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcBlobEncoding> pDisassembly;

    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembly));
    std::string IR = BlobToUtf8(pDisassembly);
    CheckMsgs(IR.c_str(), IR.size(), pCheckMsgs.data(), pCheckMsgs.size(), bRegEx);
    CheckNotMsgs(IR.c_str(), IR.size(), pCheckNotMsgs.data(), pCheckNotMsgs.size(), bRegEx);
  }

  void LinkCheckMsg(LPCWSTR pEntryName, LPCWSTR pShaderModel, IDxcLinker *pLinker,
            ArrayRef<LPCWSTR> libNames, llvm::ArrayRef<LPCSTR> pErrorMsgs,
            llvm::ArrayRef<LPCWSTR> pArguments = {}) {
    CComPtr<IDxcOperationResult> pResult;
    VERIFY_SUCCEEDED(pLinker->Link(pEntryName, pShaderModel,
                                   libNames.data(), libNames.size(),
                                   pArguments.data(), pArguments.size(),
                                   &pResult));
    CheckOperationResultMsgs(pResult, pErrorMsgs.data(), pErrorMsgs.size(),
                             false, false);
  }
};

bool LinkerTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    m_ver.Initialize(m_dllSupport);
  }

  return true;
}

TEST_F(LinkerTest, RunLinkResource) {
  CComPtr<IDxcBlob> pResLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_resource2.hlsl", &pResLib);
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_cs_entry.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);
  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libResName = L"res";
  RegisterDxcModule(libResName, pResLib, pLinker);

  Link(L"entry", L"cs_6_0", pLinker, {libResName, libName}, {} ,{});
}

TEST_F(LinkerTest, RunLinkResourceWithBinding) {
  // These two libraries both have a ConstantBuffer resource named g_buf.
  // These are explicitly bound to different slots, and the types don't match.
  // This test runs a pass to rename resources to prevent merging of resource globals.
  // Then tests linking these, which requires dxil op overload renaming
  // because of a typename collision between the two libraries.
  CComPtr<IDxcBlob> pLib1;
  CompileLib(L"..\\CodeGenHLSL\\lib_res_bound1.hlsl", &pLib1);
  CComPtr<IDxcBlob> pLib2;
  CompileLib(L"..\\CodeGenHLSL\\lib_res_bound2.hlsl", &pLib2);

  LPCWSTR optOptions[] = {
    L"-dxil-rename-resources,prefix=lib1",
    L"-dxil-rename-resources,prefix=lib2",
  };

  CComPtr<IDxcOptimizer> pOptimizer;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcOptimizer, &pOptimizer));

  CComPtr<IDxcContainerReflection> pContainerReflection;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcContainerReflection, &pContainerReflection));
  UINT32 partIdx = 0;
  VERIFY_SUCCEEDED(pContainerReflection->Load(pLib1));
  VERIFY_SUCCEEDED(pContainerReflection->FindFirstPartKind(DXC_PART_DXIL, &partIdx));
  CComPtr<IDxcBlob> pLib1Module;
  VERIFY_SUCCEEDED(pContainerReflection->GetPartContent(partIdx, &pLib1Module));

  CComPtr<IDxcBlob> pLib1ModuleRenamed;
  VERIFY_SUCCEEDED(pOptimizer->RunOptimizer(pLib1Module, &optOptions[0], 1, &pLib1ModuleRenamed, nullptr));
  pLib1Module.Release();
  pLib1.Release();
  AssembleToContainer(m_dllSupport, pLib1ModuleRenamed, &pLib1);

  VERIFY_SUCCEEDED(pContainerReflection->Load(pLib2));
  VERIFY_SUCCEEDED(pContainerReflection->FindFirstPartKind(DXC_PART_DXIL, &partIdx));
  CComPtr<IDxcBlob> pLib2Module;
  VERIFY_SUCCEEDED(pContainerReflection->GetPartContent(partIdx, &pLib2Module));

  CComPtr<IDxcBlob> pLib2ModuleRenamed;
  VERIFY_SUCCEEDED(pOptimizer->RunOptimizer(pLib2Module, &optOptions[1], 1, &pLib2ModuleRenamed, nullptr));
  pLib2Module.Release();
  pLib2.Release();
  AssembleToContainer(m_dllSupport, pLib2ModuleRenamed, &pLib2);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);
  LPCWSTR lib1Name = L"lib1";
  RegisterDxcModule(lib1Name, pLib1, pLinker);

  LPCWSTR lib2Name = L"lib2";
  RegisterDxcModule(lib2Name, pLib2, pLinker);

  Link(L"main", L"cs_6_0", pLinker, {lib1Name, lib2Name}, {} ,{});
}

TEST_F(LinkerTest, RunLinkAllProfiles) {
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  LPCWSTR option[] = { L"-Zi", L"-Qembed_debug" };

  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_entries2.hlsl", &pEntryLib, option);
  RegisterDxcModule(libName, pEntryLib, pLinker);

  Link(L"vs_main", L"vs_6_0", pLinker, {libName}, {},{});
  Link(L"hs_main", L"hs_6_0", pLinker, {libName}, {},{});
  Link(L"ds_main", L"ds_6_0", pLinker, {libName}, {},{});
  Link(L"gs_main", L"gs_6_0", pLinker, {libName}, {},{});
  Link(L"ps_main", L"ps_6_0", pLinker, {libName}, {},{});

  CComPtr<IDxcBlob> pResLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_resource2.hlsl", &pResLib);

  LPCWSTR libResName = L"res";
  RegisterDxcModule(libResName, pResLib, pLinker);
  Link(L"cs_main", L"cs_6_0", pLinker, {libName, libResName}, {},{});
}

TEST_F(LinkerTest, RunLinkFailNoDefine) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_cs_entry.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LinkCheckMsg(L"entry", L"cs_6_0", pLinker, {libName},
               {"Cannot find definition of function"});
}

TEST_F(LinkerTest, RunLinkFailReDefine) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_cs_entry.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libName2 = L"entry2";
  RegisterDxcModule(libName2, pEntryLib, pLinker);

  LinkCheckMsg(L"entry", L"cs_6_0", pLinker, {libName, libName2},
               {"Definition already exists for function"});
}

TEST_F(LinkerTest, RunLinkGlobalInit) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_global.hlsl", &pEntryLib, {}, L"lib_6_3");
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  Link(L"test", L"ps_6_0", pLinker, {libName},
       // Make sure cbuffer load is generated.
       {"dx.op.cbufferLoad"},{});
}

TEST_F(LinkerTest, RunLinkFailReDefineGlobal) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_global2.hlsl", &pEntryLib);

  CComPtr<IDxcBlob> pLib0;
  CompileLib(L"..\\CodeGenHLSL\\lib_global3.hlsl", &pLib0);

  CComPtr<IDxcBlob> pLib1;
  CompileLib(L"..\\CodeGenHLSL\\lib_global4.hlsl", &pLib1);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libName1 = L"lib0";
  RegisterDxcModule(libName1, pLib0, pLinker);

  LPCWSTR libName2 = L"lib1";
  RegisterDxcModule(libName2, pLib1, pLinker);

  LinkCheckMsg(L"entry", L"cs_6_0", pLinker, {libName, libName1, libName2},
               {"Definition already exists for global variable", "Resource already exists"});
}

TEST_F(LinkerTest, RunLinkFailProfileMismatch) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_global.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LinkCheckMsg(L"test", L"cs_6_0", pLinker, {libName},
               {"Profile mismatch between entry function and target profile"});
}

TEST_F(LinkerTest, RunLinkFailEntryNoProps) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_global.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LinkCheckMsg(L"\01?update@@YAXXZ", L"cs_6_0", pLinker, {libName},
               {"Cannot find function property for entry function"});
}

TEST_F(LinkerTest, RunLinkNoAlloca) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_no_alloca.hlsl", &pEntryLib);
  CComPtr<IDxcBlob> pLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_no_alloca.h", &pLib);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"ps_main";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libName2 = L"test";
  RegisterDxcModule(libName2, pLib, pLinker);

  Link(L"ps_main", L"ps_6_0", pLinker, {libName, libName2}, {}, {"alloca"});
}

TEST_F(LinkerTest, RunLinkMatArrayParam) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_mat_entry.hlsl", &pEntryLib);
  CComPtr<IDxcBlob> pLib;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_mat_cast.hlsl", &pLib);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"ps_main";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libName2 = L"test";
  RegisterDxcModule(libName2, pLib, pLinker);

  Link(L"main", L"ps_6_0", pLinker, {libName, libName2},
       {"alloca [24 x float]", "getelementptr [12 x float], [12 x float]*"},
       {});
}

TEST_F(LinkerTest, RunLinkMatParam) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_mat_entry2.hlsl", &pEntryLib);
  CComPtr<IDxcBlob> pLib;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_mat_cast2.hlsl", &pLib);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"ps_main";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libName2 = L"test";
  RegisterDxcModule(libName2, pLib, pLinker);

  Link(L"main", L"ps_6_0", pLinker, {libName, libName2},
       {"alloca [12 x float]"},
       {});
}

TEST_F(LinkerTest, RunLinkMatParamToLib) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_mat_entry2.hlsl", &pEntryLib);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"ps_main";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  Link(L"", L"lib_6_3", pLinker, {libName},
       // The bitcast cannot be removed because user function call use it as
       // argument.
       {"bitcast <12 x float>\\* %.* to %class\\.matrix\\.float\\.4\\.3\\*"}, {}, {}, true);
}

TEST_F(LinkerTest, RunLinkResRet) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_out_param_res.hlsl", &pEntryLib);
  CComPtr<IDxcBlob> pLib;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_out_param_res_imp.hlsl", &pLib);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"ps_main";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libName2 = L"test";
  RegisterDxcModule(libName2, pLib, pLinker);

  Link(L"test", L"ps_6_0", pLinker, {libName, libName2}, {}, {"alloca"});
}

TEST_F(LinkerTest, RunLinkToLib) {
  LPCWSTR option[] = {L"-Zi", L"-Qembed_debug"};

  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_mat_entry2.hlsl",
             &pEntryLib, option);
  CComPtr<IDxcBlob> pLib;
  CompileLib(
      L"..\\CodeGenHLSL\\linker\\lib_mat_cast2.hlsl",
      &pLib, option);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"ps_main";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libName2 = L"test";
  RegisterDxcModule(libName2, pLib, pLinker);

  Link(L"", L"lib_6_3", pLinker, {libName, libName2}, {"!llvm.dbg.cu"}, {}, option);
}

TEST_F(LinkerTest, RunLinkToLibExport) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_mat_entry2.hlsl",
             &pEntryLib);
  CComPtr<IDxcBlob> pLib;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_mat_cast2.hlsl",
             &pLib);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"ps_main";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libName2 = L"test";
  RegisterDxcModule(libName2, pLib, pLinker);
  Link(L"", L"lib_6_3", pLinker, {libName, libName2},
    { "@\"\\01?renamed_test@@","@\"\\01?cloned_test@@","@main" },
    { "@\"\\01?mat_test", "@renamed_test", "@cloned_test" },
    {L"-exports", L"renamed_test,cloned_test=\\01?mat_test@@YA?AV?$vector@M$02@@V?$vector@M$03@@0AIAV?$matrix@M$03$02@@@Z;main"});
}

TEST_F(LinkerTest, RunLinkFailSelectRes) {
  if (m_ver.SkipDxilVersion(1, 3)) return;
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_select_res_entry.hlsl", &pEntryLib);
  CComPtr<IDxcBlob> pLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_select_res.hlsl", &pLib);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"main";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libName2 = L"test";
  RegisterDxcModule(libName2, pLib, pLinker);

  LinkCheckMsg(L"main", L"ps_6_0", pLinker, {libName, libName2},
               {"local resource not guaranteed to map to unique global resource"});
}

TEST_F(LinkerTest, RunLinkToLibWithUnresolvedFunctions) {
  LPCWSTR option[] = { L"-Zi", L"-Qembed_debug" };

  CComPtr<IDxcBlob> pLib1;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func1.hlsl",
             &pLib1, option);
  CComPtr<IDxcBlob> pLib2;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func2.hlsl",
             &pLib2, option);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName1 = L"lib1";
  RegisterDxcModule(libName1, pLib1, pLinker);

  LPCWSTR libName2 = L"lib2";
  RegisterDxcModule(libName2, pLib2, pLinker);

  Link(L"", L"lib_6_3", pLinker, { libName1, libName2 }, {
    "declare float @\"\\01?external_fn1@@YAMXZ\"()",
    "declare float @\"\\01?external_fn2@@YAMXZ\"()",
    "declare float @\"\\01?external_fn@@YAMXZ\"()",
    "define float @\"\\01?lib1_fn@@YAMXZ\"()",
    "define float @\"\\01?lib2_fn@@YAMXZ\"()",
    "define float @\"\\01?call_lib1@@YAMXZ\"()",
    "define float @\"\\01?call_lib2@@YAMXZ\"()"
    }, {"declare float @\"\\01?unused_fn1", "declare float @\"\\01?unused_fn2"});
}

TEST_F(LinkerTest, RunLinkToLibWithUnresolvedFunctionsExports) {
  LPCWSTR option[] = { L"-Zi", L"-Qembed_debug" };

  CComPtr<IDxcBlob> pLib1;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func1.hlsl",
    &pLib1, option);
  CComPtr<IDxcBlob> pLib2;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func2.hlsl",
    &pLib2, option);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName1 = L"lib1";
  RegisterDxcModule(libName1, pLib1, pLinker);

  LPCWSTR libName2 = L"lib2";
  RegisterDxcModule(libName2, pLib2, pLinker);

  Link(L"", L"lib_6_3", pLinker, { libName1, libName2 },
    { "declare float @\"\\01?external_fn1@@YAMXZ\"()",
      "declare float @\"\\01?external_fn2@@YAMXZ\"()",
      "declare float @\"\\01?external_fn@@YAMXZ\"()",
      "define float @\"\\01?renamed_lib1@@YAMXZ\"()",
      "define float @\"\\01?call_lib2@@YAMXZ\"()"
    },
    { "float @\"\\01?unused_fn1", "float @\"\\01?unused_fn2",
      "float @\"\\01?lib1_fn", "float @\"\\01?lib2_fn",
      "float @\"\\01?call_lib1"
    },
    { L"-exports", L"renamed_lib1=call_lib1",
      L"-exports", L"call_lib2"
    });
}

TEST_F(LinkerTest, RunLinkToLibWithExportNamesSwapped) {
  LPCWSTR option[] = { L"-Zi", L"-Qembed_debug" };

  CComPtr<IDxcBlob> pLib1;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func1.hlsl",
    &pLib1, option);
  CComPtr<IDxcBlob> pLib2;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func2.hlsl",
    &pLib2, option);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName1 = L"lib1";
  RegisterDxcModule(libName1, pLib1, pLinker);

  LPCWSTR libName2 = L"lib2";
  RegisterDxcModule(libName2, pLib2, pLinker);

  Link(L"", L"lib_6_3", pLinker, { libName1, libName2 },
    { "declare float @\"\\01?external_fn1@@YAMXZ\"()",
      "declare float @\"\\01?external_fn2@@YAMXZ\"()",
      "declare float @\"\\01?external_fn@@YAMXZ\"()",
      "define float @\"\\01?call_lib1@@YAMXZ\"()",
      "define float @\"\\01?call_lib2@@YAMXZ\"()"
    },
    { "float @\"\\01?unused_fn1", "float @\"\\01?unused_fn2",
      "float @\"\\01?lib1_fn", "float @\"\\01?lib2_fn"
    },
    { L"-exports", L"call_lib2=call_lib1;call_lib1=call_lib2" });
}

TEST_F(LinkerTest, RunLinkToLibWithExportCollision) {
  LPCWSTR option[] = { L"-Zi", L"-Qembed_debug" };

  CComPtr<IDxcBlob> pLib1;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func1.hlsl",
    &pLib1, option);
  CComPtr<IDxcBlob> pLib2;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func2.hlsl",
    &pLib2, option);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName1 = L"lib1";
  RegisterDxcModule(libName1, pLib1, pLinker);

  LPCWSTR libName2 = L"lib2";
  RegisterDxcModule(libName2, pLib2, pLinker);

  LinkCheckMsg(L"", L"lib_6_3", pLinker, { libName1, libName2 },
    { "Export name collides with another export: \\01?call_lib2@@YAMXZ"
    },
    { L"-exports", L"call_lib2=call_lib1;call_lib2" });
}

TEST_F(LinkerTest, RunLinkToLibWithUnusedExport) {
  LPCWSTR option[] = { L"-Zi", L"-Qembed_debug" };

  CComPtr<IDxcBlob> pLib1;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func1.hlsl",
    &pLib1, option);
  CComPtr<IDxcBlob> pLib2;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func2.hlsl",
    &pLib2, option);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName1 = L"lib1";
  RegisterDxcModule(libName1, pLib1, pLinker);

  LPCWSTR libName2 = L"lib2";
  RegisterDxcModule(libName2, pLib2, pLinker);

  LinkCheckMsg(L"", L"lib_6_3", pLinker, { libName1, libName2 },
    { "Could not find target for export: call_lib"
    },
    { L"-exports", L"call_lib2=call_lib;call_lib1" });
}

TEST_F(LinkerTest, RunLinkToLibWithNoExports) {
  LPCWSTR option[] = { L"-Zi", L"-Qembed_debug" };

  CComPtr<IDxcBlob> pLib1;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func1.hlsl",
    &pLib1, option);
  CComPtr<IDxcBlob> pLib2;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_unresolved_func2.hlsl",
    &pLib2, option);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName1 = L"lib1";
  RegisterDxcModule(libName1, pLib1, pLinker);

  LPCWSTR libName2 = L"lib2";
  RegisterDxcModule(libName2, pLib2, pLinker);

  LinkCheckMsg(L"", L"lib_6_3", pLinker, { libName1, libName2 },
    { "Library has no functions to export"
    },
    { L"-exports", L"call_lib2=call_lib" });
}

TEST_F(LinkerTest, RunLinkWithPotentialIntrinsicNameCollisions) {
  LPCWSTR option[] = { L"-Zi", L"-Qembed_debug" };

  CComPtr<IDxcBlob> pLib1;
  CompileLib(L"..\\CodeGenHLSL\\linker\\createHandle_multi.hlsl",
    &pLib1, option);
  CComPtr<IDxcBlob> pLib2;
  CompileLib(L"..\\CodeGenHLSL\\linker\\createHandle_multi2.hlsl",
    &pLib2, option);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName1 = L"lib1";
  RegisterDxcModule(libName1, pLib1, pLinker);

  LPCWSTR libName2 = L"lib2";
  RegisterDxcModule(libName2, pLib2, pLinker);

  Link(L"", L"lib_6_3", pLinker, { libName1, libName2 }, {
    "declare %dx.types.Handle @\"dx.op.createHandleForLib.class.Texture2D<vector<float, 4> >\"(i32, %\"class.Texture2D<vector<float, 4> >\")",
    "declare %dx.types.Handle @\"dx.op.createHandleForLib.class.Texture2D<float>\"(i32, %\"class.Texture2D<float>\")"
  }, { });
}

TEST_F(LinkerTest, RunLinkWithValidatorVersion) {
  if (m_ver.SkipDxilVersion(1, 4)) return;

  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_mat_entry2.hlsl",
             &pEntryLib, {});
  CComPtr<IDxcBlob> pLib;
  CompileLib(
      L"..\\CodeGenHLSL\\linker\\lib_mat_cast2.hlsl",
      &pLib, {});

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"ps_main";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libName2 = L"test";
  RegisterDxcModule(libName2, pLib, pLinker);

  Link(L"", L"lib_6_3", pLinker, {libName, libName2},
       {"!dx.valver = !{(![0-9]+)}.*\n\\1 = !{i32 1, i32 3}"},
       {}, {L"-validator-version", L"1.3"}, /*regex*/ true);
}

TEST_F(LinkerTest, RunLinkWithTempReg) {
  // TempRegLoad/TempRegStore not normally usable from HLSL.
  // This assembly library exposes these through overloaded wrapper functions
  // void sreg(uint index, <type> value) to store register, overloaded for uint, int, and float
  // uint ureg(uint index) to load register as uint
  // int ireg(int index) to load register as int
  // float freg(uint index) to load register as float

  // This test verifies this scenario works, by assembling this library,
  // compiling a library with an entry point that uses sreg/ureg,
  // then linking these to a final standard vs_6_0 DXIL shader.

  CComPtr<IDxcBlob> pTempRegLib;
  AssembleLib(L"..\\HLSLFileCheck\\dxil\\linker\\TempReg.ll", &pTempRegLib);
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\HLSLFileCheck\\dxil\\linker\\use-TempReg.hlsl", &pEntryLib, {}, L"lib_6_3");
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);
  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker);

  LPCWSTR libResName = L"TempReg";
  RegisterDxcModule(libResName, pTempRegLib, pLinker);

  Link(L"main", L"vs_6_0", pLinker, {libResName, libName}, {
    "call void @dx.op.tempRegStore.i32",
    "call i32 @dx.op.tempRegLoad.i32"
    } ,{});
}

TEST_F(LinkerTest, RunLinkToLibWithGlobalCtor) {
  CComPtr<IDxcBlob> pLib0;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_static_cb_init.hlsl", &pLib0, {});
  CComPtr<IDxcBlob> pLib1;
  CompileLib(L"..\\CodeGenHLSL\\linker\\lib_use_static_cb_init.hlsl", &pLib1,
             {});

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"foo";
  RegisterDxcModule(libName, pLib0, pLinker);

  LPCWSTR libName2 = L"bar";
  RegisterDxcModule(libName2, pLib1, pLinker);
  // Make sure global_ctors created for lib to lib.
  Link(L"", L"lib_6_3", pLinker, {libName, libName2},
       {"@llvm.global_ctors = appending global [1 x { i32, void ()*, i8* }] [{ "
        "i32, void ()*, i8* } { i32 65535, void ()* "
        "@foo._GLOBAL__sub_I_lib_static_cb_init.hlsl, i8* null }]"},
       {},
       {});
}
