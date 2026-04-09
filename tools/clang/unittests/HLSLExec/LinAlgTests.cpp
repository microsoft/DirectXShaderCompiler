///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LinAlgTests.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Execution tests for dx::linalg builtins                                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// We need to keep & fix these warnings to integrate smoothly with HLK
#pragma warning(error : 4100 4242 4244 4267 4701 4389 4018)

#define INLINE_TEST_METHOD_MARKUP
#include <WexTestClass.h>

#include "ShaderOpTest.h"
#include "dxc/DXIL/DxilConstants.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"

#include "HlslExecTestUtils.h"
#include "HlslTestDataTypes.h"
#include "HlslTestUtils.h"

#include <climits>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace LinAlg {

using hlsl::DXIL::ComponentType;
using hlsl::DXIL::LinalgMatrixLayout;
using hlsl::DXIL::MatrixScope;
using hlsl::DXIL::MatrixUse;

using HLSLTestDataTypes::doValuesMatch;
using HLSLTestDataTypes::HLSLHalf_t;
using HLSLTestDataTypes::ValidationType;

using VariantCompType = std::variant<std::vector<float>, std::vector<int32_t>,
                                     std::vector<HLSLHalf_t>>;

/// Return the byte size of a single element for the given component type.
static int elementSize(ComponentType CT) {
  switch (CT) {
  case ComponentType::F16:
  case ComponentType::I16:
  case ComponentType::U16:
    return 2;
  case ComponentType::F64:
  case ComponentType::I64:
  case ComponentType::U64:
    return 8;
  default:
    return 4;
  }
}

struct MatrixParams {
  ComponentType CompType;
  int M;
  int N;
  MatrixUse Use;
  MatrixScope Scope;
  LinalgMatrixLayout Layout;
  int NumThreads;
  bool Enable16Bit;
  bool EmulateTest;

  int strideBytes() const {
    int ES = elementSize(CompType);
    if (Layout == LinalgMatrixLayout::RowMajor)
      return N * ES;
    return M * ES;
  }

  size_t totalElements() const { return static_cast<size_t>(M) * N; }

  size_t totalBytes() const { return totalElements() * elementSize(CompType); }
};

static std::string buildCompilerArgs(const MatrixParams &Params,
                                     const char *ExtraDefines = nullptr) {
  std::stringstream SS;
  SS << "-HV 202x";
  SS << " -DCOMP_TYPE=" << static_cast<int>(Params.CompType);
  SS << " -DM_DIM=" << Params.M;
  SS << " -DN_DIM=" << Params.N;
  SS << " -DUSE=" << static_cast<int>(Params.Use);
  SS << " -DSCOPE=" << static_cast<int>(Params.Scope);
  SS << " -DSTRIDE=" << Params.strideBytes();
  SS << " -DLAYOUT=" << static_cast<int>(Params.Layout);
  SS << " -DELEM_SIZE=" << elementSize(Params.CompType);
  SS << " -DNUMTHREADS=" << Params.NumThreads;
  switch (Params.CompType) {
  case ComponentType::F16:
    SS << " -DELEM_TYPE=half";
    break;
  case ComponentType::F32:
    SS << " -DELEM_TYPE=float";
    break;
  default:
    SS << " -DELEM_TYPE=uint";
    break;
  }
  if (Params.EmulateTest)
    SS << " -DEMULATE_TEST";
  if (Params.Enable16Bit)
    SS << " -enable-16bit-types";
  if (ExtraDefines)
    SS << " " << ExtraDefines;
  return SS.str();
}

static bool verifyFloatBuffer(const float *Actual, const float *Expected,
                              size_t Count, bool Verbose,
                              float Tolerance = 0.0f) {
  bool Success = true;
  for (size_t I = 0; I < Count; I++) {
    if (!doValuesMatch(Actual[I], Expected[I], Tolerance,
                       ValidationType::Epsilon)) {
      hlsl_test::LogErrorFmt(L"Mismatch at index %zu: actual=%f, expected=%f",
                             I, static_cast<double>(Actual[I]),
                             static_cast<double>(Expected[I]));
      Success = false;
    } else if (Verbose) {
      hlsl_test::LogCommentFmt(L"  [%zu] actual=%f, expected=%f (OK)", I,
                               static_cast<double>(Actual[I]),
                               static_cast<double>(Expected[I]));
    }
  }
  return Success;
}

static bool verifyIntBuffer(const int32_t *Actual, const int32_t *Expected,
                            size_t Count, bool Verbose) {
  bool Success = true;
  for (size_t I = 0; I < Count; I++) {
    if (!doValuesMatch(Actual[I], Expected[I], 0.0, ValidationType::Epsilon)) {
      hlsl_test::LogErrorFmt(L"Mismatch at index %zu: actual=%d, expected=%d",
                             I, Actual[I], Expected[I]);
      Success = false;
    } else if (Verbose) {
      hlsl_test::LogCommentFmt(L"  [%zu] actual=%d, expected=%d (OK)", I,
                               Actual[I], Expected[I]);
    }
  }
  return Success;
}

static bool verifyHalfBuffer(const HLSLHalf_t *Actual,
                             const HLSLHalf_t *Expected, size_t Count,
                             bool Verbose, HLSLHalf_t Tolerance = 0.0f) {
  bool Success = true;
  for (size_t I = 0; I < Count; I++) {
    if (!doValuesMatch(Actual[I], Expected[I], Tolerance,
                       ValidationType::Epsilon)) {
      hlsl_test::LogErrorFmt(L"Mismatch at index %zu: actual=%f, expected=%f",
                             I, static_cast<float>(Actual[I]),
                             static_cast<float>(Expected[I]));
      Success = false;
    } else if (Verbose) {
      hlsl_test::LogCommentFmt(L"  [%zu] actual=%f, expected=%f (OK)", I,
                               static_cast<float>(Actual[I]),
                               static_cast<float>(Expected[I]));
    }
  }
  return Success;
}

static bool verifyComponentBuffer(ComponentType CompType, const void *Actual,
                                  VariantCompType Expected, size_t NumElements,
                                  bool Verbose) {
  switch (CompType) {
  case ComponentType::F32: {
    const float *ActualFloats = static_cast<const float *>(Actual);
    return verifyFloatBuffer(ActualFloats,
                             std::get<std::vector<float>>(Expected).data(),
                             NumElements, Verbose);
  }
  case ComponentType::I32: {
    const int32_t *ActualInts = static_cast<const int32_t *>(Actual);
    return verifyIntBuffer(ActualInts,
                           std::get<std::vector<int32_t>>(Expected).data(),
                           NumElements, Verbose);
  }
  case ComponentType::F16: {
    const HLSLHalf_t *ActualHalfs = static_cast<const HLSLHalf_t *>(Actual);
    return verifyHalfBuffer(ActualHalfs,
                            std::get<std::vector<HLSLHalf_t>>(Expected).data(),
                            NumElements, Verbose);
  }
  }
  return false;
}

static bool fillInputBuffer(LPCSTR Name, std::vector<BYTE> &Data,
                            ComponentType CompType, size_t NumElements) {
  if (_stricmp(Name, "Input") != 0)
    return true;

  switch (CompType) {
  case ComponentType::F32: {
    float *Ptr = reinterpret_cast<float *>(Data.data());
    for (size_t I = 0; I < NumElements; I++)
      Ptr[I] = static_cast<float>(I + 1);
    return true;
  }
  case ComponentType::I32: {
    int32_t *Ptr = reinterpret_cast<int32_t *>(Data.data());
    for (size_t I = 0; I < NumElements; I++)
      Ptr[I] = static_cast<int32_t>(I + 1);
    return true;
  }
  case ComponentType::F16: {
    HLSLHalf_t *Ptr = reinterpret_cast<HLSLHalf_t *>(Data.data());
    for (size_t I = 0; I < NumElements; I++)
      Ptr[I] = HLSLHalf_t(static_cast<float>(I + 1));
    return true;
  }
  }

  return false;
}

static VariantCompType makeExpected(ComponentType CompType, int32_t M,
                                    int32_t N, float StartingVal,
                                    bool Increment = true,
                                    bool Transpose = false) {
  const size_t NumElements = M * N;
  std::vector<float> Floats(NumElements);
  std::vector<int32_t> Ints(NumElements);
  std::vector<HLSLHalf_t> Halfs(NumElements);

  for (size_t I = 0; I < M; ++I) {
    for (size_t J = 0; J < N; ++J) {
      size_t Value = I * M + J;
      size_t Idx = Transpose ? J * N + I : Value;
      switch (CompType) {
      case ComponentType::F32:
        Floats[Idx] = StartingVal + static_cast<float>(Increment ? Value : 0);
        break;
      case ComponentType::I32:
        VERIFY_IS_TRUE(StartingVal < static_cast<float>(
                                         std::numeric_limits<int32_t>::max()),
                       "Value too large to cast to int32_t");
        VERIFY_IS_TRUE(StartingVal > static_cast<float>(
                                         std::numeric_limits<int32_t>::min()),
                       "Value too small to cast to int32_t");
        Ints[Idx] = static_cast<int32_t>(StartingVal) +
                    static_cast<int32_t>(Increment ? Value : 0);
        break;
      case ComponentType::F16: {
        // Downcasting is safe here since HLSLHalf_t will clamp if F is too
        // large.
        float F = StartingVal + static_cast<float>(Increment ? Value : 0);
        Halfs[Idx] = HLSLHalf_t(F);
        break;
      }
      default:
        VERIFY_IS_TRUE(false, "Unable to fill unexpected ComponentType");
        break;
      }
    }
  }

  switch (CompType) {
  case ComponentType::F32:
    return Floats;
  case ComponentType::I32:
    return Ints;
  case ComponentType::F16:
    return Halfs;
  default:
    VERIFY_IS_TRUE(false, "Unable to fill unexpected ComponentType");
    return Floats;
  }
}

static void logCompiledButSkipping() {
  hlsl_test::LogCommentFmt(
      L"Shader compiled OK; skipping execution (no SM 6.10 device)");
  WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
}

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

  // Load/Store
  TEST_METHOD(LoadStoreRoundtrip_Wave_16x16_F16);

  // Splat Store
  TEST_METHOD(SplatStore_Wave_16x16_F16);

  // Element access
  TEST_METHOD(ElementAccess_Wave_16x16_F16);

private:
  D3D_SHADER_MODEL createDevice();

  CComPtr<ID3D12Device> D3DDevice;
  dxc::SpecificDllLoader DxcSupport;
  bool VerboseLogging = false;
  bool EmulateTest = false;
  bool Initialized = false;
  bool CompileOnly = false;
  std::optional<D3D12SDKSelector> D3D12SDK;

  WEX::TestExecution::SetVerifyOutput VerifyOutput{
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures};
};

/// Attempts to create a device. If shaders are being emulated then a SM6.8
/// device is attempted. Otherwise a SM6.10 device is attempted
D3D_SHADER_MODEL DxilConf_SM610_LinAlg::createDevice() {
  if (EmulateTest) {
    if (D3D12SDK->createDevice(&D3DDevice, D3D_SHADER_MODEL_6_8, false))
      return D3D_SHADER_MODEL_6_8;

    return D3D_SHADER_MODEL_NONE;
  }

  if (D3D12SDK->createDevice(&D3DDevice, D3D_SHADER_MODEL_6_10, false))
    return D3D_SHADER_MODEL_6_10;

  return D3D_SHADER_MODEL_NONE;
}

bool DxilConf_SM610_LinAlg::setupClass() {
  if (!Initialized) {
    Initialized = true;
    VERIFY_SUCCEEDED(
        DxcSupport.InitializeForDll(dxc::kDxCompilerLib, "DxcCreateInstance"));
    D3D12SDK = D3D12SDKSelector();
    WEX::TestExecution::RuntimeParameters::TryGetValue(L"VerboseLogging",
                                                       VerboseLogging);
    WEX::TestExecution::RuntimeParameters::TryGetValue(L"EmulateTest",
                                                       EmulateTest);
    D3D_SHADER_MODEL SupportedSM = createDevice();

    if (EmulateTest) {
      hlsl_test::LogWarningFmt(L"EmulateTest flag set. Tests are NOT REAL");
      if (SupportedSM != D3D_SHADER_MODEL_6_8) {
        hlsl_test::LogErrorFmt(
            L"Device creation failed. Expected a driver supporting SM6.8");
        return false;
      }
    }

#ifdef _HLK_CONF
    if (SupportedSM != D3D_SHADER_MODEL_6_10) {
      hlsl_test::LogErrorFmt(
          L"Device creation failed. Expected a driver supporting SM6.10");
      return false;
    }
#endif

    CompileOnly = SupportedSM == D3D_SHADER_MODEL_NONE;
  }

  return true;
}

bool DxilConf_SM610_LinAlg::setupMethod() {
  // If the device is healthy, exit otherwise it's possible a previous test
  // case caused a device removal. So we need to try and create a new device.
  if (D3DDevice && D3DDevice->GetDeviceRemovedReason() == S_OK)
    return true;

  // Device is expected to be null. No point in recreating it
  if (CompileOnly)
    return true;

  hlsl_test::LogCommentFmt(L"Device was lost!");
  D3DDevice.Release();

  hlsl_test::LogCommentFmt(L"Recreating device");

  // !CompileOnly implies we expect it to succeeded
  return createDevice() != D3D_SHADER_MODEL_NONE;
}

static const char LoadStoreShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

#ifndef EMULATE_TEST
  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, OFFSET, STRIDE, LAYOUT, 128);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, OFFSET, STRIDE, LAYOUT, 128);
  }
#else
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    for (uint I = 0; I < M_DIM*N_DIM; ++I) {
      Output.Store<ELEM_TYPE>(I*ELEM_SIZE, Input.Load<ELEM_TYPE>(I*ELEM_SIZE));
    }
  }
#endif
)";

static void runLoadStoreRoundtrip(ID3D12Device *Device,
                                  dxc::SpecificDllLoader &DxcSupport,
                                  const MatrixParams &Params, bool Verbose,
                                  bool CompileOnly) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::string Target = "cs_6_10";
  if (Params.EmulateTest)
    Target = "cs_6_8";

  // TODO: these should be varied by test to ensure full coverage
  std::stringstream ExtraDefs;
  ExtraDefs << " -DOFFSET=" << 0;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  // Always verify the shader compiles.
  compileShader(DxcSupport, LoadStoreShader, Target.c_str(), Args, Verbose);

  if (CompileOnly) {
    logCompiledButSkipping();
    return;
  }

  auto Expected = makeExpected(Params.CompType, Params.M, Params.N, 1);

  // Construct the ShaderOp: two UAV buffers, load from one, store to other.
  auto Op = createComputeOp(LoadStoreShader, Target.c_str(), "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", BufferSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Input");
  addRootUAV(Op.get(), 1, "Output");

  auto Result =
      runShaderOp(Device, DxcSupport, std::move(Op),
                  [NumElements, Params](LPCSTR Name, std::vector<BYTE> &Data,
                                        st::ShaderOp *) {
                    VERIFY_IS_TRUE(fillInputBuffer(Name, Data, Params.CompType,
                                                   NumElements),
                                   "Saw unsupported component type");
                  });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::LoadStoreRoundtrip_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  Params.EmulateTest = EmulateTest;
  runLoadStoreRoundtrip(D3DDevice, DxcSupport, Params, VerboseLogging,
                        CompileOnly);
}

static const char SplatStoreShader[] = R"(
  RWByteAddressBuffer Output : register(u0);

#ifndef EMULATE_TEST
  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_FillMatrix(Mat, FILL_VALUE);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, 0, STRIDE, LAYOUT, 128);
  }
#else
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    ELEM_TYPE fill = FILL_VALUE;
    for (uint I = 0; I < M_DIM*N_DIM; ++I) {
      Output.Store<ELEM_TYPE>(I*ELEM_SIZE, fill);
    }
  }
#endif
)";

static void runSplatStore(ID3D12Device *Device,
                          dxc::SpecificDllLoader &DxcSupport,
                          const MatrixParams &Params, float FillValue,
                          bool Verbose, bool CompileOnly) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();
  std::string Target = "cs_6_10";
  if (Params.EmulateTest)
    Target = "cs_6_8";

  std::stringstream ExtraDefs;
  ExtraDefs << "-DFILL_VALUE=" << FillValue;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  // Always verify the shader compiles.
  compileShader(DxcSupport, SplatStoreShader, Target.c_str(), Args, Verbose);

  if (CompileOnly) {
    logCompiledButSkipping();
    return;
  }

  auto Expected =
      makeExpected(Params.CompType, Params.M, Params.N, FillValue, false);

  auto Op = createComputeOp(SplatStoreShader, Target.c_str(), "UAV(u0)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::SplatStore_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  Params.EmulateTest = EmulateTest;
  runSplatStore(D3DDevice, DxcSupport, Params, 42.0f, VerboseLogging,
                CompileOnly);
}

static const char ElementAccessShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  // flatten the 2D index into a 1D index then scale by element size
  // Always store row-major and work it out in the test runner
  uint coordToByteOffset(uint2 coord) {
    return (coord.y * N_DIM + coord.x) * ELEM_SIZE;
  }

#ifndef EMULATE_TEST
  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadIndex : SV_GroupIndex) {
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, 0, STRIDE, LAYOUT, 128);

    // Copy Matrix values from input to output without assuming order
    for (uint I = 0; I < __builtin_LinAlg_MatrixLength(Mat); ++I) {
      uint2 Coord = __builtin_LinAlg_MatrixGetCoordinate(Mat, I);
      uint Offset = coordToByteOffset(Coord);
      ELEM_TYPE Elem;
      __builtin_LinAlg_MatrixGetElement(Elem, Mat, I);
      Output.Store<ELEM_TYPE>(Offset, Elem);
    }

    // Save the matrix length that this thread saw. The length is written
    // to the output right after the matrix, offset by the thread index
    uint LenIdx = (M_DIM * N_DIM * ELEM_SIZE) + (threadIndex * sizeof(uint));
    uint Len = __builtin_LinAlg_MatrixLength(Mat);
    Output.Store<uint>(LenIdx, Len);
  }
#else
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadIndex : SV_GroupIndex) {
    uint LenIdx = (M_DIM * N_DIM * ELEM_SIZE) + (threadIndex * sizeof(uint));
    Output.Store<uint>(LenIdx, M_DIM * N_DIM / NUMTHREADS);

    if (threadIndex != 0)
      return;

    for (uint I = 0; I < M_DIM*N_DIM; ++I) {
      Output.Store<ELEM_TYPE>(I*ELEM_SIZE, Input.Load<ELEM_TYPE>(I*ELEM_SIZE));
    }
  }
#endif
)";

static void runElementAccess(ID3D12Device *Device,
                             dxc::SpecificDllLoader &DxcSupport,
                             const MatrixParams &Params, bool Verbose,
                             bool CompileOnly) {
  const size_t NumElements = Params.totalElements();
  const size_t NumThreads = Params.NumThreads;
  const size_t InputBufSize = Params.totalBytes();
  const size_t ElementSize = elementSize(Params.CompType);

  // Output: ElementSize bytes per element
  //   1 element for each mat idx
  //   1 uint for each thread's length
  const size_t OutputBufSize =
      NumElements * ElementSize + NumThreads * sizeof(uint32_t);

  std::string Target = "cs_6_10";
  if (Params.EmulateTest)
    Target = "cs_6_8";

  std::stringstream ExtraDefs;
  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, ElementAccessShader, Target.c_str(), Args, Verbose);

  if (CompileOnly) {
    logCompiledButSkipping();
    return;
  }

  auto Expected = makeExpected(Params.CompType, Params.M, Params.N, 1);

  auto Op = createComputeOp(ElementAccessShader, Target.c_str(),
                            "UAV(u0), UAV(u1)", Args.c_str());
  addUAVBuffer(Op.get(), "Input", InputBufSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", OutputBufSize, true);
  addRootUAV(Op.get(), 0, "Input");
  addRootUAV(Op.get(), 1, "Output");

  auto Result =
      runShaderOp(Device, DxcSupport, std::move(Op),
                  [NumElements, Params](LPCSTR Name, std::vector<BYTE> &Data,
                                        st::ShaderOp *) {
                    VERIFY_IS_TRUE(fillInputBuffer(Name, Data, Params.CompType,
                                                   NumElements),
                                   "Saw unsupported component type");
                  });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  // Verify the front of the buffer is a list of elements of the expected type
  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumElements, Verbose));

  // Verify the end of the buffer is NumThreads number of lengths, whose
  // sum is greater than or equal to NumElements
  const BYTE *Out = static_cast<const BYTE *>(OutData.data());
  size_t MatrixEndOffset = NumElements * ElementSize;
  const uint32_t *Lengths =
      reinterpret_cast<const uint32_t *>(Out + MatrixEndOffset);
  uint32_t TotalLength = 0;
  for (size_t I = 0; I < NumThreads; ++I)
    TotalLength += Lengths[I];
  VERIFY_IS_GREATER_THAN_OR_EQUAL(
      TotalLength, NumElements, "Sum of all lengths must be gte num elements");
}

void DxilConf_SM610_LinAlg::ElementAccess_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  Params.EmulateTest = EmulateTest;
  runElementAccess(D3DDevice, DxcSupport, Params, VerboseLogging, CompileOnly);
}

} // namespace LinAlg
