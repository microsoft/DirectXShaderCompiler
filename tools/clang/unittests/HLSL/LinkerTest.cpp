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
#include "CompilationResult.h"
#include "HLSLTestData.h"
#include "llvm/Support/ManagedStatic.h"

#include <fstream>

#include "WexTestClass.h"
#include "HlslTestUtils.h"
#include "dxc/dxcapi.h"
#include "DxcTestUtils.h"

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

  TEST_METHOD(RunLinkResourceLazy);
  TEST_METHOD(RunLinkAllProfilesLazy);
  TEST_METHOD(RunLinkFailNoDefineLazy);
  TEST_METHOD(RunLinkFailReDefineLazy);
  TEST_METHOD(RunLinkGlobalInitLazy);
  TEST_METHOD(RunLinkNoAllocaLazy);
  TEST_METHOD(RunLinkFailReDefineGlobalLazy);
  TEST_METHOD(RunLinkFailProfileMismatchLazy);
  TEST_METHOD(RunLinkFailEntryNoPropsLazy);
  TEST_METHOD(RunLinkResource);
  TEST_METHOD(RunLinkAllProfiles);
  TEST_METHOD(RunLinkFailNoDefine);
  TEST_METHOD(RunLinkFailReDefine);
  TEST_METHOD(RunLinkGlobalInit);
  TEST_METHOD(RunLinkNoAlloca);
  TEST_METHOD(RunLinkFailReDefineGlobal);
  TEST_METHOD(RunLinkFailProfileMismatch);
  TEST_METHOD(RunLinkFailEntryNoProps);
  TEST_METHOD(RunLinkNoAllocaMixLazy);


  dxc::DxcDllSupport m_dllSupport;
  VersionSupportInfo m_ver;

  void CreateLinker(IDxcLinker **pResultLinker) {
    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcLinker, pResultLinker));
  }

  void CompileLib(LPCWSTR filename, IDxcBlob **pResultBlob) {
    std::wstring fullPath = hlsl_test::GetPathToHlslDataFile(filename);
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcLibrary> pLibrary;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));

    VERIFY_SUCCEEDED(
        pLibrary->CreateBlobFromFile(fullPath.c_str(), nullptr, &pSource));

    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlob> pProgram;

    CA2W shWide("lib_6_1", CP_UTF8);
    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"hlsl.hlsl", L"", shWide,
                                        nullptr, 0, nullptr, 0, nullptr,
                                        &pResult));
    VERIFY_SUCCEEDED(pResult->GetResult(pResultBlob));
  }

  void RegisterDxcModule(LPCWSTR pLibName, IDxcBlob *pBlob, IDxcLinker *pLinker,
                         bool bLazyLoad) {
    VERIFY_SUCCEEDED(pLinker->RegisterLibrary(pLibName, pBlob, bLazyLoad));
  }

  void Link(LPCWSTR pEntryName, LPCWSTR pShaderModel, IDxcLinker *pLinker,
            ArrayRef<LPCWSTR> libNames, llvm::ArrayRef<LPCSTR> pCheckMsgs,
            llvm::ArrayRef<LPCSTR> pCheckNotMsgs) {
    CComPtr<IDxcOperationResult> pResult;
    VERIFY_SUCCEEDED(pLinker->Link(pEntryName, pShaderModel, libNames.data(),
                                   libNames.size(), nullptr, 0, &pResult));
    CComPtr<IDxcBlob> pProgram;
    CheckOperationSucceeded(pResult, &pProgram);

    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcBlobEncoding> pDisassembly;

    VERIFY_SUCCEEDED(
        m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembly));
    std::string IR = BlobToUtf8(pDisassembly);
    CheckMsgs(IR.c_str(), IR.size(), pCheckMsgs.data(), pCheckMsgs.size(), false);
    for (auto notMsg : pCheckNotMsgs) {
      VERIFY_IS_TRUE(IR.find(notMsg) == std::string::npos);
    }
  }

  void LinkCheckMsg(LPCWSTR pEntryName, LPCWSTR pShaderModel, IDxcLinker *pLinker,
            ArrayRef<LPCWSTR> libNames, llvm::ArrayRef<LPCSTR> pErrorMsgs) {
    CComPtr<IDxcOperationResult> pResult;
    VERIFY_SUCCEEDED(pLinker->Link(pEntryName, pShaderModel, libNames.data(),
                                   libNames.size(), nullptr, 0, &pResult));
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

TEST_F(LinkerTest, RunLinkResourceLazy) {
  CComPtr<IDxcBlob> pResLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_resource2.hlsl", &pResLib);
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_cs_entry.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/true);

  LPCWSTR libResName = L"res";
  RegisterDxcModule(libResName, pResLib, pLinker, /*bLazyLoad*/true);

  Link(L"entry", L"cs_6_0", pLinker, {libResName, libName}, {} ,{});
}

TEST_F(LinkerTest, RunLinkAllProfilesLazy) {
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";

  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_entries2.hlsl", &pEntryLib);
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/true);

  Link(L"vs_main", L"vs_6_0", pLinker, {libName}, {},{});
  Link(L"hs_main", L"hs_6_0", pLinker, {libName}, {},{});
  Link(L"ds_main", L"ds_6_0", pLinker, {libName}, {},{});
  Link(L"gs_main", L"gs_6_0", pLinker, {libName}, {},{});
  Link(L"ps_main", L"ps_6_0", pLinker, {libName}, {},{});

  CComPtr<IDxcBlob> pResLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_resource2.hlsl", &pResLib);

  LPCWSTR libResName = L"res";
  RegisterDxcModule(libResName, pResLib, pLinker, /*bLazyLoad*/true);
  Link(L"cs_main", L"cs_6_0", pLinker, {libName, libResName}, {},{});
}

TEST_F(LinkerTest, RunLinkFailNoDefineLazy) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_cs_entry.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/true);

  LinkCheckMsg(L"entry", L"cs_6_0", pLinker, {libName},
               {"Cannot find definition of function"});
}

TEST_F(LinkerTest, RunLinkFailReDefineLazy) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_cs_entry.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/true);

  LPCWSTR libName2 = L"entry2";
  RegisterDxcModule(libName2, pEntryLib, pLinker, /*bLazyLoad*/true);

  LinkCheckMsg(L"entry", L"cs_6_0", pLinker, {libName, libName2},
               {"Definition already exists for function"});
}

TEST_F(LinkerTest, RunLinkGlobalInitLazy) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_global.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/true);

  Link(L"test", L"ps_6_0", pLinker, {libName},
       // Make sure cbuffer load is generated.
       {"dx.op.cbufferLoad"},{});
}

TEST_F(LinkerTest, RunLinkFailReDefineGlobalLazy) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_global2.hlsl", &pEntryLib);

  CComPtr<IDxcBlob> pLib0;
  CompileLib(L"..\\CodeGenHLSL\\lib_global3.hlsl", &pLib0);

  CComPtr<IDxcBlob> pLib1;
  CompileLib(L"..\\CodeGenHLSL\\lib_global4.hlsl", &pLib1);


  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/true);

  LPCWSTR libName1 = L"lib0";
  RegisterDxcModule(libName1, pLib0, pLinker, /*bLazyLoad*/true);

  LPCWSTR libName2 = L"lib1";
  RegisterDxcModule(libName2, pLib1, pLinker, /*bLazyLoad*/true);

  LinkCheckMsg(L"entry", L"cs_6_0", pLinker, {libName, libName1, libName2},
               {"Definition already exists for global variable", "Resource already exists"});
}

TEST_F(LinkerTest, RunLinkFailProfileMismatchLazy) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_global.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/true);

  LinkCheckMsg(L"test", L"cs_6_0", pLinker, {libName},
               {"Profile mismatch between entry function and target profile"});
}

TEST_F(LinkerTest, RunLinkFailEntryNoPropsLazy) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_global.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/true);

  LinkCheckMsg(L"\01?update@@YAXXZ", L"cs_6_0", pLinker, {libName},
               {"Cannot find function property for entry function"});
}

TEST_F(LinkerTest, RunLinkNoAllocaLazy) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_no_alloca.hlsl", &pEntryLib);
  CComPtr<IDxcBlob> pLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_no_alloca.h", &pLib);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"ps_main";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/true);

  LPCWSTR libName2 = L"test";
  RegisterDxcModule(libName2, pLib, pLinker, /*bLazyLoad*/true);

  Link(L"ps_main", L"ps_6_0", pLinker, {libName, libName2}, {}, {"alloca"});
}

TEST_F(LinkerTest, RunLinkResource) {
  CComPtr<IDxcBlob> pResLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_resource2.hlsl", &pResLib);
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_cs_entry.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);
  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/false);

  LPCWSTR libResName = L"res";
  RegisterDxcModule(libResName, pResLib, pLinker, /*bLazyLoad*/false);

  Link(L"entry", L"cs_6_0", pLinker, {libResName, libName}, {} ,{});
}

TEST_F(LinkerTest, RunLinkAllProfiles) {
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";

  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_entries2.hlsl", &pEntryLib);
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/false);

  Link(L"vs_main", L"vs_6_0", pLinker, {libName}, {},{});
  Link(L"hs_main", L"hs_6_0", pLinker, {libName}, {},{});
  Link(L"ds_main", L"ds_6_0", pLinker, {libName}, {},{});
  Link(L"gs_main", L"gs_6_0", pLinker, {libName}, {},{});
  Link(L"ps_main", L"ps_6_0", pLinker, {libName}, {},{});

  CComPtr<IDxcBlob> pResLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_resource2.hlsl", &pResLib);

  LPCWSTR libResName = L"res";
  RegisterDxcModule(libResName, pResLib, pLinker, /*bLazyLoad*/false);
  Link(L"cs_main", L"cs_6_0", pLinker, {libName, libResName}, {},{});
}

TEST_F(LinkerTest, RunLinkFailNoDefine) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_cs_entry.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/false);

  LinkCheckMsg(L"entry", L"cs_6_0", pLinker, {libName},
               {"Cannot find definition of function"});
}

TEST_F(LinkerTest, RunLinkFailReDefine) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_cs_entry.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/false);

  LPCWSTR libName2 = L"entry2";
  RegisterDxcModule(libName2, pEntryLib, pLinker, /*bLazyLoad*/false);

  LinkCheckMsg(L"entry", L"cs_6_0", pLinker, {libName, libName2},
               {"Definition already exists for function"});
}

TEST_F(LinkerTest, RunLinkGlobalInit) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_global.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/false);

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
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/false);

  LPCWSTR libName1 = L"lib0";
  RegisterDxcModule(libName1, pLib0, pLinker, /*bLazyLoad*/false);

  LPCWSTR libName2 = L"lib1";
  RegisterDxcModule(libName2, pLib1, pLinker, /*bLazyLoad*/false);

  LinkCheckMsg(L"entry", L"cs_6_0", pLinker, {libName, libName1, libName2},
               {"Definition already exists for global variable", "Resource already exists"});
}

TEST_F(LinkerTest, RunLinkFailProfileMismatch) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_global.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/false);

  LinkCheckMsg(L"test", L"cs_6_0", pLinker, {libName},
               {"Profile mismatch between entry function and target profile"});
}

TEST_F(LinkerTest, RunLinkFailEntryNoProps) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_global.hlsl", &pEntryLib);
  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"entry";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/false);

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
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/false);

  LPCWSTR libName2 = L"test";
  RegisterDxcModule(libName2, pLib, pLinker, /*bLazyLoad*/false);

  Link(L"ps_main", L"ps_6_0", pLinker, {libName, libName2}, {}, {"alloca"});
}

TEST_F(LinkerTest, RunLinkNoAllocaMixLazy) {
  CComPtr<IDxcBlob> pEntryLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_no_alloca.hlsl", &pEntryLib);
  CComPtr<IDxcBlob> pLib;
  CompileLib(L"..\\CodeGenHLSL\\lib_no_alloca.h", &pLib);

  CComPtr<IDxcLinker> pLinker;
  CreateLinker(&pLinker);

  LPCWSTR libName = L"ps_main";
  RegisterDxcModule(libName, pEntryLib, pLinker, /*bLazyLoad*/false);

  LPCWSTR libName2 = L"test";
  RegisterDxcModule(libName2, pLib, pLinker, /*bLazyLoad*/true);

  Link(L"ps_main", L"ps_6_0", pLinker, {libName, libName2}, {}, {"alloca"});
}