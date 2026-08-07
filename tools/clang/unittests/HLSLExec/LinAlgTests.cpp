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
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#define STREAM_FLOAT(stream, name, value)                                      \
  stream << std::showpoint << " -D" << name << "=" << value << "F"             \
         << std::noshowpoint
#include <variant>
#include <vector>

namespace LinAlg {

using hlsl::DXIL::ComponentType;
using hlsl::DXIL::MatrixLayout;
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
  MatrixLayout Layout;
  int NumThreads;
  bool Enable16Bit;
  bool EmulateTest;

  size_t strideBytes() const {
    uint32_t ES = elementSize(CompType);
    if (Layout == MatrixLayout::RowMajor)
      return N * ES;
    if (Layout == MatrixLayout::ColumnMajor)
      return M * ES;
    // If not Row/Col major, spec says to use 0
    return 0;
  }

  size_t totalElements() const { return M * N; }

  size_t totalBytes() const { return totalElements() * elementSize(CompType); }
};

static std::optional<linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE>
toCapabilityDataType(ComponentType CompType) {
  switch (CompType) {
  case ComponentType::I16:
    return linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16;
  case ComponentType::U16:
    return linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16;
  case ComponentType::I32:
    return linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32;
  case ComponentType::U32:
    return linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32;
  case ComponentType::F16:
    return linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;
  case ComponentType::F32:
    return linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32;
  case ComponentType::I8:
    return linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8;
  case ComponentType::U8:
    return linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8;
  case ComponentType::F8_E4M3FN:
    return linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT8_E4M3FN;
  case ComponentType::F8_E5M2:
    return linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT8_E5M2;
  default:
    return std::nullopt;
  }
}

static bool applyApplicability(linalg_test::Applicability Result,
                               LPCWSTR CaseName) {
  using linalg_test::Applicability;
  switch (Result) {
  case Applicability::Execute:
    return true;
  case Applicability::NotApplicable:
    hlsl_test::LogCommentFmt(
        L"Capability-gated case %s is not applicable on this device", CaseName);
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return false;
  case Applicability::Fail:
    hlsl_test::LogErrorFmt(L"Capability evaluation failed for case %s",
                           CaseName);
    VERIFY_IS_TRUE(false, "LinAlg capability evaluation failed");
    return false;
  }
  VERIFY_IS_TRUE(false, "Unknown LinAlg applicability result");
  return false;
}

// MatrixConstruction is queried with a full {M,K,N} multiply shape, but a
// single tile only pins two of those extents and leaves the third free:
//
//   Use          Tile   Pinned          Free
//   A            MxK    M=Rows, K=Cols  N
//   B            KxN    K=Rows, N=Cols  M
//   Accumulator  MxN    M=Rows, N=Cols  K
//
// The runtime accepts a shape when every extent is a positive multiple of a
// native tile, so the free extent must be swept until one is accepted. Missing
// an extent silently skips a test case, which is the dangerous direction, so
// the sweep is exhaustive rather than a sampled set: native tile extents are
// not required to be powers of two, and the specification's own example cites
// an 8x32x16 tile. Wave-Scope Matrix Dimensions guarantees at least one
// reported shape whose largest component is <= 16 for types of 16 bits or
// larger (<= 256 bits for smaller types), so a sweep to 128 is certain to
// reach a native extent whenever the device supports the type at all. Each
// probe is a CheckFeatureSupport call with no GPU work, so the sweep is cheap.
static constexpr UINT MaxFreeExtentProbe = 128;

static HRESULT supportsMatrixShape(
    ID3D12Device *Device, linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE Type,
    UINT WaveSize, MatrixUse Use, UINT Rows, UINT Columns, bool &Supported) {
  Supported = false;
  for (UINT FreeExtent = 1; FreeExtent <= MaxFreeExtentProbe; ++FreeExtent) {
    linalg_abi::D3D12_LINEAR_ALGEBRA_MATRIX_SHAPE Shape;
    switch (Use) {
    case MatrixUse::A:
      Shape = {Rows, Columns, FreeExtent};
      break;
    case MatrixUse::B:
      Shape = {FreeExtent, Rows, Columns};
      break;
    case MatrixUse::Accumulator:
      Shape = {Rows, FreeExtent, Columns};
      break;
    default:
      return E_INVALIDARG;
    }

    linalg_test::MatrixConstructionSupport Construction;
    const HRESULT HR = linalg_test::queryMatrixConstruction(
        Device, {Type, WaveSize, Shape}, Construction);
    if (FAILED(HR))
      return HR;
    if (Construction.supported()) {
      Supported = true;
      return S_OK;
    }
  }
  return S_OK;
}

// The shaders declare [WaveSize(4, 128)], so a capability query is only
// meaningful for wave sizes the device can actually launch within that range.
static HRESULT queryLaunchableWaveSizes(ID3D12Device *Device, UINT &MinWaveSize,
                                        UINT &MaxWaveSize) {
  MinWaveSize = 0;
  MaxWaveSize = 0;

  D3D12_FEATURE_DATA_D3D12_OPTIONS1 WaveOptions = {};
  const HRESULT HR = Device->CheckFeatureSupport(
      D3D12_FEATURE_D3D12_OPTIONS1, &WaveOptions, sizeof(WaveOptions));
  if (FAILED(HR)) {
    hlsl_test::LogCommentFmt(L"Wave-size capability query failed: 0x%08x", HR);
    return HR;
  }
  if (!WaveOptions.WaveOps)
    return S_OK;

  const auto IsPowerOfTwo = [](UINT Value) {
    return Value != 0 && (Value & (Value - 1)) == 0;
  };
  if (!IsPowerOfTwo(WaveOptions.WaveLaneCountMin) ||
      !IsPowerOfTwo(WaveOptions.WaveLaneCountMax) ||
      WaveOptions.WaveLaneCountMax < WaveOptions.WaveLaneCountMin) {
    hlsl_test::LogCommentFmt(
        L"Wave-size capability response is malformed: WaveOps=%u, min=%u, "
        L"max=%u",
        WaveOptions.WaveOps, WaveOptions.WaveLaneCountMin,
        WaveOptions.WaveLaneCountMax);
    return E_UNEXPECTED;
  }

  MinWaveSize = WaveOptions.WaveLaneCountMin;
  MaxWaveSize = WaveOptions.WaveLaneCountMax;
  return S_OK;
}

// MATRIX_CONSTRUCTION is answered per wave size, so a case that queries it must
// also compile for the size it asked about. Callers pass SelectedWaveSize to
// their runner, which pins it with FORCED_WAVE_SIZE. Without that pin the
// shader declares WaveSize(4, 128), the driver picks whatever it likes, and the
// query answers a question the test never asks.
//
// Uses lists every matrix role the case constructs. A wave size only qualifies
// if every role is supported there, because the roles pin different extents of
// the same {M, K, N} shape.
static HRESULT
selectMatrixConstructionWaveSize(ID3D12Device *Device,
                                 const MatrixParams &Params,
                                 std::initializer_list<MatrixUse> Uses,
                                 bool &Supported, UINT &SelectedWaveSize) {
  Supported = false;
  SelectedWaveSize = 0;
  if (!Device || Uses.size() == 0 ||
      !linalg_test::isLegalScope(
          linalg_abi::D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_MATRIX_CONSTRUCTION,
          Params.Scope))
    return E_INVALIDARG;

  const std::optional<linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE> DataType =
      toCapabilityDataType(Params.CompType);
  if (!DataType.has_value())
    return E_INVALIDARG;

  linalg_test::TierSupport Tier;
  HRESULT HR = linalg_test::queryTierSupport(Device, Tier);
  if (FAILED(HR) || !Tier.supported())
    return HR;

  UINT MinWaveSize = 0;
  UINT MaxWaveSize = 0;
  HR = queryLaunchableWaveSizes(Device, MinWaveSize, MaxWaveSize);
  if (FAILED(HR))
    return HR;
  if (MinWaveSize == 0) {
    hlsl_test::LogCommentFmt(
        L"Wave operations are unsupported; MatrixConstruction is not "
        L"applicable");
    return S_OK;
  }

  for (UINT WaveSize = 4; WaveSize <= 128; WaveSize *= 2) {
    if (WaveSize < MinWaveSize || WaveSize > MaxWaveSize ||
        WaveSize > static_cast<UINT>(Params.NumThreads))
      continue;

    bool AllRolesSupported = true;
    for (const MatrixUse Use : Uses) {
      bool ShapeSupported = false;
      HR = supportsMatrixShape(Device, *DataType, WaveSize, Use, Params.M,
                               Params.N, ShapeSupported);
      if (FAILED(HR))
        return HR;
      if (!ShapeSupported) {
        AllRolesSupported = false;
        break;
      }
    }

    if (AllRolesSupported) {
      hlsl_test::LogCommentFmt(
          L"MatrixConstruction capability matched wave=%u for the %ux%u tile",
          WaveSize, Params.M, Params.N);
      Supported = true;
      SelectedWaveSize = WaveSize;
      return S_OK;
    }
  }

  hlsl_test::LogCommentFmt(
      L"No MatrixConstruction query supports the %ux%u tile for any wave size "
      L"launchable within shader WaveSize(4,128) and a %d-thread group",
      Params.M, Params.N, Params.NumThreads);
  return S_OK;
}

// SelectedWaveSize is only meaningful when one of these helpers returns true.
// The selectors set it on the same path that reports support, so a case cleared
// to run always has a wave size to pin with FORCED_WAVE_SIZE. It stays 0 on the
// skip and failure paths, where the caller has already returned. The assert
// keeps that an invariant rather than a convention.
static bool matrixConstructionApplicable(ID3D12Device *Device,
                                         const MatrixParams &Params,
                                         std::initializer_list<MatrixUse> Uses,
                                         LPCWSTR CaseName,
                                         UINT &SelectedWaveSize) {
  bool Supported = false;
  const HRESULT QueryResult = selectMatrixConstructionWaveSize(
      Device, Params, Uses, Supported, SelectedWaveSize);
  if (!applyApplicability(
          linalg_test::classifyApplicability(
              QueryResult, Supported,
              linalg_test::CapabilityRequirement::CapabilityGated),
          CaseName))
    return false;

  VERIFY_IS_TRUE(SelectedWaveSize != 0,
                 "A case cleared to run must have a selected wave size");
  return true;
}

// Tier support is the only capability the matrix-free operations depend on.
// They construct no matrix, so there is no shape or wave size to query.
static bool linAlgTierApplicable(ID3D12Device *Device, LPCWSTR CaseName) {
  linalg_test::TierSupport Tier;
  const HRESULT QueryResult = linalg_test::queryTierSupport(Device, Tier);
  return applyApplicability(
      linalg_test::classifyApplicability(
          QueryResult, SUCCEEDED(QueryResult) && Tier.supported(),
          linalg_test::CapabilityRequirement::CapabilityGated),
      CaseName);
}

// Accumulation store reports its destinations separately: a device may support
// accumulating into a buffer but not into groupshared memory, or the reverse.
// Tier 1 requires no formats at all here, so every case is gated.
static bool
accumulateStoreApplicable(ID3D12Device *Device, ComponentType CompType,
                          linalg_test::AtomicDestination Destination,
                          LPCWSTR CaseName) {
  bool Supported = false;
  HRESULT QueryResult = E_INVALIDARG;

  const std::optional<linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE> DataType =
      toCapabilityDataType(CompType);
  if (DataType.has_value()) {
    linalg_test::TierSupport Tier;
    QueryResult = linalg_test::queryTierSupport(Device, Tier);
    if (SUCCEEDED(QueryResult) && Tier.supported()) {
      linalg_test::AtomicAccumulateStoreSupport Support;
      QueryResult =
          linalg_test::queryAtomicAccumulateStore(Device, {*DataType}, Support);
      if (SUCCEEDED(QueryResult))
        Supported = Support.supports(Destination);
    }
  }

  return applyApplicability(
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated),
      CaseName);
}

// Tier 1 requires no outer product formats, so this is always gated.
static bool outerProductApplicable(ID3D12Device *Device,
                                   ComponentType InputCompType,
                                   ComponentType ResultCompType,
                                   LPCWSTR CaseName) {
  bool Supported = false;
  HRESULT QueryResult = E_INVALIDARG;

  const std::optional<linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE> InputType =
      toCapabilityDataType(InputCompType);
  const std::optional<linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE> ResultType =
      toCapabilityDataType(ResultCompType);
  if (InputType.has_value() && ResultType.has_value()) {
    linalg_test::TierSupport Tier;
    QueryResult = linalg_test::queryTierSupport(Device, Tier);
    if (SUCCEEDED(QueryResult) && Tier.supported()) {
      linalg_test::ThreadOuterProductSupport Support;
      QueryResult = linalg_test::queryThreadOuterProduct(
          Device, {*InputType, *ResultType}, Support);
      if (SUCCEEDED(QueryResult))
        Supported = Support.supported();
    }
  }

  return applyApplicability(
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated),
      CaseName);
}

// Wave matrix multiply needs both the matrices and the operation itself, and
// both are answered per wave size, so they are resolved in one pass. Fp16 x
// Fp16 -> Fp16 is Optional at Tier 1, so these cases are gated rather than
// mandatory.
static HRESULT selectWaveMatMulWaveSize(ID3D12Device *Device,
                                        const MatrixParams &Params, MatrixDim K,
                                        bool &Supported,
                                        UINT &SelectedWaveSize) {
  Supported = false;
  SelectedWaveSize = 0;
  if (!Device ||
      !linalg_test::isLegalScope(
          linalg_abi::D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_WAVE_MATRIX_MULTIPLY,
          Params.Scope))
    return E_INVALIDARG;

  const std::optional<linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE> DataType =
      toCapabilityDataType(Params.CompType);
  if (!DataType.has_value())
    return E_INVALIDARG;

  linalg_test::TierSupport Tier;
  HRESULT HR = linalg_test::queryTierSupport(Device, Tier);
  if (FAILED(HR) || !Tier.supported())
    return HR;

  UINT MinWaveSize = 0;
  UINT MaxWaveSize = 0;
  HR = queryLaunchableWaveSizes(Device, MinWaveSize, MaxWaveSize);
  if (FAILED(HR))
    return HR;
  if (MinWaveSize == 0) {
    hlsl_test::LogCommentFmt(
        L"Wave operations are unsupported; WaveMatrixMultiply is not "
        L"applicable");
    return S_OK;
  }

  for (UINT WaveSize = 4; WaveSize <= 128; WaveSize *= 2) {
    if (WaveSize < MinWaveSize || WaveSize > MaxWaveSize ||
        WaveSize > static_cast<UINT>(Params.NumThreads))
      continue;

    linalg_abi::D3D12_LINEAR_ALGEBRA_MATRIX_SHAPE Shape = {};
    Shape.M = Params.M;
    Shape.K = K;
    Shape.N = Params.N;

    // A multiply pins all three extents, so the construction query names the
    // exact {M,K,N} shape instead of sweeping a free extent per operand. One
    // answer covers all three operands, because a supported shape means A
    // (MxK), B (KxN) and the accumulator (MxN) can all be constructed.
    linalg_test::MatrixConstructionSupport Construction;
    HR = linalg_test::queryMatrixConstruction(
        Device, {*DataType, WaveSize, Shape}, Construction);
    if (FAILED(HR))
      return HR;
    if (!Construction.supported())
      continue;

    linalg_test::WaveMatrixMultiplySupport Support;
    HR = linalg_test::queryWaveMatrixMultiply(
        Device, {{WaveSize, *DataType, *DataType, *DataType}, Shape}, Support);
    if (FAILED(HR))
      return HR;
    if (Support.supported()) {
      hlsl_test::LogCommentFmt(
          L"WaveMatrixMultiply capability matched wave=%u for %ux%ux%u",
          WaveSize, Params.M, K, Params.N);
      Supported = true;
      SelectedWaveSize = WaveSize;
      return S_OK;
    }
  }

  hlsl_test::LogCommentFmt(
      L"No WaveMatrixMultiply query supports %ux%ux%u for any wave size "
      L"launchable within shader WaveSize(4,128) and a %d-thread group",
      Params.M, K, Params.N, Params.NumThreads);
  return S_OK;
}

static bool waveMatMulApplicable(ID3D12Device *Device,
                                 const MatrixParams &Params, MatrixDim K,
                                 LPCWSTR CaseName, UINT &SelectedWaveSize) {
  bool Supported = false;
  const HRESULT QueryResult =
      selectWaveMatMulWaveSize(Device, Params, K, Supported, SelectedWaveSize);
  if (!applyApplicability(
          linalg_test::classifyApplicability(
              QueryResult, Supported,
              linalg_test::CapabilityRequirement::CapabilityGated),
          CaseName))
    return false;

  VERIFY_IS_TRUE(SelectedWaveSize != 0,
                 "A case cleared to run must have a selected wave size");
  return true;
}

namespace cpu_oracle {

using TypedMatrixValues =
    std::variant<std::vector<HLSLHalf_t>, std::vector<float>,
                 std::vector<int32_t>, std::vector<uint32_t>>;

struct TypedMatrix {
  MatrixDim M;
  MatrixDim N;
  TypedMatrixValues Values;

  // Derived from the active alternative rather than stored alongside it, so
  // the component type and the stored elements cannot disagree.
  ComponentType compType() const;

  size_t totalElements() const {
    return static_cast<size_t>(M) * static_cast<size_t>(N);
  }
};

struct MatrixBufferLayout {
  MatrixLayout Layout;
  size_t OffsetBytes;
  size_t StrideBytes;
};

enum class ComparisonMode {
  // Every mode compares encoded component bits exactly. Implementation freedom
  // is expressed by enumerating the permitted results rather than by an Epsilon
  // or Ulp tolerance, so a conforming result must match a candidate bit for
  // bit.
  Exact,
  PermittedResults,
  Excluded,
};

// MatrixResultOracle models matrix-valued outputs. Operations whose observable
// result is a complete destination buffer need a whole-buffer oracle instead.
struct MatrixResultOracle {
  ComparisonMode Mode;
  std::vector<TypedMatrix> Candidates;
  std::wstring PublicRule;
};

template <typename T, ComponentType CT> struct NativeComponentTraits {
  static constexpr ComponentType CompType = CT;
  static constexpr size_t Size = sizeof(T);

  static void store(BYTE *Dest, const T &Value) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "Component must be trivially copyable");
    std::memcpy(Dest, &Value, sizeof(Value));
  }

  static T load(const BYTE *Source) {
    T Value;
    std::memcpy(&Value, Source, sizeof(Value));
    return Value;
  }

  static bool exactMatch(const T &Actual, const T &Expected) {
    return std::memcmp(&Actual, &Expected, sizeof(T)) == 0;
  }

  static std::wstring format(const T &Value) {
    std::wstringstream Stream;
    Stream << Value;
    return Stream.str();
  }
};

template <typename T> struct ComponentTraits;

template <>
struct ComponentTraits<float>
    : NativeComponentTraits<float, ComponentType::F32> {
  static std::wstring format(const float &Value) {
    uint32_t Bits;
    std::memcpy(&Bits, &Value, sizeof(Bits));
    std::wstringstream Stream;
    Stream << Value << L" (bits=0x" << std::hex << Bits << L")";
    return Stream.str();
  }
};

template <>
struct ComponentTraits<int32_t>
    : NativeComponentTraits<int32_t, ComponentType::I32> {};

template <>
struct ComponentTraits<uint32_t>
    : NativeComponentTraits<uint32_t, ComponentType::U32> {};

template <> struct ComponentTraits<HLSLHalf_t> {
  static constexpr ComponentType CompType = ComponentType::F16;
  static constexpr size_t Size = sizeof(uint16_t);

  static void store(BYTE *Dest, const HLSLHalf_t &Value) {
    std::memcpy(Dest, &Value.Val, sizeof(Value.Val));
  }

  static HLSLHalf_t load(const BYTE *Source) {
    uint16_t Bits;
    std::memcpy(&Bits, Source, sizeof(Bits));
    return HLSLHalf_t::FromHALF(static_cast<DirectX::PackedVector::HALF>(Bits));
  }

  static bool exactMatch(const HLSLHalf_t &Actual, const HLSLHalf_t &Expected) {
    return Actual.Val == Expected.Val;
  }

  static std::wstring format(const HLSLHalf_t &Value) {
    std::wstringstream Stream;
    Stream << static_cast<float>(Value) << L" (bits=0x" << std::hex << Value.Val
           << L")";
    return Stream.str();
  }
};

ComponentType TypedMatrix::compType() const {
  return std::visit(
      [](const auto &Elements) {
        using ElementType =
            typename std::decay<decltype(Elements)>::type::value_type;
        return ComponentTraits<ElementType>::CompType;
      },
      Values);
}

static bool checkedMultiply(size_t Left, size_t Right, size_t &Result) {
  if (Right != 0 && Left > std::numeric_limits<size_t>::max() / Right)
    return false;
  Result = Left * Right;
  return true;
}

static bool checkedAdd(size_t Left, size_t Right, size_t &Result) {
  if (Left > std::numeric_limits<size_t>::max() - Right)
    return false;
  Result = Left + Right;
  return true;
}

static bool isSupportedComponentType(ComponentType CompType) {
  switch (CompType) {
  case ComponentType::F16:
  case ComponentType::F32:
  case ComponentType::I32:
  case ComponentType::U32:
    return true;
  default:
    return false;
  }
}

static LPCWSTR componentTypeName(ComponentType CompType) {
  switch (CompType) {
  case ComponentType::F16:
    return L"F16";
  case ComponentType::F32:
    return L"F32";
  case ComponentType::I32:
    return L"I32";
  case ComponentType::U32:
    return L"U32";
  default:
    return L"Unsupported";
  }
}

static LPCWSTR comparisonModeName(ComparisonMode Mode) {
  switch (Mode) {
  case ComparisonMode::Exact:
    return L"Exact";
  case ComparisonMode::PermittedResults:
    return L"PermittedResults";
  case ComparisonMode::Excluded:
    return L"Excluded";
  }
  return L"Unknown";
}

static bool isMatrixValid(const TypedMatrix &Matrix) {
  size_t ExpectedElements;
  if (Matrix.M == 0 || Matrix.N == 0 ||
      !checkedMultiply(static_cast<size_t>(Matrix.M),
                       static_cast<size_t>(Matrix.N), ExpectedElements))
    return false;

  return std::visit(
      [ExpectedElements](const auto &Elements) {
        return Elements.size() == ExpectedElements;
      },
      Matrix.Values);
}

template <typename T>
static std::optional<TypedMatrix> makeTypedMatrix(MatrixDim M, MatrixDim N,
                                                  std::vector<T> Values) {
  size_t ExpectedElements;
  if (M == 0 || N == 0 ||
      !checkedMultiply(static_cast<size_t>(M), static_cast<size_t>(N),
                       ExpectedElements) ||
      Values.size() != ExpectedElements) {
    hlsl_test::LogErrorFmt(
        L"Invalid typed matrix dimensions or element count: M=%u, N=%u, "
        L"elements=%zu",
        M, N, Values.size());
    return std::nullopt;
  }

  return TypedMatrix{M, N, std::move(Values)};
}

static std::optional<TypedMatrix>
makeSequentialMatrix(ComponentType CompType, MatrixDim M, MatrixDim N,
                     uint32_t StartingValue = 1) {
  size_t NumElements;
  if (M == 0 || N == 0 ||
      !checkedMultiply(static_cast<size_t>(M), static_cast<size_t>(N),
                       NumElements)) {
    hlsl_test::LogErrorFmt(L"Invalid sequential matrix dimensions: M=%u, N=%u",
                           M, N);
    return std::nullopt;
  }

  size_t LastValueSize;
  if (!checkedAdd(static_cast<size_t>(StartingValue), NumElements - 1,
                  LastValueSize)) {
    hlsl_test::LogErrorFmt(L"Sequential matrix value calculation overflowed");
    return std::nullopt;
  }
  const uint64_t LastValue = static_cast<uint64_t>(LastValueSize);

  switch (CompType) {
  case ComponentType::F16: {
    if (LastValue > 65504) {
      hlsl_test::LogErrorFmt(L"F16 sequential value is out of range: %llu",
                             LastValue);
      return std::nullopt;
    }
    std::vector<HLSLHalf_t> Values;
    Values.reserve(NumElements);
    for (size_t I = 0; I < NumElements; ++I)
      Values.emplace_back(static_cast<float>(
          static_cast<uint64_t>(StartingValue) + static_cast<uint64_t>(I)));
    return makeTypedMatrix(M, N, std::move(Values));
  }
  case ComponentType::F32: {
    if (LastValue > (1u << 24)) {
      hlsl_test::LogErrorFmt(
          L"F32 sequential integer cannot be represented exactly: %llu",
          LastValue);
      return std::nullopt;
    }
    std::vector<float> Values;
    Values.reserve(NumElements);
    for (size_t I = 0; I < NumElements; ++I)
      Values.push_back(static_cast<float>(static_cast<uint64_t>(StartingValue) +
                                          static_cast<uint64_t>(I)));
    return makeTypedMatrix(M, N, std::move(Values));
  }
  case ComponentType::I32: {
    if (LastValue >
        static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
      hlsl_test::LogErrorFmt(L"I32 sequential value is out of range: %llu",
                             LastValue);
      return std::nullopt;
    }
    std::vector<int32_t> Values;
    Values.reserve(NumElements);
    for (size_t I = 0; I < NumElements; ++I)
      Values.push_back(static_cast<int32_t>(
          static_cast<uint64_t>(StartingValue) + static_cast<uint64_t>(I)));
    return makeTypedMatrix(M, N, std::move(Values));
  }
  case ComponentType::U32: {
    if (LastValue > std::numeric_limits<uint32_t>::max()) {
      hlsl_test::LogErrorFmt(L"U32 sequential value is out of range: %llu",
                             LastValue);
      return std::nullopt;
    }
    std::vector<uint32_t> Values;
    Values.reserve(NumElements);
    for (size_t I = 0; I < NumElements; ++I)
      Values.push_back(static_cast<uint32_t>(
          static_cast<uint64_t>(StartingValue) + static_cast<uint64_t>(I)));
    return makeTypedMatrix(M, N, std::move(Values));
  }
  default:
    hlsl_test::LogErrorFmt(L"Unsupported sequential matrix component type: %u",
                           static_cast<uint32_t>(CompType));
    return std::nullopt;
  }
}

template <typename T>
static std::optional<TypedMatrix>
transposeTypedMatrix(const TypedMatrix &Source) {
  const std::vector<T> &SourceValues = std::get<std::vector<T>>(Source.Values);
  std::vector<T> Result(Source.totalElements());
  for (MatrixDim Row = 0; Row < Source.M; ++Row) {
    for (MatrixDim Column = 0; Column < Source.N; ++Column) {
      const size_t SourceIndex = static_cast<size_t>(Row) * Source.N + Column;
      const size_t ResultIndex = static_cast<size_t>(Column) * Source.M + Row;
      Result[ResultIndex] = SourceValues[SourceIndex];
    }
  }
  return makeTypedMatrix(Source.N, Source.M, std::move(Result));
}

static std::optional<TypedMatrix> transposeMatrix(const TypedMatrix &Source) {
  if (!isMatrixValid(Source)) {
    hlsl_test::LogErrorFmt(L"Cannot transpose an invalid typed matrix");
    return std::nullopt;
  }

  switch (Source.compType()) {
  case ComponentType::F16:
    return transposeTypedMatrix<HLSLHalf_t>(Source);
  case ComponentType::F32:
    return transposeTypedMatrix<float>(Source);
  case ComponentType::I32:
    return transposeTypedMatrix<int32_t>(Source);
  case ComponentType::U32:
    return transposeTypedMatrix<uint32_t>(Source);
  default:
    return std::nullopt;
  }
}

static bool isRowColLayout(MatrixLayout Layout) {
  return Layout == MatrixLayout::RowMajor ||
         Layout == MatrixLayout::ColumnMajor;
}

static std::optional<size_t>
getMatrixBufferSize(ComponentType CompType, MatrixDim M, MatrixDim N,
                    const MatrixBufferLayout &Layout) {
  if (!isSupportedComponentType(CompType) || M == 0 || N == 0 ||
      !isRowColLayout(Layout.Layout)) {
    hlsl_test::LogErrorFmt(
        L"Invalid matrix buffer description: component=%s, M=%u, N=%u, "
        L"layout=%u",
        componentTypeName(CompType), M, N,
        static_cast<uint32_t>(Layout.Layout));
    return std::nullopt;
  }

  const size_t ElementBytes = elementSize(CompType);
  const size_t MajorCount = Layout.Layout == MatrixLayout::RowMajor ? M : N;
  const size_t MinorCount = Layout.Layout == MatrixLayout::RowMajor ? N : M;
  size_t PackedMinorBytes;
  if (!checkedMultiply(MinorCount, ElementBytes, PackedMinorBytes)) {
    hlsl_test::LogErrorFmt(
        L"Matrix packed row or column byte calculation overflowed: "
        L"component=%s, M=%u, N=%u",
        componentTypeName(CompType), M, N);
    return std::nullopt;
  }
  if (Layout.StrideBytes < PackedMinorBytes) {
    hlsl_test::LogErrorFmt(
        L"Matrix stride is too small: component=%s, M=%u, N=%u, stride=%zu, "
        L"required=%zu",
        componentTypeName(CompType), M, N, Layout.StrideBytes,
        PackedMinorBytes);
    return std::nullopt;
  }

  size_t LastMajorOffset;
  size_t RequiredBytes;
  if (!checkedMultiply(MajorCount - 1, Layout.StrideBytes, LastMajorOffset) ||
      !checkedAdd(Layout.OffsetBytes, LastMajorOffset, RequiredBytes) ||
      !checkedAdd(RequiredBytes, PackedMinorBytes, RequiredBytes)) {
    hlsl_test::LogErrorFmt(L"Matrix buffer size calculation overflowed");
    return std::nullopt;
  }
  return RequiredBytes;
}

static std::optional<size_t>
getMatrixBufferSize(const TypedMatrix &Matrix,
                    const MatrixBufferLayout &Layout) {
  if (!isMatrixValid(Matrix)) {
    hlsl_test::LogErrorFmt(L"Cannot size an invalid typed matrix");
    return std::nullopt;
  }
  return getMatrixBufferSize(Matrix.compType(), Matrix.M, Matrix.N, Layout);
}

static std::optional<size_t>
getElementByteOffset(ComponentType CompType, MatrixDim M, MatrixDim N,
                     MatrixDim Row, MatrixDim Column,
                     const MatrixBufferLayout &Layout) {
  if (Row >= M || Column >= N)
    return std::nullopt;

  const size_t Major = Layout.Layout == MatrixLayout::RowMajor ? Row : Column;
  const size_t Minor = Layout.Layout == MatrixLayout::RowMajor ? Column : Row;
  size_t MajorOffset;
  size_t MinorOffset;
  size_t ByteOffset;
  if (!checkedMultiply(Major, Layout.StrideBytes, MajorOffset) ||
      !checkedMultiply(Minor, elementSize(CompType), MinorOffset) ||
      !checkedAdd(Layout.OffsetBytes, MajorOffset, ByteOffset) ||
      !checkedAdd(ByteOffset, MinorOffset, ByteOffset))
    return std::nullopt;
  return ByteOffset;
}

template <typename T>
static bool writeTypedMatrixBuffer(const TypedMatrix &Matrix,
                                   const MatrixBufferLayout &Layout,
                                   std::vector<BYTE> &Buffer) {
  const std::vector<T> &Values = std::get<std::vector<T>>(Matrix.Values);
  for (MatrixDim Row = 0; Row < Matrix.M; ++Row) {
    for (MatrixDim Column = 0; Column < Matrix.N; ++Column) {
      const size_t ValueIndex = static_cast<size_t>(Row) * Matrix.N + Column;
      std::optional<size_t> ByteOffset = getElementByteOffset(
          Matrix.compType(), Matrix.M, Matrix.N, Row, Column, Layout);
      if (!ByteOffset)
        return false;
      ComponentTraits<T>::store(Buffer.data() + *ByteOffset,
                                Values[ValueIndex]);
    }
  }
  return true;
}

static bool writeMatrixBuffer(const TypedMatrix &Matrix,
                              const MatrixBufferLayout &Layout,
                              std::vector<BYTE> &Buffer) {
  std::optional<size_t> RequiredBytes = getMatrixBufferSize(Matrix, Layout);
  if (!RequiredBytes || Buffer.size() < *RequiredBytes) {
    hlsl_test::LogErrorFmt(
        L"Matrix buffer is too small: actual=%zu, required=%zu", Buffer.size(),
        RequiredBytes.value_or(0));
    return false;
  }

  switch (Matrix.compType()) {
  case ComponentType::F16:
    return writeTypedMatrixBuffer<HLSLHalf_t>(Matrix, Layout, Buffer);
  case ComponentType::F32:
    return writeTypedMatrixBuffer<float>(Matrix, Layout, Buffer);
  case ComponentType::I32:
    return writeTypedMatrixBuffer<int32_t>(Matrix, Layout, Buffer);
  case ComponentType::U32:
    return writeTypedMatrixBuffer<uint32_t>(Matrix, Layout, Buffer);
  default:
    return false;
  }
}

template <typename T>
static std::optional<TypedMatrix>
decodeTypedMatrixBuffer(ComponentType CompType, MatrixDim M, MatrixDim N,
                        const MatrixBufferLayout &Layout, const BYTE *Buffer) {
  std::vector<T> Values(static_cast<size_t>(M) * N);
  for (MatrixDim Row = 0; Row < M; ++Row) {
    for (MatrixDim Column = 0; Column < N; ++Column) {
      const size_t ValueIndex = static_cast<size_t>(Row) * N + Column;
      std::optional<size_t> ByteOffset =
          getElementByteOffset(CompType, M, N, Row, Column, Layout);
      if (!ByteOffset)
        return std::nullopt;
      Values[ValueIndex] = ComponentTraits<T>::load(Buffer + *ByteOffset);
    }
  }
  return makeTypedMatrix(M, N, std::move(Values));
}

static std::optional<TypedMatrix>
decodeMatrixBuffer(ComponentType CompType, MatrixDim M, MatrixDim N,
                   const MatrixBufferLayout &Layout, const void *Buffer,
                   size_t BufferSize) {
  std::optional<size_t> RequiredBytes =
      getMatrixBufferSize(CompType, M, N, Layout);
  if (!Buffer || !RequiredBytes || BufferSize < *RequiredBytes) {
    hlsl_test::LogErrorFmt(
        L"Cannot decode matrix buffer: actual=%zu, required=%zu", BufferSize,
        RequiredBytes.value_or(0));
    return std::nullopt;
  }

  const BYTE *Bytes = static_cast<const BYTE *>(Buffer);
  switch (CompType) {
  case ComponentType::F16:
    return decodeTypedMatrixBuffer<HLSLHalf_t>(CompType, M, N, Layout, Bytes);
  case ComponentType::F32:
    return decodeTypedMatrixBuffer<float>(CompType, M, N, Layout, Bytes);
  case ComponentType::I32:
    return decodeTypedMatrixBuffer<int32_t>(CompType, M, N, Layout, Bytes);
  case ComponentType::U32:
    return decodeTypedMatrixBuffer<uint32_t>(CompType, M, N, Layout, Bytes);
  default:
    return std::nullopt;
  }
}

template <typename T>
static bool exactMatrixMatch(const TypedMatrix &Actual,
                             const TypedMatrix &Expected,
                             size_t &FirstMismatch) {
  const std::vector<T> &ActualValues = std::get<std::vector<T>>(Actual.Values);
  const std::vector<T> &ExpectedValues =
      std::get<std::vector<T>>(Expected.Values);
  for (size_t I = 0; I < ActualValues.size(); ++I) {
    if (!ComponentTraits<T>::exactMatch(ActualValues[I], ExpectedValues[I])) {
      FirstMismatch = I;
      return false;
    }
  }
  FirstMismatch = ActualValues.size();
  return true;
}

static bool exactMatrixMatch(const TypedMatrix &Actual,
                             const TypedMatrix &Expected,
                             size_t &FirstMismatch) {
  if (!isMatrixValid(Actual) || !isMatrixValid(Expected) ||
      Actual.compType() != Expected.compType() || Actual.M != Expected.M ||
      Actual.N != Expected.N) {
    FirstMismatch = 0;
    return false;
  }

  switch (Actual.compType()) {
  case ComponentType::F16:
    return exactMatrixMatch<HLSLHalf_t>(Actual, Expected, FirstMismatch);
  case ComponentType::F32:
    return exactMatrixMatch<float>(Actual, Expected, FirstMismatch);
  case ComponentType::I32:
    return exactMatrixMatch<int32_t>(Actual, Expected, FirstMismatch);
  case ComponentType::U32:
    return exactMatrixMatch<uint32_t>(Actual, Expected, FirstMismatch);
  default:
    FirstMismatch = 0;
    return false;
  }
}

static std::wstring matrixValueString(const TypedMatrix &Matrix, size_t Index) {
  switch (Matrix.compType()) {
  case ComponentType::F16:
    return ComponentTraits<HLSLHalf_t>::format(
        std::get<std::vector<HLSLHalf_t>>(Matrix.Values)[Index]);
  case ComponentType::F32:
    return ComponentTraits<float>::format(
        std::get<std::vector<float>>(Matrix.Values)[Index]);
  case ComponentType::I32:
    return ComponentTraits<int32_t>::format(
        std::get<std::vector<int32_t>>(Matrix.Values)[Index]);
  case ComponentType::U32:
    return ComponentTraits<uint32_t>::format(
        std::get<std::vector<uint32_t>>(Matrix.Values)[Index]);
  default:
    return L"unsupported";
  }
}

static MatrixResultOracle exactResult(TypedMatrix Expected,
                                      std::wstring PublicRule) {
  return MatrixResultOracle{
      ComparisonMode::Exact, {std::move(Expected)}, std::move(PublicRule)};
}

static MatrixResultOracle permittedResults(std::vector<TypedMatrix> Candidates,
                                           std::wstring PublicRule) {
  return MatrixResultOracle{ComparisonMode::PermittedResults,
                            std::move(Candidates), std::move(PublicRule)};
}

static MatrixResultOracle excludedResult(std::wstring PublicRule) {
  return MatrixResultOracle{
      ComparisonMode::Excluded, {}, std::move(PublicRule)};
}

static bool isOracleValid(const MatrixResultOracle &Oracle) {
  if (Oracle.PublicRule.empty())
    return false;
  if (Oracle.Mode == ComparisonMode::Excluded)
    return Oracle.Candidates.empty();
  if (Oracle.Mode == ComparisonMode::Exact && Oracle.Candidates.size() != 1)
    return false;
  if (Oracle.Mode == ComparisonMode::PermittedResults &&
      Oracle.Candidates.size() < 2)
    return false;

  const TypedMatrix &First = Oracle.Candidates.front();
  if (!isMatrixValid(First))
    return false;
  for (const TypedMatrix &Candidate : Oracle.Candidates) {
    if (!isMatrixValid(Candidate) || Candidate.compType() != First.compType() ||
        Candidate.M != First.M || Candidate.N != First.N)
      return false;
  }
  return true;
}

static bool
matchesAnyCompleteCandidate(const TypedMatrix &Actual,
                            const MatrixResultOracle &Oracle,
                            std::vector<size_t> *FirstMismatches = nullptr) {
  if (!isOracleValid(Oracle) || Oracle.Mode == ComparisonMode::Excluded)
    return false;

  if (FirstMismatches)
    FirstMismatches->clear();
  for (const TypedMatrix &Candidate : Oracle.Candidates) {
    size_t FirstMismatch;
    if (exactMatrixMatch(Actual, Candidate, FirstMismatch))
      return true;
    if (FirstMismatches)
      FirstMismatches->push_back(FirstMismatch);
  }
  return false;
}

static bool verifyMatrixBuffer(const void *ActualBuffer,
                               size_t ActualBufferSize,
                               const MatrixBufferLayout &Layout,
                               const MatrixResultOracle &Oracle, bool Verbose) {
  if (!isOracleValid(Oracle)) {
    hlsl_test::LogErrorFmt(L"Invalid matrix oracle");
    return false;
  }
  if (Oracle.Mode == ComparisonMode::Excluded) {
    hlsl_test::LogErrorFmt(
        L"Excluded matrix result cannot be used as a success fallback: %s",
        Oracle.PublicRule.c_str());
    return false;
  }

  const TypedMatrix &Shape = Oracle.Candidates.front();
  std::optional<TypedMatrix> Actual =
      decodeMatrixBuffer(Shape.compType(), Shape.M, Shape.N, Layout,
                         ActualBuffer, ActualBufferSize);
  if (!Actual)
    return false;

  std::vector<size_t> FirstMismatches;
  if (matchesAnyCompleteCandidate(*Actual, Oracle, &FirstMismatches)) {
    if (Verbose) {
      hlsl_test::LogCommentFmt(
          L"Matrix comparison passed: component=%s, M=%u, N=%u, mode=%s, "
          L"rule=%s",
          componentTypeName(Shape.compType()), Shape.M, Shape.N,
          comparisonModeName(Oracle.Mode), Oracle.PublicRule.c_str());
    }
    return true;
  }

  hlsl_test::LogErrorFmt(
      L"No complete matrix candidate matched: component=%s, M=%u, N=%u, "
      L"mode=%s, rule=%s",
      componentTypeName(Shape.compType()), Shape.M, Shape.N,
      comparisonModeName(Oracle.Mode), Oracle.PublicRule.c_str());
  for (size_t CandidateIndex = 0; CandidateIndex < Oracle.Candidates.size();
       ++CandidateIndex) {
    const TypedMatrix &Candidate = Oracle.Candidates[CandidateIndex];
    const size_t Mismatch = FirstMismatches[CandidateIndex];
    const size_t Row = Mismatch / Shape.N;
    const size_t Column = Mismatch % Shape.N;
    hlsl_test::LogErrorFmt(
        L"Candidate %zu first mismatch at index=%zu, coordinate=(%zu,%zu): "
        L"actual=%s, expected=%s",
        CandidateIndex, Mismatch, Row, Column,
        matrixValueString(*Actual, Mismatch).c_str(),
        matrixValueString(Candidate, Mismatch).c_str());
  }
  return false;
}

// The bytes a matrix does not occupy -- the prologue before the offset, and
// the padding between rows when the stride exceeds a packed row -- must
// survive a store untouched.
//
// Comparing elements alone would not catch a store that damages the bytes
// around them. A store that ignored the stride entirely writes its elements to
// the wrong addresses and fails the element comparison anyway, but a store
// that places every element correctly and also widens its writes over the
// padding produces a correct matrix while silently corrupting whatever else
// shared the buffer.
// Seeding the destination with a poison pattern and checking that the
// non-element bytes still hold it separates those two cases.
//
// The pattern varies with the byte offset rather than repeating a single
// value. A constant would be indistinguishable from a store that happened to
// write that same value, and also from memory nobody wrote at all -- 0xcd, the
// obvious choice, is what the MSVC debug allocator fills fresh heap with.
// Multiplying the offset by an odd number keeps consecutive bytes distinct, so
// a store writing any constant over two or more adjacent bytes is always
// caught, and a single overwritten byte survives only if it happens to match
// the pattern at exactly that offset.
//
// Counting is kept separate from reporting so the check can be unit tested in
// both directions. verifyUntouchedBytes reports through Log::Error, which
// marks the calling test failed, so a test that deliberately supplies a
// corrupted buffer cannot call it.
static constexpr BYTE PoisonSeed = 0xa5;

static BYTE poisonByteAt(size_t Offset) {
  return static_cast<BYTE>(PoisonSeed ^ static_cast<BYTE>(Offset * 31u));
}

static void fillPoison(void *Buffer, size_t BufferSize) {
  BYTE *Bytes = static_cast<BYTE *>(Buffer);
  for (size_t I = 0; I < BufferSize; ++I)
    Bytes[I] = poisonByteAt(I);
}

// Returns the number of offending bytes, or nullopt if the buffer cannot hold
// the described matrix at all. FirstOffsets, when supplied, collects the
// leading offenders for diagnostics.
static std::optional<size_t>
countTouchedBytesOutsideElements(ComponentType CompType, MatrixDim M,
                                 MatrixDim N, const MatrixBufferLayout &Layout,
                                 const void *Buffer, size_t BufferSize,
                                 std::vector<size_t> *FirstOffsets = nullptr) {
  static constexpr size_t MaxReportedOffsets = 8;

  std::optional<size_t> RequiredBytes =
      getMatrixBufferSize(CompType, M, N, Layout);
  if (!RequiredBytes || BufferSize < *RequiredBytes)
    return std::nullopt;

  const size_t ElementBytes = elementSize(CompType);
  std::vector<bool> Owned(BufferSize, false);
  for (MatrixDim Row = 0; Row < M; ++Row) {
    for (MatrixDim Column = 0; Column < N; ++Column) {
      std::optional<size_t> ByteOffset =
          getElementByteOffset(CompType, M, N, Row, Column, Layout);
      if (!ByteOffset || *ByteOffset + ElementBytes > BufferSize)
        return std::nullopt;
      for (size_t I = 0; I < ElementBytes; ++I)
        Owned[*ByteOffset + I] = true;
    }
  }

  const BYTE *Bytes = static_cast<const BYTE *>(Buffer);
  size_t Corrupted = 0;
  for (size_t I = 0; I < BufferSize; ++I) {
    if (Owned[I] || Bytes[I] == poisonByteAt(I))
      continue;
    if (FirstOffsets && FirstOffsets->size() < MaxReportedOffsets)
      FirstOffsets->push_back(I);
    ++Corrupted;
  }
  return Corrupted;
}

// Reporting wrapper around countTouchedBytesOutsideElements for the execution
// tests.
static bool verifyUntouchedBytes(ComponentType CompType, MatrixDim M,
                                 MatrixDim N, const MatrixBufferLayout &Layout,
                                 const void *Buffer, size_t BufferSize,
                                 bool Verbose) {
  std::vector<size_t> FirstOffsets;
  std::optional<size_t> Corrupted = countTouchedBytesOutsideElements(
      CompType, M, N, Layout, Buffer, BufferSize, &FirstOffsets);

  if (!Corrupted) {
    hlsl_test::LogErrorFmt(
        L"Buffer of %zu bytes cannot hold the requested matrix layout",
        BufferSize);
    return false;
  }

  if (*Corrupted == 0) {
    if (Verbose)
      hlsl_test::LogCommentFmt(L"Every byte outside the stored elements still "
                               L"holds the poison pattern for seed 0x%02x",
                               PoisonSeed);
    return true;
  }

  for (size_t Offset : FirstOffsets)
    hlsl_test::LogErrorFmt(
        L"Byte %zu is outside every element but was overwritten: "
        L"actual=0x%02x, expected poison=0x%02x",
        Offset, static_cast<const BYTE *>(Buffer)[Offset],
        poisonByteAt(Offset));
  hlsl_test::LogErrorFmt(L"%zu bytes outside the stored elements were "
                         L"overwritten",
                         *Corrupted);
  return false;
}

} // namespace cpu_oracle

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
  case ComponentType::I32:
    SS << " -DELEM_TYPE=int";
    break;
  case ComponentType::U32:
    SS << " -DELEM_TYPE=uint";
    break;
  default:
    VERIFY_IS_TRUE(false, "Unsupported LinAlg component type");
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
      size_t Value = I * N + J;
      size_t Idx = Transpose ? J * M + I : Value;
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

// Harness self-check for the CPU oracle. Deliberately carries no Kits metadata
// so HLK runs never select it; drivers are not certified against this class.
class LinAlgCPUOracleTests {
public:
  BEGIN_TEST_CLASS(LinAlgCPUOracleTests)
  TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_METHOD(TypedMatrixBufferRoundTrip);
  TEST_METHOD(UntouchedByteVerification);
};

void LinAlgCPUOracleTests::TypedMatrixBufferRoundTrip() {
  using namespace cpu_oracle;

  auto VerifyScalarEncoding = [](const std::optional<TypedMatrix> &Matrix,
                                 const std::vector<BYTE> &ExpectedBytes) {
    if (!Matrix)
      return false;
    MatrixBufferLayout Layout = {
        MatrixLayout::RowMajor,
        /*OffsetBytes=*/0,
        /*StrideBytes=*/ExpectedBytes.size(),
    };
    std::vector<BYTE> ActualBytes(ExpectedBytes.size(), 0);
    MatrixResultOracle Oracle =
        exactResult(*Matrix, L"Host scalar encoding and decoding");
    return writeMatrixBuffer(*Matrix, Layout, ActualBytes) &&
           ActualBytes == ExpectedBytes &&
           verifyMatrixBuffer(ActualBytes.data(), ActualBytes.size(), Layout,
                              Oracle, /*Verbose=*/false);
  };

  VERIFY_IS_TRUE(VerifyScalarEncoding(
      makeTypedMatrix<HLSLHalf_t>(1, 1, {HLSLHalf_t(1.5f)}), {0x00, 0x3e}));
  VERIFY_IS_TRUE(VerifyScalarEncoding(makeTypedMatrix<float>(1, 1, {-2.5f}),
                                      {0x00, 0x00, 0x20, 0xc0}));
  VERIFY_IS_TRUE(VerifyScalarEncoding(makeTypedMatrix<int32_t>(1, 1, {-7}),
                                      {0xf9, 0xff, 0xff, 0xff}));
  VERIFY_IS_TRUE(
      VerifyScalarEncoding(makeTypedMatrix<uint32_t>(1, 1, {0x89abcdefu}),
                           {0xef, 0xcd, 0xab, 0x89}));

  const uint32_t AdjacentFloatBits = 0x3f800001;
  float AdjacentFloat;
  std::memcpy(&AdjacentFloat, &AdjacentFloatBits, sizeof(AdjacentFloat));
  VERIFY_IS_TRUE(
      ComponentTraits<float>::format(AdjacentFloat).find(L"3f800001") !=
      std::wstring::npos);

  std::optional<TypedMatrix> Matrix =
      makeTypedMatrix<uint32_t>(2, 3, {1, 2, 3, 4, 5, 6});
  VERIFY_IS_TRUE(Matrix.has_value());

  MatrixBufferLayout RowMajor = {
      MatrixLayout::RowMajor,
      /*OffsetBytes=*/4,
      /*StrideBytes=*/16,
  };
  std::optional<size_t> RowBytes = getMatrixBufferSize(*Matrix, RowMajor);
  VERIFY_IS_TRUE(RowBytes.has_value());
  std::vector<BYTE> RowBuffer(*RowBytes, 0xcd);
  VERIFY_IS_TRUE(writeMatrixBuffer(*Matrix, RowMajor, RowBuffer));
  const std::vector<BYTE> ExpectedRowBuffer = {
      0xcd, 0xcd, 0xcd, 0xcd, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
      0x00, 0x03, 0x00, 0x00, 0x00, 0xcd, 0xcd, 0xcd, 0xcd, 0x04, 0x00,
      0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
  };
  VERIFY_IS_TRUE(RowBuffer == ExpectedRowBuffer);
  MatrixResultOracle Exact =
      exactResult(*Matrix, L"Host exact row-major matrix encoding");
  VERIFY_IS_TRUE(verifyMatrixBuffer(RowBuffer.data(), RowBuffer.size(),
                                    RowMajor, Exact, /*Verbose=*/false));

  MatrixBufferLayout ColumnMajor = {
      MatrixLayout::ColumnMajor,
      /*OffsetBytes=*/4,
      /*StrideBytes=*/12,
  };
  std::optional<size_t> ColumnBytes = getMatrixBufferSize(*Matrix, ColumnMajor);
  VERIFY_IS_TRUE(ColumnBytes.has_value());
  std::vector<BYTE> ColumnBuffer(*ColumnBytes, 0xcd);
  VERIFY_IS_TRUE(writeMatrixBuffer(*Matrix, ColumnMajor, ColumnBuffer));
  const std::vector<BYTE> ExpectedColumnBuffer = {
      0xcd, 0xcd, 0xcd, 0xcd, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
      0xcd, 0xcd, 0xcd, 0xcd, 0x02, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
      0xcd, 0xcd, 0xcd, 0xcd, 0x03, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
  };
  VERIFY_IS_TRUE(ColumnBuffer == ExpectedColumnBuffer);
  VERIFY_IS_TRUE(verifyMatrixBuffer(ColumnBuffer.data(), ColumnBuffer.size(),
                                    ColumnMajor, Exact, /*Verbose=*/false));

  std::optional<TypedMatrix> Transposed = transposeMatrix(*Matrix);
  std::optional<TypedMatrix> ExpectedTranspose =
      makeTypedMatrix<uint32_t>(3, 2, {1, 4, 2, 5, 3, 6});
  VERIFY_IS_TRUE(Transposed.has_value());
  VERIFY_IS_TRUE(ExpectedTranspose.has_value());
  size_t FirstMismatch;
  VERIFY_IS_TRUE(
      exactMatrixMatch(*Transposed, *ExpectedTranspose, FirstMismatch));

  std::optional<TypedMatrix> MixedActual =
      makeTypedMatrix<uint32_t>(1, 2, {1, 4});
  std::optional<TypedMatrix> CandidateA =
      makeTypedMatrix<uint32_t>(1, 2, {1, 2});
  std::optional<TypedMatrix> CandidateB =
      makeTypedMatrix<uint32_t>(1, 2, {3, 4});
  VERIFY_IS_TRUE(MixedActual.has_value());
  VERIFY_IS_TRUE(CandidateA.has_value());
  VERIFY_IS_TRUE(CandidateB.has_value());
  MatrixResultOracle Permitted =
      permittedResults({*CandidateA, *CandidateB},
                       L"Host whole-result permitted candidate semantics");
  VERIFY_IS_FALSE(matchesAnyCompleteCandidate(*MixedActual, Permitted));
  Permitted.Candidates.push_back(*MixedActual);
  VERIFY_IS_TRUE(matchesAnyCompleteCandidate(*MixedActual, Permitted));

  MatrixResultOracle Excluded =
      excludedResult(L"Host excluded-oracle classification");
  VERIFY_IS_FALSE(matchesAnyCompleteCandidate(*Matrix, Excluded));

  MatrixParams Params = {};
  Params.M = 2;
  Params.N = 3;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 4;
  Params.CompType = ComponentType::I32;
  VERIFY_IS_TRUE(buildCompilerArgs(Params).find(" -DELEM_TYPE=int") !=
                 std::string::npos);
  Params.CompType = ComponentType::U32;
  VERIFY_IS_TRUE(buildCompilerArgs(Params).find(" -DELEM_TYPE=uint") !=
                 std::string::npos);
}

// The padding check is verified here rather than only through the execution
// tests because a GPU round trip cannot easily produce a store that places
// every element correctly and still damages the bytes around them, which is
// the single case this check exists to catch.
void LinAlgCPUOracleTests::UntouchedByteVerification() {
  using namespace cpu_oracle;

  // A 2x3 uint32 matrix at a 4 byte offset with a 16 byte stride occupies
  // bytes 4..15 and 20..31, leaving a 4 byte prologue at 0..3 and 4 bytes of
  // padding at 16..19.
  std::optional<TypedMatrix> Matrix =
      makeTypedMatrix<uint32_t>(2, 3, {1, 2, 3, 4, 5, 6});
  VERIFY_IS_TRUE(Matrix.has_value());

  const MatrixBufferLayout Layout = {
      MatrixLayout::RowMajor,
      /*OffsetBytes=*/4,
      /*StrideBytes=*/16,
  };
  std::optional<size_t> Size = getMatrixBufferSize(*Matrix, Layout);
  VERIFY_IS_TRUE(Size.has_value());
  VERIFY_ARE_EQUAL(size_t(32), *Size);

  std::vector<BYTE> Buffer(*Size);
  fillPoison(Buffer.data(), Buffer.size());
  VERIFY_IS_TRUE(writeMatrixBuffer(*Matrix, Layout, Buffer));

  auto CountTouched = [&Layout](const std::vector<BYTE> &Bytes) {
    return countTouchedBytesOutsideElements(ComponentType::U32, 2, 3, Layout,
                                            Bytes.data(), Bytes.size());
  };

  // A correctly encoded buffer leaves every non-element byte poisoned.
  std::optional<size_t> Clean = CountTouched(Buffer);
  VERIFY_IS_TRUE(Clean.has_value());
  VERIFY_ARE_EQUAL(size_t(0), *Clean);
  VERIFY_IS_TRUE(verifyUntouchedBytes(ComponentType::U32, 2, 3, Layout,
                                      Buffer.data(), Buffer.size(),
                                      /*Verbose=*/false));

  // Damaging an element is the element comparison's job, not this check's, so
  // the count must stay at zero.
  std::vector<BYTE> ElementTouched = Buffer;
  ElementTouched[4] ^= 0xff;
  std::optional<size_t> AfterElement = CountTouched(ElementTouched);
  VERIFY_IS_TRUE(AfterElement.has_value());
  VERIFY_ARE_EQUAL(size_t(0), *AfterElement);

  // Damaging the prologue or the inter-row padding is what this check exists
  // to catch, so each one must be counted.
  for (size_t Offset : {size_t(0), size_t(16)}) {
    std::vector<BYTE> PaddingTouched = Buffer;
    PaddingTouched[Offset] ^= 0xff;
    std::optional<size_t> AfterPadding = CountTouched(PaddingTouched);
    VERIFY_IS_TRUE(AfterPadding.has_value());
    VERIFY_ARE_EQUAL(size_t(1), *AfterPadding);
  }

  // Every non-element byte damaged at once is still counted exactly.
  std::vector<BYTE> AllTouched(*Size);
  fillPoison(AllTouched.data(), AllTouched.size());
  for (BYTE &Byte : AllTouched)
    Byte = static_cast<BYTE>(~Byte);
  VERIFY_IS_TRUE(writeMatrixBuffer(*Matrix, Layout, AllTouched));
  std::optional<size_t> AfterAll = CountTouched(AllTouched);
  VERIFY_IS_TRUE(AfterAll.has_value());
  VERIFY_ARE_EQUAL(size_t(8), *AfterAll);

  // A buffer filled with one repeated value is fully detected, which is the
  // reason the pattern varies with the offset. A constant poison would score
  // zero here whenever the store happened to pick that same value, and 0xcd in
  // particular is what the MSVC debug allocator leaves in memory nobody wrote.
  std::vector<BYTE> ConstantFill(*Size, BYTE(0xcd));
  std::optional<size_t> AfterConstant = CountTouched(ConstantFill);
  VERIFY_IS_TRUE(AfterConstant.has_value());
  VERIFY_ARE_EQUAL(size_t(8), *AfterConstant);

  // The case a constant poison cannot survive: a store that writes a value the
  // poison pattern itself uses. Because the pattern varies, that value matches
  // at exactly one offset, so seven of the eight non-element bytes are still
  // caught. A constant poison would match everywhere and report nothing.
  std::vector<BYTE> PoisonValuedFill(*Size, poisonByteAt(0));
  std::optional<size_t> AfterPoisonValued = CountTouched(PoisonValuedFill);
  VERIFY_IS_TRUE(AfterPoisonValued.has_value());
  VERIFY_ARE_EQUAL(size_t(7), *AfterPoisonValued);

  // No two adjacent bytes share a poison value, so a constant written over any
  // two neighbours cannot hide in both.
  for (size_t Offset = 1; Offset < *Size; ++Offset)
    VERIFY_ARE_NOT_EQUAL(poisonByteAt(Offset - 1), poisonByteAt(Offset));

  // The diagnostic list is capped but the count is not, so the two have to be
  // checked against a buffer with more offenders than the cap. A 2x3 uint32
  // matrix at a 16 byte offset with a 16 byte stride occupies bytes 16..27 and
  // 32..43, leaving twenty bytes outside the elements.
  const MatrixBufferLayout PaddedLayout = {
      MatrixLayout::RowMajor,
      /*OffsetBytes=*/16,
      /*StrideBytes=*/16,
  };
  std::optional<size_t> PaddedSize =
      getMatrixBufferSize(ComponentType::U32, 2, 3, PaddedLayout);
  VERIFY_IS_TRUE(PaddedSize.has_value());
  VERIFY_ARE_EQUAL(size_t(44), *PaddedSize);

  std::vector<BYTE> AllPaddingTouched(*PaddedSize);
  fillPoison(AllPaddingTouched.data(), AllPaddingTouched.size());
  for (BYTE &Byte : AllPaddingTouched)
    Byte = static_cast<BYTE>(~Byte);
  std::vector<size_t> ReportedOffsets;
  std::optional<size_t> AfterPadded = countTouchedBytesOutsideElements(
      ComponentType::U32, 2, 3, PaddedLayout, AllPaddingTouched.data(),
      AllPaddingTouched.size(), &ReportedOffsets);
  VERIFY_IS_TRUE(AfterPadded.has_value());
  VERIFY_ARE_EQUAL(size_t(20), *AfterPadded);
  VERIFY_ARE_EQUAL(size_t(8), ReportedOffsets.size());
  for (size_t I = 0; I < ReportedOffsets.size(); ++I)
    VERIFY_ARE_EQUAL(I, ReportedOffsets[I]);

  // Below the cap every offender is reported, and by its offset in the buffer
  // rather than its position among the offenders.
  std::vector<BYTE> TwoPaddingBytes(*PaddedSize);
  fillPoison(TwoPaddingBytes.data(), TwoPaddingBytes.size());
  TwoPaddingBytes[28] ^= 0xff;
  TwoPaddingBytes[29] ^= 0xff;
  std::vector<size_t> TwoOffsets;
  std::optional<size_t> AfterTwo = countTouchedBytesOutsideElements(
      ComponentType::U32, 2, 3, PaddedLayout, TwoPaddingBytes.data(),
      TwoPaddingBytes.size(), &TwoOffsets);
  VERIFY_IS_TRUE(AfterTwo.has_value());
  VERIFY_ARE_EQUAL(size_t(2), *AfterTwo);
  VERIFY_ARE_EQUAL(size_t(2), TwoOffsets.size());
  VERIFY_ARE_EQUAL(size_t(28), TwoOffsets[0]);
  VERIFY_ARE_EQUAL(size_t(29), TwoOffsets[1]);

  // A buffer too small for the layout cannot be checked at all.
  std::vector<BYTE> TooSmall(*Size - 1);
  fillPoison(TooSmall.data(), TooSmall.size());
  VERIFY_IS_FALSE(CountTouched(TooSmall).has_value());
}

class LinAlgCapabilityTests {
public:
  BEGIN_TEST_CLASS(LinAlgCapabilityTests)
  TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_METHOD(CapabilityPolicyAndPredicates);
};

void LinAlgCapabilityTests::CapabilityPolicyAndPredicates() {
  using namespace linalg_test;

  VERIFY_IS_TRUE(
      classifyApplicability(S_OK, true, CapabilityRequirement::Mandatory) ==
      Applicability::Execute);
  VERIFY_IS_TRUE(classifyApplicability(
                     S_OK, false, CapabilityRequirement::CapabilityGated) ==
                 Applicability::NotApplicable);
  VERIFY_IS_TRUE(
      classifyApplicability(S_OK, false, CapabilityRequirement::Mandatory) ==
      Applicability::Fail);
  VERIFY_IS_TRUE(
      classifyApplicability(E_UNEXPECTED, true,
                            CapabilityRequirement::CapabilityGated) ==
      Applicability::Fail);

  VERIFY_IS_TRUE(isLegalScope(
      linalg_abi::D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_MATRIX_CONSTRUCTION,
      MatrixScope::Wave));
  VERIFY_IS_TRUE(isLegalScope(
      linalg_abi::D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_MATRIX_CONSTRUCTION,
      MatrixScope::ThreadGroup));
  VERIFY_IS_FALSE(isLegalScope(
      linalg_abi::D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_MATRIX_CONSTRUCTION,
      MatrixScope::Thread));
  VERIFY_IS_TRUE(isLegalScope(
      linalg_abi::D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_WAVE_MATRIX_MULTIPLY,
      MatrixScope::Wave));
  VERIFY_IS_TRUE(isLegalScope(
      linalg_abi::
          D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_THREADGROUP_MATRIX_MULTIPLY,
      MatrixScope::ThreadGroup));
  VERIFY_IS_TRUE(isLegalScope(
      linalg_abi::
          D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_THREAD_VECTOR_MATRIX_MULTIPLY,
      MatrixScope::Thread));
  VERIFY_IS_TRUE(isLegalScope(
      linalg_abi::D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_THREAD_OUTER_PRODUCT,
      MatrixScope::Thread));
  VERIFY_IS_TRUE(isLegalScope(
      linalg_abi::D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_ATOMIC_ACCUMULATE_STORE,
      MatrixScope::Thread));
  VERIFY_IS_TRUE(isLegalScope(
      linalg_abi::D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_ATOMIC_ACCUMULATE_STORE,
      MatrixScope::Wave));
  VERIFY_IS_TRUE(isLegalScope(
      linalg_abi::D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_ATOMIC_ACCUMULATE_STORE,
      MatrixScope::ThreadGroup));

  MatrixConstructionSupport Construction = {TRUE};
  VERIFY_IS_TRUE(Construction.valid());
  VERIFY_IS_TRUE(Construction.supported());
  MatrixConstructionSupport UnsupportedConstruction = {FALSE};
  VERIFY_IS_TRUE(UnsupportedConstruction.valid());
  VERIFY_IS_FALSE(UnsupportedConstruction.supported());
  // The runtime contract is a canonical BOOL; anything else is a driver bug.
  MatrixConstructionSupport InvalidConstruction = {2};
  VERIFY_IS_FALSE(InvalidConstruction.valid());
  VERIFY_IS_FALSE(InvalidConstruction.supported());

  WaveMatrixMultiplySupport Wave = {
      linalg_abi::D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_SUPPORTED};
  VERIFY_IS_TRUE(Wave.valid());
  VERIFY_IS_TRUE(Wave.supported());
  WaveMatrixMultiplySupport UnsupportedWave = {
      linalg_abi::D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_NONE};
  VERIFY_IS_TRUE(UnsupportedWave.valid());
  VERIFY_IS_FALSE(UnsupportedWave.supported());
  WaveMatrixMultiplySupport InvalidWave = {
      static_cast<
          linalg_abi::D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAGS>(
          static_cast<UINT>(
              linalg_abi::
                  D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_SUPPORTED) |
          static_cast<UINT>(
              linalg_abi::
                  D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_EMULATED_INPUTS)),
  };
  VERIFY_IS_FALSE(InvalidWave.valid());

  ThreadGroupMatrixMultiplySupport ThreadGroup = {
      linalg_abi::D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_SUPPORTED,
      32,
      128,
      64,
  };
  VERIFY_IS_TRUE(ThreadGroup.valid());
  VERIFY_IS_TRUE(ThreadGroup.supportsThreadGroupSize(64));
  VERIFY_IS_FALSE(ThreadGroup.supportsThreadGroupSize(48));
  ThreadGroup.PreferredThreadGroupSize = 48;
  VERIFY_IS_FALSE(ThreadGroup.valid());
  ThreadGroup = {
      static_cast<
          linalg_abi::D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAGS>(
          static_cast<UINT>(
              linalg_abi::
                  D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_SUPPORTED) |
          static_cast<UINT>(
              linalg_abi::
                  D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_TRANSPOSE)),
      32,
      128,
      64,
  };
  VERIFY_IS_FALSE(ThreadGroup.valid());

  ThreadVectorMatrixMultiplySupport ThreadVector = {
      static_cast<
          linalg_abi::D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAGS>(
          static_cast<UINT>(
              linalg_abi::
                  D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_SUPPORTED) |
          static_cast<UINT>(
              linalg_abi::
                  D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_TRANSPOSE)),
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32,
  };
  VERIFY_IS_TRUE(ThreadVector.valid());
  VERIFY_IS_TRUE(ThreadVector.supported());
  ThreadVector.SupportFlags = static_cast<
      linalg_abi::D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAGS>(
      static_cast<UINT>(
          linalg_abi::
              D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_SUPPORTED) |
      static_cast<UINT>(
          linalg_abi::
              D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_EMULATED_INPUTS));
  VERIFY_IS_FALSE(ThreadVector.valid());
  ThreadVector.MatrixInputType =
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT8_E4M3FN;
  VERIFY_IS_TRUE(ThreadVector.valid());
  ThreadVector.SupportFlags = linalg_abi::
      D3D12_LINEAR_ALGEBRA_MULTIPLICATION_SUPPORT_FLAG_EMULATED_INPUTS;
  VERIFY_IS_FALSE(ThreadVector.valid());

  ThreadOuterProductSupport OuterProduct = {true};
  VERIFY_IS_TRUE(OuterProduct.supported());
  AtomicAccumulateStoreSupport Atomic = {true, false};
  VERIFY_IS_TRUE(Atomic.supports(AtomicDestination::RWByteAddressBuffer));
  VERIFY_IS_FALSE(Atomic.supports(AtomicDestination::GroupShared));

  VERIFY_ARE_EQUAL(
      0u, static_cast<UINT>(linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_NONE));
  MatrixConstructionQuery ConstructionQuery = {
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32, 32, {8, 8, 8}};
  WaveMatrixMultiplyInputs WaveInputs = {
      32,
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16,
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16,
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32,
  };
  WaveMatrixMultiplyQuery WaveQuery = {WaveInputs, {16, 16, 16}};
  ThreadGroupMatrixMultiplyQuery ThreadGroupQuery = {
      WaveInputs,
      {16, 16, 16},
  };
  ThreadVectorMatrixMultiplyQuery ThreadVectorQuery = {
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16,
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16,
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_NONE,
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16,
  };
  ThreadOuterProductQuery OuterProductQuery = {
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16,
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16,
  };
  AtomicAccumulateStoreQuery AtomicQuery = {
      linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16};
  VERIFY_ARE_EQUAL(32u, ConstructionQuery.WaveSize);
  VERIFY_ARE_EQUAL(8u, ConstructionQuery.Shape.K);
  VERIFY_ARE_EQUAL(32u, WaveQuery.Inputs.WaveSize);
  VERIFY_ARE_EQUAL(16u, WaveQuery.Shape.M);
  VERIFY_ARE_EQUAL(32u, ThreadGroupQuery.WaveInputs.WaveSize);
  VERIFY_ARE_EQUAL(16u, ThreadGroupQuery.Shape.M);
  VERIFY_IS_TRUE(ThreadVectorQuery.BiasInputType ==
                 linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_NONE);
  VERIFY_IS_TRUE(OuterProductQuery.InputComponentType ==
                 linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16);
  VERIFY_IS_TRUE(AtomicQuery.ComponentType ==
                 linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16);
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

  // Load/Store/Accumulate Memory
  TEST_METHOD(LoadMemory_Wave_16x16_F16);
  TEST_METHOD(StoreMemory_Wave_16x16_F16);
  TEST_METHOD(AccumulateMemory_Wave_16x16_F16);

  // Element access
  TEST_METHOD(ElementAccess_Wave_16x16_F16);
  TEST_METHOD(ElementAccess_Wave_4x8_F32);
  TEST_METHOD(ElementSet_Wave_16x16_F16);
  TEST_METHOD(ElementGetOOB_Wave_4x8_F32);
  TEST_METHOD(ElementSetOOB_Wave_4x8_F32);

  // Cast/Convert
  TEST_METHOD(CopyConvert_Wave_16x16_F16);
  TEST_METHOD(CopyConvert_Wave_16x16_F16_Transpose);
  TEST_METHOD(CopyConvert_Wave_4x8_F32_Transpose);

  // Matrix Matrix Arithmetic
  TEST_METHOD(MatMatMul_Wave_16x16x16_F16);
  TEST_METHOD(MatMatMulAccum_Wave_16x16x16_F16);
  TEST_METHOD(MatAccum_Wave_16x16_F16);

  // Matrix Vector Arithmetic
  TEST_METHOD(MatVecMul_Thread_16x16_F16);
  TEST_METHOD(MatVecMul_Thread_4x8_F32);
  TEST_METHOD(MatVecMulAdd_Thread_16x16_F16);
  TEST_METHOD(MatVecMulAdd_Thread_4x8_F32);
  TEST_METHOD(OuterProduct_Thread_16x16_F16);

  // Query Accumulator Layout
  TEST_METHOD(QueryAccumLayout);

  // Convert
  TEST_METHOD(Convert);

  // Vector Accumulate
  TEST_METHOD(VectorAccumulateDescriptor_Thread_F16);

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

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, LOAD_OFFSET, LOAD_STRIDE, LOAD_LAYOUT, 128);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, STORE_OFFSET, STORE_STRIDE, STORE_LAYOUT, 128);
  }
)";

static void
runLoadStoreDescriptor(ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
                       const MatrixParams &Params,
                       const cpu_oracle::MatrixBufferLayout &LoadLayout,
                       const cpu_oracle::MatrixBufferLayout &StoreLayout,
                       bool Verbose, UINT ForcedWaveSize = 0) {
  std::optional<cpu_oracle::TypedMatrix> Input =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N);
  VERIFY_IS_TRUE(Input.has_value(),
                 "Unable to construct typed LoadStoreDescriptor input");

  std::optional<size_t> InputSize =
      cpu_oracle::getMatrixBufferSize(*Input, LoadLayout);
  std::optional<size_t> OutputSize =
      cpu_oracle::getMatrixBufferSize(*Input, StoreLayout);
  VERIFY_IS_TRUE(InputSize.has_value() && OutputSize.has_value(),
                 "Unable to size the LoadStoreDescriptor buffers");

  std::stringstream ExtraDefs;
  ExtraDefs << " -DLOAD_OFFSET=" << LoadLayout.OffsetBytes;
  ExtraDefs << " -DLOAD_STRIDE=" << LoadLayout.StrideBytes;
  ExtraDefs << " -DLOAD_LAYOUT=" << static_cast<int>(LoadLayout.Layout);
  ExtraDefs << " -DSTORE_OFFSET=" << StoreLayout.OffsetBytes;
  ExtraDefs << " -DSTORE_STRIDE=" << StoreLayout.StrideBytes;
  ExtraDefs << " -DSTORE_LAYOUT=" << static_cast<int>(StoreLayout.Layout);

  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, LoadStoreDescriptorShader, "cs_6_10", Args,
                Verbose);

  const cpu_oracle::TypedMatrix InputMatrix = *Input;
  cpu_oracle::MatrixResultOracle Oracle = cpu_oracle::exactResult(
      InputMatrix, L"HLSL proposal 0035 MatrixLoadFromDescriptor and "
                   L"MatrixStoreToDescriptor "
                   L"round trip at the requested offset, stride and layout");

  // Two UAV buffers, load from one, store to the other. The destination is
  // filled by name rather than zeroed so unowned bytes carry the poison.
  auto Op = createComputeOp(LoadStoreDescriptorShader, "cs_6_10",
                            "UAV(u0), UAV(u1)", Args.c_str());
  addUAVBuffer(Op.get(), "Input", *InputSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", *OutputSize, true, "byname");
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [InputMatrix, LoadLayout](LPCSTR Name, std::vector<BYTE> &Data,
                                st::ShaderOp *) {
        cpu_oracle::fillPoison(Data.data(), Data.size());
        if (_stricmp(Name, "Input") != 0)
          return;
        VERIFY_IS_TRUE(
            cpu_oracle::writeMatrixBuffer(InputMatrix, LoadLayout, Data),
            "Unable to encode typed LoadStoreDescriptor input");
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(cpu_oracle::verifyMatrixBuffer(OutData.data(), OutData.size(),
                                                StoreLayout, Oracle, Verbose));
  VERIFY_IS_TRUE(cpu_oracle::verifyUntouchedBytes(
      Params.CompType, Params.M, Params.N, StoreLayout, OutData.data(),
      OutData.size(), Verbose));
}

// No offset and a tightly packed stride: a matrix occupying the whole buffer.
static cpu_oracle::MatrixBufferLayout packedLayout(const MatrixParams &Params) {
  return cpu_oracle::MatrixBufferLayout{
      Params.Layout,
      /*OffsetBytes=*/0,
      /*StrideBytes=*/Params.strideBytes(),
  };
}

void DxilConf_SM610_LinAlg::LoadStoreDescriptor_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(D3DDevice, Params, {Params.Use},
                                    L"LoadStoreDescriptor_Wave_16x16_F16",
                                    SelectedWaveSize))
    return;

  runLoadStoreDescriptor(D3DDevice, DxcSupport, Params, packedLayout(Params),
                         packedLayout(Params), VerboseLogging,
                         SelectedWaveSize);
}

static const char SplatStoreShader[] = R"(
  RWByteAddressBuffer Output : register(u0);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
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
                          bool Verbose, UINT ForcedWaveSize = 0) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  STREAM_FLOAT(ExtraDefs, "FILL_VALUE", FillValue);

  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, SplatStoreShader, "cs_6_10", Args, Verbose);

  auto Expected =
      makeExpectedMat(Params.CompType, Params.M, Params.N, FillValue, false);

  auto Op =
      createComputeOp(SplatStoreShader, "cs_6_10", "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Output");

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
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(D3DDevice, Params, {Params.Use},
                                    L"SplatStore_Wave_16x16_F16",
                                    SelectedWaveSize))
    return;

  runSplatStore(D3DDevice, DxcSupport, Params, 42.0f, VerboseLogging,
                SelectedWaveSize);
}

static const char AccumulateDescriptorShader[] = R"(
  #define USE_ACC 2

  ByteAddressBuffer Input : register(t0);
  RWByteAddressBuffer Output : register(u1);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
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
                                    bool Verbose, UINT ForcedWaveSize = 0) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, AccumulateDescriptorShader, "cs_6_10", Args,
                Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N,
                                  static_cast<float>(FillValue) * 2, false);

  auto Op = createComputeOp(AccumulateDescriptorShader, "cs_6_10",
                            "SRV(t0), UAV(u1)", Args.c_str());
  addSRVBuffer(Op.get(), "Input", BufferSize, "byname");
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

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
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(D3DDevice, Params, {Params.Use},
                                    L"AccumulateDescriptor_Wave_16x16_F16",
                                    SelectedWaveSize))
    return;
  if (!accumulateStoreApplicable(
          D3DDevice, Params.CompType,
          linalg_test::AtomicDestination::RWByteAddressBuffer,
          L"AccumulateDescriptor_Wave_16x16_F16"))
    return;

  runAccumulateDescriptor(D3DDevice, DxcSupport, Params, 12, VerboseLogging,
                          SelectedWaveSize);
}

// Element access constructs a wave-scope matrix and then reads or writes its
// components, so applicability is exactly MatrixConstruction for the tile the
// case declares. D3D12LinearAlgebraRuntimeFeatureSupport.md guarantees only
// that some shape whose largest component is 16 or less is reported for a
// supported type, and directs applications wanting smaller shapes to query
// them case by case. Neither Fp32 nor Fp16 matrices are required at Tier 1, so
// every element-access case is capability gated rather than mandatory.
static const char ElementAccessShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  // flatten the 2D index into a 1D index then scale by element size
  // Always store row-major and work it out in the test runner
  uint coordToByteOffset(uint2 coord) {
    return (coord.x * N_DIM + coord.y) * ELEM_SIZE;
  }

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (GetGroupWaveIndex() != 0)
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
                             const MatrixParams &Params, bool Verbose,
                             UINT ForcedWaveSize = 0) {
  const size_t NumElements = Params.totalElements();
  const size_t NumThreads = Params.NumThreads;
  const size_t MatrixSize = Params.totalBytes();
  // OutputBuf needs to fit the Matrix plus one uint per thread
  const size_t OutputBufSize = MatrixSize + NumThreads * sizeof(uint32_t);

  std::stringstream ExtraDefs;
  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;
  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, ElementAccessShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, 1);

  auto Op = createComputeOp(ElementAccessShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", MatrixSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", OutputBufSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

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
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(D3DDevice, Params, {Params.Use},
                                    L"ElementAccess_Wave_16x16_F16",
                                    SelectedWaveSize))
    return;

  runElementAccess(D3DDevice, DxcSupport, Params, VerboseLogging,
                   SelectedWaveSize);
}

void DxilConf_SM610_LinAlg::ElementAccess_Wave_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = false;

  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(D3DDevice, Params, {Params.Use},
                                    L"ElementAccess_Wave_4x8_F32",
                                    SelectedWaveSize))
    return;

  // Non-square dimensions make the row-major coordinate mapping observable: a
  // transposed GetCoordinate would land inside the matrix for a square tile
  // but out of it here.
  runElementAccess(D3DDevice, DxcSupport, Params, VerboseLogging,
                   SelectedWaveSize);
}

static const char ElementSetShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
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
                          const MatrixParams &Params, bool Verbose,
                          UINT ForcedWaveSize = 0) {
  const size_t NumElements = Params.totalElements();
  const size_t MatrixSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;
  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, ElementSetShader, "cs_6_10", Args, Verbose);

  // Start counting from 6 since each element was increased by 5
  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, 6);

  auto Op = createComputeOp(ElementSetShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", MatrixSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", MatrixSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

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
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(D3DDevice, Params, {Params.Use},
                                    L"ElementSet_Wave_16x16_F16",
                                    SelectedWaveSize))
    return;

  runElementSet(D3DDevice, DxcSupport, Params, VerboseLogging,
                SelectedWaveSize);
}

// Length() is thread local, so the first index past a lane's own length is
// already out of bounds even though the wave collectively holds more elements.
// Probing Length() and a index far beyond it covers both a driver that clamps
// only at the wave total and one that wraps a large index back into range.
static constexpr UINT FarOOBOffset = 64;

// Per-lane record: {uint Length, uint Executed, ELEM_TYPE Just, ELEM_TYPE Far}.
static constexpr UINT OOBRecordSize = 16;

// Seeds every output byte so a lane that never writes cannot be mistaken for a
// lane that correctly wrote the specified zero. The output buffer must be
// created "byname" for this to run at all: ShaderOpTest only invokes the
// initializer callback for that mode, and the default "zero" mode would leave
// the buffer holding exactly the value the out-of-bounds read is required to
// produce, making the comparison vacuous.
static constexpr BYTE OOBSentinelByte = 0xCD;

// Seeds the shader's destination locals. Distinct from zero, so a read that is
// dropped rather than performed cannot masquerade as a correct out-of-bounds
// result, and exactly representable in F32.
static constexpr int OOBGetPoisonValue = 999;

static const char ElementGetOOBShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, 0, STRIDE, LAYOUT, 128);

    uint Len = __builtin_LinAlg_MatrixLength(Mat);

    // Seeded so that a dropped read leaves a value distinguishable from the
    // zero a correct out-of-bounds read must produce.
    ELEM_TYPE Just = (ELEM_TYPE)POISON_VALUE;
    __builtin_LinAlg_MatrixGetElement(Just, Mat, Len);
    ELEM_TYPE Far = (ELEM_TYPE)POISON_VALUE;
    __builtin_LinAlg_MatrixGetElement(Far, Mat, Len + FAR_OOB_OFFSET);

    // Record unconditionally so the runner can tell that this lane ran.
    uint Base = threadID * OOB_RECORD_SIZE;
    Output.Store<uint>(Base + 0, Len);
    Output.Store<uint>(Base + 4, 1);
    Output.Store<ELEM_TYPE>(Base + 8, Just);
    Output.Store<ELEM_TYPE>(Base + 12, Far);
  }
)";

// Reads back the {Length, Executed} half of each lane record and checks the
// wave actually ran. Returns the total element count the wave reported.
static uint32_t verifyOOBLaneRecords(const BYTE *Records, size_t NumThreads,
                                     UINT SelectedWaveSize, UINT RecordStride,
                                     size_t NumElements, bool Verbose) {
  uint32_t ExecutedLanes = 0;
  uint32_t TotalLength = 0;
  for (size_t I = 0; I < NumThreads; ++I) {
    const BYTE *Record = Records + I * RecordStride;
    uint32_t Length = 0;
    uint32_t Executed = 0;
    memcpy(&Length, Record, sizeof(Length));
    memcpy(&Executed, Record + 4, sizeof(Executed));
    if (Executed != 1)
      continue;
    ++ExecutedLanes;
    TotalLength += Length;
    if (Verbose)
      hlsl_test::LogCommentFmt(L"lane %u reported Length=%u",
                               static_cast<UINT>(I), Length);
  }

  // Only wave 0 runs, so exactly the lanes of the wave the capability query
  // selected must have written a record.
  VERIFY_ARE_EQUAL(ExecutedLanes, SelectedWaveSize,
                   "Every lane of the selected wave must execute");
  VERIFY_IS_GREATER_THAN_OR_EQUAL(
      TotalLength, static_cast<uint32_t>(NumElements),
      "Sum of all lengths must be gte num elements");
  return TotalLength;
}

static void runElementGetOOB(ID3D12Device *Device,
                             dxc::SpecificDllLoader &DxcSupport,
                             const MatrixParams &Params, bool Verbose,
                             UINT ForcedWaveSize) {
  VERIFY_IS_TRUE(Params.CompType == ComponentType::F32,
                 "Out-of-bounds Get records assume a 4-byte element");
  const size_t NumElements = Params.totalElements();
  const size_t NumThreads = Params.NumThreads;
  const size_t MatrixSize = Params.totalBytes();
  const size_t OutputBufSize = NumThreads * OOBRecordSize;

  std::stringstream ExtraDefs;
  ExtraDefs << " -DFAR_OOB_OFFSET=" << FarOOBOffset;
  ExtraDefs << " -DOOB_RECORD_SIZE=" << OOBRecordSize;
  ExtraDefs << " -DPOISON_VALUE=" << OOBGetPoisonValue;
  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;
  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, ElementGetOOBShader, "cs_6_10", Args, Verbose);

  auto Op = createComputeOp(ElementGetOOBShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", MatrixSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", OutputBufSize, true, "byname");
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

  auto Result =
      runShaderOp(Device, DxcSupport, std::move(Op),
                  [NumElements, Params](LPCSTR Name, std::vector<BYTE> &Data,
                                        st::ShaderOp *) {
                    if (_stricmp(Name, "Output") == 0) {
                      std::fill(Data.begin(), Data.end(), OOBSentinelByte);
                      return;
                    }
                    VERIFY_IS_TRUE(fillInputBuffer(Name, Data, Params.CompType,
                                                   NumElements),
                                   "Saw unsupported component type");
                  });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  const BYTE *Out = static_cast<const BYTE *>(OutData.data());

  verifyOOBLaneRecords(Out, NumThreads, ForcedWaveSize, OOBRecordSize,
                       NumElements, Verbose);

  // 0035-linalg-matrix.md: reading an index outside [0, Length()-1] yields
  // zero cast to the element type.
  for (size_t I = 0; I < NumThreads; ++I) {
    const BYTE *Record = Out + I * OOBRecordSize;
    uint32_t Executed = 0;
    memcpy(&Executed, Record + 4, sizeof(Executed));
    if (Executed != 1)
      continue;

    float Just = 0.0f;
    float Far = 0.0f;
    memcpy(&Just, Record + 8, sizeof(Just));
    memcpy(&Far, Record + 12, sizeof(Far));
    VERIFY_ARE_EQUAL(Just, 0.0f,
                     "Get at Length() must return zero cast to the element "
                     "type");
    VERIFY_ARE_EQUAL(Far, 0.0f,
                     "Get far past Length() must return zero cast to the "
                     "element type");
  }
}

void DxilConf_SM610_LinAlg::ElementGetOOB_Wave_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = false;

  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(D3DDevice, Params, {Params.Use},
                                    L"ElementGetOOB_Wave_4x8_F32",
                                    SelectedWaveSize))
    return;

  runElementGetOOB(D3DDevice, DxcSupport, Params, VerboseLogging,
                   SelectedWaveSize);
}

static const char ElementSetOOBShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, 0, STRIDE, LAYOUT, 128);

    uint Len = __builtin_LinAlg_MatrixLength(Mat);

    // Both indices are outside this lane's range, so both writes must be
    // no-ops and the stored matrix must still equal the loaded one.
    __builtin_LinAlg_MatrixSetElement(Mat, Mat, Len, (ELEM_TYPE)POISON_VALUE);
    __builtin_LinAlg_MatrixSetElement(Mat, Mat, Len + FAR_OOB_OFFSET,
                                      (ELEM_TYPE)POISON_VALUE);

    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, 0, STRIDE, LAYOUT, 128);

    uint Base = MATRIX_BYTES + threadID * OOB_RECORD_SIZE;
    Output.Store<uint>(Base + 0, Len);
    Output.Store<uint>(Base + 4, 1);
  }
)";

static void runElementSetOOB(ID3D12Device *Device,
                             dxc::SpecificDllLoader &DxcSupport,
                             const MatrixParams &Params, bool Verbose,
                             UINT ForcedWaveSize) {
  const size_t NumElements = Params.totalElements();
  const size_t NumThreads = Params.NumThreads;
  const size_t MatrixSize = Params.totalBytes();
  const size_t OutputBufSize = MatrixSize + NumThreads * OOBRecordSize;

  // Distinct from every sequential input value, so a stray write is visible.
  const int PoisonValue = 999;

  std::stringstream ExtraDefs;
  ExtraDefs << " -DFAR_OOB_OFFSET=" << FarOOBOffset;
  ExtraDefs << " -DOOB_RECORD_SIZE=" << OOBRecordSize;
  ExtraDefs << " -DMATRIX_BYTES=" << MatrixSize;
  ExtraDefs << " -DPOISON_VALUE=" << PoisonValue;
  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;
  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, ElementSetOOBShader, "cs_6_10", Args, Verbose);

  // The matrix must come back exactly as it went in.
  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, 1);

  auto Op = createComputeOp(ElementSetOOBShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", MatrixSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", OutputBufSize, true, "byname");
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

  auto Result =
      runShaderOp(Device, DxcSupport, std::move(Op),
                  [NumElements, Params](LPCSTR Name, std::vector<BYTE> &Data,
                                        st::ShaderOp *) {
                    if (_stricmp(Name, "Output") == 0) {
                      std::fill(Data.begin(), Data.end(), OOBSentinelByte);
                      return;
                    }
                    VERIFY_IS_TRUE(fillInputBuffer(Name, Data, Params.CompType,
                                                   NumElements),
                                   "Saw unsupported component type");
                  });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  const BYTE *Out = static_cast<const BYTE *>(OutData.data());

  verifyOOBLaneRecords(Out + MatrixSize, NumThreads, ForcedWaveSize,
                       OOBRecordSize, NumElements, Verbose);

  // 0035-linalg-matrix.md: setting an index outside [0, Length()-1] is a
  // no-op, so no poisoned value may appear anywhere in the matrix.
  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::ElementSetOOB_Wave_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = false;

  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(D3DDevice, Params, {Params.Use},
                                    L"ElementSetOOB_Wave_4x8_F32",
                                    SelectedWaveSize))
    return;

  runElementSetOOB(D3DDevice, DxcSupport, Params, VerboseLogging,
                   SelectedWaveSize);
}

static const char CopyConvertShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Src;
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, DST_M_DIM, DST_N_DIM, USE, SCOPE)]]
      Dst;

    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Src, Input, 0, SRC_STRIDE, LAYOUT, 128);
    __builtin_LinAlg_CopyConvertMatrix(Dst, Src, TRANSPOSE);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Dst, Output, 0, DST_STRIDE, LAYOUT, 128);
  }
)";

static HRESULT selectCopyConvertWaveSize(ID3D12Device *Device,
                                         const MatrixParams &Params,
                                         bool Transpose, bool &Supported,
                                         UINT &SelectedWaveSize) {
  Supported = false;
  SelectedWaveSize = 0;
  if (!Device || Params.Use != MatrixUse::A ||
      !linalg_test::isLegalScope(
          linalg_abi::D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_MATRIX_CONSTRUCTION,
          Params.Scope))
    return E_INVALIDARG;

  std::optional<linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE> DataType =
      toCapabilityDataType(Params.CompType);
  if (!DataType.has_value())
    return E_INVALIDARG;

  linalg_test::TierSupport Tier;
  HRESULT HR = linalg_test::queryTierSupport(Device, Tier);
  if (FAILED(HR) || !Tier.supported())
    return HR;

  UINT MinWaveSize = 0;
  UINT MaxWaveSize = 0;
  HR = queryLaunchableWaveSizes(Device, MinWaveSize, MaxWaveSize);
  if (FAILED(HR))
    return HR;
  if (MinWaveSize == 0) {
    hlsl_test::LogCommentFmt(
        L"Wave operations are unsupported; MatrixConstruction is not "
        L"applicable");
    return S_OK;
  }

  MatrixParams Destination = Params;
  if (Transpose) {
    Destination.M = Params.N;
    Destination.N = Params.M;
  }

  for (UINT WaveSize = 4; WaveSize <= 128; WaveSize *= 2) {
    if (WaveSize < MinWaveSize || WaveSize > MaxWaveSize ||
        WaveSize > static_cast<UINT>(Params.NumThreads))
      continue;

    bool SourceSupported = false;
    HR = supportsMatrixShape(Device, *DataType, WaveSize, MatrixUse::A,
                             Params.M, Params.N, SourceSupported);
    if (FAILED(HR))
      return HR;

    bool DestinationSupported = false;
    HR =
        supportsMatrixShape(Device, *DataType, WaveSize, MatrixUse::A,
                            Destination.M, Destination.N, DestinationSupported);
    if (FAILED(HR))
      return HR;

    if (SourceSupported && DestinationSupported) {
      hlsl_test::LogCommentFmt(
          L"CopyConvert capability matched wave=%u for source=%ux%u and "
          L"destination=%ux%u",
          WaveSize, Params.M, Params.N, Destination.M, Destination.N);
      Supported = true;
      SelectedWaveSize = WaveSize;
      return S_OK;
    }
  }

  hlsl_test::LogCommentFmt(
      L"No MatrixConstruction query supports CopyConvert source=%ux%u and "
      L"destination=%ux%u for any wave size launchable within shader "
      L"WaveSize(4,128) and a %d-thread group",
      Params.M, Params.N, Destination.M, Destination.N, Params.NumThreads);
  return S_OK;
}

static bool copyConvertApplicable(ID3D12Device *Device,
                                  const MatrixParams &Params, bool Transpose,
                                  LPCWSTR CaseName, UINT &SelectedWaveSize) {
  bool Supported = false;
  const HRESULT QueryResult = selectCopyConvertWaveSize(
      Device, Params, Transpose, Supported, SelectedWaveSize);
  if (!applyApplicability(
          linalg_test::classifyApplicability(
              QueryResult, Supported,
              linalg_test::CapabilityRequirement::CapabilityGated),
          CaseName))
    return false;

  VERIFY_IS_TRUE(SelectedWaveSize != 0,
                 "A case cleared to run must have a selected wave size");
  return true;
}

static void runCopyConvert(ID3D12Device *Device,
                           dxc::SpecificDllLoader &DxcSupport,
                           const MatrixParams &Params, bool Verbose,
                           bool Transpose, UINT ForcedWaveSize = 0) {
  MatrixParams DstParams = Params;
  if (Transpose) {
    DstParams.M = Params.N;
    DstParams.N = Params.M;
  }

  std::stringstream ExtraDefs;
  ExtraDefs << " -DTRANSPOSE=" << Transpose;
  ExtraDefs << " -DDST_M_DIM=" << DstParams.M;
  ExtraDefs << " -DDST_N_DIM=" << DstParams.N;
  ExtraDefs << " -DSRC_STRIDE=" << Params.strideBytes();
  ExtraDefs << " -DDST_STRIDE=" << DstParams.strideBytes();
  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, CopyConvertShader, "cs_6_10", Args, Verbose);

  std::optional<cpu_oracle::TypedMatrix> Input =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N);
  VERIFY_IS_TRUE(Input.has_value(),
                 "Unable to construct typed CopyConvert input");
  std::optional<cpu_oracle::TypedMatrix> Expected =
      Transpose ? cpu_oracle::transposeMatrix(*Input) : Input;
  VERIFY_IS_TRUE(Expected.has_value(),
                 "Unable to construct independent CopyConvert oracle");

  cpu_oracle::MatrixBufferLayout SourceLayout = {
      Params.Layout,
      /*OffsetBytes=*/0,
      /*StrideBytes=*/Params.strideBytes(),
  };
  cpu_oracle::MatrixBufferLayout DestinationLayout = {
      DstParams.Layout,
      /*OffsetBytes=*/0,
      /*StrideBytes=*/DstParams.strideBytes(),
  };
  std::optional<size_t> SourceBufferSize =
      cpu_oracle::getMatrixBufferSize(*Input, SourceLayout);
  std::optional<size_t> DestinationBufferSize =
      cpu_oracle::getMatrixBufferSize(*Expected, DestinationLayout);
  VERIFY_IS_TRUE(SourceBufferSize.has_value(),
                 "Unable to size typed CopyConvert input");
  VERIFY_IS_TRUE(DestinationBufferSize.has_value(),
                 "Unable to size typed CopyConvert output");

  cpu_oracle::TypedMatrix InputMatrix = *Input;
  cpu_oracle::MatrixResultOracle Oracle = cpu_oracle::exactResult(
      *Expected,
      L"HLSL proposal 0035 CopyConvertMatrix transpose and descriptor layout");

  // Construct the ShaderOp: two UAV buffers, load from one, store to other.
  auto Op = createComputeOp(CopyConvertShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", *SourceBufferSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", *DestinationBufferSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [InputMatrix, SourceLayout](LPCSTR Name, std::vector<BYTE> &Data,
                                  st::ShaderOp *) {
        if (_stricmp(Name, "Input") != 0)
          return;
        VERIFY_IS_TRUE(
            cpu_oracle::writeMatrixBuffer(InputMatrix, SourceLayout, Data),
            "Unable to encode typed CopyConvert input");
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(cpu_oracle::verifyMatrixBuffer(
      OutData.data(), OutData.size(), DestinationLayout, Oracle, Verbose));
}

void DxilConf_SM610_LinAlg::CopyConvert_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!copyConvertApplicable(D3DDevice, Params, /*Transpose=*/false,
                             L"CopyConvert_Wave_16x16_F16", SelectedWaveSize))
    return;

  runCopyConvert(D3DDevice, DxcSupport, Params, VerboseLogging,
                 /*Transpose=*/false, SelectedWaveSize);
}

void DxilConf_SM610_LinAlg::CopyConvert_Wave_16x16_F16_Transpose() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!copyConvertApplicable(D3DDevice, Params, /*Transpose=*/true,
                             L"CopyConvert_Wave_16x16_F16_Transpose",
                             SelectedWaveSize))
    return;

  runCopyConvert(D3DDevice, DxcSupport, Params, VerboseLogging,
                 /*Transpose=*/true, SelectedWaveSize);
}

void DxilConf_SM610_LinAlg::CopyConvert_Wave_4x8_F32_Transpose() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = false;

  UINT SelectedWaveSize = 0;
  if (!copyConvertApplicable(D3DDevice, Params, /*Transpose=*/true,
                             L"CopyConvert_Wave_4x8_F32_Transpose",
                             SelectedWaveSize))
    return;

  // Non-square dimensions make the destination shape and row stride observable.
  runCopyConvert(D3DDevice, DxcSupport, Params, VerboseLogging,
                 /*Transpose=*/true, SelectedWaveSize);
}

static const char MatMatMulShader[] = R"(
  #define USE_A 0
  #define USE_B 1
  #define USE_ACC 2

  RWByteAddressBuffer Output : register(u0);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
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
                         float AFill, float BFill, UINT ForcedWaveSize = 0) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DK_DIM=" << K;
  STREAM_FLOAT(ExtraDefs, "A_FILL", AFill);
  STREAM_FLOAT(ExtraDefs, "B_FILL", BFill);

  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, MatMatMulShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N,
                                  AFill * BFill * K, /*Increment=*/false);

  auto Op =
      createComputeOp(MatMatMulShader, "cs_6_10", "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Output");

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
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!waveMatMulApplicable(D3DDevice, Params, /*K=*/16,
                            L"MatMatMul_Wave_16x16x16_F16", SelectedWaveSize))
    return;

  runMatMatMul(D3DDevice, DxcSupport, Params, VerboseLogging, /*K=*/16,
               /*AFill=*/2.0f, /*BFill=*/3.0f, SelectedWaveSize);
}

static const char MatMatMulAccumShader[] = R"(
  #define USE_A 0
  #define USE_B 1
  #define USE_ACC 2

  RWByteAddressBuffer Output : register(u0);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
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
                              float CFill, UINT ForcedWaveSize = 0) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DK_DIM=" << K;
  STREAM_FLOAT(ExtraDefs, "A_FILL", AFill);
  STREAM_FLOAT(ExtraDefs, "B_FILL", BFill);
  STREAM_FLOAT(ExtraDefs, "C_FILL", CFill);

  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, MatMatMulAccumShader, "cs_6_10", Args, Verbose);

  auto Expected =
      makeExpectedMat(Params.CompType, Params.M, Params.N,
                      AFill * BFill * K + CFill, /*Increment=*/false);

  auto Op =
      createComputeOp(MatMatMulAccumShader, "cs_6_10", "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Output");

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
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!waveMatMulApplicable(D3DDevice, Params, /*K=*/16,
                            L"MatMatMulAccum_Wave_16x16x16_F16",
                            SelectedWaveSize))
    return;

  runMatMatMulAccum(D3DDevice, DxcSupport, Params, VerboseLogging, /*K=*/16,
                    /*AFill=*/2.0f, /*BFill=*/3.0f, /*CFill=*/4.0f,
                    SelectedWaveSize);
}

static const char MatAccumShader[] = R"(
  #define USE_A 0
  #define USE_ACC 2

  RWByteAddressBuffer Output : register(u0);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
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
                        float RHSFill, UINT ForcedWaveSize = 0) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  STREAM_FLOAT(ExtraDefs, "LHS_FILL", LHSFill);
  STREAM_FLOAT(ExtraDefs, "RHS_FILL", RHSFill);

  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, MatAccumShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N,
                                  LHSFill + RHSFill, /*Increment=*/false);

  auto Op = createComputeOp(MatAccumShader, "cs_6_10", "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Output");

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
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  // MatAccum builds both an accumulator and an A matrix, and the two roles pin
  // different extents of the same shape, so both must be constructible.
  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(
          D3DDevice, Params, {MatrixUse::Accumulator, MatrixUse::A},
          L"MatAccum_Wave_16x16_F16", SelectedWaveSize))
    return;

  runMatAccum(D3DDevice, DxcSupport, Params, VerboseLogging,
              /*LHSFill=*/2.0f, /*RHSFill=*/3.0f, SelectedWaveSize);
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

    vector<ELEM_TYPE, N_DIM> InVec;
    for (uint I = 0; I < N_DIM; ++I) {
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

// Thread-scope vector-matrix multiplication is described entirely by its type
// combination. D3D12LinearAlgebraRuntimeFeatureSupport.md scopes
// MatrixConstruction to "wave-scope and group-scope matrices" and states there
// is no requirement around thread-scope vector-matrix multiplication
// dimensions, which is why neither the support struct nor the enumeration
// entry for this operation carries a shape. Applicability therefore rests on
// ThreadVectorMatrixMultiply alone.
static HRESULT queryMatVecMulSupport(ID3D12Device *Device,
                                     const MatrixParams &Params,
                                     ComponentType InputInterp, bool HasBias,
                                     bool &TierSupported, bool &Supported) {
  TierSupported = false;
  Supported = false;
  if (!Device ||
      !linalg_test::isLegalScope(
          linalg_abi::
              D3D12_LINEAR_ALGEBRA_OPERATION_TYPE_THREAD_VECTOR_MATRIX_MULTIPLY,
          Params.Scope))
    return E_INVALIDARG;

  const std::optional<linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE> MatrixType =
      toCapabilityDataType(Params.CompType);
  const std::optional<linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE> VectorType =
      toCapabilityDataType(InputInterp);
  if (!MatrixType.has_value() || !VectorType.has_value())
    return E_INVALIDARG;

  linalg_test::TierSupport Tier;
  HRESULT HR = linalg_test::queryTierSupport(Device, Tier);
  if (FAILED(HR))
    return HR;
  TierSupported = Tier.supported();
  if (!TierSupported)
    return S_OK;

  // The shaders declare the bias and result vectors with the matrix component
  // type. A multiply with no bias is expressed as DATATYPE_NONE, which Tier 1
  // requires alongside a bias type matching the result type.
  const linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE BiasType =
      HasBias ? *MatrixType : linalg_abi::D3D12_LINEAR_ALGEBRA_DATATYPE_NONE;

  linalg_test::ThreadVectorMatrixMultiplySupport Multiply;
  HR = linalg_test::queryThreadVectorMatrixMultiply(
      Device, {*VectorType, *MatrixType, BiasType, *MatrixType}, Multiply);
  if (FAILED(HR))
    return HR;
  if (!Multiply.supported()) {
    hlsl_test::LogCommentFmt(
        L"ThreadVectorMatrixMultiply reports vector=%u matrix=%u bias=%u "
        L"result=%u is unsupported",
        static_cast<UINT>(*VectorType), static_cast<UINT>(*MatrixType),
        static_cast<UINT>(BiasType), static_cast<UINT>(*MatrixType));
    return S_OK;
  }

  Supported = true;
  return S_OK;
}

static bool matVecMulApplicable(ID3D12Device *Device,
                                const MatrixParams &Params,
                                ComponentType InputInterp, bool HasBias,
                                linalg_test::CapabilityRequirement Requirement,
                                LPCWSTR CaseName) {
  bool TierSupported = false;
  bool Supported = false;
  const HRESULT QueryResult = queryMatVecMulSupport(
      Device, Params, InputInterp, HasBias, TierSupported, Supported);

  // A device that does not implement linear algebra at all is outside the
  // Tier 1 requirements, so it skips rather than failing even where the
  // configuration is mandatory.
  const linalg_test::CapabilityRequirement Effective =
      SUCCEEDED(QueryResult) && !TierSupported
          ? linalg_test::CapabilityRequirement::CapabilityGated
          : Requirement;

  return applyApplicability(
      linalg_test::classifyApplicability(QueryResult, Supported, Effective),
      CaseName);
}

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
  addSRVBuffer(Op.get(), "Input", BufferSize, "byname");
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

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
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 1;
  Params.Enable16Bit = true;

  // Tier 1 requires Fp16 vector x Fp16 matrix -> Fp16, and requires a bias
  // matching the result type as well as no bias at all, so a Tier 1 device
  // reporting this unsupported is a conformance failure rather than a skip.
  if (!matVecMulApplicable(D3DDevice, Params, ComponentType::F16,
                           /*HasBias=*/false,
                           linalg_test::CapabilityRequirement::Mandatory,
                           L"MatVecMul_Thread_16x16_F16"))
    return;

  runMatVecMul(D3DDevice, DxcSupport, Params, VerboseLogging,
               /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F16);
}

void DxilConf_SM610_LinAlg::MatVecMul_Thread_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Scope = MatrixScope::Thread;
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 1;

  // Fp32 vector x Fp32 matrix -> Fp32 is absent from the Tier 1 table, so it
  // is optional and a device reporting it unsupported skips.
  if (!matVecMulApplicable(D3DDevice, Params, ComponentType::F32,
                           /*HasBias=*/false,
                           linalg_test::CapabilityRequirement::CapabilityGated,
                           L"MatVecMul_Thread_4x8_F32"))
    return;

  runMatVecMul(D3DDevice, DxcSupport, Params, VerboseLogging,
               /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F32);
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

    vector<ELEM_TYPE, N_DIM> InVec;
    for (uint I = 0; I < N_DIM; ++I) {
      InVec[I] = Input.Load<ELEM_TYPE>(I * ELEM_SIZE);
    }

    vector<ELEM_TYPE, M_DIM> BiasVec;
    for (uint I = 0; I < M_DIM; ++I) {
      BiasVec[I] = Input.Load<ELEM_TYPE>(I * ELEM_SIZE);
    }

    vector<ELEM_TYPE, M_DIM> OutVec;
    __builtin_LinAlg_MatrixVectorMultiplyAdd(
      OutVec, Mat, OUTPUT_SIGNED, InVec, IN_INTERP, BiasVec);

    for (uint I = 0; I < M_DIM; ++I) {
      Output.Store<ELEM_TYPE>(I * ELEM_SIZE, OutVec[I]);
    }
  }
)";

static void runMatVecMulAdd(ID3D12Device *Device,
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

  compileShader(DxcSupport, MatVecMulAddShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedVec(
      Params.CompType, Params.M,
      static_cast<float>(FillValue * FillValue * Params.N + FillValue),
      /*Increment=*/false);

  auto Op = createComputeOp(MatVecMulAddShader, "cs_6_10", "SRV(t0), UAV(u1)",
                            Args.c_str());
  addSRVBuffer(Op.get(), "Input", BufferSize, "byname");
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

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
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 1;
  Params.Enable16Bit = true;

  // Required by Tier 1: Fp16 throughout, with a bias matching the result type.
  if (!matVecMulApplicable(D3DDevice, Params, ComponentType::F16,
                           /*HasBias=*/true,
                           linalg_test::CapabilityRequirement::Mandatory,
                           L"MatVecMulAdd_Thread_16x16_F16"))
    return;

  runMatVecMulAdd(D3DDevice, DxcSupport, Params, VerboseLogging,
                  /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F16);
}

void DxilConf_SM610_LinAlg::MatVecMulAdd_Thread_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Scope = MatrixScope::Thread;
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 1;

  // Optional: see MatVecMul_Thread_4x8_F32.
  if (!matVecMulApplicable(D3DDevice, Params, ComponentType::F32,
                           /*HasBias=*/true,
                           linalg_test::CapabilityRequirement::CapabilityGated,
                           L"MatVecMulAdd_Thread_4x8_F32"))
    return;

  runMatVecMulAdd(D3DDevice, DxcSupport, Params, VerboseLogging,
                  /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F32);
}

// Map a DXIL ComponentType to the D3D12 linear-algebra datatype used by the
// host-side matrix conversion API.
#if defined(DIRECT3D_LINEAR_ALGEBRA)
static D3D12_LINEAR_ALGEBRA_DATATYPE toLinAlgDataType(ComponentType CT) {
  switch (CT) {
  case ComponentType::F16:
    return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;
  case ComponentType::F32:
    return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32;
  case ComponentType::I16:
    return D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16;
  case ComponentType::U16:
    return D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16;
  case ComponentType::I32:
    return D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32;
  case ComponentType::U32:
    return D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32;
  default:
    VERIFY_IS_TRUE(false, "Unsupported component type for linalg conversion");
    return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;
  }
}

static const char OuterProductShader[] = R"(
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
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE_THREAD)]]
      Mat;
    __builtin_LinAlg_MatrixOuterProduct(Mat, VecA, VecB);

    // Outer product accumulators are stored in the OuterProductOptimal layout
    // with stride 0 and no alignment requirement (align 0), matching the
    // dx::linalg header's thread-scoped InterlockedAccumulate.
    __builtin_LinAlg_MatrixAccumulateToDescriptor(
      Mat, Output, 0, STRIDE, LAYOUT, 0);
  }
)";

static void runOuterProduct(ID3D12Device *Device,
                            dxc::SpecificDllLoader &DxcSupport,
                            const MatrixParams &Params, bool Verbose) {
  VERIFY_IS_TRUE(
      Params.Layout == MatrixLayout::OuterProductOptimal,
      "Outer product must output its matrix in OuterProductOptimal layout");
  VERIFY_IS_TRUE(Params.Use == MatrixUse::Accumulator,
                 "Outer product must output an accumulator matrix");
  const size_t NumVecElements = Params.M + Params.N;
  const size_t InBuffSize = NumVecElements * elementSize(Params.CompType);
  const size_t NumMatElements = Params.totalElements();
  const D3D12_LINEAR_ALGEBRA_DATATYPE DataType =
      toLinAlgDataType(Params.CompType);

  const UINT OutBufferSize = getLinAlgMatrixByteSize(
      Device, Params.M, Params.N, DataType,
      D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL, /*Stride=*/0);

  const UINT RowMajorStride =
      static_cast<UINT>(Params.N * elementSize(Params.CompType));
  const UINT RowMajorSize = getLinAlgMatrixByteSize(
      Device, Params.M, Params.N, DataType,
      D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_ROW_MAJOR, RowMajorStride);

  std::string Args = buildCompilerArgs(Params);

  compileShader(DxcSupport, OuterProductShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, 4,
                                  /*Increment=*/false);

  auto Op = createComputeOp(OuterProductShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", InBuffSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", OutBufferSize, /*ReadBack=*/false);
  addUAVBuffer(Op.get(), "OutputRowMajor", RowMajorSize, /*ReadBack=*/true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [NumVecElements, Params](LPCSTR Name, std::vector<BYTE> &Data,
                               st::ShaderOp *) {
        VERIFY_IS_TRUE(fillInputBuffer(Name, Data, Params.CompType,
                                       NumVecElements,
                                       /*StartingVal=*/2, /*Increment=*/false),
                       "Saw unsupported component type");
      },
      [OutBufferSize, RowMajorSize, RowMajorStride, DataType,
       Params](ID3D12GraphicsCommandList *List, st::ShaderOpTest *Test) {
        ID3D12Resource *OptimalBuffer = nullptr;
        ID3D12Resource *RowMajorBuffer = nullptr;
        Test->GetResource("Output", &OptimalBuffer);
        Test->GetResource("OutputRowMajor", &RowMajorBuffer);
        recordLinAlgMatrixConversion(
            List, OptimalBuffer, OutBufferSize, RowMajorBuffer, RowMajorSize,
            Params.M, Params.N, DataType,
            D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL,
            /*SrcStride=*/0, D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_ROW_MAJOR,
            RowMajorStride);
      });

  MappedData OutData;
  Result->Test->GetReadBackData("OutputRowMajor", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(Params.CompType, OutData.data(),
                                       Expected, NumMatElements, Verbose));
}
#endif // defined(DIRECT3D_LINEAR_ALGEBRA)

void DxilConf_SM610_LinAlg::OuterProduct_Thread_16x16_F16() {
#if defined(DIRECT3D_LINEAR_ALGEBRA)
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Thread;
  Params.Layout = MatrixLayout::OuterProductOptimal;
  Params.NumThreads = 1;
  Params.Enable16Bit = true;

  // Tier 1 requires no outer product formats at all, so this is gated.
  if (!outerProductApplicable(D3DDevice, Params.CompType, Params.CompType,
                              L"OuterProduct_Thread_16x16_F16"))
    return;

  // The shader accumulates its result into an RWByteAddressBuffer, which is
  // reported independently of the outer product itself. A device may produce
  // the outer product yet not support accumulating this component type into a
  // buffer, so the destination has to be gated too or that device fails to
  // create the pipeline instead of skipping.
  if (!accumulateStoreApplicable(
          D3DDevice, Params.CompType,
          linalg_test::AtomicDestination::RWByteAddressBuffer,
          L"OuterProduct_Thread_16x16_F16"))
    return;

  runOuterProduct(D3DDevice, DxcSupport, Params, VerboseLogging);
#else
#ifdef _HLK_CONF
  // HLK forbids skipping, so treat the missing linear-algebra matrix-conversion
  // API as a failure rather than emitting a (compiled-out) skip.
  hlsl_test::LogErrorFmt(L"OuterProduct_Thread_16x16_F16 requires the "
                         L"linear-algebra matrix-conversion API "
                         L"(DIRECT3D_LINEAR_ALGEBRA), which this build lacks");
#else
  WEX::Logging::Log::Comment(
      L"Skipping OuterProduct_Thread_16x16_F16: built against a D3D12 SDK "
      L"without the linear-algebra matrix-conversion API "
      L"(DIRECT3D_LINEAR_ALGEBRA undefined); the host-side conversion helpers "
      L"are compiled out.");
  WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
#endif // _HLK_CONF
#endif // defined(DIRECT3D_LINEAR_ALGEBRA)
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
  addRootView(Op.get(), 0, "Output");

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
  // Constructs no matrix, so tier support is the only capability it needs.
  if (!linAlgTierApplicable(D3DDevice, L"QueryAccumLayout"))
    return;

  runQueryAccumLayout(D3DDevice, DxcSupport, VerboseLogging);
}

static const char LoadMemoryShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);
  groupshared ELEM_TYPE GsData[M_DIM * N_DIM];

  #define ELEM_PER_THREAD (M_DIM * N_DIM / NUMTHREADS)

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    for (uint I = 0; I < ELEM_PER_THREAD; ++I) {
      uint Index = threadID * ELEM_PER_THREAD + I;
      GsData[Index] = Input.Load<ELEM_TYPE>(Index * ELEM_SIZE);
    }

    GroupMemoryBarrierWithGroupSync();

    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromMemory(
      Mat, GsData, OFFSET / ELEM_SIZE, STRIDE / ELEM_SIZE, LAYOUT);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, OFFSET, STRIDE, LAYOUT, 128);
  }
)";

static void runLoadMemory(ID3D12Device *Device,
                          dxc::SpecificDllLoader &DxcSupport,
                          const MatrixParams &Params, bool Verbose,
                          UINT ForcedWaveSize = 0) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DOFFSET=" << 0;

  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, LoadMemoryShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N, 1);

  auto Op = createComputeOp(LoadMemoryShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Input", BufferSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

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

void DxilConf_SM610_LinAlg::LoadMemory_Wave_16x16_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 16;
  Params.N = 16;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(D3DDevice, Params, {Params.Use},
                                    L"LoadMemory_Wave_16x16_F16",
                                    SelectedWaveSize))
    return;

  runLoadMemory(D3DDevice, DxcSupport, Params, VerboseLogging,
                SelectedWaveSize);
}

static const char StoreMemoryShader[] = R"(
  RWByteAddressBuffer Output : register(u0);
  groupshared ELEM_TYPE GsData[M_DIM * N_DIM];

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_FillMatrix(Mat, FILL_VALUE);

    __builtin_LinAlg_MatrixStoreToMemory(
      Mat, GsData, OFFSET / ELEM_SIZE, STRIDE / ELEM_SIZE, LAYOUT);

    for (uint I = 0; I < M_DIM*N_DIM; ++I) {
      Output.Store<ELEM_TYPE>(I*ELEM_SIZE, GsData[I]);
    }
  }
)";

static void runStoreMemory(ID3D12Device *Device,
                           dxc::SpecificDllLoader &DxcSupport,
                           const MatrixParams &Params, bool Verbose,
                           float FillValue, UINT ForcedWaveSize = 0) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DOFFSET=" << 0;
  STREAM_FLOAT(ExtraDefs, "FILL_VALUE", FillValue);

  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, StoreMemoryShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N,
                                  FillValue, /*Increment=*/false);

  auto Op =
      createComputeOp(StoreMemoryShader, "cs_6_10", "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Output");

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
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(D3DDevice, Params, {Params.Use},
                                    L"StoreMemory_Wave_16x16_F16",
                                    SelectedWaveSize))
    return;

  runStoreMemory(D3DDevice, DxcSupport, Params, VerboseLogging,
                 /*FillValue=*/7.0f, SelectedWaveSize);
}

static const char AccumulateMemoryShader[] = R"(
  RWByteAddressBuffer Output : register(u0);
  groupshared ELEM_TYPE GsData[M_DIM * N_DIM];

  #define ELEM_PER_THREAD (M_DIM * N_DIM / NUMTHREADS)

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 128)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    ELEM_TYPE fill = FILL_VALUE;
    for (uint I = 0; I < ELEM_PER_THREAD; ++I) {
      uint Index = threadID * ELEM_PER_THREAD + I;
      GsData[Index] = fill;
    }

    GroupMemoryBarrierWithGroupSync();

    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_FillMatrix(Mat, FILL_VALUE);

    __builtin_LinAlg_MatrixAccumulateToMemory(
      Mat, GsData, COMP_TYPE, OFFSET / ELEM_SIZE, STRIDE / ELEM_SIZE, LAYOUT);

    for (uint I = 0; I < M_DIM*N_DIM; ++I) {
      Output.Store<ELEM_TYPE>(I*ELEM_SIZE, GsData[I]);
    }
  }
)";

static void runAccumulateMemory(ID3D12Device *Device,
                                dxc::SpecificDllLoader &DxcSupport,
                                const MatrixParams &Params, bool Verbose,
                                float FillValue, UINT ForcedWaveSize = 0) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DOFFSET=" << 0;
  STREAM_FLOAT(ExtraDefs, "FILL_VALUE", FillValue);

  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, AccumulateMemoryShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedMat(Params.CompType, Params.M, Params.N,
                                  FillValue * 2, /*Increment=*/false);

  auto Op = createComputeOp(AccumulateMemoryShader, "cs_6_10", "UAV(u0)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Output");

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
  Params.Layout = MatrixLayout::RowMajor;
  Params.NumThreads = 128;
  Params.Enable16Bit = true;

  UINT SelectedWaveSize = 0;
  if (!matrixConstructionApplicable(D3DDevice, Params, {Params.Use},
                                    L"AccumulateMemory_Wave_16x16_F16",
                                    SelectedWaveSize))
    return;
  if (!accumulateStoreApplicable(D3DDevice, Params.CompType,
                                 linalg_test::AtomicDestination::GroupShared,
                                 L"AccumulateMemory_Wave_16x16_F16"))
    return;

  runAccumulateMemory(D3DDevice, DxcSupport, Params, VerboseLogging,
                      /*FillValue=*/7.0f, SelectedWaveSize);
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

static void runConvert(ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
                       bool Verbose) {
  std::string Args = "-HV 202x -enable-16bit-types";
  MatrixDim NumElements = 4;
  size_t BufferSize = elementSize(ComponentType::F32) * NumElements;

  compileShader(DxcSupport, ConvertShader, "cs_6_10", Args, Verbose);

  auto Expected = makeExpectedVec(ComponentType::F32, NumElements, 1.0);

  auto Op = createComputeOp(ConvertShader, "cs_6_10", "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(ComponentType::F32, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::Convert() {
  // Operates on vectors rather than matrices, so tier support is the only
  // capability it needs.
  if (!linAlgTierApplicable(D3DDevice, L"Convert"))
    return;

  runConvert(D3DDevice, DxcSupport, VerboseLogging);
}

static const char VectorAccumulateDescriptorShader[] = R"(
  RWByteAddressBuffer Output : register(u0);

  [numthreads(1, 1, 1)]
  void main() {
    vector<half, 4> InVec = {1.0, 2.0, 3.0, 4.0};
    __builtin_LinAlg_VectorAccumulateToDescriptor(Output, 0, 64, InVec);
  }
)";

static void runVectorAccumulateDescriptor(ID3D12Device *Device,
                                          dxc::SpecificDllLoader &DxcSupport,
                                          bool Verbose) {
  std::string Args = "-HV 202x -enable-16bit-types";
  MatrixDim NumElements = 4;
  size_t BufferSize = elementSize(ComponentType::F16) * NumElements;

  compileShader(DxcSupport, VectorAccumulateDescriptorShader, "cs_6_10", Args,
                Verbose);

  auto Expected = makeExpectedVec(ComponentType::F16, NumElements, 1.0);

  auto Op = createComputeOp(VectorAccumulateDescriptorShader, "cs_6_10",
                            "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(verifyComponentBuffer(ComponentType::F16, OutData.data(),
                                       Expected, NumElements, Verbose));
}

void DxilConf_SM610_LinAlg::VectorAccumulateDescriptor_Thread_F16() {
  // Tier 1 requires no accumulation store formats, so this is gated.
  if (!accumulateStoreApplicable(
          D3DDevice, ComponentType::F16,
          linalg_test::AtomicDestination::RWByteAddressBuffer,
          L"VectorAccumulateDescriptor_Thread_F16"))
    return;

  runVectorAccumulateDescriptor(D3DDevice, DxcSupport, VerboseLogging);
}

} // namespace LinAlg
