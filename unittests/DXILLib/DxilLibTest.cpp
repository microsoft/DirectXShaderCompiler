//===- DxilLibTest.cpp -- Test for external dxil lib loading --------===//
//
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Tests the ability to load and make trivial use of a dxil library that is
// accessible to the test executable via the relevant path.
//
//===----------------------------------------------------------------------===//

#include "dxc/Support/WinFunctions.h"
#include "dxc/Support/WinIncludes.h"

#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/microcom.h"
#include "dxc/Test/DxcTestUtils.h"
#include "gtest/gtest.h"

#include <string.h>

// Basic IMalloc class to allow testing DxcCreateInstance2
struct HeapMalloc : public IMalloc {
public:
  ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
  ULONG STDMETHODCALLTYPE Release() override { return 1; }
  STDMETHODIMP QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IMalloc>(this, iid, ppvObject);
  }
  void *STDMETHODCALLTYPE Alloc(SIZE_T cb) override {
    return HeapAlloc(GetProcessHeap(), 0, cb);
  }

  void *STDMETHODCALLTYPE Realloc(void *pv, SIZE_T cb) override {
    return HeapReAlloc(GetProcessHeap(), 0, pv, cb);
  }

  void STDMETHODCALLTYPE Free(void *pv) override {
    HeapFree(GetProcessHeap(), 0, pv);
  }

  SIZE_T STDMETHODCALLTYPE GetSize(void *pv) override {
    return HeapSize(GetProcessHeap(), 0, pv);
  }

  int STDMETHODCALLTYPE DidAlloc(void *pv) override {
    return -1; // don't know
  }

  void STDMETHODCALLTYPE HeapMinimize(void) override {}
};

// Just need to compile something simple to verify things about its output
void CompileSomething(dxc::DxcDllSupport &compilerLib,
                      IDxcOperationResult **outResult) {

  CComPtr<IDxcCompiler> compiler;
  CComPtr<IDxcLibrary> library;
  CComPtr<IDxcOperationResult> result;
  const char *source = "float4 main() : SV_Target { return 1; }";
  ASSERT_TRUE(
      SUCCEEDED(compilerLib.CreateInstance(CLSID_DxcCompiler, &compiler)));
  ASSERT_TRUE(
      SUCCEEDED(compilerLib.CreateInstance(CLSID_DxcLibrary, &library)));

  CComPtr<IDxcBlobEncoding> sourceBlob;
  IFT(library->CreateBlobWithEncodingOnHeapCopy(source, strlen(source), CP_UTF8,
                                                &sourceBlob));

  ASSERT_TRUE(
      SUCCEEDED(compiler->Compile(sourceBlob, L"hlsl.hlsl", L"main", L"ps_6_0",
                                  nullptr, 0, nullptr, 0, nullptr, &result)));
  HRESULT status = S_OK;
  ASSERT_TRUE(SUCCEEDED(result->GetStatus(&status)));
  EXPECT_TRUE(SUCCEEDED(status));
  *outResult = result.Detach();
}

// Check that dxil lib can be loaded by default when creating
// the compiler library. If the external DXIL Library isn't found, a warning is
// issued. Test that there is no such warning.
TEST(DxilLibTest, LoadFromCompiler) {

  // Preloading validator lib so dxcompiler.dll will find it without RPATH's
  // help Only Linux needs this, but we <3 Linux, so we help it out sometimes
  dxc::DxcDllSupport validatorLib;
  ASSERT_TRUE(SUCCEEDED(
      validatorLib.InitializeForDll(dxc::kDxilLib, "DxcCreateInstance")))
      << "Couldn't find " << dxc::kDxilLib;

  dxc::DxcDllSupport compilerLib;
  ASSERT_TRUE(SUCCEEDED(compilerLib.Initialize()));

  // Compile trivial, valid shader and check for warnings
  CComPtr<IDxcOperationResult> result;
  CompileSomething(compilerLib, &result);

  CComPtr<IDxcBlobEncoding> errBuf;
  ASSERT_TRUE(SUCCEEDED(result->GetErrorBuffer(&errBuf)));

  if (errBuf)
    EXPECT_EQ(errBuf->GetBufferSize(), 0U)
      << "Unexpected diagnostics found in compiler error buffer: "
      << (const char *)errBuf->GetBufferPointer();
}

// Check that dxil lib can be loaded and trivially used by a validator object
TEST(DxilLibTest, LoadFromValidator) {
  // Still need a compiler object to give us something to validate

  dxc::DxcDllSupport validatorLib;
  CComPtr<IDxcValidator> validator1;
  CComPtr<IDxcValidator> validator2;
  ASSERT_TRUE(SUCCEEDED(
      validatorLib.InitializeForDll(dxc::kDxilLib, "DxcCreateInstance")))
      << "Couldn't find " << dxc::kDxilLib;
  EXPECT_TRUE(validatorLib.HasCreateWithMalloc());
  ASSERT_TRUE(
      SUCCEEDED(validatorLib.CreateInstance(CLSID_DxcValidator, &validator1)));

  HeapMalloc allocator;
  ASSERT_TRUE(SUCCEEDED(validatorLib.CreateInstance2(
      &allocator, CLSID_DxcValidator, &validator2)));

  // Create a simple compiled output to validate
  dxc::DxcDllSupport compilerLib;
  ASSERT_TRUE(SUCCEEDED(compilerLib.Initialize()));
  CComPtr<IDxcOperationResult> compileResult;
  CompileSomething(compilerLib, &compileResult);
  CComPtr<IDxcBlob> blob;
  ASSERT_TRUE(SUCCEEDED(compileResult->GetResult(&blob)));

  CComPtr<IDxcOperationResult> validationResult1;
  HRESULT status1 = S_OK;
  CComPtr<IDxcBlobEncoding> errBuf1;
  ASSERT_TRUE(SUCCEEDED(validator1->Validate(blob, DxcValidatorFlags_Default,
                                             &validationResult1)));
  ASSERT_TRUE(SUCCEEDED(validationResult1->GetStatus(&status1)));
  EXPECT_TRUE(SUCCEEDED(status1));
  ASSERT_TRUE(SUCCEEDED(validationResult1->GetErrorBuffer(&errBuf1)));
  if (errBuf1)
    EXPECT_EQ(errBuf1->GetBufferSize(), 0U) << "Unexpected diagnostics found in validator1 error buffer: " << (const char *)errBuf1->GetBufferPointer();

  CComPtr<IDxcOperationResult> validationResult2;
  HRESULT status2 = S_OK;
  CComPtr<IDxcBlobEncoding> errBuf2;
  ASSERT_TRUE(SUCCEEDED(validator2->Validate(blob, DxcValidatorFlags_Default,
                                             &validationResult2)));
  ASSERT_TRUE(SUCCEEDED(validationResult2->GetStatus(&status2)));
  EXPECT_TRUE(SUCCEEDED(status2));
  ASSERT_TRUE(SUCCEEDED(validationResult2->GetErrorBuffer(&errBuf2)));
  if (errBuf2)
    EXPECT_EQ(errBuf2->GetBufferSize(), 0U) << "Unexpected diagnostics found in validator2 error buffer: " << (const char *)errBuf2->GetBufferPointer();
}
