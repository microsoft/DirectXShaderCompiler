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
using MatrixDim = uint32_t;

/// Return the byte size of a single element for the given component type.
static uint8_t elementSize(ComponentType CT) {
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
  MatrixDim M;
  MatrixDim N;
  MatrixUse Use;
  MatrixScope Scope;
  LinalgMatrixLayout Layout;
  int NumThreads;
  bool Enable16Bit;
  bool EmulateTest;

  size_t strideBytes() const {
    uint32_t ES = elementSize(CompType);
    if (Layout == LinalgMatrixLayout::RowMajor)
      return N * ES;
    return M * ES;
  }

  size_t totalElements() const { return M * N; }

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
  SS << " -DELEM_SIZE=" << static_cast<int>(elementSize(Params.CompType));
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
                            ComponentType CompType, size_t NumElements,
                            size_t StartingVal = 1, bool Increment = true) {
  if (_stricmp(Name, "Input") != 0)
    return true;

  switch (CompType) {
  case ComponentType::F32:
  case ComponentType::I32:
  case ComponentType::F16:
    break;
  default:
    return false;
  }

  for (size_t I = 0; I < NumElements; ++I) {
    size_t Value = StartingVal + (Increment ? I : 0);
    switch (CompType) {
    case ComponentType::F32: {
      float *Ptr = reinterpret_cast<float *>(Data.data());
      Ptr[I] = static_cast<float>(Value);
      break;
    }
    case ComponentType::I32: {
      int32_t *Ptr = reinterpret_cast<int32_t *>(Data.data());
      Ptr[I] = static_cast<int32_t>(Value);
      break;
    }
    case ComponentType::F16: {
      HLSLHalf_t *Ptr = reinterpret_cast<HLSLHalf_t *>(Data.data());
      Ptr[I] = HLSLHalf_t(static_cast<float>(Value));
      break;
    }
    }
  }

  return true;
}

static VariantCompType makeExpectedMat(ComponentType CompType, MatrixDim M,
                                       MatrixDim N, float StartingVal,
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

static VariantCompType makeExpectedVec(ComponentType CompType,
                                       MatrixDim NumElements, float StartingVal,
                                       bool Increment = true) {
  return makeExpectedMat(CompType, 1, NumElements, StartingVal, Increment,
                         false);
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

  // Load/Store/Accumulate Descriptor
  TEST_METHOD(LoadStoreDescriptor_Wave_16x16_F16);
  TEST_METHOD(SplatStore_Wave_16x16_F16);
  TEST_METHOD(AccumulateDescriptor_Wave_16x16_F16);
  TEST_METHOD(AccumulateDescriptor_Thread_16x16_F16);

  // Load/Store/Accumulate Memory
  TEST_METHOD(LoadMemory_Wave_16x16_F16);
  TEST_METHOD(StoreMemory_Wave_16x16_F16);
  TEST_METHOD(AccumulateMemory_Wave_16x16_F16);

  // Element access
  TEST_METHOD(ElementAccess_Wave_16x16_F16);
  TEST_METHOD(ElementSet_Wave_16x16_F16);

  // Cast/Convert
  TEST_METHOD(CopyConvert_Wave_16x16_F16);
  TEST_METHOD(CopyConvert_Wave_16x16_F16_Transpose);

  // Matrix Matrix Arithmetic
  TEST_METHOD(MatMatMul_Wave_16x16x16_F16);
  TEST_METHOD(MatMatMulAccum_Wave_16x16x16_F16);
  TEST_METHOD(MatAccum_Wave_16x16_F16);

  // Matrix Vector Arithmetic
  TEST_METHOD(MatVecMul_Thread_16x16_F16);
  TEST_METHOD(MatVecMulAdd_Thread_16x16_F16);
  TEST_METHOD(OuterProduct_Thread_16x16_F16);

  // Query Accumulator Layout
  TEST_METHOD(QueryAccumLayout);

  // Convert
  TEST_METHOD(Convert);

private:
  CComPtr<ID3D12Device> D3DDevice;
  dxc::SpecificDllLoader DxcSupport;
  bool VerboseLogging = false;
  bool Initialized = false;
  std::optional<D3D12SDKSelector> D3D12SDK;

  WEX::TestExecution::SetVerifyOutput VerifyOutput{
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures};
};

bool DxilConf_SM610_LinAlg::setupClass() {
  if (!Initialized) {
    Initialized = true;
    VERIFY_SUCCEEDED(
        DxcSupport.InitializeForDll(dxc::kDxCompilerLib, "DxcCreateInstance"));
    D3D12SDK = D3D12SDKSelector();
    WEX::TestExecution::RuntimeParameters::TryGetValue(L"VerboseLogging",
                                                       VerboseLogging);

    if (!D3D12SDK->createDevice(&D3DDevice, D3D_SHADER_MODEL_6_10, false)) {
#ifdef _HLK_CONF
      hlsl_test::LogErrorFmt(
          L"Device creation failed. Expected a driver supporting SM6.10");
#else
      hlsl_test::LogWarningFmt(
          L"Device creation failed. Expected a driver supporting SM6.10");
      WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
#endif
      return false;
    }
  }

  return true;
}

bool DxilConf_SM610_LinAlg::setupMethod() {
  // If the device is healthy, exit otherwise it's possible a previous test
  // case caused a device removal. So we need to try and create a new device.
  if (D3DDevice && D3DDevice->GetDeviceRemovedReason() == S_OK)
    return true;

  hlsl_test::LogCommentFmt(L"Device was lost!");
  D3DDevice.Release();

  hlsl_test::LogCommentFmt(L"Recreating device");

  return D3D12SDK->createDevice(&D3DDevice, D3D_SHADER_MODEL_6_10, false);
}

static const char LoadStoreDescriptorShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (WaveReadLaneFirst(threadID) != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, OFFSET, STRIDE, LAYOUT, 128);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, OFFSET, STRIDE, LAYOUT, 128);
  }
)";

static void runLoadStoreDescriptor(ID3D12Device *Device,
                                   dxc::SpecificDllLoader &DxcSupport,
                                   const MatrixParams &Params, bool Verbose) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  // TODO: these should be varied by test to ensure full coverage
  std::stringstream ExtraDefs;
  ExtraDefs << " -DOFFSET=" << 0;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, LoadStoreDescriptorShader, "cs_6_10", Args,
                Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, 1);

  // Construct the ShaderOp: two UAV buffers, load from one, store to other.
  auto Op = createComputeOp(LoadStoreDescriptorShader, "cs_6_10",
                            "UAV(u0), UAV(u1)", Args.c_str());
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

void DxilConf_SM610_LinAlg::LoadStoreDescriptor_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runLoadStoreDescriptor(D3DDevice, DxcSupport, Params, VerboseLogging);
}

static const char SplatStoreShader[] = R"(
  RWByteAddressBuffer Output : register(u0);

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (WaveReadLaneFirst(threadID) != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_FillMatrix(Mat, FILL_VALUE);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, 0, STRIDE, LAYOUT, 128);
  }
)";

static void runSplatStore(ID3D12Device *Device,
                          dxc::SpecificDllLoader &DxcSupport,
                          const MatrixParams &Params, float FillValue,
                          bool Verbose) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << "-DFILL_VALUE=" << FillValue;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, SplatStoreShader, "cs_6_10", Args, Verbose);

  auto Expected =
      makeExpectedMat(Params.CompType, Params.M, Params.N, FillValue, false);

  auto Op =
      createComputeOp(SplatStoreShader, "cs_6_10", "UAV(u0)", Args.c_str());
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
  runSplatStore(D3DDevice, DxcSupport, Params, 42.0f, VerboseLogging);
}

static const char AccumulateDescriptorShader[] = R"(
  #define USE_ACC 2

  ByteAddressBuffer Input : register(t0);
  RWByteAddressBuffer Output : register(u1);

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (WaveReadLaneFirst(threadID) != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, 0, STRIDE, LAYOUT, 128);
    __builtin_LinAlg_MatrixAccumulateToDescriptor(
      Mat, Output, 0, STRIDE, LAYOUT, 128);
    __builtin_LinAlg_MatrixAccumulateToDescriptor(
      Mat, Output, 0, STRIDE, LAYOUT, 128);
  }
)";

static void runAccumulateDescriptor(ID3D12Device *Device,
                                    dxc::SpecificDllLoader &DxcSupport,
                                    const MatrixParams &Params, int FillValue,
                                    bool Verbose) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::string Args = buildCompilerArgs(Params);

  compileShader(DxcSupport, AccumulateDescriptorShader, "cs_6_10", Args,
                Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N,
                                  static_cast<float>(FillValue) * 2, false);

  auto Op = createComputeOp(AccumulateDescriptorShader, "cs_6_10",
                            "SRV(t0), UAV(u1)", Args.c_str());
  addUAVBuffer(Op.get(), "Input", BufferSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Input");
  addRootUAV(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [NumElements, Params, FillValue](LPCSTR Name, std::vector<BYTE> &Data,
                                       st::ShaderOp *) {
        VERIFY_IS_TRUE(fillInputBuffer(Name, Data, Params.CompType, NumElements,
                                       /*StartingVal=*/FillValue,
                                       /*Increment=*/false),
                       "Saw unsupported component type");
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::AccumulateDescriptor_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runAccumulateDescriptor(D3DDevice, DxcSupport, Params, 12, VerboseLogging);
}

void DxilConf_SM610_LinAlg::AccumulateDescriptor_Thread_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Thread;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 1;
  Params.Enable16Bit = true;
  runAccumulateDescriptor(D3DDevice, DxcSupport, Params, 19, VerboseLogging);
}

static const char ElementAccessShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  // flatten the 2D index into a 1D index then scale by element size
  // Always store row-major and work it out in the test runner
  uint coordToByteOffset(uint2 coord) {
    return (coord.y * N_DIM + coord.x) * ELEM_SIZE;
  }

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (WaveReadLaneFirst(threadID) != 0)
      return;

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
    uint LenIdx = (M_DIM * N_DIM * ELEM_SIZE) + (threadID * sizeof(uint));
    uint Len = __builtin_LinAlg_MatrixLength(Mat);
    Output.Store<uint>(LenIdx, Len);
  }
)";

static void runElementAccess(ID3D12Device *Device,
                             dxc::SpecificDllLoader &DxcSupport,
                             const MatrixParams &Params, bool Verbose) {
  const size_t NumElements = Params.totalElements();
  const size_t NumThreads = Params.NumThreads;
  const size_t MatrixSize = Params.totalBytes();
  // OutputBuf needs to fit the Matrix plus one uint per thread
  const size_t OutputBufSize = MatrixSize + NumThreads * sizeof(uint32_t);

  std::stringstream ExtraDefs;
  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, ElementAccessShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, 1);

  auto Op = createComputeOp(ElementAccessShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", MatrixSize, false, "byname");
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
  const uint32_t *Lengths =
      reinterpret_cast<const uint32_t *>(Out + MatrixSize);
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
  runElementAccess(D3DDevice, DxcSupport, Params, VerboseLogging);
}

static const char ElementSetShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (WaveReadLaneFirst(threadID) != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, 0, STRIDE, LAYOUT, 128);

    // Increment every element by 5
    for (uint I = 0; I < __builtin_LinAlg_MatrixLength(Mat); ++I) {
      ELEM_TYPE Elem;
      __builtin_LinAlg_MatrixGetElement(Elem, Mat, I);
      Elem = Elem + 5;
      __builtin_LinAlg_MatrixSetElement(Mat, Mat, I, Elem);
    }

    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, 0, STRIDE, LAYOUT, 128);
  }
)";

static void runElementSet(ID3D12Device *Device,
                          dxc::SpecificDllLoader &DxcSupport,
                          const MatrixParams &Params, bool Verbose) {
  const size_t NumElements = Params.totalElements();
  const size_t MatrixSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, ElementSetShader, "cs_6_10", Args, Verbose);

  // Start counting from 6 since each element was increased by 5
  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, 6);

  auto Op = createComputeOp(ElementSetShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", MatrixSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", MatrixSize, true);
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
}

void DxilConf_SM610_LinAlg::ElementSet_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runElementSet(D3DDevice, DxcSupport, Params, VerboseLogging);
}

static const char CopyConvertShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (WaveReadLaneFirst(threadID) != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Src;
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, N_DIM, M_DIM, USE, SCOPE)]]
      Dst;

    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Src, Input, 0, STRIDE, LAYOUT, 128);
    __builtin_LinAlg_CopyConvertMatrix(Dst, Src, TRANSPOSE);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Dst, Output, 0, STRIDE, LAYOUT, 128);
  }
)";

static void runCopyConvert(ID3D12Device *Device,
                           dxc::SpecificDllLoader &DxcSupport,
                           const MatrixParams &Params, bool Verbose,
                           bool Transpose) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DTRANSPOSE=" << Transpose;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, CopyConvertShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, 1,
                                  /*Increment=*/true, Transpose);

  // Construct the ShaderOp: two UAV buffers, load from one, store to other.
  auto Op = createComputeOp(CopyConvertShader, "cs_6_10", "UAV(u0), UAV(u1)",
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

void DxilConf_SM610_LinAlg::CopyConvert_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runCopyConvert(D3DDevice, DxcSupport, Params, VerboseLogging,
                 /*Transpose=*/false);
}

void DxilConf_SM610_LinAlg::CopyConvert_Wave_16x16_F16_Transpose() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runCopyConvert(D3DDevice, DxcSupport, Params, VerboseLogging,
                 /*Transpose=*/true);
}

static const char MatMatMulShader[] = R"(
  #define USE_A 0
  #define USE_B 1
  #define USE_ACC 2

  RWByteAddressBuffer Output : register(u0);

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (WaveReadLaneFirst(threadID) != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, K_DIM, USE_A, SCOPE)]]
      MatA;
    __builtin_LinAlg_FillMatrix(MatA, A_FILL);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, K_DIM, N_DIM, USE_B, SCOPE)]]
      MatB;
    __builtin_LinAlg_FillMatrix(MatB, B_FILL);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE)]]
      MatC;
    __builtin_LinAlg_MatrixMatrixMultiply(MatC, MatA, MatB);

    __builtin_LinAlg_MatrixStoreToDescriptor(
      MatC, Output, 0, STRIDE, LAYOUT, 128);
  }
)";

static void runMatMatMul(ID3D12Device *Device,
                         dxc::SpecificDllLoader &DxcSupport,
                         const MatrixParams &Params, bool Verbose, MatrixDim K,
                         float AFill, float BFill) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DK_DIM=" << K;
  ExtraDefs << " -DA_FILL=" << AFill;
  ExtraDefs << " -DB_FILL=" << BFill;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, MatMatMulShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N,
                                  AFill * BFill * K, /*Increment=*/false);

  auto Op =
      createComputeOp(MatMatMulShader, "cs_6_10", "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::MatMatMul_Wave_16x16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runMatMatMul(D3DDevice, DxcSupport, Params, VerboseLogging, /*K=*/16,
               /*AFill=*/2.0f, /*BFill=*/3.0f);
}

static const char MatMatMulAccumShader[] = R"(
  #define USE_A 0
  #define USE_B 1
  #define USE_ACC 2

  RWByteAddressBuffer Output : register(u0);

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (WaveReadLaneFirst(threadID) != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, K_DIM, USE_A, SCOPE)]]
      MatA;
    __builtin_LinAlg_FillMatrix(MatA, A_FILL);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, K_DIM, N_DIM, USE_B, SCOPE)]]
      MatB;
    __builtin_LinAlg_FillMatrix(MatB, B_FILL);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE)]]
      MatC;
    __builtin_LinAlg_FillMatrix(MatC, C_FILL);

    __builtin_LinAlg_MatrixMatrixMultiplyAccumulate(MatC, MatA, MatB, MatC);

    __builtin_LinAlg_MatrixStoreToDescriptor(
      MatC, Output, 0, STRIDE, LAYOUT, 128);
  }
)";

static void runMatMatMulAccum(ID3D12Device *Device,
                              dxc::SpecificDllLoader &DxcSupport,
                              const MatrixParams &Params, bool Verbose,
                              MatrixDim K, float AFill, float BFill,
                              float CFill) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DK_DIM=" << K;
  ExtraDefs << " -DA_FILL=" << AFill;
  ExtraDefs << " -DB_FILL=" << BFill;
  ExtraDefs << " -DC_FILL=" << CFill;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, MatMatMulAccumShader, "cs_6_10", Args, Verbose);

  auto Expected =
      makeExpectedMat(Params.CompType, Params.M, Params.N,
                      AFill * BFill * K + CFill, /*Increment=*/false);

  auto Op =
      createComputeOp(MatMatMulAccumShader, "cs_6_10", "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::MatMatMulAccum_Wave_16x16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runMatMatMulAccum(D3DDevice, DxcSupport, Params, VerboseLogging, /*K=*/16,
                    /*AFill=*/2.0f, /*BFill=*/3.0f, /*CFill=*/4.0f);
}

static const char MatAccumShader[] = R"(
  #define USE_A 0
  #define USE_ACC 2

  RWByteAddressBuffer Output : register(u0);

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (WaveReadLaneFirst(threadID) != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE)]]
      MatLHS;
    __builtin_LinAlg_FillMatrix(MatLHS, LHS_FILL);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE_A, SCOPE)]]
      MatRHS;
    __builtin_LinAlg_FillMatrix(MatRHS, RHS_FILL);

    __builtin_LinAlg_MatrixAccumulate(MatLHS, MatLHS, MatRHS);

    __builtin_LinAlg_MatrixStoreToDescriptor(
      MatLHS, Output, 0, STRIDE, LAYOUT, 128);
  }
)";

static void runMatAccum(ID3D12Device *Device,
                        dxc::SpecificDllLoader &DxcSupport,
                        const MatrixParams &Params, bool Verbose, float LHSFill,
                        float RHSFill) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DLHS_FILL=" << LHSFill;
  ExtraDefs << " -DRHS_FILL=" << RHSFill;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, MatAccumShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N,
                                  LHSFill + RHSFill, /*Increment=*/false);

  auto Op = createComputeOp(MatAccumShader, "cs_6_10", "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::MatAccum_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runMatAccum(D3DDevice, DxcSupport, Params, VerboseLogging,
              /*LHSFill=*/2.0f, /*RHSFill=*/3.0f);
}

static const char MatVecMulShader[] = R"(
  #define USE_A 0
  #define SCOPE_THREAD 0

  ByteAddressBuffer Input : register(t0);
  RWByteAddressBuffer Output : register(u1);

  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE_A, SCOPE_THREAD)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, 0, STRIDE, LAYOUT, 128);

    vector<ELEM_TYPE, M_DIM> InVec;
    for (uint I = 0; I < M_DIM; ++I) {
      InVec[I] = Input.Load<ELEM_TYPE>(I * ELEM_SIZE);
    }

    vector<ELEM_TYPE, M_DIM> OutVec;
    __builtin_LinAlg_MatrixVectorMultiply(
      OutVec, Mat, OUTPUT_SIGNED, InVec, IN_INTERP);

    for (uint I = 0; I < M_DIM; ++I) {
      Output.Store<ELEM_TYPE>(I * ELEM_SIZE, OutVec[I]);
    }
  }
)";

static void runMatVecMul(ID3D12Device *Device,
                         dxc::SpecificDllLoader &DxcSupport,
                         const MatrixParams &Params, bool Verbose,
                         int FillValue, bool OutputSigned,
                         ComponentType InputInterp) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DOUTPUT_SIGNED=" << OutputSigned;
  ExtraDefs << " -DIN_INTERP=" << static_cast<int>(InputInterp);

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, MatVecMulShader, "cs_6_10", Args, Verbose);

  auto Expected =
      makeExpectedVec(Params.CompType, Params.M,
                      static_cast<float>(FillValue * FillValue * Params.N),
                      /*Increment=*/false);

  auto Op = createComputeOp(MatVecMulShader, "cs_6_10", "SRV(t0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", BufferSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Input");
  addRootUAV(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [NumElements, Params, FillValue](LPCSTR Name, std::vector<BYTE> &Data,
                                       st::ShaderOp *) {
        VERIFY_IS_TRUE(fillInputBuffer(Name, Data, Params.CompType, NumElements,
                                       /*StartingVal=*/FillValue,
                                       /*Increment=*/false),
                       "Saw unsupported component type");
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, Params.M, Verbose));
}

void DxilConf_SM610_LinAlg::MatVecMul_Thread_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Scope = MatrixScope::Thread;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 1;
  Params.Enable16Bit = true;
  runMatVecMul(D3DDevice, DxcSupport, Params, VerboseLogging,
               /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F16);
}

static const char MatVecMulAddShader[] = R"(
  #define USE_A 0
  #define SCOPE_THREAD 0

  ByteAddressBuffer Input : register(t0);
  RWByteAddressBuffer Output : register(u1);

  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE_A, SCOPE_THREAD)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, 0, STRIDE, LAYOUT, 128);

    vector<ELEM_TYPE, M_DIM> InVec;
    for (uint I = 0; I < M_DIM; ++I) {
      InVec[I] = Input.Load<ELEM_TYPE>(I * ELEM_SIZE);
    }

    vector<ELEM_TYPE, M_DIM> BiasVec;
    for (uint I = 0; I < M_DIM; ++I) {
      BiasVec[I] = Input.Load<ELEM_TYPE>(I * ELEM_SIZE);
    }

    vector<ELEM_TYPE, M_DIM> OutVec;
    __builtin_LinAlg_MatrixVectorMultiplyAdd(
      OutVec, Mat, OUTPUT_SIGNED, InVec, IN_INTERP, BiasVec, BIAS_INTERP);

    for (uint I = 0; I < M_DIM; ++I) {
      Output.Store<ELEM_TYPE>(I * ELEM_SIZE, OutVec[I]);
    }
  }
)";

static void runMatVecMulAdd(ID3D12Device *Device,
                            dxc::SpecificDllLoader &DxcSupport,
                            const MatrixParams &Params, bool Verbose,
                            int FillValue, bool OutputSigned,
                            ComponentType InputInterp,
                            ComponentType BiasInterp) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DOUTPUT_SIGNED=" << OutputSigned;
  ExtraDefs << " -DIN_INTERP=" << static_cast<int>(InputInterp);
  ExtraDefs << " -DBIAS_INTERP=" << static_cast<int>(BiasInterp);

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, MatVecMulAddShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedVec(
      Params.CompType, Params.M,
      static_cast<float>(FillValue * FillValue * Params.N + FillValue),
      /*Increment=*/false);

  auto Op = createComputeOp(MatVecMulAddShader, "cs_6_10", "SRV(t0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", BufferSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Input");
  addRootUAV(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [NumElements, Params, FillValue](LPCSTR Name, std::vector<BYTE> &Data,
                                       st::ShaderOp *) {
        VERIFY_IS_TRUE(fillInputBuffer(Name, Data, Params.CompType, NumElements,
                                       /*StartingVal=*/FillValue,
                                       /*Increment=*/false),
                       "Saw unsupported component type");
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, Params.M, Verbose));
}

void DxilConf_SM610_LinAlg::MatVecMulAdd_Thread_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Scope = MatrixScope::Thread;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 1;
  Params.Enable16Bit = true;
  runMatVecMulAdd(D3DDevice, DxcSupport, Params, VerboseLogging,
                  /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F16,
                  ComponentType::F16);
}

static const char OuterProductShader[] = R"(
  #define USE_A 0
  #define SCOPE_THREAD 0

  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    vector<ELEM_TYPE, M_DIM> VecA;
    for (uint I = 0; I < M_DIM; ++I) {
      VecA[I] = Input.Load<ELEM_TYPE>(I * ELEM_SIZE);
    }

    uint EndVecA = M_DIM * ELEM_SIZE;

    vector<ELEM_TYPE, N_DIM> VecB;
    for (uint I = 0; I < N_DIM; ++I) {
      VecB[I] = Input.Load<ELEM_TYPE>(EndVecA + I * ELEM_SIZE);
    }

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE_A, SCOPE_THREAD)]]
      Mat;
    __builtin_LinAlg_MatrixOuterProduct(Mat, VecA, VecB);

    __builtin_LinAlg_MatrixAccumulateToDescriptor(
      Mat, Output, 0, STRIDE, LAYOUT, 128);
  }
)";

static void runOuterProduct(ID3D12Device *Device,
                            dxc::SpecificDllLoader &DxcSupport,
                            const MatrixParams &Params, bool Verbose) {
  const size_t NumVecElements = Params.M + Params.N;
  const size_t InBuffSize = NumVecElements * elementSize(Params.CompType);
  const size_t NumMatElements = Params.totalElements();
  const size_t OutBufferSize = Params.totalBytes();

  std::string Args = buildCompilerArgs(Params);

  compileShader(DxcSupport, OuterProductShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, 4,
                                  /*Increment=*/false);

  auto Op = createComputeOp(OuterProductShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", InBuffSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", OutBufferSize, true);
  addRootUAV(Op.get(), 0, "Input");
  addRootUAV(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [NumVecElements, Params](LPCSTR Name, std::vector<BYTE> &Data,
                               st::ShaderOp *) {
        VERIFY_IS_TRUE(fillInputBuffer(Name, Data, Params.CompType,
                                       NumVecElements,
                                       /*StartingVal=*/2, /*Increment=*/false),
                       "Saw unsupported component type");
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumMatElements, Verbose));
}

void DxilConf_SM610_LinAlg::OuterProduct_Thread_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Scope = MatrixScope::Thread;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 1;
  Params.Enable16Bit = true;
  runOuterProduct(D3DDevice, DxcSupport, Params, VerboseLogging);
}

static const char QueryAccumLayoutShader[] = R"(
  RWByteAddressBuffer Output : register(u0);

  [numthreads(1, 1, 1)]
  void main() {
    uint Layout = __builtin_LinAlg_MatrixQueryAccumulatorLayout();
    Output.Store<uint>(0, Layout);
  }
)";

static void runQueryAccumLayout(ID3D12Device *Device,
                                dxc::SpecificDllLoader &DxcSupport,
                                bool Verbose) {
  std::string Args = "-HV 202x";
  size_t BufferSize = elementSize(ComponentType::I32);

  compileShader(DxcSupport, QueryAccumLayoutShader, "cs_6_10", Args, Verbose);

  auto Op = createComputeOp(QueryAccumLayoutShader, "cs_6_10", "UAV(u0)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  const uint32_t *Out = static_cast<const uint32_t *>(OutData.data());

  // Accum Layout must be A or B
  VERIFY_IS_TRUE(Out[0] == static_cast<uint32_t>(MatrixUse::A) ||
                 Out[0] == static_cast<uint32_t>(MatrixUse::B));
  if (Verbose)
    hlsl_test::LogCommentFmt(L"AccumulatorLayout = %u", Out[0]);
}

void DxilConf_SM610_LinAlg::QueryAccumLayout() {
  runQueryAccumLayout(D3DDevice, DxcSupport, VerboseLogging);
}

static const char LoadMemoryShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);
  groupshared ELEM_TYPE GsData[M_DIM * N_DIM];

  #define ELEM_PER_THREAD (M_DIM * N_DIM / NUMTHREADS)

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    for (uint I = 0; I < ELEM_PER_THREAD; ++I) {
      uint Index = threadID * ELEM_PER_THREAD + I;
      GsData[Index] = Input.Load<ELEM_TYPE>(Index * ELEM_SIZE);
    }

    GroupMemoryBarrierWithGroupSync();

    if (WaveReadLaneFirst(threadID) != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromMemory(
      Mat, GsData, OFFSET, STRIDE, LAYOUT);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, OFFSET, STRIDE, LAYOUT, 128);
  }
)";

static void runLoadMemory(ID3D12Device *Device,
                                   dxc::SpecificDllLoader &DxcSupport,
                                   const MatrixParams &Params, bool Verbose) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DOFFSET=" << 0;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, LoadMemoryShader, "cs_6_10", Args,
                Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, 1);

  auto Op = createComputeOp(LoadMemoryShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", BufferSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Input");
  addRootUAV(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
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

void DxilConf_SM610_LinAlg::LoadMemory_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runLoadMemory(D3DDevice, DxcSupport, Params, VerboseLogging);
}

static const char StoreMemoryShader[] = R"(
  RWByteAddressBuffer Output : register(u0);
  groupshared ELEM_TYPE GsData[M_DIM * N_DIM];

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (WaveReadLaneFirst(threadID) != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_FillMatrix(Mat, FILL_VALUE);

    __builtin_LinAlg_MatrixStoreToMemory(
      Mat, GsData, OFFSET, STRIDE, LAYOUT);

    for (uint I = 0; I < M_DIM*N_DIM; ++I) {
      Output.Store<ELEM_TYPE>(I*ELEM_SIZE, GsData[I]);
    }
  }
)";

static void runStoreMemory(ID3D12Device *Device,
                                   dxc::SpecificDllLoader &DxcSupport,
                                   const MatrixParams &Params, bool Verbose,
                                   float FillValue) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DOFFSET=" << 0;
  ExtraDefs << " -DFILL_VALUE=" << FillValue;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, StoreMemoryShader, "cs_6_10", Args,
                Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, FillValue, /*Increment=*/false);

  auto Op = createComputeOp(StoreMemoryShader, "cs_6_10", "UAV(u0)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::StoreMemory_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runStoreMemory(D3DDevice, DxcSupport, Params, VerboseLogging, /*FillValue=*/7.0f);
}

static const char AccumulateMemoryShader[] = R"(
  RWByteAddressBuffer Output : register(u0);
  groupshared ELEM_TYPE GsData[M_DIM * N_DIM];

  #define ELEM_PER_THREAD (M_DIM * N_DIM / NUMTHREADS)

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    ELEM_TYPE fill = FILL_VALUE;
    for (uint I = 0; I < ELEM_PER_THREAD; ++I) {
      uint Index = threadID * ELEM_PER_THREAD + I;
      GsData[Index] = fill;
    }

    GroupMemoryBarrierWithGroupSync();

    if (WaveReadLaneFirst(threadID) != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_FillMatrix(Mat, FILL_VALUE);

    __builtin_LinAlg_MatrixAccumulateToMemory(
      Mat, GsData, OFFSET, STRIDE, LAYOUT);

    for (uint I = 0; I < M_DIM*N_DIM; ++I) {
      Output.Store<ELEM_TYPE>(I*ELEM_SIZE, GsData[I]);
    }
  }
)";

static void runAccumulateMemory(ID3D12Device *Device,
                                   dxc::SpecificDllLoader &DxcSupport,
                                   const MatrixParams &Params, bool Verbose,
                                   float FillValue) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DOFFSET=" << 0;
  ExtraDefs << " -DFILL_VALUE=" << FillValue;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, AccumulateMemoryShader, "cs_6_10", Args,
                Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, FillValue * 2, /*Increment=*/false);

  auto Op = createComputeOp(AccumulateMemoryShader, "cs_6_10", "UAV(u0)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::AccumulateMemory_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runAccumulateMemory(D3DDevice, DxcSupport, Params, VerboseLogging, /*FillValue=*/7.0f);
}

static const char ConvertShader[] = R"(
  #define CT_F16 8
  #define CT_F32 9

  RWByteAddressBuffer Output : register(u0);

  [numthreads(1, 1, 1)]
  void main() {
    vector<half, 4> InVec = {1.0, 2.0, 3.0, 4.0};
    vector<float, 4> OutVec;
    __builtin_LinAlg_Convert(OutVec, InVec, CT_F16, CT_F32);
    Output.Store<float>(0, OutVec.x);
    Output.Store<float>(4, OutVec.y);
    Output.Store<float>(8, OutVec.z);
    Output.Store<float>(12, OutVec.w);
  }
)";

static void runConvert(ID3D12Device *Device,
                                dxc::SpecificDllLoader &DxcSupport,
                                bool Verbose) {
  std::string Args = "-HV 202x";
  MatrixDim NumElements = 4;
  size_t BufferSize = elementSize(ComponentType::F32) * NumElements;

  compileShader(DxcSupport, ConvertShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedVec(ComponentType::F32, NumElements, 1.0);

  auto Op = createComputeOp(ConvertShader, "cs_6_10", "UAV(u0)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootUAV(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(ComponentType::F32, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::Convert() {
  runConvert(D3DDevice, DxcSupport, VerboseLogging);
}

} // namespace LinAlg
