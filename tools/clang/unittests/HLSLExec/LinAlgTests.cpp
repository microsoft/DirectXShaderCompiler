///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LinAlgTests.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Execution tests for dx::linalg builtins (Proposal 0035, SM 6.10).        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// We need to keep & fix these warnings to integrate smoothly with HLK
#pragma warning(error : 4100 4242 4244 4267 4701 4389 4018)

#define INLINE_TEST_METHOD_MARKUP
#include <WexTestClass.h>

#include "ShaderOpTest.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"

#include "HlslTestUtils.h"

#include "HlslExecTestUtils.h"

#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace LinAlg {

// ===========================================================================
// DXIL component type constants
// ===========================================================================
enum ComponentType {
  CT_I16 = 2,
  CT_U16 = 3,
  CT_I32 = 4,
  CT_U32 = 5,
  CT_I64 = 6,
  CT_U64 = 7,
  CT_F16 = 8,
  CT_F32 = 9,
  CT_F64 = 10,
};

enum MatrixUse { MU_A = 0, MU_B = 1, MU_Accumulator = 2 };

enum MatrixScope { MS_Thread = 0, MS_Wave = 1, MS_ThreadGroup = 2 };

enum MatrixLayout {
  ML_RowMajor = 0,
  ML_ColMajor = 1,
  ML_MulOptimal = 2,
  ML_OuterProductOptimal = 3,
};

// ===========================================================================
// ShaderOp construction helpers
// ===========================================================================

/// Create a ShaderOp for a compute shader dispatch.
static std::unique_ptr<st::ShaderOp>
createComputeOp(const char *Source, const char *Target, const char *RootSig,
                const char *Args = nullptr, UINT DispatchX = 1,
                UINT DispatchY = 1, UINT DispatchZ = 1) {
  auto Op = std::make_unique<st::ShaderOp>();
  LPCSTR CSName = Op->Strings.insert("CS");
  Op->Name = CSName;
  Op->CS = CSName;
  Op->RootSignature = Op->Strings.insert(RootSig);
  Op->DispatchX = DispatchX;
  Op->DispatchY = DispatchY;
  Op->DispatchZ = DispatchZ;
  Op->UseWarpDevice = true;

  st::ShaderOpShader Shader = {};
  Shader.Name = CSName;
  Shader.Target = Op->Strings.insert(Target);
  Shader.EntryPoint = Op->Strings.insert("main");
  Shader.Text = Op->Strings.insert(Source);
  Shader.Arguments = Args ? Op->Strings.insert(Args) : nullptr;
  Shader.Compiled = FALSE;
  Shader.Callback = FALSE;
  Op->Shaders.push_back(Shader);

  return Op;
}

/// Add a UAV buffer resource to a ShaderOp.
static void addUAVBuffer(st::ShaderOp *Op, const char *Name, UINT64 Width,
                         bool ReadBack, const char *Init = "zero") {
  st::ShaderOpResource Res = {};
  Res.Name = Op->Strings.insert(Name);
  Res.Init = Op->Strings.insert(Init);
  Res.ReadBack = ReadBack ? TRUE : FALSE;

  Res.HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
  Res.HeapFlags = D3D12_HEAP_FLAG_NONE;
  Res.Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  Res.Desc.Width = Width;
  Res.Desc.Height = 1;
  Res.Desc.DepthOrArraySize = 1;
  Res.Desc.MipLevels = 1;
  Res.Desc.SampleDesc.Count = 1;
  Res.Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  Res.Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  Res.InitialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
  Res.TransitionTo = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

  Op->Resources.push_back(Res);
}

/// Bind a resource to a root UAV parameter by index.
static void addRootUAV(st::ShaderOp *Op, UINT Index, const char *ResName) {
  st::ShaderOpRootValue RV = {};
  RV.ResName = Op->Strings.insert(ResName);
  RV.HeapName = nullptr;
  RV.Index = Index;
  Op->RootValues.push_back(RV);
}

/// Run a programmatically-built ShaderOp and return the result.
static std::shared_ptr<st::ShaderOpTestResult>
runShaderOp(ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
            std::unique_ptr<st::ShaderOp> Op,
            st::ShaderOpTest::TInitCallbackFn InitCallback = nullptr) {
  auto OpSet = std::make_shared<st::ShaderOpSet>();
  OpSet->ShaderOps.push_back(std::move(Op));

  return st::RunShaderOpTestAfterParse(
      Device, DxcSupport, nullptr, std::move(InitCallback), std::move(OpSet));
}

// ===========================================================================
// Shader compilation helper
// ===========================================================================

/// Compiles an HLSL shader using the DXC API to verify it is well-formed.
/// This runs without a D3D12 device, so it works even when no SM 6.10
/// hardware is available. Fails the test (via VERIFY) on compile error.

static void compileShader(dxc::SpecificDllLoader &DxcSupport,
                          const char *Source, const char *Target,
                          const std::string &Args) {
  CComPtr<IDxcCompiler3> Compiler;
  VERIFY_SUCCEEDED(DxcSupport.CreateInstance(CLSID_DxcCompiler, &Compiler));

  CComPtr<IDxcUtils> Utils;
  VERIFY_SUCCEEDED(DxcSupport.CreateInstance(CLSID_DxcUtils, &Utils));

  CComPtr<IDxcBlobEncoding> SourceBlob;
  VERIFY_SUCCEEDED(Utils->CreateBlobFromPinned(
      Source, static_cast<UINT32>(strlen(Source)), DXC_CP_UTF8, &SourceBlob));

  // Build wide-string argument list: -T <target> -E main <extra args>.
  std::vector<std::wstring> WArgStorage;
  WArgStorage.push_back(L"-T");
  WArgStorage.push_back(std::wstring(Target, Target + strlen(Target)));
  WArgStorage.push_back(L"-E");
  WArgStorage.push_back(L"main");

  // Tokenize the additional arguments string.
  std::istringstream SS(Args);
  std::string Tok;
  while (SS >> Tok)
    WArgStorage.push_back(std::wstring(Tok.begin(), Tok.end()));

  std::vector<LPCWSTR> WArgPtrs;
  for (const auto &A : WArgStorage)
    WArgPtrs.push_back(A.c_str());

  DxcBuffer Buf = {};
  Buf.Ptr = SourceBlob->GetBufferPointer();
  Buf.Size = SourceBlob->GetBufferSize();
  Buf.Encoding = DXC_CP_UTF8;

  CComPtr<IDxcResult> Result;
  VERIFY_SUCCEEDED(Compiler->Compile(&Buf, WArgPtrs.data(),
                                     static_cast<UINT32>(WArgPtrs.size()),
                                     nullptr, IID_PPV_ARGS(&Result)));

  HRESULT HR;
  VERIFY_SUCCEEDED(Result->GetStatus(&HR));

  if (FAILED(HR)) {
    CComPtr<IDxcBlobUtf8> Errors;
    Result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&Errors), nullptr);
    if (Errors && Errors->GetStringLength() > 0)
      hlsl_test::LogErrorFmt(L"Shader compilation failed:\n%S",
                             Errors->GetStringPointer());
    VERIFY_SUCCEEDED(HR);
  }
}

// ===========================================================================
// Compiler arguments builder
// ===========================================================================

static std::string
buildCompilerArgs(int CompType, int M, int N, int Use, int Scope, int Stride,
                  int Layout, int NumThreads, bool Enable16Bit = false,
                  const char *ExtraDefines = nullptr) {
  std::stringstream SS;
  SS << "-HV 202x";
  SS << " -DCOMP_TYPE=" << CompType;
  SS << " -DM_DIM=" << M;
  SS << " -DN_DIM=" << N;
  SS << " -DUSE=" << Use;
  SS << " -DSCOPE=" << Scope;
  SS << " -DSTRIDE=" << Stride;
  SS << " -DLAYOUT=" << Layout;
  SS << " -DNUMTHREADS=" << NumThreads;
  if (Enable16Bit)
    SS << " -enable-16bit-types";
  if (ExtraDefines)
    SS << " " << ExtraDefines;
  return SS.str();
}

// ===========================================================================
// Verification helpers
// ===========================================================================

static bool verifyFloatBuffer(const void *Actual, const float *Expected,
                              size_t Count, bool Verbose,
                              float Tolerance = 0.0f) {
  const float *ActualFloats = static_cast<const float *>(Actual);
  bool Success = true;
  for (size_t I = 0; I < Count; I++) {
    float Diff = ActualFloats[I] - Expected[I];
    if (Diff < 0)
      Diff = -Diff;
    if (Diff > Tolerance) {
      hlsl_test::LogErrorFmt(L"Mismatch at index %zu: actual=%f, expected=%f",
                             I, static_cast<double>(ActualFloats[I]),
                             static_cast<double>(Expected[I]));
      Success = false;
    } else if (Verbose) {
      hlsl_test::LogCommentFmt(L"  [%zu] actual=%f, expected=%f (OK)", I,
                               static_cast<double>(ActualFloats[I]),
                               static_cast<double>(Expected[I]));
    }
  }
  return Success;
}

static bool verifyIntBuffer(const void *Actual, const int32_t *Expected,
                            size_t Count, bool Verbose) {
  const int32_t *ActualInts = static_cast<const int32_t *>(Actual);
  bool Success = true;
  for (size_t I = 0; I < Count; I++) {
    if (ActualInts[I] != Expected[I]) {
      hlsl_test::LogErrorFmt(L"Mismatch at index %zu: actual=%d, expected=%d",
                             I, ActualInts[I], Expected[I]);
      Success = false;
    } else if (Verbose) {
      hlsl_test::LogCommentFmt(L"  [%zu] actual=%d, expected=%d (OK)", I,
                               ActualInts[I], Expected[I]);
    }
  }
  return Success;
}

// ===========================================================================
// Test parameters
// ===========================================================================

struct MatrixParams {
  int CompType;
  int M;
  int N;
  int Use;
  int Scope;
  int Layout;
  int NumThreads;
  bool Enable16Bit;

  int strideBytes() const {
    int ElemSize = 4; // default F32/I32
    if (CompType == CT_F16 || CompType == CT_I16 || CompType == CT_U16)
      ElemSize = 2;
    else if (CompType == CT_F64 || CompType == CT_I64 || CompType == CT_U64)
      ElemSize = 8;
    if (Layout == ML_RowMajor)
      return N * ElemSize;
    else
      return M * ElemSize;
  }

  size_t totalElements() const { return static_cast<size_t>(M) * N; }

  size_t totalBytes() const {
    int ElemSize = 4;
    if (CompType == CT_F16 || CompType == CT_I16 || CompType == CT_U16)
      ElemSize = 2;
    else if (CompType == CT_F64 || CompType == CT_I64 || CompType == CT_U64)
      ElemSize = 8;
    return totalElements() * ElemSize;
  }
};

// ===========================================================================
// Test class
// ===========================================================================

class DxilConf_SM610_LinAlg {
public:
  BEGIN_TEST_CLASS(DxilConf_SM610_LinAlg)
  TEST_CLASS_PROPERTY("Kits.TestName",
                      "D3D12 - Shader Model 6.10 - LinAlg Matrix Operations")
  TEST_CLASS_PROPERTY("Kits.TestId", "a1b2c3d4-e5f6-7890-abcd-ef1234567890")
  TEST_CLASS_PROPERTY(
      "Kits.Description",
      "Validates SM 6.10 linear algebra matrix operations execute correctly")
  TEST_CLASS_PROPERTY(
      "Kits.Specification",
      "Device.Graphics.D3D12.DXILCore.ShaderModel610.CoreRequirement")
  TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(setupClass);
  TEST_METHOD_SETUP(setupMethod);

  TEST_METHOD(LoadStoreRoundtrip_Wave_F32);
  TEST_METHOD(LoadStoreRoundtrip_Wave_I32);
  TEST_METHOD(SplatStore_Wave_F32);
  TEST_METHOD(SplatStore_Wave_I32);

private:
  CComPtr<ID3D12Device> D3DDevice;
  dxc::SpecificDllLoader DxcSupport;
  bool VerboseLogging = false;
  bool Initialized = false;
  std::optional<D3D12SDKSelector> D3D12SDK;
};

// ===========================================================================
// Class setup
// ===========================================================================

bool DxilConf_SM610_LinAlg::setupClass() {
  WEX::TestExecution::SetVerifyOutput VerifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  if (!Initialized) {
    Initialized = true;

    VERIFY_SUCCEEDED(
        DxcSupport.InitializeForDll(dxc::kDxCompilerLib, "DxcCreateInstance"));

    D3D12SDK = D3D12SDKSelector();

    WEX::TestExecution::RuntimeParameters::TryGetValue(L"VerboseLogging",
                                                       VerboseLogging);

    bool FailIfRequirementsNotMet = false;
#ifdef _HLK_CONF
    FailIfRequirementsNotMet = true;
#endif
    WEX::TestExecution::RuntimeParameters::TryGetValue(
        L"FailIfRequirementsNotMet", FailIfRequirementsNotMet);

    // Try to create a device. In HLK mode, fail if unavailable.
    // In dev mode, D3DDevice stays null and tests will compile shaders
    // then skip GPU execution.
    if (!D3D12SDK->createDevice(&D3DDevice, D3D_SHADER_MODEL_6_10,
                                /*SkipUnsupported=*/false)) {
      if (FailIfRequirementsNotMet) {
        hlsl_test::LogErrorFmt(
            L"Device creation failed for SM 6.10, and "
            L"FailIfRequirementsNotMet is set.");
        return false;
      }
      // No device — tests will compile shaders and skip execution.
    }
  }

  return true;
}

bool DxilConf_SM610_LinAlg::setupMethod() {
  // Re-create device if it was lost. If we never had one, that's fine —
  // tests compile shaders and skip GPU execution.
  if (D3DDevice && D3DDevice->GetDeviceRemovedReason() != S_OK) {
    hlsl_test::LogCommentFmt(L"Device was lost!");
    D3DDevice.Release();
    D3D12SDK->createDevice(&D3DDevice, D3D_SHADER_MODEL_6_10,
                           /*SkipUnsupported=*/false);
  }
  return true;
}

// ===========================================================================
// Load/Store roundtrip
// ===========================================================================

static const char LoadStoreShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, 0, STRIDE, LAYOUT);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, 0, STRIDE, LAYOUT);
  }
)";

static void runLoadStoreRoundtrip(ID3D12Device *Device,
                                  dxc::SpecificDllLoader &DxcSupport,
                                  const MatrixParams &Params, bool Verbose) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();
  const int Stride = Params.strideBytes();

  std::string Args =
      buildCompilerArgs(Params.CompType, Params.M, Params.N, Params.Use,
                        Params.Scope, Stride, Params.Layout,
                        Params.NumThreads, Params.Enable16Bit);

  // Always verify the shader compiles.
  compileShader(DxcSupport, LoadStoreShader, "cs_6_10", Args);

  // Skip GPU execution if no device.
  if (!Device) {
    hlsl_test::LogCommentFmt(
        L"Shader compiled OK; skipping execution (no SM 6.10 device)");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
  }

  // Build expected data.
  std::vector<float> ExpectedFloats(NumElements);
  std::vector<int32_t> ExpectedInts(NumElements);
  for (size_t I = 0; I < NumElements; I++) {
    ExpectedFloats[I] = static_cast<float>(I + 1);
    ExpectedInts[I] = static_cast<int32_t>(I + 1);
  }

  // Construct the ShaderOp: two UAV buffers, load from one, store to other.
  auto Op = createComputeOp(LoadStoreShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", BufferSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Input");
  addRootUAV(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp * /*pOp*/) {
        if (_stricmp(Name, "Input") != 0)
          return;
        if (Params.CompType == CT_F32) {
          float *Ptr = reinterpret_cast<float *>(Data.data());
          for (size_t I = 0; I < NumElements; I++)
            Ptr[I] = static_cast<float>(I + 1);
        } else if (Params.CompType == CT_I32) {
          int32_t *Ptr = reinterpret_cast<int32_t *>(Data.data());
          for (size_t I = 0; I < NumElements; I++)
            Ptr[I] = static_cast<int32_t>(I + 1);
        }
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  if (Params.CompType == CT_F32) {
    VERIFY_IS_TRUE(verifyFloatBuffer(OutData.data(), ExpectedFloats.data(),
                                     NumElements, Verbose));
  } else if (Params.CompType == CT_I32) {
    VERIFY_IS_TRUE(verifyIntBuffer(OutData.data(), ExpectedInts.data(),
                                   NumElements, Verbose));
  }
}

void DxilConf_SM610_LinAlg::LoadStoreRoundtrip_Wave_F32() {
  MatrixParams Params = {};
  Params.CompType = CT_F32;
  Params.M = 8;
  Params.N = 8;
  Params.Use = MU_A;
  Params.Scope = MS_Wave;
  Params.Layout = ML_RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;
  runLoadStoreRoundtrip(D3DDevice, DxcSupport, Params, VerboseLogging);
}

void DxilConf_SM610_LinAlg::LoadStoreRoundtrip_Wave_I32() {
  MatrixParams Params = {};
  Params.CompType = CT_I32;
  Params.M = 8;
  Params.N = 8;
  Params.Use = MU_A;
  Params.Scope = MS_Wave;
  Params.Layout = ML_RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;
  runLoadStoreRoundtrip(D3DDevice, DxcSupport, Params, VerboseLogging);
}

// ===========================================================================
// Splat + Store
// ===========================================================================

static const char SplatStoreShader[] = R"(
  RWByteAddressBuffer Output : register(u0);

  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_FillMatrix(Mat, FILL_VALUE);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, 0, STRIDE, LAYOUT);
  }
)";

static void runSplatStore(ID3D12Device *Device,
                          dxc::SpecificDllLoader &DxcSupport,
                          const MatrixParams &Params, float FillValue,
                          bool Verbose) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();
  const int Stride = Params.strideBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << "-DFILL_VALUE=" << FillValue;

  std::string Args = buildCompilerArgs(
      Params.CompType, Params.M, Params.N, Params.Use, Params.Scope, Stride,
      Params.Layout, Params.NumThreads, Params.Enable16Bit,
      ExtraDefs.str().c_str());

  // Always verify the shader compiles.
  compileShader(DxcSupport, SplatStoreShader, "cs_6_10", Args);

  // Skip GPU execution if no device.
  if (!Device) {
    hlsl_test::LogCommentFmt(
        L"Shader compiled OK; skipping execution (no SM 6.10 device)");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
  }

  std::vector<float> ExpectedFloats(NumElements, FillValue);
  std::vector<int32_t> ExpectedInts(NumElements,
                                    static_cast<int32_t>(FillValue));

  auto Op =
      createComputeOp(SplatStoreShader, "cs_6_10", "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  if (Params.CompType == CT_F32) {
    VERIFY_IS_TRUE(verifyFloatBuffer(OutData.data(), ExpectedFloats.data(),
                                     NumElements, Verbose));
  } else if (Params.CompType == CT_I32) {
    VERIFY_IS_TRUE(verifyIntBuffer(OutData.data(), ExpectedInts.data(),
                                   NumElements, Verbose));
  }
}

void DxilConf_SM610_LinAlg::SplatStore_Wave_F32() {
  MatrixParams Params = {};
  Params.CompType = CT_F32;
  Params.M = 8;
  Params.N = 8;
  Params.Use = MU_Accumulator;
  Params.Scope = MS_Wave;
  Params.Layout = ML_RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;
  runSplatStore(D3DDevice, DxcSupport, Params, 42.0f, VerboseLogging);
}

void DxilConf_SM610_LinAlg::SplatStore_Wave_I32() {
  MatrixParams Params = {};
  Params.CompType = CT_I32;
  Params.M = 8;
  Params.N = 8;
  Params.Use = MU_Accumulator;
  Params.Scope = MS_Wave;
  Params.Layout = ML_RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;
  runSplatStore(D3DDevice, DxcSupport, Params, 7.0f, VerboseLogging);
}

} // namespace LinAlg
