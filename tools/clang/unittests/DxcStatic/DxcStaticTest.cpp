//===- unittests/DxcStatic/DxcStaticTest.cpp ------------------------------===//
//
// Copyright (C) Microsoft Corporation. All rights reserved.
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Tests for dxcompiler_static. This binary links the static library directly
// and calls DxcCreateInstance(2) without loading dxcompiler dynamically.
//
//===----------------------------------------------------------------------===//

#include "dxc/Support/WinIncludes.h"

#ifdef _WIN32
// The static library does not use dllimport/dllexport.
#define DXC_API_IMPORT
#endif
#include "dxc/dxcapi.h"

#include "gtest/gtest.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

const char PixelShader[] = "float4 main() : SV_Target { return 1; }";

struct CompileResult {
  HRESULT Status = E_FAIL;
  std::string Errors;
  std::vector<uint8_t> Object;
};

CompileResult Compile(IDxcCompiler3 *Compiler, const char *Source,
                      std::vector<LPCWSTR> Args) {
  CompileResult R;
  DxcBuffer Buf = {Source, strlen(Source), DXC_CP_UTF8};
  CComPtr<IDxcResult> Result;
  if (FAILED(Compiler->Compile(&Buf, Args.data(), (UINT32)Args.size(), nullptr,
                               IID_PPV_ARGS(&Result))))
    return R;
  Result->GetStatus(&R.Status);
  CComPtr<IDxcBlobUtf8> Errors;
  if (SUCCEEDED(Result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&Errors),
                                  nullptr)) &&
      Errors)
    R.Errors.assign(Errors->GetStringPointer(), Errors->GetStringLength());
  CComPtr<IDxcBlob> Object;
  if (SUCCEEDED(Result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&Object),
                                  nullptr)) &&
      Object) {
    const uint8_t *P = (const uint8_t *)Object->GetBufferPointer();
    R.Object.assign(P, P + Object->GetBufferSize());
  }
  return R;
}

bool IsDxilContainer(const std::vector<uint8_t> &Obj) {
  return Obj.size() > 4 && memcmp(Obj.data(), "DXBC", 4) == 0;
}

// Must stay first in the file: gtest runs tests in definition order, so this
// exercises concurrent first-time initialization of the static library.
TEST(DxcStaticTest, ConcurrentFirstUse) {
  const int NumThreads = 4;
  std::atomic<int> Successes(0);
  std::vector<std::thread> Threads;
  for (int I = 0; I < NumThreads; ++I) {
    Threads.emplace_back([&Successes]() {
      CComPtr<IDxcCompiler3> Compiler;
      if (FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler))))
        return;
      CompileResult R = Compile(Compiler, PixelShader, {L"-T", L"ps_6_0"});
      if (SUCCEEDED(R.Status) && IsDxilContainer(R.Object))
        ++Successes;
    });
  }
  for (std::thread &T : Threads)
    T.join();
  EXPECT_EQ(NumThreads, Successes.load());
}

TEST(DxcStaticTest, CompileAndDisassembleDxil) {
  CComPtr<IDxcCompiler3> Compiler;
  ASSERT_EQ(S_OK,
            DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)));
  CompileResult R = Compile(Compiler, PixelShader, {L"-T", L"ps_6_0"});
  ASSERT_EQ(S_OK, R.Status) << R.Errors;
  ASSERT_TRUE(IsDxilContainer(R.Object));

  CComPtr<IDxcUtils> Utils;
  ASSERT_EQ(S_OK, DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils)));
  CComPtr<IDxcBlobEncoding> Blob;
  ASSERT_EQ(S_OK, Utils->CreateBlob(R.Object.data(), (UINT32)R.Object.size(),
                                    DXC_CP_ACP, &Blob));
  DxcBuffer Buf = {Blob->GetBufferPointer(), Blob->GetBufferSize(), 0};
  CComPtr<IDxcResult> Disasm;
  ASSERT_EQ(S_OK, Compiler->Disassemble(&Buf, IID_PPV_ARGS(&Disasm)));
  CComPtr<IDxcBlobUtf8> Text;
  ASSERT_EQ(S_OK,
            Disasm->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(&Text), nullptr));
  std::string S(Text->GetStringPointer(), Text->GetStringLength());
  EXPECT_NE(std::string::npos, S.find("define void @main()")) << S;
}

TEST(DxcStaticTest, ValidatorIsAvailable) {
  CComPtr<IDxcValidator> Validator;
  ASSERT_EQ(S_OK,
            DxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&Validator)));
  CComPtr<IDxcCompiler3> Compiler;
  ASSERT_EQ(S_OK,
            DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)));
  CompileResult R =
      Compile(Compiler, PixelShader, {L"-T", L"ps_6_0", L"-Vd"});
  ASSERT_EQ(S_OK, R.Status) << R.Errors;

  CComPtr<IDxcUtils> Utils;
  ASSERT_EQ(S_OK, DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&Utils)));
  CComPtr<IDxcBlobEncoding> Blob;
  ASSERT_EQ(S_OK, Utils->CreateBlob(R.Object.data(), (UINT32)R.Object.size(),
                                    DXC_CP_ACP, &Blob));
  CComPtr<IDxcOperationResult> VResult;
  ASSERT_EQ(S_OK,
            Validator->Validate(Blob, DxcValidatorFlags_InPlaceEdit, &VResult));
  HRESULT VStatus = E_FAIL;
  ASSERT_EQ(S_OK, VResult->GetStatus(&VStatus));
  EXPECT_EQ(S_OK, VStatus);
}

TEST(DxcStaticTest, ReportsErrors) {
  CComPtr<IDxcCompiler3> Compiler;
  ASSERT_EQ(S_OK,
            DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)));
  CompileResult R = Compile(Compiler, "float4 main() : SV_Target { return x; }",
                            {L"-T", L"ps_6_0"});
  EXPECT_TRUE(FAILED(R.Status));
  EXPECT_NE(std::string::npos, R.Errors.find("undeclared identifier 'x'"))
      << R.Errors;
}

#ifdef ENABLE_SPIRV_CODEGEN
TEST(DxcStaticTest, CompileSpirv) {
  CComPtr<IDxcCompiler3> Compiler;
  ASSERT_EQ(S_OK,
            DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&Compiler)));
  CompileResult R =
      Compile(Compiler, PixelShader, {L"-T", L"ps_6_0", L"-spirv"});
  ASSERT_EQ(S_OK, R.Status) << R.Errors;
  ASSERT_GE(R.Object.size(), 4u);
  uint32_t Magic;
  memcpy(&Magic, R.Object.data(), sizeof(Magic));
  EXPECT_EQ(0x07230203u, Magic);
}
#endif // ENABLE_SPIRV_CODEGEN

TEST(DxcStaticTest, NullOutputPointer) {
  EXPECT_EQ(E_POINTER,
            DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler3),
                              nullptr));
  EXPECT_EQ(E_POINTER, DxcCreateInstance2(nullptr, CLSID_DxcCompiler,
                                          __uuidof(IDxcCompiler3), nullptr));
}

TEST(DxcStaticTest, UnknownClass) {
  // An arbitrary CLSID that dxcompiler does not implement.
  const CLSID Bogus = {0x12345678,
                       0x1234,
                       0x1234,
                       {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}};
  IUnknown *Unk = reinterpret_cast<IUnknown *>(1);
  EXPECT_EQ(REGDB_E_CLASSNOTREG,
            DxcCreateInstance(Bogus, __uuidof(IUnknown), (void **)&Unk));
  EXPECT_EQ(nullptr, Unk);
}

// A minimal IMalloc that counts allocations, to check that DxcCreateInstance2
// routes the compiler's IMalloc allocations through the caller's allocator.
class CountingMalloc : public IMalloc {
  std::atomic<ULONG> RefCount{1};
  static const size_t HeaderSize = 16;

public:
  std::atomic<int> Allocs{0};
  std::atomic<int> Frees{0};

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (IsEqualIID(riid, __uuidof(IUnknown)) ||
        IsEqualIID(riid, __uuidof(IMalloc))) {
      AddRef();
      *ppv = static_cast<IMalloc *>(this);
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++RefCount; }
  // Lives on the stack; never deleted through Release.
  ULONG STDMETHODCALLTYPE Release() override { return --RefCount; }

  void *STDMETHODCALLTYPE Alloc(SIZE_T Size) override {
    char *P = (char *)malloc(Size + HeaderSize);
    if (!P)
      return nullptr;
    memcpy(P, &Size, sizeof(Size));
    ++Allocs;
    return P + HeaderSize;
  }
  void *STDMETHODCALLTYPE Realloc(void *Ptr, SIZE_T Size) override {
    if (!Ptr)
      return Alloc(Size);
    if (Size == 0) {
      Free(Ptr);
      return nullptr;
    }
    void *New = Alloc(Size);
    if (New) {
      SIZE_T Old = GetSize(Ptr);
      memcpy(New, Ptr, Old < Size ? Old : Size);
      Free(Ptr);
    }
    return New;
  }
  void STDMETHODCALLTYPE Free(void *Ptr) override {
    if (!Ptr)
      return;
    ++Frees;
    free((char *)Ptr - HeaderSize);
  }
  SIZE_T STDMETHODCALLTYPE GetSize(void *Ptr) override {
    if (!Ptr)
      return (SIZE_T)-1;
    SIZE_T Size;
    memcpy(&Size, (char *)Ptr - HeaderSize, sizeof(Size));
    return Size;
  }
  int STDMETHODCALLTYPE DidAlloc(void *) override { return -1; }
  void STDMETHODCALLTYPE HeapMinimize() override {}
};

TEST(DxcStaticTest, CreateInstance2WithCustomMalloc) {
  CountingMalloc Malloc;
  {
    CComPtr<IDxcCompiler3> Compiler;
    ASSERT_EQ(S_OK, DxcCreateInstance2(&Malloc, CLSID_DxcCompiler,
                                       IID_PPV_ARGS(&Compiler)));
    CompileResult R = Compile(Compiler, PixelShader, {L"-T", L"ps_6_0"});
    ASSERT_EQ(S_OK, R.Status) << R.Errors;
    EXPECT_TRUE(IsDxilContainer(R.Object));
  }
  EXPECT_GT(Malloc.Allocs.load(), 0);
  EXPECT_GT(Malloc.Frees.load(), 0);
}

} // namespace
