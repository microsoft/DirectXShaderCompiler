///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilConvPassRegistryTestHelper.cpp                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"

#include "DxbcConverter.h"
#include "DxilConvPasses/InitializePasses.h"
#include "llvm/PassRegistry.h"

#include <atlbase.h>
#include <d3dcompiler.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

static int VerifyPassRegistration() {
  HRESULT Result = hlsl::SetupRegistryPassForDxilConvPasses();
  if (FAILED(Result))
    return 1;

  llvm::PassRegistry *Registry = llvm::PassRegistry::getPassRegistry();
  const char *RequiredPasses[] = {
      "dce",           "mem2reg",       "assumption-cache-tracker",
      "red",           "loops",         "domtree",
      "dxil-cleanup",  "normalizedxil", "scopenested",
      "scopenestinfo",
  };
  for (const char *PassName : RequiredPasses) {
    if (!Registry->getPassInfo(llvm::StringRef(PassName))) {
      std::fprintf(stderr, "Pass was not registered: %s\n", PassName);
      return 1;
    }
  }
  return 0;
}

static std::wstring GetDxilConvPath() {
  wchar_t Path[MAX_PATH];
  DWORD Length = GetModuleFileNameW(nullptr, Path, _countof(Path));
  if (Length == 0 || Length == _countof(Path))
    return {};

  std::wstring Result(Path, Length);
  size_t Separator = Result.find_last_of(L"\\/");
  if (Separator == std::wstring::npos)
    return {};
  Result.resize(Separator + 1);
  Result += L"dxilconv.dll";
  return Result;
}

static bool CompileShaders(std::vector<CComPtr<ID3DBlob>> &Shaders) {
  for (size_t Index = 0; Index < Shaders.size(); ++Index) {
    std::string Source =
        "float4 main(float4 position : SV_Position) : SV_Target {"
        " return float4(position.x + " +
        std::to_string(Index) + ".0f, position.y, 0.0f, 1.0f); }";
    CComPtr<ID3DBlob> Errors;
    HRESULT Result =
        D3DCompile(Source.data(), Source.size(), "concurrent-init.hlsl",
                   nullptr, nullptr, "main", "ps_5_0",
                   D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &Shaders[Index], &Errors);
    if (FAILED(Result)) {
      if (Errors)
        std::fwrite(Errors->GetBufferPointer(), 1, Errors->GetBufferSize(),
                    stderr);
      return false;
    }
  }
  return true;
}

static int RunConcurrentConversions() {
  constexpr unsigned WorkerCount = 256;
  std::vector<CComPtr<ID3DBlob>> Shaders(WorkerCount);
  if (!CompileShaders(Shaders))
    return 1;

  std::wstring DxilConvPath = GetDxilConvPath();
  if (DxilConvPath.empty())
    return 1;

  HMODULE DxilConv = LoadLibraryW(DxilConvPath.c_str());
  if (!DxilConv)
    return 1;

  auto CreateInstance = reinterpret_cast<DxcCreateInstanceProc>(
      GetProcAddress(DxilConv, "DxcCreateInstance"));
  if (!CreateInstance) {
    FreeLibrary(DxilConv);
    return 1;
  }

  HANDLE ReadyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  HANDLE StartEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!ReadyEvent || !StartEvent) {
    if (ReadyEvent)
      CloseHandle(ReadyEvent);
    if (StartEvent)
      CloseHandle(StartEvent);
    FreeLibrary(DxilConv);
    return 1;
  }

  std::atomic<unsigned> ReadyCount{0};
  std::vector<HRESULT> Results(WorkerCount, E_PENDING);
  std::vector<std::thread> Workers;
  Workers.reserve(WorkerCount);

  for (unsigned Index = 0; Index < WorkerCount; ++Index) {
    Workers.emplace_back([&, Index]() {
      IDxbcConverter *RawConverter = nullptr;
      HRESULT Result =
          CreateInstance(CLSID_DxbcConverter, __uuidof(IDxbcConverter),
                         reinterpret_cast<void **>(&RawConverter));
      CComPtr<IDxbcConverter> Converter;
      Converter.Attach(RawConverter);

      if (ReadyCount.fetch_add(1, std::memory_order_release) + 1 == WorkerCount)
        SetEvent(ReadyEvent);
      WaitForSingleObject(StartEvent, INFINITE);

      if (SUCCEEDED(Result)) {
        void *Dxil = nullptr;
        UINT32 DxilSize = 0;
        LPWSTR Diagnostics = nullptr;
        Result = Converter->Convert(
            Shaders[Index]->GetBufferPointer(),
            static_cast<UINT32>(Shaders[Index]->GetBufferSize()), nullptr,
            &Dxil, &DxilSize, &Diagnostics);
        CoTaskMemFree(Dxil);
        CoTaskMemFree(Diagnostics);
      }
      Results[Index] = Result;
    });
  }

  DWORD ReadyResult = WaitForSingleObject(ReadyEvent, 60000);
  SetEvent(StartEvent);
  for (std::thread &Worker : Workers)
    Worker.join();

  CloseHandle(StartEvent);
  CloseHandle(ReadyEvent);
  FreeLibrary(DxilConv);

  if (ReadyResult != WAIT_OBJECT_0)
    return 1;
  for (HRESULT Result : Results) {
    if (FAILED(Result))
      return 1;
  }
  return 0;
}

int __cdecl wmain(int ArgCount, wchar_t **Arguments) {
  if (ArgCount != 2)
    return 1;
  if (wcscmp(Arguments[1], L"--verify-pass-registration") == 0)
    return VerifyPassRegistration();
  if (wcscmp(Arguments[1], L"--concurrent-conversion") == 0)
    return RunConcurrentConversions();
  return 1;
}
