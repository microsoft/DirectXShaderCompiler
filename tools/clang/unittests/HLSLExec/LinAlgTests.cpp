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

#include <algorithm>
#include <climits>
#include <cstring>
#include <initializer_list>
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
  case ComponentType::I8:
  case ComponentType::U8:
  case ComponentType::F8_E4M3FN:
  case ComponentType::F8_E5M2:
    return 1;
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
    if (Layout == LinalgMatrixLayout::ColumnMajor)
      return M * ES;
    // If not Row/Col major, spec says to use 0
    return 0;
  }

  size_t totalElements() const { return M * N; }

  size_t totalBytes() const { return totalElements() * elementSize(CompType); }
};

static std::optional<linalg_test::DataType>
toCapabilityDataType(ComponentType CompType) {
  using linalg_test::DataType;
  switch (CompType) {
  case ComponentType::I16:
    return DataType::SInt16;
  case ComponentType::U16:
    return DataType::UInt16;
  case ComponentType::I32:
    return DataType::SInt32;
  case ComponentType::U32:
    return DataType::UInt32;
  case ComponentType::F16:
    return DataType::Float16;
  case ComponentType::F32:
    return DataType::Float32;
  case ComponentType::I8:
    return DataType::SInt8;
  case ComponentType::U8:
    return DataType::UInt8;
  case ComponentType::F8_E4M3FN:
    return DataType::Float8E4M3FN;
  case ComponentType::F8_E5M2:
    return DataType::Float8E5M2;
  default:
    return std::nullopt;
  }
}

static linalg_test::ExecutionScope toCapabilityScope(MatrixScope Scope) {
  switch (Scope) {
  case MatrixScope::Thread:
    return linalg_test::ExecutionScope::Thread;
  case MatrixScope::Wave:
    return linalg_test::ExecutionScope::Wave;
  case MatrixScope::ThreadGroup:
    return linalg_test::ExecutionScope::ThreadGroup;
  }
  VERIFY_IS_TRUE(false, "Unsupported LinAlg matrix scope");
  return linalg_test::ExecutionScope::Thread;
}

static bool applyApplicability(linalg_test::Applicability Result,
                               LPCWSTR CaseName) {
  using linalg_test::Applicability;
  switch (Result) {
  case Applicability::Execute:
    return true;
  case Applicability::NotApplicable:
#ifdef _HLK_CONF
    hlsl_test::LogErrorFmt(
        L"Capability-gated case %s reached HLK execution on an unsupported "
        L"device. Requirement or playlist applicability must exclude it.",
        CaseName);
    VERIFY_IS_TRUE(false,
                   "Unsupported capability-gated case reached HLK execution");
#else
    hlsl_test::LogCommentFmt(
        L"Capability-gated case %s is not applicable on this device", CaseName);
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
#endif
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

namespace cpu_oracle {

using TypedMatrixValues =
    std::variant<std::vector<HLSLHalf_t>, std::vector<float>,
                 std::vector<int32_t>, std::vector<uint32_t>>;

struct TypedMatrix {
  ComponentType CompType;
  MatrixDim M;
  MatrixDim N;
  TypedMatrixValues Values;

  size_t totalElements() const {
    return static_cast<size_t>(M) * static_cast<size_t>(N);
  }
};

struct MatrixBufferLayout {
  LinalgMatrixLayout Layout;
  size_t OffsetBytes;
  size_t StrideBytes;
};

enum class ComparisonMode {
  // Floating-point alternatives allowed by the specification must be listed as
  // permitted results; Exact compares the encoded component bits.
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

struct BufferResultOracle {
  ComparisonMode Mode;
  std::vector<std::vector<BYTE>> Candidates;
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
    : NativeComponentTraits<float, ComponentType::F32> {};

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

static bool checkedAddInt64(int64_t Left, int64_t Right, int64_t &Result) {
  if ((Right > 0 && Left > std::numeric_limits<int64_t>::max() - Right) ||
      (Right < 0 && Left < std::numeric_limits<int64_t>::min() - Right))
    return false;
  Result = Left + Right;
  return true;
}

static bool checkedMultiplyInt64(int64_t Left, int64_t Right, int64_t &Result) {
  if (Left == 0 || Right == 0) {
    Result = 0;
    return true;
  }
  if (Left == -1) {
    if (Right == std::numeric_limits<int64_t>::min())
      return false;
    Result = -Right;
    return true;
  }
  if (Right == -1) {
    if (Left == std::numeric_limits<int64_t>::min())
      return false;
    Result = -Left;
    return true;
  }

  if ((Left > 0 && Right > 0 &&
       Left > std::numeric_limits<int64_t>::max() / Right) ||
      (Left > 0 && Right < 0 &&
       Right < std::numeric_limits<int64_t>::min() / Left) ||
      (Left < 0 && Right > 0 &&
       Left < std::numeric_limits<int64_t>::min() / Right) ||
      (Left < 0 && Right < 0 &&
       Left < std::numeric_limits<int64_t>::max() / Right))
    return false;

  Result = Left * Right;
  return true;
}

static std::optional<std::vector<int64_t>>
multiplyIntegerMatrices(MatrixDim M, MatrixDim K, MatrixDim N,
                        const std::vector<int64_t> &MatrixA,
                        const std::vector<int64_t> &MatrixB,
                        const std::vector<int64_t> *Accumulator = nullptr) {
  size_t MatrixAElements;
  size_t MatrixBElements;
  size_t AccumulatorElements;
  if (M == 0 || K == 0 || N == 0 ||
      !checkedMultiply(static_cast<size_t>(M), K, MatrixAElements) ||
      !checkedMultiply(static_cast<size_t>(K), N, MatrixBElements) ||
      !checkedMultiply(static_cast<size_t>(M), N, AccumulatorElements) ||
      MatrixA.size() != MatrixAElements || MatrixB.size() != MatrixBElements ||
      (Accumulator && Accumulator->size() != AccumulatorElements))
    return std::nullopt;

  std::vector<int64_t> Result(AccumulatorElements, 0);
  for (MatrixDim Row = 0; Row < M; ++Row) {
    for (MatrixDim Column = 0; Column < N; ++Column) {
      const size_t OutputIndex = static_cast<size_t>(Row) * N + Column;
      int64_t Sum = Accumulator ? (*Accumulator)[OutputIndex] : 0;
      for (MatrixDim Inner = 0; Inner < K; ++Inner) {
        int64_t Product;
        if (!checkedMultiplyInt64(
                MatrixA[static_cast<size_t>(Row) * K + Inner],
                MatrixB[static_cast<size_t>(Inner) * N + Column], Product) ||
            !checkedAddInt64(Sum, Product, Sum))
          return std::nullopt;
      }
      Result[OutputIndex] = Sum;
    }
  }
  return Result;
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
  case ComponentType::F8_E4M3FN:
    return L"F8_E4M3FN";
  case ComponentType::F8_E5M2:
    return L"F8_E5M2";
  default:
    return L"Unsupported";
  }
}

static const char *hlslElementTypeName(ComponentType CompType) {
  switch (CompType) {
  case ComponentType::F16:
    return "half";
  case ComponentType::F32:
    return "float";
  case ComponentType::I32:
    return "int";
  case ComponentType::U32:
    return "uint";
  default:
    return nullptr;
  }
}

static bool isPackedVectorComponent(ComponentType CompType) {
  switch (CompType) {
  case ComponentType::I8:
  case ComponentType::U8:
  case ComponentType::F8_E4M3FN:
  case ComponentType::F8_E5M2:
    return true;
  default:
    return false;
  }
}

static const char *hlslVectorStorageTypeName(ComponentType CompType) {
  return isPackedVectorComponent(CompType) ? "uint"
                                           : hlslElementTypeName(CompType);
}

static MatrixDim vectorStorageCount(ComponentType CompType,
                                    MatrixDim LogicalCount) {
  return isPackedVectorComponent(CompType) ? (LogicalCount + 3) / 4
                                           : LogicalCount;
}

static size_t vectorStorageElementSize(ComponentType CompType) {
  return isPackedVectorComponent(CompType) ? sizeof(uint32_t)
                                           : elementSize(CompType);
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

  switch (Matrix.CompType) {
  case ComponentType::F16:
    return std::holds_alternative<std::vector<HLSLHalf_t>>(Matrix.Values) &&
           std::get<std::vector<HLSLHalf_t>>(Matrix.Values).size() ==
               ExpectedElements;
  case ComponentType::F32:
    return std::holds_alternative<std::vector<float>>(Matrix.Values) &&
           std::get<std::vector<float>>(Matrix.Values).size() ==
               ExpectedElements;
  case ComponentType::I32:
    return std::holds_alternative<std::vector<int32_t>>(Matrix.Values) &&
           std::get<std::vector<int32_t>>(Matrix.Values).size() ==
               ExpectedElements;
  case ComponentType::U32:
    return std::holds_alternative<std::vector<uint32_t>>(Matrix.Values) &&
           std::get<std::vector<uint32_t>>(Matrix.Values).size() ==
               ExpectedElements;
  default:
    return false;
  }
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

  return TypedMatrix{ComponentTraits<T>::CompType, M, N, std::move(Values)};
}

static std::optional<TypedMatrix>
makeSequentialMatrix(ComponentType CompType, MatrixDim M, MatrixDim N,
                     uint32_t StartingValue = 1, bool Increment = true) {
  size_t NumElements;
  if (M == 0 || N == 0 ||
      !checkedMultiply(static_cast<size_t>(M), static_cast<size_t>(N),
                       NumElements)) {
    hlsl_test::LogErrorFmt(L"Invalid sequential matrix dimensions: M=%u, N=%u",
                           M, N);
    return std::nullopt;
  }

  size_t LastValueSize;
  const size_t ValueSpan = Increment ? NumElements - 1 : 0;
  if (!checkedAdd(static_cast<size_t>(StartingValue), ValueSpan,
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
      Values.emplace_back(
          static_cast<float>(static_cast<uint64_t>(StartingValue) +
                             static_cast<uint64_t>(Increment ? I : 0)));
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
      Values.push_back(
          static_cast<float>(static_cast<uint64_t>(StartingValue) +
                             static_cast<uint64_t>(Increment ? I : 0)));
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
      Values.push_back(
          static_cast<int32_t>(static_cast<uint64_t>(StartingValue) +
                               static_cast<uint64_t>(Increment ? I : 0)));
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
      Values.push_back(
          static_cast<uint32_t>(static_cast<uint64_t>(StartingValue) +
                                static_cast<uint64_t>(Increment ? I : 0)));
    return makeTypedMatrix(M, N, std::move(Values));
  }
  default:
    hlsl_test::LogErrorFmt(L"Unsupported sequential matrix component type: %u",
                           static_cast<uint32_t>(CompType));
    return std::nullopt;
  }
}

static std::optional<TypedMatrix> makeZeroMatrix(ComponentType CompType,
                                                 MatrixDim M, MatrixDim N) {
  size_t NumElements;
  if (M == 0 || N == 0 ||
      !checkedMultiply(static_cast<size_t>(M), static_cast<size_t>(N),
                       NumElements)) {
    hlsl_test::LogErrorFmt(L"Invalid zero matrix dimensions: M=%u, N=%u", M, N);
    return std::nullopt;
  }

  switch (CompType) {
  case ComponentType::F16:
    return makeTypedMatrix(
        M, N, std::vector<HLSLHalf_t>(NumElements, HLSLHalf_t(0.0f)));
  case ComponentType::F32:
    return makeTypedMatrix(M, N, std::vector<float>(NumElements, 0.0f));
  case ComponentType::I32:
    return makeTypedMatrix(M, N, std::vector<int32_t>(NumElements, 0));
  case ComponentType::U32:
    return makeTypedMatrix(M, N, std::vector<uint32_t>(NumElements, 0));
  default:
    hlsl_test::LogErrorFmt(L"Unsupported zero matrix component type: %u",
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

  switch (Source.CompType) {
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

static bool isMemoryLayout(LinalgMatrixLayout Layout) {
  return Layout == LinalgMatrixLayout::RowMajor ||
         Layout == LinalgMatrixLayout::ColumnMajor;
}

static std::optional<size_t>
getMatrixBufferSize(ComponentType CompType, MatrixDim M, MatrixDim N,
                    const MatrixBufferLayout &Layout) {
  if (!isSupportedComponentType(CompType) || M == 0 || N == 0 ||
      !isMemoryLayout(Layout.Layout)) {
    hlsl_test::LogErrorFmt(
        L"Invalid matrix buffer description: component=%s, M=%u, N=%u, "
        L"layout=%u",
        componentTypeName(CompType), M, N,
        static_cast<uint32_t>(Layout.Layout));
    return std::nullopt;
  }

  const size_t ElementBytes = elementSize(CompType);
  const size_t MajorCount =
      Layout.Layout == LinalgMatrixLayout::RowMajor ? M : N;
  const size_t MinorCount =
      Layout.Layout == LinalgMatrixLayout::RowMajor ? N : M;
  size_t PackedMinorBytes;
  if (!checkedMultiply(MinorCount, ElementBytes, PackedMinorBytes) ||
      Layout.StrideBytes < PackedMinorBytes) {
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
  return getMatrixBufferSize(Matrix.CompType, Matrix.M, Matrix.N, Layout);
}

static std::optional<size_t>
getElementByteOffset(ComponentType CompType, MatrixDim M, MatrixDim N,
                     MatrixDim Row, MatrixDim Column,
                     const MatrixBufferLayout &Layout) {
  if (Row >= M || Column >= N)
    return std::nullopt;

  const size_t Major =
      Layout.Layout == LinalgMatrixLayout::RowMajor ? Row : Column;
  const size_t Minor =
      Layout.Layout == LinalgMatrixLayout::RowMajor ? Column : Row;
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
          Matrix.CompType, Matrix.M, Matrix.N, Row, Column, Layout);
      if (!ByteOffset.has_value())
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
  if (!RequiredBytes.has_value() || Buffer.size() < *RequiredBytes) {
    hlsl_test::LogErrorFmt(
        L"Matrix buffer is too small: actual=%zu, required=%zu", Buffer.size(),
        RequiredBytes.value_or(0));
    return false;
  }

  switch (Matrix.CompType) {
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
static bool writeTypedMatrixBufferWithinBounds(const TypedMatrix &Matrix,
                                               const MatrixBufferLayout &Layout,
                                               size_t AccessibleBytes,
                                               std::vector<BYTE> &Buffer) {
  const std::vector<T> &Values = std::get<std::vector<T>>(Matrix.Values);
  for (MatrixDim Row = 0; Row < Matrix.M; ++Row) {
    for (MatrixDim Column = 0; Column < Matrix.N; ++Column) {
      const size_t ValueIndex = static_cast<size_t>(Row) * Matrix.N + Column;
      std::optional<size_t> ByteOffset = getElementByteOffset(
          Matrix.CompType, Matrix.M, Matrix.N, Row, Column, Layout);
      size_t EndOffset;
      if (!ByteOffset.has_value() ||
          !checkedAdd(*ByteOffset, ComponentTraits<T>::Size, EndOffset))
        return false;
      if (EndOffset > AccessibleBytes)
        continue;
      ComponentTraits<T>::store(Buffer.data() + *ByteOffset,
                                Values[ValueIndex]);
    }
  }
  return true;
}

static bool writeMatrixBufferWithinBounds(const TypedMatrix &Matrix,
                                          const MatrixBufferLayout &Layout,
                                          size_t AccessibleBytes,
                                          std::vector<BYTE> &Buffer) {
  std::optional<size_t> RequiredBytes = getMatrixBufferSize(Matrix, Layout);
  if (!RequiredBytes.has_value() || Buffer.size() < *RequiredBytes ||
      AccessibleBytes > Buffer.size()) {
    hlsl_test::LogErrorFmt(
        L"Invalid bounded matrix buffer: actual=%zu, required=%zu, bound=%zu",
        Buffer.size(), RequiredBytes.value_or(0), AccessibleBytes);
    return false;
  }

  switch (Matrix.CompType) {
  case ComponentType::F16:
    return writeTypedMatrixBufferWithinBounds<HLSLHalf_t>(
        Matrix, Layout, AccessibleBytes, Buffer);
  case ComponentType::F32:
    return writeTypedMatrixBufferWithinBounds<float>(Matrix, Layout,
                                                     AccessibleBytes, Buffer);
  case ComponentType::I32:
    return writeTypedMatrixBufferWithinBounds<int32_t>(Matrix, Layout,
                                                       AccessibleBytes, Buffer);
  case ComponentType::U32:
    return writeTypedMatrixBufferWithinBounds<uint32_t>(
        Matrix, Layout, AccessibleBytes, Buffer);
  default:
    return false;
  }
}

static std::optional<std::vector<BYTE>>
encodeMatrixBuffer(const TypedMatrix &Matrix, const MatrixBufferLayout &Layout,
                   BYTE FillByte = 0) {
  std::optional<size_t> RequiredBytes = getMatrixBufferSize(Matrix, Layout);
  if (!RequiredBytes.has_value())
    return std::nullopt;
  std::vector<BYTE> Buffer(*RequiredBytes, FillByte);
  if (!writeMatrixBuffer(Matrix, Layout, Buffer))
    return std::nullopt;
  return Buffer;
}

static std::optional<std::vector<BYTE>>
makeFilledComponentBuffer(ComponentType CompType, size_t BufferSize,
                          uint32_t FillValue) {
  const size_t ElementBytes = elementSize(CompType);
  if (!isSupportedComponentType(CompType) || BufferSize == 0 ||
      BufferSize % ElementBytes != 0 ||
      BufferSize / ElementBytes > (std::numeric_limits<MatrixDim>::max)())
    return std::nullopt;

  const MatrixDim NumElements =
      static_cast<MatrixDim>(BufferSize / ElementBytes);
  std::optional<TypedMatrix> Fill =
      makeSequentialMatrix(CompType, 1, NumElements, FillValue,
                           /*Increment=*/false);
  if (!Fill.has_value())
    return std::nullopt;

  return encodeMatrixBuffer(*Fill,
                            {LinalgMatrixLayout::RowMajor, 0, BufferSize});
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
      if (!ByteOffset.has_value())
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
  if (!Buffer || !RequiredBytes.has_value() || BufferSize < *RequiredBytes) {
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

static std::optional<TypedMatrix>
makeBoundedReadResult(const TypedMatrix &Source,
                      const MatrixBufferLayout &Layout,
                      size_t AccessibleBytes) {
  std::optional<size_t> RequiredBytes = getMatrixBufferSize(Source, Layout);
  if (!RequiredBytes.has_value() || AccessibleBytes > *RequiredBytes)
    return std::nullopt;

  std::vector<BYTE> Buffer(*RequiredBytes, 0);
  if (!writeMatrixBufferWithinBounds(Source, Layout, AccessibleBytes, Buffer))
    return std::nullopt;
  return decodeMatrixBuffer(Source.CompType, Source.M, Source.N, Layout,
                            Buffer.data(), Buffer.size());
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
      Actual.CompType != Expected.CompType || Actual.M != Expected.M ||
      Actual.N != Expected.N) {
    FirstMismatch = 0;
    return false;
  }

  switch (Actual.CompType) {
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
  switch (Matrix.CompType) {
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

static BufferResultOracle exactBufferResult(std::vector<BYTE> Expected,
                                            std::wstring PublicRule) {
  return BufferResultOracle{
      ComparisonMode::Exact, {std::move(Expected)}, std::move(PublicRule)};
}

static BufferResultOracle
permittedBufferResults(std::vector<std::vector<BYTE>> Candidates,
                       std::wstring PublicRule) {
  return BufferResultOracle{ComparisonMode::PermittedResults,
                            std::move(Candidates), std::move(PublicRule)};
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
    if (!isMatrixValid(Candidate) || Candidate.CompType != First.CompType ||
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
  std::optional<TypedMatrix> Actual = decodeMatrixBuffer(
      Shape.CompType, Shape.M, Shape.N, Layout, ActualBuffer, ActualBufferSize);
  if (!Actual.has_value())
    return false;

  std::vector<size_t> FirstMismatches;
  if (matchesAnyCompleteCandidate(*Actual, Oracle, &FirstMismatches)) {
    if (Verbose) {
      hlsl_test::LogCommentFmt(
          L"Matrix comparison passed: component=%s, M=%u, N=%u, mode=%s, "
          L"rule=%s",
          componentTypeName(Shape.CompType), Shape.M, Shape.N,
          comparisonModeName(Oracle.Mode), Oracle.PublicRule.c_str());
    }
    return true;
  }

  hlsl_test::LogErrorFmt(
      L"No complete matrix candidate matched: component=%s, M=%u, N=%u, "
      L"mode=%s, rule=%s",
      componentTypeName(Shape.CompType), Shape.M, Shape.N,
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

static bool isBufferOracleValid(const BufferResultOracle &Oracle) {
  if (Oracle.PublicRule.empty() || Oracle.Candidates.empty())
    return false;
  if (Oracle.Mode == ComparisonMode::Excluded)
    return false;
  if (Oracle.Mode == ComparisonMode::Exact && Oracle.Candidates.size() != 1)
    return false;
  if (Oracle.Mode == ComparisonMode::PermittedResults &&
      Oracle.Candidates.size() < 2)
    return false;

  const size_t ExpectedSize = Oracle.Candidates.front().size();
  if (ExpectedSize == 0)
    return false;
  for (const std::vector<BYTE> &Candidate : Oracle.Candidates) {
    if (Candidate.size() != ExpectedSize)
      return false;
  }
  return true;
}

static bool matchesAnyCompleteBufferCandidate(
    const std::vector<BYTE> &Actual, const BufferResultOracle &Oracle,
    std::vector<size_t> *FirstMismatches = nullptr) {
  if (!isBufferOracleValid(Oracle) ||
      Actual.size() != Oracle.Candidates.front().size())
    return false;

  if (FirstMismatches)
    FirstMismatches->clear();
  for (const std::vector<BYTE> &Candidate : Oracle.Candidates) {
    const auto Mismatch =
        std::mismatch(Actual.begin(), Actual.end(), Candidate.begin());
    const size_t FirstMismatch =
        static_cast<size_t>(std::distance(Actual.begin(), Mismatch.first));
    if (FirstMismatch == Actual.size())
      return true;
    if (FirstMismatches)
      FirstMismatches->push_back(FirstMismatch);
  }
  return false;
}

static bool verifyBufferResult(const void *ActualBuffer,
                               size_t ActualBufferSize,
                               const BufferResultOracle &Oracle, bool Verbose) {
  if (!ActualBuffer || !isBufferOracleValid(Oracle)) {
    hlsl_test::LogErrorFmt(L"Invalid whole-buffer result or oracle");
    return false;
  }

  const size_t ExpectedSize = Oracle.Candidates.front().size();
  if (ActualBufferSize != ExpectedSize) {
    hlsl_test::LogErrorFmt(
        L"Whole-buffer size mismatch: actual=%zu, expected=%zu, rule=%s",
        ActualBufferSize, ExpectedSize, Oracle.PublicRule.c_str());
    return false;
  }

  const BYTE *ActualBytes = static_cast<const BYTE *>(ActualBuffer);
  std::vector<BYTE> Actual(ActualBytes, ActualBytes + ActualBufferSize);
  std::vector<size_t> FirstMismatches;
  if (matchesAnyCompleteBufferCandidate(Actual, Oracle, &FirstMismatches)) {
    if (Verbose) {
      hlsl_test::LogCommentFmt(
          L"Whole-buffer comparison passed: bytes=%zu, mode=%s, rule=%s",
          ActualBufferSize, comparisonModeName(Oracle.Mode),
          Oracle.PublicRule.c_str());
    }
    return true;
  }

  hlsl_test::LogErrorFmt(
      L"No complete whole-buffer candidate matched: bytes=%zu, mode=%s, "
      L"rule=%s",
      ActualBufferSize, comparisonModeName(Oracle.Mode),
      Oracle.PublicRule.c_str());
  for (size_t CandidateIndex = 0; CandidateIndex < Oracle.Candidates.size();
       ++CandidateIndex) {
    const size_t Mismatch = FirstMismatches[CandidateIndex];
    hlsl_test::LogErrorFmt(
        L"Candidate %zu first mismatch at byte=%zu: actual=0x%02x, "
        L"expected=0x%02x",
        CandidateIndex, Mismatch, Actual[Mismatch],
        Oracle.Candidates[CandidateIndex][Mismatch]);
  }
  return false;
}

template <typename T>
static std::vector<BYTE>
encodeNativeVector(const std::vector<T> &NativeValues) {
  static_assert(std::is_trivially_copyable<T>::value,
                "Vector values must be trivially copyable");
  std::vector<BYTE> Bytes(NativeValues.size() * sizeof(T));
  if (!Bytes.empty())
    std::memcpy(Bytes.data(), NativeValues.data(), Bytes.size());
  return Bytes;
}

template <typename T>
static std::vector<BYTE> encodeNativeVector(std::initializer_list<T> Values) {
  return encodeNativeVector(std::vector<T>(Values));
}

static std::optional<std::vector<BYTE>>
encodeExactComponents(ComponentType CompType,
                      const std::vector<int64_t> &Values) {
  switch (CompType) {
  case ComponentType::F16: {
    std::vector<HLSLHalf_t> NativeValues;
    NativeValues.reserve(Values.size());
    for (int64_t Value : Values) {
      HLSLHalf_t Half(static_cast<float>(Value));
      if (static_cast<float>(Half) != static_cast<float>(Value))
        return std::nullopt;
      NativeValues.push_back(Half);
    }
    return encodeNativeVector(NativeValues);
  }
  case ComponentType::F32: {
    std::vector<float> NativeValues;
    NativeValues.reserve(Values.size());
    for (int64_t Value : Values) {
      const float FloatValue = static_cast<float>(Value);
      if (static_cast<int64_t>(FloatValue) != Value)
        return std::nullopt;
      NativeValues.push_back(FloatValue);
    }
    return encodeNativeVector(NativeValues);
  }
  case ComponentType::I32: {
    std::vector<int32_t> NativeValues;
    NativeValues.reserve(Values.size());
    for (int64_t Value : Values) {
      if (Value < std::numeric_limits<int32_t>::min() ||
          Value > std::numeric_limits<int32_t>::max())
        return std::nullopt;
      NativeValues.push_back(static_cast<int32_t>(Value));
    }
    return encodeNativeVector(NativeValues);
  }
  case ComponentType::U32: {
    std::vector<uint32_t> NativeValues;
    NativeValues.reserve(Values.size());
    for (int64_t Value : Values) {
      if (Value < 0 ||
          static_cast<uint64_t>(Value) > std::numeric_limits<uint32_t>::max())
        return std::nullopt;
      NativeValues.push_back(static_cast<uint32_t>(Value));
    }
    return encodeNativeVector(NativeValues);
  }
  case ComponentType::I8:
  case ComponentType::U8: {
    std::vector<BYTE> Bytes;
    Bytes.reserve((Values.size() + 3) & ~size_t(3));
    for (int64_t Value : Values) {
      if (CompType == ComponentType::I8) {
        if (Value < std::numeric_limits<int8_t>::min() ||
            Value > std::numeric_limits<int8_t>::max())
          return std::nullopt;
        Bytes.push_back(static_cast<BYTE>(static_cast<int8_t>(Value)));
      } else {
        if (Value < 0 || Value > std::numeric_limits<uint8_t>::max())
          return std::nullopt;
        Bytes.push_back(static_cast<BYTE>(Value));
      }
    }
    while (Bytes.size() % sizeof(uint32_t) != 0)
      Bytes.push_back(0);
    return Bytes;
  }
  default:
    return std::nullopt;
  }
}

static std::optional<std::vector<BYTE>>
encodeLogicalMatrixBuffer(const MatrixParams &Matrix,
                          const std::vector<int64_t> &Values) {
  size_t ExpectedElements;
  if (Matrix.M == 0 || Matrix.N == 0 ||
      !checkedMultiply(static_cast<size_t>(Matrix.M), Matrix.N,
                       ExpectedElements) ||
      Values.size() != ExpectedElements ||
      (Matrix.Layout != LinalgMatrixLayout::RowMajor &&
       Matrix.Layout != LinalgMatrixLayout::ColumnMajor))
    return std::nullopt;

  const std::optional<std::vector<BYTE>> LogicalComponents =
      encodeExactComponents(Matrix.CompType, Values);
  if (!LogicalComponents.has_value())
    return std::nullopt;

  const size_t ComponentSize = elementSize(Matrix.CompType);
  size_t LogicalByteCount;
  if (!checkedMultiply(ExpectedElements, ComponentSize, LogicalByteCount))
    return std::nullopt;
  const size_t Stride = Matrix.strideBytes();
  const size_t MinorCount =
      Matrix.Layout == LinalgMatrixLayout::RowMajor ? Matrix.N : Matrix.M;
  const size_t MajorCount =
      Matrix.Layout == LinalgMatrixLayout::RowMajor ? Matrix.M : Matrix.N;
  size_t MinimumStride;
  if (!checkedMultiply(MinorCount, ComponentSize, MinimumStride) ||
      LogicalComponents->size() < LogicalByteCount || Stride < MinimumStride)
    return std::nullopt;

  size_t BufferSize;
  if (!checkedMultiply(MajorCount, Stride, BufferSize))
    return std::nullopt;
  std::vector<BYTE> Buffer(BufferSize, 0);
  for (MatrixDim Row = 0; Row < Matrix.M; ++Row) {
    for (MatrixDim Column = 0; Column < Matrix.N; ++Column) {
      const size_t SourceOffset =
          (static_cast<size_t>(Row) * Matrix.N + Column) * ComponentSize;
      const size_t DestinationOffset =
          Matrix.Layout == LinalgMatrixLayout::RowMajor
              ? static_cast<size_t>(Row) * Stride + Column * ComponentSize
              : static_cast<size_t>(Column) * Stride + Row * ComponentSize;
      std::memcpy(Buffer.data() + DestinationOffset,
                  LogicalComponents->data() + SourceOffset, ComponentSize);
    }
  }
  return Buffer;
}

static std::optional<BYTE> encodeExactFP8(HLSLHalf_t Value,
                                          ComponentType CompType) {
  unsigned MantissaBits;
  unsigned ExponentBias;
  unsigned MaxFiniteExponent;
  switch (CompType) {
  case ComponentType::F8_E4M3FN:
    MantissaBits = 3;
    ExponentBias = 7;
    MaxFiniteExponent = 0xf;
    break;
  case ComponentType::F8_E5M2:
    MantissaBits = 2;
    ExponentBias = 15;
    MaxFiniteExponent = 0x1e;
    break;
  default:
    return std::nullopt;
  }

  const unsigned Sign = Value.Val >> 15;
  const unsigned SourceExponent = (Value.Val >> 10) & 0x1f;
  const unsigned SourceMantissa = Value.Val & 0x3ff;
  if (SourceExponent == 0) {
    if (SourceMantissa != 0)
      return std::nullopt;
    return static_cast<BYTE>(Sign << 7);
  }
  if (SourceExponent == 0x1f)
    return std::nullopt;

  const int DestinationExponent =
      static_cast<int>(SourceExponent) - 15 + static_cast<int>(ExponentBias);
  if (DestinationExponent <= 0 ||
      static_cast<unsigned>(DestinationExponent) > MaxFiniteExponent)
    return std::nullopt;

  const unsigned DiscardedBits = 10 - MantissaBits;
  const unsigned DiscardedMask = (1u << DiscardedBits) - 1;
  if ((SourceMantissa & DiscardedMask) != 0)
    return std::nullopt;

  const unsigned DestinationMantissa = SourceMantissa >> DiscardedBits;
  if (CompType == ComponentType::F8_E4M3FN && DestinationExponent == 0xf &&
      DestinationMantissa == 0x7)
    return std::nullopt;

  return static_cast<BYTE>(
      (Sign << 7) |
      (static_cast<unsigned>(DestinationExponent) << MantissaBits) |
      DestinationMantissa);
}

static std::optional<std::vector<BYTE>>
encodeExactFP8RoundTrip(const std::vector<float> &Values,
                        ComponentType CompType) {
  if (Values.empty() || Values.size() % 4 != 0)
    return std::nullopt;

  const size_t PackedBytes = Values.size();
  std::vector<BYTE> Expected(PackedBytes + Values.size() * sizeof(uint16_t));
  for (size_t I = 0; I < Values.size(); ++I) {
    const HLSLHalf_t Half(Values[I]);
    const std::optional<BYTE> Encoded = encodeExactFP8(Half, CompType);
    if (!Encoded.has_value())
      return std::nullopt;

    Expected[I] = *Encoded;
    const size_t HalfOffset = PackedBytes + I * sizeof(uint16_t);
    Expected[HalfOffset] = static_cast<BYTE>(Half.Val & 0xff);
    Expected[HalfOffset + 1] = static_cast<BYTE>(Half.Val >> 8);
  }
  return Expected;
}

} // namespace cpu_oracle

static bool needs16BitTypes(ComponentType CompType) {
  return CompType == ComponentType::F16 || CompType == ComponentType::I16 ||
         CompType == ComponentType::U16;
}

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
  const char *ElementType = cpu_oracle::hlslElementTypeName(Params.CompType);
  if (!ElementType) {
    VERIFY_IS_TRUE(false, "Unsupported LinAlg component type");
  } else {
    SS << " -DELEM_TYPE=" << ElementType;
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

class LinAlgCPUOracleTests {
public:
  BEGIN_TEST_CLASS(LinAlgCPUOracleTests)
  TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_METHOD(TypedMatrixBufferRoundTrip);
};

void LinAlgCPUOracleTests::TypedMatrixBufferRoundTrip() {
  using namespace cpu_oracle;

  auto VerifyScalarEncoding = [](const std::optional<TypedMatrix> &Matrix,
                                 const std::vector<BYTE> &ExpectedBytes) {
    if (!Matrix.has_value())
      return false;
    MatrixBufferLayout Layout = {
        LinalgMatrixLayout::RowMajor,
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

  std::optional<TypedMatrix> Matrix =
      makeTypedMatrix<uint32_t>(2, 3, {1, 2, 3, 4, 5, 6});
  VERIFY_IS_TRUE(Matrix.has_value());

  MatrixBufferLayout RowMajor = {
      LinalgMatrixLayout::RowMajor,
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
      LinalgMatrixLayout::ColumnMajor,
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

  const std::optional<std::vector<int64_t>> Product =
      multiplyIntegerMatrices(/*M=*/2, /*K=*/3, /*N=*/2,
                              /*MatrixA=*/{1, 2, 3, 4, 5, 6},
                              /*MatrixB=*/{7, 8, 9, 10, 11, 12});
  VERIFY_IS_TRUE(Product.has_value());
  VERIFY_IS_TRUE(*Product == std::vector<int64_t>({58, 64, 139, 154}));
  const std::vector<int64_t> InitialAccumulator = {1, -1, 2, -2};
  const std::optional<std::vector<int64_t>> ProductAccumulated =
      multiplyIntegerMatrices(/*M=*/2, /*K=*/3, /*N=*/2,
                              /*MatrixA=*/{1, 2, 3, 4, 5, 6},
                              /*MatrixB=*/{7, 8, 9, 10, 11, 12},
                              &InitialAccumulator);
  VERIFY_IS_TRUE(ProductAccumulated.has_value());
  VERIFY_IS_TRUE(*ProductAccumulated ==
                 std::vector<int64_t>({59, 63, 141, 152}));

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

  const std::vector<BYTE> BufferCandidateA = {1, 2, 3, 4};
  const std::vector<BYTE> BufferCandidateB = {5, 6, 7, 8};
  const std::vector<BYTE> MixedBuffer = {1, 2, 7, 8};
  BufferResultOracle BufferPermitted = permittedBufferResults(
      {BufferCandidateA, BufferCandidateB},
      L"Host whole-buffer permitted candidate semantics");
  VERIFY_IS_FALSE(
      matchesAnyCompleteBufferCandidate(MixedBuffer, BufferPermitted));
  BufferPermitted.Candidates.push_back(MixedBuffer);
  VERIFY_IS_TRUE(
      matchesAnyCompleteBufferCandidate(MixedBuffer, BufferPermitted));

  const std::optional<std::vector<BYTE>> PackedSInt8 =
      encodeExactComponents(ComponentType::I8, {-1, 2, -3, 4, 5});
  const std::optional<std::vector<BYTE>> PackedUInt8 =
      encodeExactComponents(ComponentType::U8, {255, 2, 253, 4, 5});
  VERIFY_IS_TRUE(PackedSInt8.has_value());
  VERIFY_IS_TRUE(PackedUInt8.has_value());
  VERIFY_IS_TRUE(*PackedSInt8 == std::vector<BYTE>({0xff, 0x02, 0xfd, 0x04,
                                                    0x05, 0x00, 0x00, 0x00}));
  VERIFY_IS_TRUE(*PackedUInt8 == std::vector<BYTE>({0xff, 0x02, 0xfd, 0x04,
                                                    0x05, 0x00, 0x00, 0x00}));
  VERIFY_ARE_EQUAL(1u, static_cast<unsigned>(elementSize(ComponentType::I8)));
  VERIFY_ARE_EQUAL(2u, vectorStorageCount(ComponentType::I8, 8));
  VERIFY_ARE_EQUAL(8u, vectorStorageCount(ComponentType::F32, 8));

  MatrixParams Params = {};
  Params.M = 2;
  Params.N = 3;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 4;
  Params.CompType = ComponentType::I32;
  VERIFY_IS_TRUE(buildCompilerArgs(Params).find(" -DELEM_TYPE=int") !=
                 std::string::npos);
  Params.CompType = ComponentType::U32;
  VERIFY_IS_TRUE(buildCompilerArgs(Params).find(" -DELEM_TYPE=uint") !=
                 std::string::npos);

  const std::vector<float> FP8Values = {
      0.0f, 1.0f, -1.0f, 1.5f, -1.5f, 3.0f, 6.0f, -6.0f,
  };
  const std::optional<std::vector<BYTE>> E4M3 =
      encodeExactFP8RoundTrip(FP8Values, ComponentType::F8_E4M3FN);
  const std::optional<std::vector<BYTE>> E5M2 =
      encodeExactFP8RoundTrip(FP8Values, ComponentType::F8_E5M2);
  VERIFY_IS_TRUE(E4M3.has_value());
  VERIFY_IS_TRUE(E5M2.has_value());
  if (E4M3.has_value()) {
    const std::vector<BYTE> ExpectedE4M3 = {
        0x00, 0x38, 0xb8, 0x3c, 0xbc, 0x44, 0x4c, 0xcc, 0x00, 0x00, 0x00, 0x3c,
        0x00, 0xbc, 0x00, 0x3e, 0x00, 0xbe, 0x00, 0x42, 0x00, 0x46, 0x00, 0xc6,
    };
    VERIFY_IS_TRUE(*E4M3 == ExpectedE4M3);
  }
  if (E5M2.has_value()) {
    const std::vector<BYTE> ExpectedE5M2 = {
        0x00, 0x3c, 0xbc, 0x3e, 0xbe, 0x42, 0x46, 0xc6, 0x00, 0x00, 0x00, 0x3c,
        0x00, 0xbc, 0x00, 0x3e, 0x00, 0xbe, 0x00, 0x42, 0x00, 0x46, 0x00, 0xc6,
    };
    VERIFY_IS_TRUE(*E5M2 == ExpectedE5M2);
  }
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

  VERIFY_IS_TRUE(
      isLegalScope(OperationType::MatrixConstruction, ExecutionScope::Wave));
  VERIFY_IS_TRUE(isLegalScope(OperationType::MatrixConstruction,
                              ExecutionScope::ThreadGroup));
  VERIFY_IS_FALSE(
      isLegalScope(OperationType::MatrixConstruction, ExecutionScope::Thread));
  VERIFY_IS_TRUE(
      isLegalScope(OperationType::WaveMatrixMultiply, ExecutionScope::Wave));
  VERIFY_IS_TRUE(isLegalScope(OperationType::ThreadGroupMatrixMultiply,
                              ExecutionScope::ThreadGroup));
  VERIFY_IS_TRUE(isLegalScope(OperationType::ThreadVectorMatrixMultiply,
                              ExecutionScope::Thread));
  VERIFY_IS_TRUE(
      isLegalScope(OperationType::ThreadOuterProduct, ExecutionScope::Thread));
  VERIFY_IS_TRUE(isLegalScope(OperationType::AtomicAccumulateStore,
                              ExecutionScope::Thread));
  VERIFY_IS_TRUE(
      isLegalScope(OperationType::AtomicAccumulateStore, ExecutionScope::Wave));
  VERIFY_IS_TRUE(isLegalScope(OperationType::AtomicAccumulateStore,
                              ExecutionScope::ThreadGroup));

  MatrixConstructionSupport Construction = {4, 8, 16};
  VERIFY_IS_TRUE(Construction.valid());
  VERIFY_IS_TRUE(Construction.supports(MatrixRole::A, 4, 8));
  VERIFY_IS_TRUE(Construction.supports(MatrixRole::B, 8, 16));
  VERIFY_IS_TRUE(Construction.supports(MatrixRole::Accumulator, 4, 16));
  VERIFY_IS_FALSE(Construction.supports(MatrixRole::A, 3, 8));
  MatrixConstructionSupport InvalidConstruction = {4, 0, 16};
  VERIFY_IS_FALSE(InvalidConstruction.valid());

  WaveMatrixMultiplySupport Wave = {
      MultiplicationFlags::Supported,
      {{8, 16, 8}, {16, 16, 16}},
  };
  VERIFY_IS_TRUE(Wave.valid());
  VERIFY_IS_TRUE(Wave.supportsShape(32, 32, 16));
  VERIFY_IS_FALSE(Wave.supportsShape(12, 32, 16));
  WaveMatrixMultiplySupport InvalidWave = {
      MultiplicationFlags::EmulatedInputs,
      {},
  };
  VERIFY_IS_FALSE(InvalidWave.valid());

  ThreadGroupMatrixMultiplySupport ThreadGroup = {
      MultiplicationFlags::Supported,
      32,
      128,
      64,
  };
  VERIFY_IS_TRUE(ThreadGroup.valid());
  VERIFY_IS_TRUE(ThreadGroup.supportsThreadGroupSize(64));
  VERIFY_IS_FALSE(ThreadGroup.supportsThreadGroupSize(48));
  ThreadGroup.PreferredThreadGroupSize = 48;
  VERIFY_IS_FALSE(ThreadGroup.valid());

  ThreadVectorMatrixMultiplySupport ThreadVector = {
      static_cast<MultiplicationFlags>(
          static_cast<UINT>(MultiplicationFlags::Supported) |
          static_cast<UINT>(MultiplicationFlags::EmulatedInputs)),
  };
  VERIFY_IS_TRUE(ThreadVector.valid());
  VERIFY_IS_TRUE(ThreadVector.supported());
  ThreadVector.SupportFlags = MultiplicationFlags::EmulatedInputs;
  VERIFY_IS_FALSE(ThreadVector.valid());

  ThreadOuterProductSupport OuterProduct = {true};
  VERIFY_IS_TRUE(OuterProduct.supported());
  AtomicAccumulateStoreSupport Atomic = {true, false};
  VERIFY_IS_TRUE(Atomic.supports(AtomicDestination::RWByteAddressBuffer));
  VERIFY_IS_FALSE(Atomic.supports(AtomicDestination::GroupShared));

  VERIFY_ARE_EQUAL(0u, static_cast<UINT>(DataType::None));
  MatrixConstructionQuery ConstructionQuery = {DataType::Float32, 32};
  WaveMatrixMultiplyQuery WaveQuery = {
      32,
      DataType::Float16,
      DataType::Float16,
      DataType::Float32,
  };
  ThreadGroupMatrixMultiplyQuery ThreadGroupQuery = {
      WaveQuery,
      {16, 16, 16},
  };
  ThreadVectorMatrixMultiplyQuery ThreadVectorQuery = {
      DataType::Float16,
      DataType::Float16,
      DataType::None,
      DataType::Float16,
  };
  ThreadOuterProductQuery OuterProductQuery = {
      DataType::Float16,
      DataType::Float16,
  };
  AtomicAccumulateStoreQuery AtomicQuery = {DataType::Float16};
  VERIFY_ARE_EQUAL(32u, ConstructionQuery.WaveSize);
  VERIFY_ARE_EQUAL(16u, ThreadGroupQuery.Shape.M);
  VERIFY_IS_TRUE(ThreadVectorQuery.BiasInputType == DataType::None);
  VERIFY_IS_TRUE(OuterProductQuery.InputComponentType == DataType::Float16);
  VERIFY_IS_TRUE(AtomicQuery.ComponentType == DataType::Float16);
}

struct MatrixConstructionRequirement {
  linalg_test::MatrixRole Role;
  MatrixDim Rows;
  MatrixDim Columns;
  ComponentType CompType = ComponentType::Invalid;
  bool RequireShape = true;
};

static HRESULT queryMatrixConstructionSupport(
    ID3D12Device *Device, const MatrixParams &Params,
    std::initializer_list<MatrixConstructionRequirement> Requirements,
    LPCWSTR CaseName, bool &Supported, UINT &SelectedWaveSize);

static HRESULT queryDescriptorAccumulateSupport(ID3D12Device *Device,
                                                const MatrixParams &Params,
                                                LPCWSTR CaseName,
                                                bool &Supported,
                                                UINT &SelectedWaveSize);

static HRESULT queryThreadVectorMatrixMultiplySupport(
    ID3D12Device *Device, const MatrixParams &Params,
    ComponentType VectorInputType, ComponentType BiasInputType,
    ComponentType ResultType, LPCWSTR CaseName, bool &Supported);

static HRESULT
queryAtomicAccumulateSupport(ID3D12Device *Device, const MatrixParams &Params,
                             linalg_test::AtomicDestination Destination,
                             LPCWSTR CaseName, bool &Supported,
                             UINT &SelectedWaveSize);

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
  TEST_METHOD(LoadStoreDescriptor_Wave_4x8_F16_RowMajorOffsetPadded);
  TEST_METHOD(LoadStoreDescriptor_Wave_4x8_F32_ColumnMajorOffset);
  TEST_METHOD(LoadDescriptorBounds_Wave_4x8_F32);
  TEST_METHOD(StoreDescriptorBounds_Wave_4x8_F32);
  TEST_METHOD(SplatStore_Wave_16x16_F16);
  TEST_METHOD(AccumulateDescriptor_Wave_16x16_F16);
  TEST_METHOD(AccumulateDescriptorBounds_Wave_4x8_F32);

  // Load/Store/Accumulate Memory
  TEST_METHOD(LoadMemory_Wave_16x16_F16);
  TEST_METHOD(StoreMemory_Wave_16x16_F16);
  TEST_METHOD(AccumulateMemory_Wave_16x16_F16);
  TEST_METHOD(LoadStoreMemory_Wave_4x8_F16_RowMajorOffsetPadded);
  TEST_METHOD(LoadStoreMemory_Wave_4x8_F32_ColumnMajorOffsetPadded);
  TEST_METHOD(LoadStoreMemory_ThreadGroup_4x8_F16);
  TEST_METHOD(AccumulateMemory_Wave_4x8_F16_RowMajorOffsetPadded);

  // Element access
  TEST_METHOD(ElementAccess_Wave_16x16_F16);
  TEST_METHOD(ElementAccess_Wave_4x8_F32);
  TEST_METHOD(ElementGetOOB_Wave_4x8_F32);
  TEST_METHOD(ElementSet_Wave_16x16_F16);
  TEST_METHOD(ElementSetOOB_Wave_4x8_F32);

  // Cast/Convert
  TEST_METHOD(CopyConvert_Wave_16x16_F16);
  TEST_METHOD(CopyConvert_Wave_16x16_F16_Transpose);
  TEST_METHOD(CopyConvert_Wave_4x8_F32_Transpose);
  TEST_METHOD(CopyConvert_Wave_4x8_F16_ToF32);
  TEST_METHOD(CopyConvert_Wave_4x8_F32_ToF16_Transpose);

  // Matrix Matrix Arithmetic
  TEST_METHOD(MatMatMul_Wave_16x16x16_F16);
  TEST_METHOD(MatMatMul_Wave_8x32x16_F16_NonUniform);
  TEST_METHOD(MatMatMul_Wave_8x32x16_F16_ToF32);
  TEST_METHOD(MatMatMul_Wave_16x16x16_I32);
  TEST_METHOD(MatMatMulAccum_Wave_16x16x16_F16);
  TEST_METHOD(MatMatMulAccum_Wave_8x32x16_F16_ToF32_NonUniform);
  TEST_METHOD(MatAccum_Wave_16x16_F16);
  TEST_METHOD(MatAccum_Wave_8x32_F16_BUse_NonUniform);
  TEST_METHOD(MatMatMul_ThreadGroup_8x16x8_F16_NonUniform);
  TEST_METHOD(MatMatMulAccum_ThreadGroup_8x16x8_F16_ToF32_NonUniform);
  TEST_METHOD(MatMatMul_ThreadGroup_8x8x8_I32);

  // Matrix Vector Arithmetic
  TEST_METHOD(MatVecMul_Thread_16x16_F16);
  TEST_METHOD(MatVecMul_Thread_4x8_F32);
  TEST_METHOD(MatVecMul_Thread_4x8_F16_NonUniform);
  TEST_METHOD(MatVecMul_Thread_4x8_F16_ColumnMajor);
  TEST_METHOD(MatVecMul_Thread_4x8_I8_Interpreted);
  TEST_METHOD(MatVecMul_Thread_4x8_U8_Interpreted);
  TEST_METHOD(MatVecMul_Thread_4x8_F32_ToI8);
  TEST_METHOD(MatVecMul_Thread_4x8_U32_UnsignedOutput);
  TEST_METHOD(MatVecMulAdd_Thread_16x16_F16);
  TEST_METHOD(MatVecMulAdd_Thread_4x8_F32);
  TEST_METHOD(MatVecMulAdd_Thread_4x8_F16_IndependentBias);
  TEST_METHOD(OuterProduct_Thread_16x16_F16);
  TEST_METHOD(OuterProduct_Thread_8x16_F16_NonUniform);
  TEST_METHOD(OuterProduct_Thread_4x8_F32_NonUniform);

  // Query Accumulator Layout
  TEST_METHOD(QueryAccumLayout);

  // Convert
  TEST_METHOD(Convert);
  TEST_METHOD(Convert_I16_ToI32_Exact);
  TEST_METHOD(Convert_F32_ToI16_RTNE_Saturate);
  TEST_METHOD(Convert_F16_FP8_RoundTrip);

  // Vector Accumulate
  TEST_METHOD(VectorAccumulateDescriptor_Thread_F16);
  TEST_METHOD(VectorAccumulateDescriptor_Thread_F16_Length8_NonZero);
  TEST_METHOD(VectorAccumulateDescriptor_Thread_F32_Length8_NonZero);

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

static const char DescriptorIOShader[] = R"(
  ByteAddressBuffer Input : register(t0);
  RWByteAddressBuffer Output : register(u1);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 64)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, Input, SRC_OFFSET, SRC_STRIDE, SRC_LAYOUT, SRC_ALIGNMENT);
  #ifdef ACCUMULATE_COUNT
    for (uint I = 0; I < ACCUMULATE_COUNT; ++I) {
      __builtin_LinAlg_MatrixAccumulateToDescriptor(
        Mat, Output, DST_OFFSET, DST_STRIDE, DST_LAYOUT, DST_ALIGNMENT);
    }
  #else
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, DST_OFFSET, DST_STRIDE, DST_LAYOUT, DST_ALIGNMENT);
  #endif
  }
)";

static void runDescriptorIO(
    ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
    const MatrixParams &Params, const cpu_oracle::TypedMatrix &InputMatrix,
    const cpu_oracle::MatrixBufferLayout &SourceLayout,
    const cpu_oracle::MatrixBufferLayout &DestinationLayout,
    UINT SourceAlignment, UINT DestinationAlignment, size_t SourceViewBytes,
    size_t DestinationViewBytes, std::vector<BYTE> InitialOutput,
    const cpu_oracle::BufferResultOracle &Oracle, bool Verbose,
    UINT ForcedWaveSize = 0, UINT AccumulateCount = 0) {
  std::optional<std::vector<BYTE>> SourceBuffer =
      cpu_oracle::encodeMatrixBuffer(InputMatrix, SourceLayout, 0xcd);
  VERIFY_IS_TRUE(SourceBuffer.has_value(),
                 "Unable to encode descriptor input matrix");
  VERIFY_IS_TRUE(SourceViewBytes <= SourceBuffer->size() &&
                     DestinationViewBytes <= InitialOutput.size(),
                 "Descriptor view exceeds its backing resource");

  std::stringstream ExtraDefs;
  ExtraDefs << " -DSRC_OFFSET=" << SourceLayout.OffsetBytes;
  ExtraDefs << " -DSRC_STRIDE=" << SourceLayout.StrideBytes;
  ExtraDefs << " -DSRC_LAYOUT=" << static_cast<UINT>(SourceLayout.Layout);
  ExtraDefs << " -DSRC_ALIGNMENT=" << SourceAlignment;
  ExtraDefs << " -DDST_OFFSET=" << DestinationLayout.OffsetBytes;
  ExtraDefs << " -DDST_STRIDE=" << DestinationLayout.StrideBytes;
  ExtraDefs << " -DDST_LAYOUT=" << static_cast<UINT>(DestinationLayout.Layout);
  ExtraDefs << " -DDST_ALIGNMENT=" << DestinationAlignment;
  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;
  if (AccumulateCount != 0)
    ExtraDefs << " -DACCUMULATE_COUNT=" << AccumulateCount;

  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, DescriptorIOShader, "cs_6_10", Args, Verbose);

  auto Op = createComputeOp(DescriptorIOShader, "cs_6_10",
                            "DescriptorTable(SRV(t0), UAV(u1))", Args.c_str());
  addSRVBuffer(Op.get(), "Input", SourceBuffer->size(), "byname");
  addUAVBuffer(Op.get(), "Output", InitialOutput.size(), true, "byname");
  addRawBufferDescriptorTable(
      Op.get(), 0, "DescriptorHeap",
      {
          {"InputView", "Input", RawBufferViewKind::SRV, 0, SourceViewBytes},
          {"OutputView", "Output", RawBufferViewKind::UAV, 0,
           DestinationViewBytes},
      });

  const std::vector<BYTE> SourceData = *SourceBuffer;
  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [SourceData, InitialOutput](LPCSTR Name, std::vector<BYTE> &Data,
                                  st::ShaderOp *) {
        const std::vector<BYTE> *ExpectedData = nullptr;
        if (_stricmp(Name, "Input") == 0)
          ExpectedData = &SourceData;
        else if (_stricmp(Name, "Output") == 0)
          ExpectedData = &InitialOutput;
        if (!ExpectedData)
          return;
        VERIFY_IS_TRUE(Data.size() == ExpectedData->size(),
                       "Descriptor resource initializer size mismatch");
        std::copy(ExpectedData->begin(), ExpectedData->end(), Data.begin());
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(cpu_oracle::verifyBufferResult(OutData.data(), OutData.size(),
                                                Oracle, Verbose));
}

static void runExactDescriptorRoundTrip(
    ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
    const MatrixParams &Params,
    const cpu_oracle::MatrixBufferLayout &SourceLayout,
    const cpu_oracle::MatrixBufferLayout &DestinationLayout,
    UINT SourceAlignment, UINT DestinationAlignment, bool Verbose,
    UINT ForcedWaveSize = 0) {
  std::optional<cpu_oracle::TypedMatrix> Input =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N);
  VERIFY_IS_TRUE(Input.has_value(),
                 "Unable to construct descriptor round-trip input");
  std::optional<size_t> SourceBytes =
      cpu_oracle::getMatrixBufferSize(*Input, SourceLayout);
  std::optional<size_t> DestinationBytes =
      cpu_oracle::getMatrixBufferSize(*Input, DestinationLayout);
  VERIFY_IS_TRUE(SourceBytes.has_value() && DestinationBytes.has_value(),
                 "Unable to size descriptor round-trip buffers");

  std::vector<BYTE> InitialOutput(*DestinationBytes, 0xcd);
  std::vector<BYTE> Expected = InitialOutput;
  VERIFY_IS_TRUE(
      cpu_oracle::writeMatrixBuffer(*Input, DestinationLayout, Expected),
      "Unable to encode descriptor round-trip expectation");
  cpu_oracle::BufferResultOracle Oracle = cpu_oracle::exactBufferResult(
      std::move(Expected), L"Descriptor load/store preserves logical values, "
                           L"addressing and padding");

  runDescriptorIO(Device, DxcSupport, Params, *Input, SourceLayout,
                  DestinationLayout, SourceAlignment, DestinationAlignment,
                  *SourceBytes, *DestinationBytes, std::move(InitialOutput),
                  Oracle, Verbose, ForcedWaveSize);
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
  const cpu_oracle::MatrixBufferLayout Layout = {
      LinalgMatrixLayout::RowMajor,
      /*OffsetBytes=*/0,
      /*StrideBytes=*/32,
  };
  runExactDescriptorRoundTrip(D3DDevice, DxcSupport, Params, Layout, Layout,
                              /*SourceAlignment=*/128,
                              /*DestinationAlignment=*/128, VerboseLogging);
}

void DxilConf_SM610_LinAlg::
    LoadStoreDescriptor_Wave_4x8_F16_RowMajorOffsetPadded() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryMatrixConstructionSupport(
      D3DDevice, Params, {{linalg_test::MatrixRole::A, Params.M, Params.N}},
      L"LoadStoreDescriptor_Wave_4x8_F16_RowMajorOffsetPadded", Supported,
      SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(
          Applicability,
          L"LoadStoreDescriptor_Wave_4x8_F16_RowMajorOffsetPadded"))
    return;

  const cpu_oracle::MatrixBufferLayout Target = {
      LinalgMatrixLayout::RowMajor,
      /*OffsetBytes=*/16,
      /*StrideBytes=*/24,
  };
  const cpu_oracle::MatrixBufferLayout Canonical = {
      LinalgMatrixLayout::RowMajor,
      /*OffsetBytes=*/0,
      /*StrideBytes=*/16,
  };
  runExactDescriptorRoundTrip(D3DDevice, DxcSupport, Params, Target, Canonical,
                              /*SourceAlignment=*/16,
                              /*DestinationAlignment=*/128, VerboseLogging,
                              SelectedWaveSize);
  runExactDescriptorRoundTrip(D3DDevice, DxcSupport, Params, Canonical, Target,
                              /*SourceAlignment=*/128,
                              /*DestinationAlignment=*/16, VerboseLogging,
                              SelectedWaveSize);
}

void DxilConf_SM610_LinAlg::
    LoadStoreDescriptor_Wave_4x8_F32_ColumnMajorOffset() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::ColumnMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryMatrixConstructionSupport(
      D3DDevice, Params, {{linalg_test::MatrixRole::A, Params.M, Params.N}},
      L"LoadStoreDescriptor_Wave_4x8_F32_ColumnMajorOffset", Supported,
      SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(
          Applicability, L"LoadStoreDescriptor_Wave_4x8_F32_ColumnMajorOffset"))
    return;

  const cpu_oracle::MatrixBufferLayout Target = {
      LinalgMatrixLayout::ColumnMajor,
      /*OffsetBytes=*/32,
      /*StrideBytes=*/16,
  };
  const cpu_oracle::MatrixBufferLayout Canonical = {
      LinalgMatrixLayout::RowMajor,
      /*OffsetBytes=*/0,
      /*StrideBytes=*/32,
  };
  runExactDescriptorRoundTrip(D3DDevice, DxcSupport, Params, Target, Canonical,
                              /*SourceAlignment=*/32,
                              /*DestinationAlignment=*/128, VerboseLogging,
                              SelectedWaveSize);
  runExactDescriptorRoundTrip(D3DDevice, DxcSupport, Params, Canonical, Target,
                              /*SourceAlignment=*/128,
                              /*DestinationAlignment=*/32, VerboseLogging,
                              SelectedWaveSize);
}

void DxilConf_SM610_LinAlg::LoadDescriptorBounds_Wave_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryMatrixConstructionSupport(
      D3DDevice, Params, {{linalg_test::MatrixRole::A, Params.M, Params.N}},
      L"LoadDescriptorBounds_Wave_4x8_F32", Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability, L"LoadDescriptorBounds_Wave_4x8_F32"))
    return;

  const cpu_oracle::MatrixBufferLayout SourceLayout = {
      LinalgMatrixLayout::RowMajor,
      /*OffsetBytes=*/16,
      /*StrideBytes=*/32,
  };
  const cpu_oracle::MatrixBufferLayout DestinationLayout = {
      LinalgMatrixLayout::RowMajor,
      /*OffsetBytes=*/0,
      /*StrideBytes=*/32,
  };
  constexpr size_t SourceViewBytes = 128;

  std::optional<cpu_oracle::TypedMatrix> Input =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N);
  std::optional<cpu_oracle::TypedMatrix> WholeLoadZero =
      cpu_oracle::makeZeroMatrix(Params.CompType, Params.M, Params.N);
  std::optional<cpu_oracle::TypedMatrix> PerElementLoad =
      Input ? cpu_oracle::makeBoundedReadResult(*Input, SourceLayout,
                                                SourceViewBytes)
            : std::nullopt;
  VERIFY_IS_TRUE(Input.has_value() && WholeLoadZero.has_value() &&
                     PerElementLoad.has_value(),
                 "Unable to construct bounded descriptor load expectations");

  std::optional<size_t> SourceBytes =
      cpu_oracle::getMatrixBufferSize(*Input, SourceLayout);
  std::optional<size_t> DestinationBytes =
      cpu_oracle::getMatrixBufferSize(*Input, DestinationLayout);
  VERIFY_IS_TRUE(SourceBytes.has_value() && DestinationBytes.has_value() &&
                     SourceViewBytes < *SourceBytes,
                 "Bounded descriptor load must cross the source view end");

  std::vector<BYTE> InitialOutput(*DestinationBytes, 0xcd);
  std::vector<BYTE> WholeZero = InitialOutput;
  std::vector<BYTE> PerElement = InitialOutput;
  VERIFY_IS_TRUE(cpu_oracle::writeMatrixBuffer(*WholeLoadZero,
                                               DestinationLayout, WholeZero) &&
                     cpu_oracle::writeMatrixBuffer(
                         *PerElementLoad, DestinationLayout, PerElement),
                 "Unable to encode bounded descriptor load candidates");
  cpu_oracle::BufferResultOracle Oracle = cpu_oracle::permittedBufferResults(
      {std::move(WholeZero), std::move(PerElement)},
      L"Descriptor load may zero the whole matrix or only out-of-bounds "
      L"elements");

  runDescriptorIO(D3DDevice, DxcSupport, Params, *Input, SourceLayout,
                  DestinationLayout, /*SourceAlignment=*/16,
                  /*DestinationAlignment=*/128, SourceViewBytes,
                  *DestinationBytes, std::move(InitialOutput), Oracle,
                  VerboseLogging, SelectedWaveSize);
}

void DxilConf_SM610_LinAlg::StoreDescriptorBounds_Wave_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryMatrixConstructionSupport(
      D3DDevice, Params, {{linalg_test::MatrixRole::A, Params.M, Params.N}},
      L"StoreDescriptorBounds_Wave_4x8_F32", Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability, L"StoreDescriptorBounds_Wave_4x8_F32"))
    return;

  const cpu_oracle::MatrixBufferLayout SourceLayout = {
      LinalgMatrixLayout::RowMajor,
      /*OffsetBytes=*/0,
      /*StrideBytes=*/32,
  };
  const cpu_oracle::MatrixBufferLayout DestinationLayout = {
      LinalgMatrixLayout::RowMajor,
      /*OffsetBytes=*/16,
      /*StrideBytes=*/32,
  };
  constexpr size_t DestinationViewBytes = 128;

  std::optional<cpu_oracle::TypedMatrix> Input =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N);
  std::optional<cpu_oracle::TypedMatrix> InitialMatrix =
      cpu_oracle::makeZeroMatrix(Params.CompType, Params.M, Params.N);
  VERIFY_IS_TRUE(Input.has_value() && InitialMatrix.has_value(),
                 "Unable to construct bounded descriptor store data");
  std::optional<std::vector<BYTE>> InitialOutput =
      cpu_oracle::encodeMatrixBuffer(*InitialMatrix, DestinationLayout, 0xcd);
  std::optional<size_t> SourceBytes =
      cpu_oracle::getMatrixBufferSize(*Input, SourceLayout);
  VERIFY_IS_TRUE(
      InitialOutput.has_value() && SourceBytes.has_value() &&
          DestinationViewBytes < InitialOutput->size(),
      "Bounded descriptor store must cross the destination view end");

  std::vector<BYTE> WholeNoOp = *InitialOutput;
  std::vector<BYTE> PerElementNoOp = *InitialOutput;
  VERIFY_IS_TRUE(
      cpu_oracle::writeMatrixBufferWithinBounds(
          *Input, DestinationLayout, DestinationViewBytes, PerElementNoOp),
      "Unable to encode bounded descriptor store candidate");
  cpu_oracle::BufferResultOracle Oracle = cpu_oracle::permittedBufferResults(
      {std::move(WholeNoOp), std::move(PerElementNoOp)},
      L"Descriptor store may suppress the whole operation or only "
      L"out-of-bounds "
      L"elements");

  runDescriptorIO(D3DDevice, DxcSupport, Params, *Input, SourceLayout,
                  DestinationLayout, /*SourceAlignment=*/128,
                  /*DestinationAlignment=*/16, *SourceBytes,
                  DestinationViewBytes, std::move(*InitialOutput), Oracle,
                  VerboseLogging, SelectedWaveSize);
}

static const char SplatStoreShader[] = R"(
  RWByteAddressBuffer Output : register(u0);

  [WaveSize(4, 64)]
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
                          bool Verbose) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  STREAM_FLOAT(ExtraDefs, "FILL_VALUE", FillValue);

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
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runSplatStore(D3DDevice, DxcSupport, Params, 42.0f, VerboseLogging);
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

  const cpu_oracle::MatrixBufferLayout Layout = {
      LinalgMatrixLayout::RowMajor,
      /*OffsetBytes=*/0,
      /*StrideBytes=*/32,
  };
  std::optional<cpu_oracle::TypedMatrix> Input =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N,
                                       /*StartingValue=*/12,
                                       /*Increment=*/false);
  std::optional<cpu_oracle::TypedMatrix> InitialMatrix =
      cpu_oracle::makeZeroMatrix(Params.CompType, Params.M, Params.N);
  std::optional<cpu_oracle::TypedMatrix> ExpectedMatrix =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N,
                                       /*StartingValue=*/24,
                                       /*Increment=*/false);
  VERIFY_IS_TRUE(Input.has_value() && InitialMatrix.has_value() &&
                     ExpectedMatrix.has_value(),
                 "Unable to construct descriptor accumulation data");
  std::optional<std::vector<BYTE>> InitialOutput =
      cpu_oracle::encodeMatrixBuffer(*InitialMatrix, Layout);
  std::optional<std::vector<BYTE>> Expected =
      cpu_oracle::encodeMatrixBuffer(*ExpectedMatrix, Layout);
  VERIFY_IS_TRUE(InitialOutput.has_value() && Expected.has_value(),
                 "Unable to encode descriptor accumulation buffers");
  cpu_oracle::BufferResultOracle Oracle = cpu_oracle::exactBufferResult(
      std::move(*Expected), L"Two descriptor accumulations add the matrix to "
                            L"each destination element");
  const size_t BufferSize = InitialOutput->size();

  runDescriptorIO(D3DDevice, DxcSupport, Params, *Input, Layout, Layout,
                  /*SourceAlignment=*/128, /*DestinationAlignment=*/128,
                  BufferSize, BufferSize, std::move(*InitialOutput), Oracle,
                  VerboseLogging,
                  /*ForcedWaveSize=*/0, /*AccumulateCount=*/2);
}

void DxilConf_SM610_LinAlg::AccumulateDescriptorBounds_Wave_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryDescriptorAccumulateSupport(
      D3DDevice, Params, L"AccumulateDescriptorBounds_Wave_4x8_F32", Supported,
      SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability,
                          L"AccumulateDescriptorBounds_Wave_4x8_F32"))
    return;

  const cpu_oracle::MatrixBufferLayout SourceLayout = {
      LinalgMatrixLayout::RowMajor,
      /*OffsetBytes=*/0,
      /*StrideBytes=*/32,
  };
  const cpu_oracle::MatrixBufferLayout DestinationLayout = {
      LinalgMatrixLayout::RowMajor,
      /*OffsetBytes=*/16,
      /*StrideBytes=*/32,
  };
  constexpr size_t DestinationViewBytes = 128;

  std::optional<cpu_oracle::TypedMatrix> Input =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N);
  std::optional<cpu_oracle::TypedMatrix> InitialMatrix =
      cpu_oracle::makeZeroMatrix(Params.CompType, Params.M, Params.N);
  VERIFY_IS_TRUE(Input.has_value() && InitialMatrix.has_value(),
                 "Unable to construct bounded descriptor accumulation data");
  std::optional<std::vector<BYTE>> InitialOutput =
      cpu_oracle::encodeMatrixBuffer(*InitialMatrix, DestinationLayout, 0xcd);
  std::optional<size_t> SourceBytes =
      cpu_oracle::getMatrixBufferSize(*Input, SourceLayout);
  VERIFY_IS_TRUE(
      InitialOutput.has_value() && SourceBytes.has_value() &&
          DestinationViewBytes < InitialOutput->size(),
      "Bounded descriptor accumulation must cross the destination view end");

  std::vector<BYTE> WholeNoOp = *InitialOutput;
  std::vector<BYTE> PerElementNoOp = *InitialOutput;
  VERIFY_IS_TRUE(
      cpu_oracle::writeMatrixBufferWithinBounds(
          *Input, DestinationLayout, DestinationViewBytes, PerElementNoOp),
      "Unable to encode bounded descriptor accumulation candidate");
  cpu_oracle::BufferResultOracle Oracle = cpu_oracle::permittedBufferResults(
      {std::move(WholeNoOp), std::move(PerElementNoOp)},
      L"Descriptor accumulation may suppress the whole operation or only "
      L"out-of-bounds elements");

  runDescriptorIO(D3DDevice, DxcSupport, Params, *Input, SourceLayout,
                  DestinationLayout, /*SourceAlignment=*/128,
                  /*DestinationAlignment=*/16, *SourceBytes,
                  DestinationViewBytes, std::move(*InitialOutput), Oracle,
                  VerboseLogging, SelectedWaveSize,
                  /*AccumulateCount=*/1);
}

static LPCWSTR matrixRoleName(linalg_test::MatrixRole Role) {
  switch (Role) {
  case linalg_test::MatrixRole::A:
    return L"A";
  case linalg_test::MatrixRole::B:
    return L"B";
  case linalg_test::MatrixRole::Accumulator:
    return L"Accumulator";
  }
  return L"Unknown";
}

static HRESULT queryMatrixConstructionSupport(
    ID3D12Device *Device, const MatrixParams &Params,
    std::initializer_list<MatrixConstructionRequirement> Requirements,
    LPCWSTR CaseName, bool &Supported, UINT &SelectedWaveSize) {
  Supported = false;
  SelectedWaveSize = 0;
  if (!Device || !CaseName || Requirements.size() == 0 ||
      !linalg_test::isLegalScope(linalg_test::OperationType::MatrixConstruction,
                                 toCapabilityScope(Params.Scope)))
    return E_INVALIDARG;

  for (const MatrixConstructionRequirement &Requirement : Requirements) {
    if (Requirement.RequireShape &&
        (Requirement.Rows == 0 || Requirement.Columns == 0))
      return E_INVALIDARG;
  }

  for (const MatrixConstructionRequirement &Requirement : Requirements) {
    const ComponentType CompType =
        Requirement.CompType == ComponentType::Invalid ? Params.CompType
                                                       : Requirement.CompType;
    if (!toCapabilityDataType(CompType).has_value())
      return E_INVALIDARG;
  }

  linalg_test::TierSupport Tier;
  HRESULT HR = linalg_test::queryTierSupport(Device, Tier);
  if (FAILED(HR) || !Tier.supported())
    return HR;

  D3D12_FEATURE_DATA_D3D12_OPTIONS1 WaveOptions = {};
  HR = Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &WaveOptions,
                                   sizeof(WaveOptions));
  if (FAILED(HR)) {
    hlsl_test::LogCommentFmt(L"Wave-size capability query failed: 0x%08x", HR);
    return HR;
  }
  if (!WaveOptions.WaveOps) {
    hlsl_test::LogCommentFmt(
        L"Wave operations are unsupported; %s is not applicable", CaseName);
    return S_OK;
  }

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

  for (UINT WaveSize = 4; WaveSize <= 64; WaveSize *= 2) {
    if (WaveSize < WaveOptions.WaveLaneCountMin ||
        WaveSize > WaveOptions.WaveLaneCountMax)
      continue;

    std::vector<std::pair<linalg_test::DataType,
                          linalg_test::MatrixConstructionSupport>>
        ConstructionByType;
    bool AllSupported = true;
    for (const MatrixConstructionRequirement &Requirement : Requirements) {
      const ComponentType CompType =
          Requirement.CompType == ComponentType::Invalid ? Params.CompType
                                                         : Requirement.CompType;
      const linalg_test::DataType DataType =
          toCapabilityDataType(CompType).value();
      auto Construction = std::find_if(
          ConstructionByType.begin(), ConstructionByType.end(),
          [DataType](const auto &Entry) { return Entry.first == DataType; });
      if (Construction == ConstructionByType.end()) {
        linalg_test::MatrixConstructionSupport Support;
        HR = linalg_test::queryMatrixConstruction(Device, {DataType, WaveSize},
                                                  Support);
        if (FAILED(HR))
          return HR;
        Construction = ConstructionByType.emplace(ConstructionByType.end(),
                                                  DataType, Support);
      }
      const bool RequirementSupported =
          Requirement.RequireShape
              ? Construction->second.supports(
                    Requirement.Role, Requirement.Rows, Requirement.Columns)
              : Construction->second.supported();
      if (!RequirementSupported) {
        AllSupported = false;
        break;
      }
    }
    if (!AllSupported)
      continue;

    hlsl_test::LogCommentFmt(
        L"MatrixConstruction capability matched wave=%u for %s", WaveSize,
        CaseName);
    for (const MatrixConstructionRequirement &Requirement : Requirements) {
      const ComponentType CompType =
          Requirement.CompType == ComponentType::Invalid ? Params.CompType
                                                         : Requirement.CompType;
      if (Requirement.RequireShape) {
        hlsl_test::LogCommentFmt(L"  type=%s, role=%s, rows=%u, columns=%u",
                                 cpu_oracle::componentTypeName(CompType),
                                 matrixRoleName(Requirement.Role),
                                 Requirement.Rows, Requirement.Columns);
      } else {
        hlsl_test::LogCommentFmt(L"  type=%s",
                                 cpu_oracle::componentTypeName(CompType));
      }
    }
    Supported = true;
    SelectedWaveSize = WaveSize;
    return S_OK;
  }

  hlsl_test::LogCommentFmt(
      L"No MatrixConstruction query within shader WaveSize(4,64) supports %s",
      CaseName);
  return S_OK;
}

static HRESULT queryWaveMatrixMultiplyCaseSupport(
    ID3D12Device *Device, ComponentType MatrixAType, ComponentType MatrixBType,
    ComponentType AccumulatorType, MatrixDim M, MatrixDim K, MatrixDim N,
    LPCWSTR CaseName, bool &Supported, UINT &SelectedWaveSize) {
  Supported = false;
  SelectedWaveSize = 0;
  const std::optional<linalg_test::DataType> MatrixADataType =
      toCapabilityDataType(MatrixAType);
  const std::optional<linalg_test::DataType> MatrixBDataType =
      toCapabilityDataType(MatrixBType);
  const std::optional<linalg_test::DataType> AccumulatorDataType =
      toCapabilityDataType(AccumulatorType);
  if (!Device || !CaseName || M == 0 || K == 0 || N == 0 ||
      !MatrixADataType.has_value() || !MatrixBDataType.has_value() ||
      !AccumulatorDataType.has_value() ||
      !linalg_test::isLegalScope(linalg_test::OperationType::WaveMatrixMultiply,
                                 linalg_test::ExecutionScope::Wave))
    return E_INVALIDARG;

  linalg_test::TierSupport Tier;
  HRESULT HR = linalg_test::queryTierSupport(Device, Tier);
  if (FAILED(HR) || !Tier.supported())
    return HR;

  D3D12_FEATURE_DATA_D3D12_OPTIONS1 WaveOptions = {};
  HR = Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &WaveOptions,
                                   sizeof(WaveOptions));
  if (FAILED(HR)) {
    hlsl_test::LogCommentFmt(L"Wave-size capability query failed: 0x%08x", HR);
    return HR;
  }
  const auto IsPowerOfTwo = [](UINT Value) {
    return Value != 0 && (Value & (Value - 1)) == 0;
  };
  if (!WaveOptions.WaveOps || !IsPowerOfTwo(WaveOptions.WaveLaneCountMin) ||
      !IsPowerOfTwo(WaveOptions.WaveLaneCountMax) ||
      WaveOptions.WaveLaneCountMax < WaveOptions.WaveLaneCountMin) {
    if (!WaveOptions.WaveOps)
      return S_OK;
    hlsl_test::LogCommentFmt(
        L"Wave-size capability response is malformed: WaveOps=%u, min=%u, "
        L"max=%u",
        WaveOptions.WaveOps, WaveOptions.WaveLaneCountMin,
        WaveOptions.WaveLaneCountMax);
    return E_UNEXPECTED;
  }

  for (UINT WaveSize = 4; WaveSize <= 64; WaveSize *= 2) {
    if (WaveSize < WaveOptions.WaveLaneCountMin ||
        WaveSize > WaveOptions.WaveLaneCountMax)
      continue;

    linalg_test::MatrixConstructionSupport MatrixASupport;
    linalg_test::MatrixConstructionSupport MatrixBSupport;
    linalg_test::MatrixConstructionSupport AccumulatorSupport;
    HR = linalg_test::queryMatrixConstruction(
        Device, {*MatrixADataType, WaveSize}, MatrixASupport);
    if (FAILED(HR))
      return HR;
    HR = linalg_test::queryMatrixConstruction(
        Device, {*MatrixBDataType, WaveSize}, MatrixBSupport);
    if (FAILED(HR))
      return HR;
    HR = linalg_test::queryMatrixConstruction(
        Device, {*AccumulatorDataType, WaveSize}, AccumulatorSupport);
    if (FAILED(HR))
      return HR;
    if (!MatrixASupport.supports(linalg_test::MatrixRole::A, M, K) ||
        !MatrixBSupport.supports(linalg_test::MatrixRole::B, K, N) ||
        !AccumulatorSupport.supports(linalg_test::MatrixRole::Accumulator, M,
                                     N))
      continue;

    linalg_test::WaveMatrixMultiplySupport MultiplySupport;
    HR = linalg_test::queryWaveMatrixMultiply(
        Device,
        {WaveSize, *MatrixADataType, *MatrixBDataType, *AccumulatorDataType},
        MultiplySupport);
    if (FAILED(HR))
      return HR;
    if (!MultiplySupport.supportsShape(M, K, N))
      continue;

    hlsl_test::LogCommentFmt(
        L"WaveMatrixMultiply capability matched wave=%u, M=%u, K=%u, N=%u "
        L"for %s",
        WaveSize, M, K, N, CaseName);
    Supported = true;
    SelectedWaveSize = WaveSize;
    return S_OK;
  }

  hlsl_test::LogCommentFmt(
      L"No WaveMatrixMultiply query within the device wave range supports %s",
      CaseName);
  return S_OK;
}

static HRESULT queryThreadGroupMatrixMultiplyCaseSupport(
    ID3D12Device *Device, ComponentType MatrixAType, ComponentType MatrixBType,
    ComponentType AccumulatorType, MatrixDim M, MatrixDim K, MatrixDim N,
    LPCWSTR CaseName, bool &Supported, UINT &SelectedWaveSize,
    UINT &SelectedThreadGroupSize) {
  Supported = false;
  SelectedWaveSize = 0;
  SelectedThreadGroupSize = 0;
  const std::optional<linalg_test::DataType> MatrixADataType =
      toCapabilityDataType(MatrixAType);
  const std::optional<linalg_test::DataType> MatrixBDataType =
      toCapabilityDataType(MatrixBType);
  const std::optional<linalg_test::DataType> AccumulatorDataType =
      toCapabilityDataType(AccumulatorType);
  if (!Device || !CaseName || M == 0 || K == 0 || N == 0 ||
      !MatrixADataType.has_value() || !MatrixBDataType.has_value() ||
      !AccumulatorDataType.has_value() ||
      !linalg_test::isLegalScope(
          linalg_test::OperationType::ThreadGroupMatrixMultiply,
          linalg_test::ExecutionScope::ThreadGroup))
    return E_INVALIDARG;

  linalg_test::TierSupport Tier;
  HRESULT HR = linalg_test::queryTierSupport(Device, Tier);
  if (FAILED(HR) || !Tier.supported())
    return HR;

  D3D12_FEATURE_DATA_D3D12_OPTIONS1 WaveOptions = {};
  HR = Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &WaveOptions,
                                   sizeof(WaveOptions));
  if (FAILED(HR)) {
    hlsl_test::LogCommentFmt(L"Wave-size capability query failed: 0x%08x", HR);
    return HR;
  }
  const auto IsPowerOfTwo = [](UINT Value) {
    return Value != 0 && (Value & (Value - 1)) == 0;
  };
  if (!WaveOptions.WaveOps || !IsPowerOfTwo(WaveOptions.WaveLaneCountMin) ||
      !IsPowerOfTwo(WaveOptions.WaveLaneCountMax) ||
      WaveOptions.WaveLaneCountMax < WaveOptions.WaveLaneCountMin) {
    if (!WaveOptions.WaveOps)
      return S_OK;
    hlsl_test::LogCommentFmt(
        L"Wave-size capability response is malformed: WaveOps=%u, min=%u, "
        L"max=%u",
        WaveOptions.WaveOps, WaveOptions.WaveLaneCountMin,
        WaveOptions.WaveLaneCountMax);
    return E_UNEXPECTED;
  }

  constexpr UINT MaxThreadsPerGroup = 1024;
  for (UINT WaveSize = 4; WaveSize <= 64; WaveSize *= 2) {
    if (WaveSize < WaveOptions.WaveLaneCountMin ||
        WaveSize > WaveOptions.WaveLaneCountMax)
      continue;

    linalg_test::MatrixConstructionSupport MatrixASupport;
    linalg_test::MatrixConstructionSupport MatrixBSupport;
    linalg_test::MatrixConstructionSupport AccumulatorSupport;
    HR = linalg_test::queryMatrixConstruction(
        Device, {*MatrixADataType, WaveSize}, MatrixASupport);
    if (FAILED(HR))
      return HR;
    HR = linalg_test::queryMatrixConstruction(
        Device, {*MatrixBDataType, WaveSize}, MatrixBSupport);
    if (FAILED(HR))
      return HR;
    HR = linalg_test::queryMatrixConstruction(
        Device, {*AccumulatorDataType, WaveSize}, AccumulatorSupport);
    if (FAILED(HR))
      return HR;
    if (!MatrixASupport.supports(linalg_test::MatrixRole::A, M, K) ||
        !MatrixBSupport.supports(linalg_test::MatrixRole::B, K, N) ||
        !AccumulatorSupport.supports(linalg_test::MatrixRole::Accumulator, M,
                                     N))
      continue;

    linalg_test::ThreadGroupMatrixMultiplySupport MultiplySupport;
    HR = linalg_test::queryThreadGroupMatrixMultiply(
        Device,
        {{WaveSize, *MatrixADataType, *MatrixBDataType, *AccumulatorDataType},
         {M, K, N}},
        MultiplySupport);
    if (FAILED(HR))
      return HR;
    if (!MultiplySupport.supported())
      continue;

    UINT ThreadGroupSize = MultiplySupport.PreferredThreadGroupSize;
    if (ThreadGroupSize == 0 || ThreadGroupSize > MaxThreadsPerGroup)
      ThreadGroupSize = MultiplySupport.MinThreadGroupSize;
    if (!MultiplySupport.supportsThreadGroupSize(ThreadGroupSize) ||
        ThreadGroupSize > MaxThreadsPerGroup) {
      hlsl_test::LogCommentFmt(
          L"ThreadGroupMatrixMultiply returned no executable group size for "
          L"%s: min=%u, max=%u, preferred=%u",
          CaseName, MultiplySupport.MinThreadGroupSize,
          MultiplySupport.MaxThreadGroupSize,
          MultiplySupport.PreferredThreadGroupSize);
      return E_UNEXPECTED;
    }

    hlsl_test::LogCommentFmt(
        L"ThreadGroupMatrixMultiply capability matched wave=%u, threads=%u, "
        L"M=%u, K=%u, N=%u for %s",
        WaveSize, ThreadGroupSize, M, K, N, CaseName);
    Supported = true;
    SelectedWaveSize = WaveSize;
    SelectedThreadGroupSize = ThreadGroupSize;
    return S_OK;
  }

  hlsl_test::LogCommentFmt(
      L"No ThreadGroupMatrixMultiply query within the device wave range "
      L"supports %s",
      CaseName);
  return S_OK;
}

static HRESULT queryDescriptorAccumulateSupport(ID3D12Device *Device,
                                                const MatrixParams &Params,
                                                LPCWSTR CaseName,
                                                bool &Supported,
                                                UINT &SelectedWaveSize) {
  return queryAtomicAccumulateSupport(
      Device, Params, linalg_test::AtomicDestination::RWByteAddressBuffer,
      CaseName, Supported, SelectedWaveSize);
}

static HRESULT queryThreadVectorMatrixMultiplySupport(
    ID3D12Device *Device, const MatrixParams &Params,
    ComponentType VectorInputType, ComponentType BiasInputType,
    ComponentType ResultType, LPCWSTR CaseName, bool &Supported) {
  Supported = false;
  if (!Device || !CaseName || Params.Use != MatrixUse::A ||
      !linalg_test::isLegalScope(
          linalg_test::OperationType::ThreadVectorMatrixMultiply,
          toCapabilityScope(Params.Scope)))
    return E_INVALIDARG;

  const std::optional<linalg_test::DataType> VectorType =
      toCapabilityDataType(VectorInputType);
  const std::optional<linalg_test::DataType> MatrixType =
      toCapabilityDataType(Params.CompType);
  const std::optional<linalg_test::DataType> BiasType =
      BiasInputType == ComponentType::Invalid
          ? std::optional<linalg_test::DataType>(linalg_test::DataType::None)
          : toCapabilityDataType(BiasInputType);
  const std::optional<linalg_test::DataType> VectorResultType =
      toCapabilityDataType(ResultType);
  if (!VectorType.has_value() || !MatrixType.has_value() ||
      !BiasType.has_value() || !VectorResultType.has_value())
    return E_INVALIDARG;

  linalg_test::TierSupport Tier;
  HRESULT HR = linalg_test::queryTierSupport(Device, Tier);
  if (FAILED(HR) || !Tier.supported())
    return HR;

  linalg_test::ThreadVectorMatrixMultiplySupport Support;
  HR = linalg_test::queryThreadVectorMatrixMultiply(
      Device, {*VectorType, *MatrixType, *BiasType, *VectorResultType},
      Support);
  if (FAILED(HR))
    return HR;

  const UINT SupportFlags = static_cast<UINT>(Support.SupportFlags);
  const bool EmulatedInputs =
      (SupportFlags &
       static_cast<UINT>(linalg_test::MultiplicationFlags::EmulatedInputs)) !=
      0;
  if (EmulatedInputs && Params.CompType != ComponentType::F8_E4M3FN &&
      Params.CompType != ComponentType::F8_E5M2) {
    hlsl_test::LogCommentFmt(
        L"Thread-vector query returned EMULATED_INPUTS for non-FP8 matrix %s",
        CaseName);
    return E_UNEXPECTED;
  }

  const bool SupportsTranspose =
      (SupportFlags &
       static_cast<UINT>(linalg_test::MultiplicationFlags::Transpose)) != 0;
  Supported =
      Support.supported() &&
      (Params.Layout != LinalgMatrixLayout::ColumnMajor || SupportsTranspose);
  if (!Supported)
    hlsl_test::LogCommentFmt(
        L"Thread-vector matrix multiplication is unsupported for %s", CaseName);
  return S_OK;
}

static HRESULT queryThreadOuterProductSupport(ID3D12Device *Device,
                                              ComponentType InputType,
                                              ComponentType ResultType,
                                              LPCWSTR CaseName,
                                              bool &Supported) {
  Supported = false;
  const std::optional<linalg_test::DataType> InputDataType =
      toCapabilityDataType(InputType);
  const std::optional<linalg_test::DataType> ResultDataType =
      toCapabilityDataType(ResultType);
  if (!Device || !CaseName || !InputDataType.has_value() ||
      !ResultDataType.has_value() ||
      !linalg_test::isLegalScope(linalg_test::OperationType::ThreadOuterProduct,
                                 linalg_test::ExecutionScope::Thread))
    return E_INVALIDARG;

  linalg_test::TierSupport Tier;
  HRESULT HR = linalg_test::queryTierSupport(Device, Tier);
  if (FAILED(HR) || !Tier.supported())
    return HR;

  linalg_test::ThreadOuterProductSupport Support;
  HR = linalg_test::queryThreadOuterProduct(
      Device, {*InputDataType, *ResultDataType}, Support);
  if (FAILED(HR))
    return HR;

  Supported = Support.supported();
  if (!Supported)
    hlsl_test::LogCommentFmt(L"Thread OuterProduct is unsupported for %s",
                             CaseName);
  return S_OK;
}

static HRESULT queryVectorAccumulateDescriptorSupport(ID3D12Device *Device,
                                                      ComponentType CompType,
                                                      LPCWSTR CaseName,
                                                      bool &Supported) {
  Supported = false;
  const std::optional<linalg_test::DataType> DataType =
      toCapabilityDataType(CompType);
  if (!Device || !CaseName || !DataType.has_value() ||
      !linalg_test::isLegalScope(
          linalg_test::OperationType::AtomicAccumulateStore,
          linalg_test::ExecutionScope::Thread))
    return E_INVALIDARG;

  linalg_test::TierSupport Tier;
  HRESULT HR = linalg_test::queryTierSupport(Device, Tier);
  if (FAILED(HR) || !Tier.supported())
    return HR;

  linalg_test::AtomicAccumulateStoreSupport Support;
  HR = linalg_test::queryAtomicAccumulateStore(Device, {*DataType}, Support);
  if (FAILED(HR))
    return HR;

  Supported =
      Support.supports(linalg_test::AtomicDestination::RWByteAddressBuffer);
  if (!Supported)
    hlsl_test::LogCommentFmt(
        L"Vector descriptor accumulation is unsupported for %s", CaseName);
  return S_OK;
}

static HRESULT
queryAtomicAccumulateSupport(ID3D12Device *Device, const MatrixParams &Params,
                             linalg_test::AtomicDestination Destination,
                             LPCWSTR CaseName, bool &Supported,
                             UINT &SelectedWaveSize) {
  Supported = false;
  SelectedWaveSize = 0;
  if (!CaseName ||
      (Destination != linalg_test::AtomicDestination::RWByteAddressBuffer &&
       Destination != linalg_test::AtomicDestination::GroupShared) ||
      !linalg_test::isLegalScope(
          linalg_test::OperationType::AtomicAccumulateStore,
          toCapabilityScope(Params.Scope)))
    return E_INVALIDARG;

  bool ConstructionSupported;
  HRESULT HR = queryMatrixConstructionSupport(
      Device, Params,
      {{linalg_test::MatrixRole::Accumulator, Params.M, Params.N}}, CaseName,
      ConstructionSupported, SelectedWaveSize);
  if (FAILED(HR) || !ConstructionSupported)
    return HR;

  std::optional<linalg_test::DataType> DataType =
      toCapabilityDataType(Params.CompType);
  if (!DataType.has_value())
    return E_INVALIDARG;

  linalg_test::AtomicAccumulateStoreSupport AtomicSupport;
  HR = linalg_test::queryAtomicAccumulateStore(Device, {*DataType},
                                               AtomicSupport);
  if (FAILED(HR))
    return HR;

  Supported = AtomicSupport.supports(Destination);
  if (!Supported) {
    SelectedWaveSize = 0;
    hlsl_test::LogCommentFmt(
        L"Atomic %s accumulation is unsupported for %s",
        Destination == linalg_test::AtomicDestination::RWByteAddressBuffer
            ? L"descriptor"
            : L"group-shared",
        CaseName);
  }
  return S_OK;
}

static const char ElementAccessShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Coordinates : register(u1);
  RWByteAddressBuffer Values : register(u2);
  RWByteAddressBuffer Lengths : register(u3);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 64)]
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
    Lengths.Store<uint>(threadID * sizeof(uint), Len);

    // Each thread writes to a private slice because implementations may map
    // multiple threads to the same matrix component.
    uint SafeLen = min(Len, (uint)(M_DIM * N_DIM));
    for (uint I = 0; I < SafeLen; ++I) {
      uint2 Coord = __builtin_LinAlg_MatrixGetCoordinate(Mat, I);
      ELEM_TYPE Elem;
      __builtin_LinAlg_MatrixGetElement(Elem, Mat, I);
      uint Slot = threadID * (M_DIM * N_DIM) + I;
      Coordinates.Store2(Slot * sizeof(uint2), Coord);
      Values.Store<ELEM_TYPE>(Slot * ELEM_SIZE, Elem);
    }
  }
)";

static void runElementAccess(ID3D12Device *Device,
                             dxc::SpecificDllLoader &DxcSupport,
                             const MatrixParams &Params, bool Verbose,
                             UINT ForcedWaveSize = 0) {
  const size_t NumElements = Params.totalElements();
  const size_t NumThreads = Params.NumThreads;
  const size_t MatrixSize = Params.totalBytes();
  const size_t MaxRecords = NumThreads * NumElements;
  const size_t CoordinateBufferSize = MaxRecords * 2 * sizeof(uint32_t);
  const size_t ValueBufferSize = MaxRecords * elementSize(Params.CompType);
  const size_t LengthBufferSize = NumThreads * sizeof(uint32_t);

  std::stringstream ExtraDefs;
  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;
  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, ElementAccessShader, "cs_6_10", Args, Verbose);

  std::optional<cpu_oracle::TypedMatrix> Expected =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N);
  VERIFY_IS_TRUE(Expected.has_value(),
                 "Unable to construct typed element-access input");
  const cpu_oracle::MatrixBufferLayout InputLayout = {
      Params.Layout,
      0,
      Params.strideBytes(),
  };
  const cpu_oracle::MatrixBufferLayout PackedRowMajor = {
      LinalgMatrixLayout::RowMajor,
      0,
      static_cast<size_t>(Params.N) * elementSize(Params.CompType),
  };
  std::vector<BYTE> ExpectedBytes(MatrixSize, 0);
  VERIFY_IS_TRUE(
      cpu_oracle::writeMatrixBuffer(*Expected, PackedRowMajor, ExpectedBytes),
      "Unable to encode typed element-access expectation");
  const cpu_oracle::TypedMatrix ExpectedMatrix = *Expected;

  auto Op = createComputeOp(ElementAccessShader, "cs_6_10",
                            "UAV(u0), UAV(u1), UAV(u2), UAV(u3)", Args.c_str());
  addUAVBuffer(Op.get(), "Input", MatrixSize, false, "byname");
  addUAVBuffer(Op.get(), "Coordinates", CoordinateBufferSize, true);
  addUAVBuffer(Op.get(), "Values", ValueBufferSize, true);
  addUAVBuffer(Op.get(), "Lengths", LengthBufferSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Coordinates");
  addRootView(Op.get(), 2, "Values");
  addRootView(Op.get(), 3, "Lengths");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [ExpectedMatrix, InputLayout](LPCSTR Name, std::vector<BYTE> &Data,
                                    st::ShaderOp *) {
        if (_stricmp(Name, "Input") != 0)
          return;
        VERIFY_IS_TRUE(
            cpu_oracle::writeMatrixBuffer(ExpectedMatrix, InputLayout, Data),
            "Unable to encode element-access input");
      });

  MappedData CoordinateData;
  MappedData ValueData;
  MappedData LengthData;
  Result->Test->GetReadBackData("Coordinates", &CoordinateData);
  Result->Test->GetReadBackData("Values", &ValueData);
  Result->Test->GetReadBackData("Lengths", &LengthData);
  VERIFY_IS_GREATER_THAN_OR_EQUAL(CoordinateData.size(),
                                  static_cast<UINT32>(CoordinateBufferSize));
  VERIFY_IS_GREATER_THAN_OR_EQUAL(ValueData.size(),
                                  static_cast<UINT32>(ValueBufferSize));
  VERIFY_IS_GREATER_THAN_OR_EQUAL(LengthData.size(),
                                  static_cast<UINT32>(LengthBufferSize));

  const BYTE *CoordinateBytes =
      static_cast<const BYTE *>(CoordinateData.data());
  const BYTE *ValueBytes = static_cast<const BYTE *>(ValueData.data());
  const BYTE *LengthBytes = static_cast<const BYTE *>(LengthData.data());
  const size_t ElementBytes = elementSize(Params.CompType);
  std::vector<bool> Seen(NumElements, false);
  uint64_t TotalLength = 0;
  bool RecordsValid = true;

  for (size_t Thread = 0; Thread < NumThreads && RecordsValid; ++Thread) {
    uint32_t Length;
    std::memcpy(&Length, LengthBytes + Thread * sizeof(Length), sizeof(Length));
    TotalLength += Length;
    if (Length > NumElements) {
      hlsl_test::LogErrorFmt(
          L"MatrixLength returned too many elements: thread=%zu, length=%u, "
          L"matrixElements=%zu",
          Thread, Length, NumElements);
      RecordsValid = false;
      break;
    }

    for (size_t LocalIndex = 0; LocalIndex < Length; ++LocalIndex) {
      const size_t Slot = Thread * NumElements + LocalIndex;
      uint32_t Coordinate[2];
      std::memcpy(Coordinate, CoordinateBytes + Slot * 2 * sizeof(uint32_t),
                  sizeof(Coordinate));
      const uint32_t Row = Coordinate[0];
      const uint32_t Column = Coordinate[1];
      if (Row >= Params.M || Column >= Params.N) {
        hlsl_test::LogErrorFmt(
            L"GetCoordinate returned an invalid coordinate: thread=%zu, "
            L"index=%zu, coordinate=(%u,%u), matrix=(%u,%u)",
            Thread, LocalIndex, Row, Column, Params.M, Params.N);
        RecordsValid = false;
        break;
      }

      const size_t MatrixIndex = static_cast<size_t>(Row) * Params.N + Column;
      if (std::memcmp(ValueBytes + Slot * ElementBytes,
                      ExpectedBytes.data() + MatrixIndex * ElementBytes,
                      ElementBytes) != 0) {
        hlsl_test::LogErrorFmt(
            L"GetElement did not match its reported coordinate: thread=%zu, "
            L"index=%zu, coordinate=(%u,%u)",
            Thread, LocalIndex, Row, Column);
        RecordsValid = false;
        break;
      }
      Seen[MatrixIndex] = true;
    }
  }

  VERIFY_IS_TRUE(RecordsValid,
                 "Matrix Length/GetCoordinate/GetElement records were invalid");
  VERIFY_IS_GREATER_THAN_OR_EQUAL(
      TotalLength, static_cast<uint64_t>(NumElements),
      "Sum of all lengths must cover at least the matrix element count");
  for (size_t MatrixIndex = 0; MatrixIndex < NumElements; ++MatrixIndex) {
    if (!Seen[MatrixIndex]) {
      hlsl_test::LogErrorFmt(
          L"No thread-local element mapped to matrix coordinate=(%zu,%zu)",
          MatrixIndex / Params.N, MatrixIndex % Params.N);
      VERIFY_IS_TRUE(false, "Matrix coordinate was not covered");
    }
  }
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

void DxilConf_SM610_LinAlg::ElementAccess_Wave_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryMatrixConstructionSupport(
      D3DDevice, Params,
      {{linalg_test::MatrixRole::Accumulator, Params.M, Params.N}},
      L"ElementAccess_Wave_4x8_F32", Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability, L"ElementAccess_Wave_4x8_F32"))
    return;

  runElementAccess(D3DDevice, DxcSupport, Params, VerboseLogging,
                   SelectedWaveSize);
}

static const char ElementGetOOBShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Values : register(u1);
  RWByteAddressBuffer Executed : register(u2);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 64)]
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

    uint OOBIndex = __builtin_LinAlg_MatrixLength(Mat);
    ELEM_TYPE Elem;
    __builtin_LinAlg_MatrixGetElement(Elem, Mat, OOBIndex);
    Values.Store<ELEM_TYPE>(threadID * ELEM_SIZE, Elem);
    Executed.Store<uint>(threadID * sizeof(uint), 1);
  }
)";

static void runElementGetOOB(ID3D12Device *Device,
                             dxc::SpecificDllLoader &DxcSupport,
                             const MatrixParams &Params, bool Verbose,
                             UINT ForcedWaveSize = 0) {
  const size_t NumThreads = Params.NumThreads;
  const size_t MatrixSize = Params.totalBytes();
  const size_t ValueBufferSize = NumThreads * elementSize(Params.CompType);
  const size_t ExecutedBufferSize = NumThreads * sizeof(uint32_t);
  std::stringstream ExtraDefs;
  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;
  const std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, ElementGetOOBShader, "cs_6_10", Args, Verbose);

  std::optional<cpu_oracle::TypedMatrix> Input =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N);
  std::optional<cpu_oracle::TypedMatrix> Expected = cpu_oracle::makeZeroMatrix(
      Params.CompType, 1, static_cast<MatrixDim>(NumThreads));
  VERIFY_IS_TRUE(Input.has_value() && Expected.has_value(),
                 "Unable to construct typed GetElement OOB data");
  const cpu_oracle::MatrixBufferLayout InputLayout = {
      Params.Layout,
      0,
      Params.strideBytes(),
  };
  const cpu_oracle::MatrixBufferLayout ValueLayout = {
      LinalgMatrixLayout::RowMajor,
      0,
      ValueBufferSize,
  };
  const cpu_oracle::MatrixResultOracle Oracle = cpu_oracle::exactResult(
      *Expected, L"Matrix::Get returns zero when Index equals Length()");
  const cpu_oracle::TypedMatrix InputMatrix = *Input;

  auto Op = createComputeOp(ElementGetOOBShader, "cs_6_10",
                            "UAV(u0), UAV(u1), UAV(u2)", Args.c_str());
  addUAVBuffer(Op.get(), "Input", MatrixSize, false, "byname");
  addUAVBuffer(Op.get(), "Values", ValueBufferSize, true);
  addUAVBuffer(Op.get(), "Executed", ExecutedBufferSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Values");
  addRootView(Op.get(), 2, "Executed");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [InputMatrix, InputLayout](LPCSTR Name, std::vector<BYTE> &Data,
                                 st::ShaderOp *) {
        if (_stricmp(Name, "Input") != 0)
          return;
        VERIFY_IS_TRUE(
            cpu_oracle::writeMatrixBuffer(InputMatrix, InputLayout, Data),
            "Unable to encode GetElement OOB input");
      });

  MappedData ValueData;
  MappedData ExecutedData;
  Result->Test->GetReadBackData("Values", &ValueData);
  Result->Test->GetReadBackData("Executed", &ExecutedData);
  VERIFY_IS_TRUE(cpu_oracle::verifyMatrixBuffer(
      ValueData.data(), ValueData.size(), ValueLayout, Oracle, Verbose));
  VERIFY_IS_GREATER_THAN_OR_EQUAL(ExecutedData.size(),
                                  static_cast<UINT32>(ExecutedBufferSize));

  const BYTE *ExecutedBytes = static_cast<const BYTE *>(ExecutedData.data());
  size_t ExecutedThreads = 0;
  for (size_t Thread = 0; Thread < NumThreads; ++Thread) {
    uint32_t Marker;
    std::memcpy(&Marker, ExecutedBytes + Thread * sizeof(Marker),
                sizeof(Marker));
    VERIFY_IS_TRUE(Marker == 0 || Marker == 1,
                   "GetElement OOB execution marker was invalid");
    ExecutedThreads += Marker;
  }
  VERIFY_IS_GREATER_THAN(ExecutedThreads, static_cast<size_t>(0),
                         "At least one thread must execute the OOB read");
}

void DxilConf_SM610_LinAlg::ElementGetOOB_Wave_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryMatrixConstructionSupport(
      D3DDevice, Params,
      {{linalg_test::MatrixRole::Accumulator, Params.M, Params.N}},
      L"ElementGetOOB_Wave_4x8_F32", Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability, L"ElementGetOOB_Wave_4x8_F32"))
    return;

  runElementGetOOB(D3DDevice, DxcSupport, Params, VerboseLogging,
                   SelectedWaveSize);
}

static const char ElementSetShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 64)]
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

static const char ElementSetOOBShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 64)]
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

    uint OOBIndex = __builtin_LinAlg_MatrixLength(Mat);
    __builtin_LinAlg_MatrixSetElement(
      Mat, Mat, OOBIndex, (ELEM_TYPE)123);

    __builtin_LinAlg_MatrixStoreToDescriptor(
      Mat, Output, 0, STRIDE, LAYOUT, 128);
  }
)";

static void runElementSet(ID3D12Device *Device,
                          dxc::SpecificDllLoader &DxcSupport,
                          const MatrixParams &Params, const char *Shader,
                          uint32_t ExpectedStartingValue, LPCWSTR PublicRule,
                          bool Verbose, UINT ForcedWaveSize = 0) {
  const size_t MatrixSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  if (ForcedWaveSize != 0)
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << ForcedWaveSize;
  std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());

  compileShader(DxcSupport, Shader, "cs_6_10", Args, Verbose);

  std::optional<cpu_oracle::TypedMatrix> Input =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N);
  std::optional<cpu_oracle::TypedMatrix> Expected =
      cpu_oracle::makeSequentialMatrix(Params.CompType, Params.M, Params.N,
                                       ExpectedStartingValue);
  VERIFY_IS_TRUE(Input.has_value() && Expected.has_value(),
                 "Unable to construct typed SetElement data");
  const cpu_oracle::MatrixBufferLayout Layout = {
      Params.Layout,
      0,
      Params.strideBytes(),
  };
  const cpu_oracle::MatrixResultOracle Oracle =
      cpu_oracle::exactResult(*Expected, PublicRule);
  const cpu_oracle::TypedMatrix InputMatrix = *Input;

  auto Op =
      createComputeOp(Shader, "cs_6_10", "UAV(u0), UAV(u1)", Args.c_str());
  addUAVBuffer(Op.get(), "Input", MatrixSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", MatrixSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [InputMatrix, Layout](LPCSTR Name, std::vector<BYTE> &Data,
                            st::ShaderOp *) {
        if (_stricmp(Name, "Input") != 0)
          return;
        VERIFY_IS_TRUE(cpu_oracle::writeMatrixBuffer(InputMatrix, Layout, Data),
                       "Unable to encode SetElement input");
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);

  VERIFY_IS_TRUE(cpu_oracle::verifyMatrixBuffer(OutData.data(), OutData.size(),
                                                Layout, Oracle, Verbose));
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
  runElementSet(D3DDevice, DxcSupport, Params, ElementSetShader,
                /*ExpectedStartingValue=*/6,
                L"Matrix::Set updates every in-range thread-local element",
                VerboseLogging);
}

void DxilConf_SM610_LinAlg::ElementSetOOB_Wave_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryMatrixConstructionSupport(
      D3DDevice, Params,
      {{linalg_test::MatrixRole::Accumulator, Params.M, Params.N}},
      L"ElementSetOOB_Wave_4x8_F32", Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability, L"ElementSetOOB_Wave_4x8_F32"))
    return;

  runElementSet(D3DDevice, DxcSupport, Params, ElementSetOOBShader,
                /*ExpectedStartingValue=*/1,
                L"Matrix::Set is a no-op when Index equals Length()",
                VerboseLogging, SelectedWaveSize);
}

static const char CopyConvertShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);
  RWByteAddressBuffer SourceAfter : register(u2);

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 64)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Src;
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(DST_COMP_TYPE, DST_M_DIM, DST_N_DIM, USE, SCOPE)]]
      Dst;

    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Src, Input, 0, SRC_STRIDE, LAYOUT, 128);
    __builtin_LinAlg_CopyConvertMatrix(Dst, Src, TRANSPOSE);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Dst, Output, 0, DST_STRIDE, LAYOUT, 128);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Src, SourceAfter, 0, SRC_STRIDE, LAYOUT, 128);
  }
)";

static HRESULT queryCopyConvertSupport(ID3D12Device *Device,
                                       const MatrixParams &Params,
                                       ComponentType DestinationCompType,
                                       bool Transpose, bool &Supported,
                                       UINT &SelectedWaveSize) {
  Supported = false;
  SelectedWaveSize = 0;
  if (Params.Use != MatrixUse::A ||
      DestinationCompType == ComponentType::Invalid)
    return E_INVALIDARG;

  MatrixParams Destination = Params;
  Destination.CompType = DestinationCompType;
  if (Transpose) {
    Destination.M = Params.N;
    Destination.N = Params.M;
  }

  return queryMatrixConstructionSupport(
      Device, Params,
      {
          {linalg_test::MatrixRole::A, Params.M, Params.N, Params.CompType},
          {linalg_test::MatrixRole::A, Destination.M, Destination.N,
           Destination.CompType},
      },
      L"CopyConvert source and destination", Supported, SelectedWaveSize);
}

static void runCopyConvert(ID3D12Device *Device,
                           dxc::SpecificDllLoader &DxcSupport,
                           const MatrixParams &Params,
                           ComponentType DestinationCompType, bool Verbose,
                           bool Transpose, UINT ForcedWaveSize = 0) {
  MatrixParams DstParams = Params;
  DstParams.CompType = DestinationCompType;
  if (Transpose) {
    DstParams.M = Params.N;
    DstParams.N = Params.M;
  }

  std::stringstream ExtraDefs;
  ExtraDefs << " -DTRANSPOSE=" << Transpose;
  ExtraDefs << " -DDST_COMP_TYPE=" << static_cast<int>(DstParams.CompType);
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
  std::optional<cpu_oracle::TypedMatrix> Converted =
      cpu_oracle::makeSequentialMatrix(DstParams.CompType, Params.M, Params.N);
  VERIFY_IS_TRUE(Converted.has_value(),
                 "Unable to construct typed CopyConvert conversion oracle");
  std::optional<cpu_oracle::TypedMatrix> Expected =
      Transpose ? cpu_oracle::transposeMatrix(*Converted) : Converted;
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
  cpu_oracle::MatrixResultOracle SourceOracle = cpu_oracle::exactResult(
      *Input, L"CopyConvertMatrix leaves the source matrix unmodified");

  auto Op = createComputeOp(CopyConvertShader, "cs_6_10",
                            "UAV(u0), UAV(u1), UAV(u2)", Args.c_str());
  addUAVBuffer(Op.get(), "Input", *SourceBufferSize, false, "byname");
  addUAVBuffer(Op.get(), "Output", *DestinationBufferSize, true);
  addUAVBuffer(Op.get(), "SourceAfter", *SourceBufferSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");
  addRootView(Op.get(), 2, "SourceAfter");

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
  MappedData SourceAfterData;
  Result->Test->GetReadBackData("Output", &OutData);
  Result->Test->GetReadBackData("SourceAfter", &SourceAfterData);

  VERIFY_IS_TRUE(cpu_oracle::verifyMatrixBuffer(
      OutData.data(), OutData.size(), DestinationLayout, Oracle, Verbose));
  VERIFY_IS_TRUE(cpu_oracle::verifyMatrixBuffer(
      SourceAfterData.data(), SourceAfterData.size(), SourceLayout,
      SourceOracle, Verbose));
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
  runCopyConvert(D3DDevice, DxcSupport, Params, ComponentType::F16,
                 VerboseLogging,
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
  runCopyConvert(D3DDevice, DxcSupport, Params, ComponentType::F16,
                 VerboseLogging,
                 /*Transpose=*/true);
}

void DxilConf_SM610_LinAlg::CopyConvert_Wave_4x8_F32_Transpose() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult =
      queryCopyConvertSupport(D3DDevice, Params, ComponentType::F32,
                              /*Transpose=*/true, Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(
          Applicability,
          L"CopyConvert_Wave_4x8_F32_Transpose MatrixConstruction"))
    return;

  // Non-square dimensions make the destination shape and row stride observable.
  runCopyConvert(D3DDevice, DxcSupport, Params, ComponentType::F32,
                 VerboseLogging,
                 /*Transpose=*/true, SelectedWaveSize);
}

void DxilConf_SM610_LinAlg::CopyConvert_Wave_4x8_F16_ToF32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult =
      queryCopyConvertSupport(D3DDevice, Params, ComponentType::F32,
                              /*Transpose=*/false, Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability,
                          L"CopyConvert_Wave_4x8_F16_ToF32 MatrixConstruction"))
    return;

  runCopyConvert(D3DDevice, DxcSupport, Params, ComponentType::F32,
                 VerboseLogging, /*Transpose=*/false, SelectedWaveSize);
}

void DxilConf_SM610_LinAlg::CopyConvert_Wave_4x8_F32_ToF16_Transpose() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult =
      queryCopyConvertSupport(D3DDevice, Params, ComponentType::F16,
                              /*Transpose=*/true, Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(
          Applicability,
          L"CopyConvert_Wave_4x8_F32_ToF16_Transpose MatrixConstruction"))
    return;

  runCopyConvert(D3DDevice, DxcSupport, Params, ComponentType::F16,
                 VerboseLogging, /*Transpose=*/true, SelectedWaveSize);
}

static const char MatMatMulShader[] = R"(
  #define USE_A 0
  #define USE_B 1
  #define USE_ACC 2

  RWByteAddressBuffer Output : register(u0);

  [WaveSize(4, 64)]
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
                         float AFill, float BFill) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DK_DIM=" << K;
  STREAM_FLOAT(ExtraDefs, "A_FILL", AFill);
  STREAM_FLOAT(ExtraDefs, "B_FILL", BFill);

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
                              float CFill) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DK_DIM=" << K;
  STREAM_FLOAT(ExtraDefs, "A_FILL", AFill);
  STREAM_FLOAT(ExtraDefs, "B_FILL", BFill);
  STREAM_FLOAT(ExtraDefs, "C_FILL", CFill);

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
                        float RHSFill) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  STREAM_FLOAT(ExtraDefs, "LHS_FILL", LHSFill);
  STREAM_FLOAT(ExtraDefs, "RHS_FILL", RHSFill);

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
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runMatAccum(D3DDevice, DxcSupport, Params, VerboseLogging,
              /*LHSFill=*/2.0f, /*RHSFill=*/3.0f);
}

static MatrixParams makeWaveArithmeticParams(ComponentType CompType,
                                             MatrixDim M, MatrixDim N,
                                             MatrixUse Use, UINT WaveSize) {
  MatrixParams Params = {};
  Params.CompType = CompType;
  Params.M = M;
  Params.N = N;
  Params.Use = Use;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = static_cast<int>(WaveSize);
  Params.Enable16Bit = needs16BitTypes(CompType);
  return Params;
}

static std::vector<int64_t>
makeMatrixArithmeticPattern(MatrixDim M, MatrixDim N, int64_t RowScale,
                            int64_t ColumnScale, int64_t Modulus,
                            int64_t Center) {
  VERIFY_IS_TRUE(M != 0 && N != 0 && Modulus > 0);
  if (M == 0 || N == 0 || Modulus <= 0)
    return {};
  std::vector<int64_t> Values(static_cast<size_t>(M) * N);
  for (MatrixDim Row = 0; Row < M; ++Row) {
    for (MatrixDim Column = 0; Column < N; ++Column) {
      Values[static_cast<size_t>(Row) * N + Column] =
          (static_cast<int64_t>(Row) * RowScale +
           static_cast<int64_t>(Column) * ColumnScale) %
              Modulus -
          Center;
    }
  }
  return Values;
}

enum class MatrixMultiplyOperation {
  Multiply,
  MultiplyAccumulate,
};

struct MatrixMultiplyCaseData {
  ComponentType MatrixAType = ComponentType::Invalid;
  ComponentType MatrixBType = ComponentType::Invalid;
  ComponentType AccumulatorType = ComponentType::Invalid;
  MatrixDim M = 0;
  MatrixDim K = 0;
  MatrixDim N = 0;
  MatrixMultiplyOperation Operation = MatrixMultiplyOperation::Multiply;
  std::vector<int64_t> MatrixAValues;
  std::vector<int64_t> MatrixBValues;
  std::vector<int64_t> AccumulatorValues;
  std::wstring PublicRule;

  bool hasInitialAccumulator() const {
    return Operation == MatrixMultiplyOperation::MultiplyAccumulate;
  }
};

static bool isMatrixMultiplyCaseValid(const MatrixMultiplyCaseData &Case) {
  const size_t MatrixAElements = static_cast<size_t>(Case.M) * Case.K;
  const size_t MatrixBElements = static_cast<size_t>(Case.K) * Case.N;
  const size_t AccumulatorElements = static_cast<size_t>(Case.M) * Case.N;
  return Case.M != 0 && Case.K != 0 && Case.N != 0 &&
         Case.MatrixAValues.size() == MatrixAElements &&
         Case.MatrixBValues.size() == MatrixBElements &&
         Case.hasInitialAccumulator() ==
             (Case.AccumulatorValues.size() == AccumulatorElements) &&
         toCapabilityDataType(Case.MatrixAType).has_value() &&
         toCapabilityDataType(Case.MatrixBType).has_value() &&
         toCapabilityDataType(Case.AccumulatorType).has_value() &&
         !Case.PublicRule.empty();
}

static std::optional<std::vector<int64_t>>
calculateMatrixMultiplyExpected(const MatrixMultiplyCaseData &Case) {
  const std::vector<int64_t> *Accumulator =
      Case.hasInitialAccumulator() ? &Case.AccumulatorValues : nullptr;
  return cpu_oracle::multiplyIntegerMatrices(Case.M, Case.K, Case.N,
                                             Case.MatrixAValues,
                                             Case.MatrixBValues, Accumulator);
}

static const char WaveMultiplyShader[] = R"(
  #define USE_A 0
  #define USE_B 1
  #define USE_ACC 2
  #define SCOPE_WAVE 1
  #define LAYOUT_ROW_MAJOR 0

  ByteAddressBuffer MatrixAInput : register(t0);
  ByteAddressBuffer MatrixBInput : register(t1);
#if DO_ACCUMULATE
  ByteAddressBuffer AccumulatorInput : register(t2);
  RWByteAddressBuffer Output : register(u3);
#else
  RWByteAddressBuffer Output : register(u2);
#endif

  [WaveSize(FORCED_WAVE_SIZE)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        MATRIX_A_COMP_TYPE, M_DIM, K_DIM, USE_A, SCOPE_WAVE)]]
      MatA;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      MatA, MatrixAInput, 0, MATRIX_A_STRIDE, LAYOUT_ROW_MAJOR, 128);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        MATRIX_B_COMP_TYPE, K_DIM, N_DIM, USE_B, SCOPE_WAVE)]]
      MatB;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      MatB, MatrixBInput, 0, MATRIX_B_STRIDE, LAYOUT_ROW_MAJOR, 128);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        ACCUMULATOR_COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE_WAVE)]]
      Result;
#if DO_ACCUMULATE
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        ACCUMULATOR_COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE_WAVE)]]
      Accumulator;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Accumulator, AccumulatorInput, 0, ACCUMULATOR_STRIDE,
      LAYOUT_ROW_MAJOR, 128);
    __builtin_LinAlg_MatrixMatrixMultiplyAccumulate(
      Result, MatA, MatB, Accumulator);
#else
    __builtin_LinAlg_MatrixMatrixMultiply(Result, MatA, MatB);
#endif

    __builtin_LinAlg_MatrixStoreToDescriptor(
      Result, Output, 0, ACCUMULATOR_STRIDE, LAYOUT_ROW_MAJOR, 128);
  }
)";

static std::optional<std::string>
buildWaveMultiplyCompilerArgs(const MatrixMultiplyCaseData &Case,
                              UINT WaveSize) {
  if (!isMatrixMultiplyCaseValid(Case) || WaveSize == 0)
    return std::nullopt;

  const MatrixParams MatrixA = makeWaveArithmeticParams(
      Case.MatrixAType, Case.M, Case.K, MatrixUse::A, WaveSize);
  const MatrixParams MatrixB = makeWaveArithmeticParams(
      Case.MatrixBType, Case.K, Case.N, MatrixUse::B, WaveSize);
  const MatrixParams Accumulator = makeWaveArithmeticParams(
      Case.AccumulatorType, Case.M, Case.N, MatrixUse::Accumulator, WaveSize);

  std::stringstream SS;
  SS << "-HV 202x";
  SS << " -DMATRIX_A_COMP_TYPE=" << static_cast<int>(Case.MatrixAType);
  SS << " -DMATRIX_B_COMP_TYPE=" << static_cast<int>(Case.MatrixBType);
  SS << " -DACCUMULATOR_COMP_TYPE=" << static_cast<int>(Case.AccumulatorType);
  SS << " -DM_DIM=" << Case.M;
  SS << " -DK_DIM=" << Case.K;
  SS << " -DN_DIM=" << Case.N;
  SS << " -DMATRIX_A_STRIDE=" << MatrixA.strideBytes();
  SS << " -DMATRIX_B_STRIDE=" << MatrixB.strideBytes();
  SS << " -DACCUMULATOR_STRIDE=" << Accumulator.strideBytes();
  SS << " -DNUMTHREADS=" << WaveSize;
  SS << " -DFORCED_WAVE_SIZE=" << WaveSize;
  SS << " -DDO_ACCUMULATE=" << Case.hasInitialAccumulator();
  if (needs16BitTypes(Case.MatrixAType) || needs16BitTypes(Case.MatrixBType) ||
      needs16BitTypes(Case.AccumulatorType))
    SS << " -enable-16bit-types";
  return SS.str();
}

static void runWaveMultiplyCase(ID3D12Device *Device,
                                dxc::SpecificDllLoader &DxcSupport,
                                const MatrixMultiplyCaseData &Case,
                                UINT WaveSize, bool Verbose) {
  const MatrixParams MatrixA = makeWaveArithmeticParams(
      Case.MatrixAType, Case.M, Case.K, MatrixUse::A, WaveSize);
  const MatrixParams MatrixB = makeWaveArithmeticParams(
      Case.MatrixBType, Case.K, Case.N, MatrixUse::B, WaveSize);
  const MatrixParams Accumulator = makeWaveArithmeticParams(
      Case.AccumulatorType, Case.M, Case.N, MatrixUse::Accumulator, WaveSize);
  const std::optional<std::vector<BYTE>> MatrixABuffer =
      cpu_oracle::encodeLogicalMatrixBuffer(MatrixA, Case.MatrixAValues);
  const std::optional<std::vector<BYTE>> MatrixBBuffer =
      cpu_oracle::encodeLogicalMatrixBuffer(MatrixB, Case.MatrixBValues);
  const std::optional<std::vector<BYTE>> AccumulatorBuffer =
      Case.hasInitialAccumulator() ? cpu_oracle::encodeLogicalMatrixBuffer(
                                         Accumulator, Case.AccumulatorValues)
                                   : std::optional<std::vector<BYTE>>();
  const std::optional<std::vector<int64_t>> ExpectedValues =
      calculateMatrixMultiplyExpected(Case);
  const std::optional<std::vector<BYTE>> Expected =
      ExpectedValues.has_value()
          ? cpu_oracle::encodeLogicalMatrixBuffer(Accumulator, *ExpectedValues)
          : std::optional<std::vector<BYTE>>();
  const std::optional<std::string> Args =
      buildWaveMultiplyCompilerArgs(Case, WaveSize);
  VERIFY_IS_TRUE(MatrixABuffer.has_value());
  VERIFY_IS_TRUE(MatrixBBuffer.has_value());
  VERIFY_IS_TRUE(!Case.hasInitialAccumulator() ||
                 AccumulatorBuffer.has_value());
  VERIFY_IS_TRUE(Expected.has_value());
  VERIFY_IS_TRUE(Args.has_value());
  if (!MatrixABuffer.has_value() || !MatrixBBuffer.has_value() ||
      (Case.hasInitialAccumulator() && !AccumulatorBuffer.has_value()) ||
      !Expected.has_value() || !Args.has_value())
    return;

  const char *RootSignature = Case.hasInitialAccumulator()
                                  ? "SRV(t0), SRV(t1), SRV(t2), UAV(u3)"
                                  : "SRV(t0), SRV(t1), UAV(u2)";
  compileShader(DxcSupport, WaveMultiplyShader, "cs_6_10", *Args, Verbose);

  auto Op = createComputeOp(WaveMultiplyShader, "cs_6_10", RootSignature,
                            Args->c_str());
  addSRVBuffer(Op.get(), "MatrixAInput", MatrixABuffer->size(), "byname");
  addSRVBuffer(Op.get(), "MatrixBInput", MatrixBBuffer->size(), "byname");
  if (Case.hasInitialAccumulator())
    addSRVBuffer(Op.get(), "AccumulatorInput", AccumulatorBuffer->size(),
                 "byname");
  addUAVBuffer(Op.get(), "Output", Expected->size(), true);
  addRootView(Op.get(), 0, "MatrixAInput");
  addRootView(Op.get(), 1, "MatrixBInput");
  if (Case.hasInitialAccumulator()) {
    addRootView(Op.get(), 2, "AccumulatorInput");
    addRootView(Op.get(), 3, "Output");
  } else {
    addRootView(Op.get(), 2, "Output");
  }

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *) {
        const std::vector<BYTE> *Source = nullptr;
        if (strcmp(Name, "MatrixAInput") == 0)
          Source = &*MatrixABuffer;
        else if (strcmp(Name, "MatrixBInput") == 0)
          Source = &*MatrixBBuffer;
        else if (Case.hasInitialAccumulator() &&
                 strcmp(Name, "AccumulatorInput") == 0)
          Source = &*AccumulatorBuffer;
        if (!Source)
          return;
        VERIFY_IS_TRUE(Data.size() == Source->size(),
                       "Wave matrix arithmetic initializer size mismatch");
        std::copy(Source->begin(), Source->end(), Data.begin());
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  const cpu_oracle::BufferResultOracle Oracle =
      cpu_oracle::exactBufferResult(*Expected, Case.PublicRule);
  VERIFY_IS_TRUE(cpu_oracle::verifyBufferResult(OutData.data(), OutData.size(),
                                                Oracle, Verbose));
}

static void
runCapabilityCheckedWaveMultiply(ID3D12Device *Device,
                                 dxc::SpecificDllLoader &DxcSupport,
                                 const MatrixMultiplyCaseData &Case,
                                 linalg_test::CapabilityRequirement Requirement,
                                 LPCWSTR CaseName, bool Verbose) {
  VERIFY_IS_TRUE(isMatrixMultiplyCaseValid(Case));
  if (!isMatrixMultiplyCaseValid(Case))
    return;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT HR = queryWaveMatrixMultiplyCaseSupport(
      Device, Case.MatrixAType, Case.MatrixBType, Case.AccumulatorType, Case.M,
      Case.K, Case.N, CaseName, Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(HR, Supported, Requirement);
  if (!applyApplicability(Applicability, CaseName))
    return;
  runWaveMultiplyCase(Device, DxcSupport, Case, SelectedWaveSize, Verbose);
}

static MatrixMultiplyCaseData
makeRectangularF16WaveMultiplyCase(ComponentType AccumulatorType,
                                   MatrixMultiplyOperation Operation) {
  MatrixMultiplyCaseData Case = {};
  Case.MatrixAType = ComponentType::F16;
  Case.MatrixBType = ComponentType::F16;
  Case.AccumulatorType = AccumulatorType;
  Case.M = 8;
  Case.K = 32;
  Case.N = 16;
  Case.Operation = Operation;
  Case.MatrixAValues = makeMatrixArithmeticPattern(Case.M, Case.K, 3, 2, 5, 2);
  Case.MatrixBValues = makeMatrixArithmeticPattern(Case.K, Case.N, 1, 3, 7, 3);
  if (Case.hasInitialAccumulator()) {
    Case.AccumulatorValues =
        makeMatrixArithmeticPattern(Case.M, Case.N, 2, 1, 5, 2);
    Case.PublicRule =
        L"Exact non-uniform F16 products plus an independent F32 accumulator";
  } else if (AccumulatorType == ComponentType::F32) {
    Case.PublicRule =
        L"Exact non-uniform F16 matrix product accumulated and stored as F32";
  } else {
    Case.PublicRule = L"Exact non-uniform rectangular F16 matrix product";
  }
  return Case;
}

void DxilConf_SM610_LinAlg::MatMatMul_Wave_8x32x16_F16_NonUniform() {
  const MatrixMultiplyCaseData Case = makeRectangularF16WaveMultiplyCase(
      ComponentType::F16, MatrixMultiplyOperation::Multiply);
  runCapabilityCheckedWaveMultiply(
      D3DDevice, DxcSupport, Case,
      linalg_test::CapabilityRequirement::CapabilityGated,
      L"MatMatMul_Wave_8x32x16_F16_NonUniform", VerboseLogging);
}

void DxilConf_SM610_LinAlg::MatMatMul_Wave_8x32x16_F16_ToF32() {
  const MatrixMultiplyCaseData Case = makeRectangularF16WaveMultiplyCase(
      ComponentType::F32, MatrixMultiplyOperation::Multiply);
  runCapabilityCheckedWaveMultiply(
      D3DDevice, DxcSupport, Case,
      linalg_test::CapabilityRequirement::CapabilityGated,
      L"MatMatMul_Wave_8x32x16_F16_ToF32", VerboseLogging);
}

void DxilConf_SM610_LinAlg::MatMatMulAccum_Wave_8x32x16_F16_ToF32_NonUniform() {
  const MatrixMultiplyCaseData Case = makeRectangularF16WaveMultiplyCase(
      ComponentType::F32, MatrixMultiplyOperation::MultiplyAccumulate);
  runCapabilityCheckedWaveMultiply(
      D3DDevice, DxcSupport, Case,
      linalg_test::CapabilityRequirement::CapabilityGated,
      L"MatMatMulAccum_Wave_8x32x16_F16_ToF32_NonUniform", VerboseLogging);
}

void DxilConf_SM610_LinAlg::MatMatMul_Wave_16x16x16_I32() {
  MatrixMultiplyCaseData Case = {};
  Case.MatrixAType = ComponentType::I32;
  Case.MatrixBType = ComponentType::I32;
  Case.AccumulatorType = ComponentType::I32;
  Case.M = 16;
  Case.K = 16;
  Case.N = 16;
  Case.MatrixAValues = makeMatrixArithmeticPattern(Case.M, Case.K, 3, 2, 5, 2);
  Case.MatrixBValues = makeMatrixArithmeticPattern(Case.K, Case.N, 1, 3, 7, 3);
  Case.PublicRule = L"Exact non-uniform I32 matrix product";
  runCapabilityCheckedWaveMultiply(
      D3DDevice, DxcSupport, Case,
      linalg_test::CapabilityRequirement::CapabilityGated,
      L"MatMatMul_Wave_16x16x16_I32", VerboseLogging);
}

static MatrixParams makeThreadGroupArithmeticParams(ComponentType CompType,
                                                    MatrixDim M, MatrixDim N,
                                                    MatrixUse Use,
                                                    UINT ThreadGroupSize) {
  MatrixParams Params = {};
  Params.CompType = CompType;
  Params.M = M;
  Params.N = N;
  Params.Use = Use;
  Params.Scope = MatrixScope::ThreadGroup;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = static_cast<int>(ThreadGroupSize);
  Params.Enable16Bit = needs16BitTypes(CompType);
  return Params;
}

static const char ThreadGroupMultiplyShader[] = R"(
  #define USE_A 0
  #define USE_B 1
  #define USE_ACC 2
  #define SCOPE_THREAD_GROUP 2
  #define LAYOUT_ROW_MAJOR 0

  ByteAddressBuffer MatrixAInput : register(t0);
  ByteAddressBuffer MatrixBInput : register(t1);
  #if DO_ACCUMULATE
  ByteAddressBuffer AccumulatorInput : register(t2);
  RWByteAddressBuffer Output : register(u3);
  #else
  RWByteAddressBuffer Output : register(u2);
  #endif

  groupshared MATRIX_A_ELEM_TYPE MatrixAData[MATRIX_A_ELEMENTS];
  groupshared MATRIX_B_ELEM_TYPE MatrixBData[MATRIX_B_ELEMENTS];
  #if DO_ACCUMULATE
  groupshared ACCUMULATOR_ELEM_TYPE AccumulatorData[ACCUMULATOR_ELEMENTS];
  #endif
  groupshared ACCUMULATOR_ELEM_TYPE ResultData[ACCUMULATOR_ELEMENTS];

  [WaveSize(FORCED_WAVE_SIZE)]
  [numthreads(THREADGROUP_SIZE, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    for (uint Index = threadID; Index < MATRIX_A_ELEMENTS;
         Index += THREADGROUP_SIZE) {
      MatrixAData[Index] =
        MatrixAInput.Load<MATRIX_A_ELEM_TYPE>(Index * MATRIX_A_ELEM_SIZE);
    }
    for (uint Index = threadID; Index < MATRIX_B_ELEMENTS;
         Index += THREADGROUP_SIZE) {
      MatrixBData[Index] =
        MatrixBInput.Load<MATRIX_B_ELEM_TYPE>(Index * MATRIX_B_ELEM_SIZE);
    }
    #if DO_ACCUMULATE
    for (uint Index = threadID; Index < ACCUMULATOR_ELEMENTS;
         Index += THREADGROUP_SIZE) {
      AccumulatorData[Index] =
        AccumulatorInput.Load<ACCUMULATOR_ELEM_TYPE>(
          Index * ACCUMULATOR_ELEM_SIZE);
    }
    #endif
    for (uint Index = threadID; Index < ACCUMULATOR_ELEMENTS;
         Index += THREADGROUP_SIZE) {
      ResultData[Index] = (ACCUMULATOR_ELEM_TYPE)-4096;
    }

    GroupMemoryBarrierWithGroupSync();

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        MATRIX_A_COMP_TYPE, M_DIM, K_DIM, USE_A, SCOPE_THREAD_GROUP)]]
      MatA;
    __builtin_LinAlg_MatrixLoadFromMemory(
      MatA, MatrixAData, 0, MATRIX_A_STRIDE, LAYOUT_ROW_MAJOR);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        MATRIX_B_COMP_TYPE, K_DIM, N_DIM, USE_B, SCOPE_THREAD_GROUP)]]
      MatB;
    __builtin_LinAlg_MatrixLoadFromMemory(
      MatB, MatrixBData, 0, MATRIX_B_STRIDE, LAYOUT_ROW_MAJOR);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        ACCUMULATOR_COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE_THREAD_GROUP)]]
      Result;
    #if DO_ACCUMULATE
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        ACCUMULATOR_COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE_THREAD_GROUP)]]
      Accumulator;
    __builtin_LinAlg_MatrixLoadFromMemory(
      Accumulator, AccumulatorData, 0, ACCUMULATOR_STRIDE, LAYOUT_ROW_MAJOR);
    __builtin_LinAlg_MatrixMatrixMultiplyAccumulate(
      Result, MatA, MatB, Accumulator);
    #else
    __builtin_LinAlg_MatrixMatrixMultiply(Result, MatA, MatB);
    #endif

    __builtin_LinAlg_MatrixStoreToMemory(
      Result, ResultData, 0, ACCUMULATOR_STRIDE, LAYOUT_ROW_MAJOR);

    GroupMemoryBarrierWithGroupSync();

    for (uint Index = threadID; Index < ACCUMULATOR_ELEMENTS;
         Index += THREADGROUP_SIZE) {
      Output.Store<ACCUMULATOR_ELEM_TYPE>(
        Index * ACCUMULATOR_ELEM_SIZE, ResultData[Index]);
    }
  }
)";

static std::optional<std::string>
buildThreadGroupMultiplyCompilerArgs(const MatrixMultiplyCaseData &Case,
                                     UINT WaveSize, UINT ThreadGroupSize) {
  if (!isMatrixMultiplyCaseValid(Case) || WaveSize == 0 || ThreadGroupSize == 0)
    return std::nullopt;

  const char *MatrixAElementType =
      cpu_oracle::hlslElementTypeName(Case.MatrixAType);
  const char *MatrixBElementType =
      cpu_oracle::hlslElementTypeName(Case.MatrixBType);
  const char *AccumulatorElementType =
      cpu_oracle::hlslElementTypeName(Case.AccumulatorType);
  if (!MatrixAElementType || !MatrixBElementType || !AccumulatorElementType)
    return std::nullopt;

  const MatrixParams MatrixA = makeThreadGroupArithmeticParams(
      Case.MatrixAType, Case.M, Case.K, MatrixUse::A, ThreadGroupSize);
  const MatrixParams MatrixB = makeThreadGroupArithmeticParams(
      Case.MatrixBType, Case.K, Case.N, MatrixUse::B, ThreadGroupSize);
  const MatrixParams Accumulator =
      makeThreadGroupArithmeticParams(Case.AccumulatorType, Case.M, Case.N,
                                      MatrixUse::Accumulator, ThreadGroupSize);

  std::stringstream SS;
  SS << "-HV 202x";
  SS << " -DMATRIX_A_COMP_TYPE=" << static_cast<int>(Case.MatrixAType);
  SS << " -DMATRIX_B_COMP_TYPE=" << static_cast<int>(Case.MatrixBType);
  SS << " -DACCUMULATOR_COMP_TYPE=" << static_cast<int>(Case.AccumulatorType);
  SS << " -DMATRIX_A_ELEM_TYPE=" << MatrixAElementType;
  SS << " -DMATRIX_B_ELEM_TYPE=" << MatrixBElementType;
  SS << " -DACCUMULATOR_ELEM_TYPE=" << AccumulatorElementType;
  SS << " -DM_DIM=" << Case.M;
  SS << " -DK_DIM=" << Case.K;
  SS << " -DN_DIM=" << Case.N;
  SS << " -DMATRIX_A_ELEMENTS=" << MatrixA.totalElements();
  SS << " -DMATRIX_B_ELEMENTS=" << MatrixB.totalElements();
  SS << " -DACCUMULATOR_ELEMENTS=" << Accumulator.totalElements();
  SS << " -DMATRIX_A_ELEM_SIZE="
     << static_cast<int>(elementSize(Case.MatrixAType));
  SS << " -DMATRIX_B_ELEM_SIZE="
     << static_cast<int>(elementSize(Case.MatrixBType));
  SS << " -DACCUMULATOR_ELEM_SIZE="
     << static_cast<int>(elementSize(Case.AccumulatorType));
  SS << " -DMATRIX_A_STRIDE=" << MatrixA.N;
  SS << " -DMATRIX_B_STRIDE=" << MatrixB.N;
  SS << " -DACCUMULATOR_STRIDE=" << Accumulator.N;
  SS << " -DTHREADGROUP_SIZE=" << ThreadGroupSize;
  SS << " -DFORCED_WAVE_SIZE=" << WaveSize;
  SS << " -DDO_ACCUMULATE=" << Case.hasInitialAccumulator();
  if (needs16BitTypes(Case.MatrixAType) || needs16BitTypes(Case.MatrixBType) ||
      needs16BitTypes(Case.AccumulatorType))
    SS << " -enable-16bit-types";
  return SS.str();
}

static void runThreadGroupMultiplyCase(ID3D12Device *Device,
                                       dxc::SpecificDllLoader &DxcSupport,
                                       const MatrixMultiplyCaseData &Case,
                                       UINT WaveSize, UINT ThreadGroupSize,
                                       bool Verbose) {
  const MatrixParams MatrixA = makeThreadGroupArithmeticParams(
      Case.MatrixAType, Case.M, Case.K, MatrixUse::A, ThreadGroupSize);
  const MatrixParams MatrixB = makeThreadGroupArithmeticParams(
      Case.MatrixBType, Case.K, Case.N, MatrixUse::B, ThreadGroupSize);
  const MatrixParams Accumulator =
      makeThreadGroupArithmeticParams(Case.AccumulatorType, Case.M, Case.N,
                                      MatrixUse::Accumulator, ThreadGroupSize);
  const std::optional<std::vector<BYTE>> MatrixABuffer =
      cpu_oracle::encodeLogicalMatrixBuffer(MatrixA, Case.MatrixAValues);
  const std::optional<std::vector<BYTE>> MatrixBBuffer =
      cpu_oracle::encodeLogicalMatrixBuffer(MatrixB, Case.MatrixBValues);
  const std::optional<std::vector<BYTE>> AccumulatorBuffer =
      Case.hasInitialAccumulator() ? cpu_oracle::encodeLogicalMatrixBuffer(
                                         Accumulator, Case.AccumulatorValues)
                                   : std::optional<std::vector<BYTE>>();
  const std::optional<std::vector<int64_t>> ExpectedValues =
      calculateMatrixMultiplyExpected(Case);
  const std::optional<std::vector<BYTE>> Expected =
      ExpectedValues.has_value()
          ? cpu_oracle::encodeLogicalMatrixBuffer(Accumulator, *ExpectedValues)
          : std::optional<std::vector<BYTE>>();
  const std::optional<std::string> Args =
      buildThreadGroupMultiplyCompilerArgs(Case, WaveSize, ThreadGroupSize);
  VERIFY_IS_TRUE(MatrixABuffer.has_value());
  VERIFY_IS_TRUE(MatrixBBuffer.has_value());
  VERIFY_IS_TRUE(!Case.hasInitialAccumulator() ||
                 AccumulatorBuffer.has_value());
  VERIFY_IS_TRUE(Expected.has_value());
  VERIFY_IS_TRUE(Args.has_value());
  if (!MatrixABuffer.has_value() || !MatrixBBuffer.has_value() ||
      (Case.hasInitialAccumulator() && !AccumulatorBuffer.has_value()) ||
      !Expected.has_value() || !Args.has_value())
    return;

  const char *RootSignature = Case.hasInitialAccumulator()
                                  ? "SRV(t0), SRV(t1), SRV(t2), UAV(u3)"
                                  : "SRV(t0), SRV(t1), UAV(u2)";
  compileShader(DxcSupport, ThreadGroupMultiplyShader, "cs_6_10", *Args,
                Verbose);

  auto Op = createComputeOp(ThreadGroupMultiplyShader, "cs_6_10", RootSignature,
                            Args->c_str());
  addSRVBuffer(Op.get(), "MatrixAInput", MatrixABuffer->size(), "byname");
  addSRVBuffer(Op.get(), "MatrixBInput", MatrixBBuffer->size(), "byname");
  if (Case.hasInitialAccumulator())
    addSRVBuffer(Op.get(), "AccumulatorInput", AccumulatorBuffer->size(),
                 "byname");
  addUAVBuffer(Op.get(), "Output", Expected->size(), true);
  addRootView(Op.get(), 0, "MatrixAInput");
  addRootView(Op.get(), 1, "MatrixBInput");
  if (Case.hasInitialAccumulator()) {
    addRootView(Op.get(), 2, "AccumulatorInput");
    addRootView(Op.get(), 3, "Output");
  } else {
    addRootView(Op.get(), 2, "Output");
  }

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *) {
        const std::vector<BYTE> *Source = nullptr;
        if (strcmp(Name, "MatrixAInput") == 0)
          Source = &*MatrixABuffer;
        else if (strcmp(Name, "MatrixBInput") == 0)
          Source = &*MatrixBBuffer;
        else if (Case.hasInitialAccumulator() &&
                 strcmp(Name, "AccumulatorInput") == 0)
          Source = &*AccumulatorBuffer;
        if (!Source)
          return;
        VERIFY_IS_TRUE(
            Data.size() == Source->size(),
            "ThreadGroup matrix arithmetic initializer size mismatch");
        std::copy(Source->begin(), Source->end(), Data.begin());
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  const cpu_oracle::BufferResultOracle Oracle =
      cpu_oracle::exactBufferResult(*Expected, Case.PublicRule);
  VERIFY_IS_TRUE(cpu_oracle::verifyBufferResult(OutData.data(), OutData.size(),
                                                Oracle, Verbose));
}

static void runCapabilityCheckedThreadGroupMultiply(
    ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
    const MatrixMultiplyCaseData &Case,
    linalg_test::CapabilityRequirement Requirement, LPCWSTR CaseName,
    bool Verbose) {
  VERIFY_IS_TRUE(isMatrixMultiplyCaseValid(Case));
  if (!isMatrixMultiplyCaseValid(Case))
    return;

  bool Supported;
  UINT SelectedWaveSize;
  UINT SelectedThreadGroupSize;
  const HRESULT HR = queryThreadGroupMatrixMultiplyCaseSupport(
      Device, Case.MatrixAType, Case.MatrixBType, Case.AccumulatorType, Case.M,
      Case.K, Case.N, CaseName, Supported, SelectedWaveSize,
      SelectedThreadGroupSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(HR, Supported, Requirement);
  if (!applyApplicability(Applicability, CaseName))
    return;
  runThreadGroupMultiplyCase(Device, DxcSupport, Case, SelectedWaveSize,
                             SelectedThreadGroupSize, Verbose);
}

static MatrixMultiplyCaseData
makeRectangularF16ThreadGroupMultiplyCase(ComponentType AccumulatorType,
                                          MatrixMultiplyOperation Operation) {
  MatrixMultiplyCaseData Case = {};
  Case.MatrixAType = ComponentType::F16;
  Case.MatrixBType = ComponentType::F16;
  Case.AccumulatorType = AccumulatorType;
  Case.M = 8;
  Case.K = 16;
  Case.N = 8;
  Case.Operation = Operation;
  Case.MatrixAValues = makeMatrixArithmeticPattern(Case.M, Case.K, 3, 2, 5, 2);
  Case.MatrixBValues = makeMatrixArithmeticPattern(Case.K, Case.N, 1, 3, 7, 3);
  if (Case.hasInitialAccumulator()) {
    Case.AccumulatorValues =
        makeMatrixArithmeticPattern(Case.M, Case.N, 2, 1, 5, 2);
    Case.PublicRule =
        L"Exact ThreadGroup F16 products plus an independent F32 accumulator";
  } else {
    Case.PublicRule =
        L"Exact non-uniform rectangular ThreadGroup F16 matrix product";
  }
  return Case;
}

void DxilConf_SM610_LinAlg::MatMatMul_ThreadGroup_8x16x8_F16_NonUniform() {
  const MatrixMultiplyCaseData Case = makeRectangularF16ThreadGroupMultiplyCase(
      ComponentType::F16, MatrixMultiplyOperation::Multiply);
  runCapabilityCheckedThreadGroupMultiply(
      D3DDevice, DxcSupport, Case,
      linalg_test::CapabilityRequirement::CapabilityGated,
      L"MatMatMul_ThreadGroup_8x16x8_F16_NonUniform", VerboseLogging);
}

void DxilConf_SM610_LinAlg::
    MatMatMulAccum_ThreadGroup_8x16x8_F16_ToF32_NonUniform() {
  const MatrixMultiplyCaseData Case = makeRectangularF16ThreadGroupMultiplyCase(
      ComponentType::F32, MatrixMultiplyOperation::MultiplyAccumulate);
  runCapabilityCheckedThreadGroupMultiply(
      D3DDevice, DxcSupport, Case,
      linalg_test::CapabilityRequirement::CapabilityGated,
      L"MatMatMulAccum_ThreadGroup_8x16x8_F16_ToF32_NonUniform",
      VerboseLogging);
}

void DxilConf_SM610_LinAlg::MatMatMul_ThreadGroup_8x8x8_I32() {
  MatrixMultiplyCaseData Case = {};
  Case.MatrixAType = ComponentType::I32;
  Case.MatrixBType = ComponentType::I32;
  Case.AccumulatorType = ComponentType::I32;
  Case.M = 8;
  Case.K = 8;
  Case.N = 8;
  Case.MatrixAValues = makeMatrixArithmeticPattern(Case.M, Case.K, 3, 2, 5, 2);
  Case.MatrixBValues = makeMatrixArithmeticPattern(Case.K, Case.N, 1, 3, 7, 3);
  Case.PublicRule = L"Exact non-uniform ThreadGroup I32 matrix product";
  runCapabilityCheckedThreadGroupMultiply(
      D3DDevice, DxcSupport, Case,
      linalg_test::CapabilityRequirement::CapabilityGated,
      L"MatMatMul_ThreadGroup_8x8x8_I32", VerboseLogging);
}

struct WaveAccumulateCaseData {
  ComponentType AccumulatorType = ComponentType::Invalid;
  ComponentType RHSType = ComponentType::Invalid;
  MatrixUse RHSUse = MatrixUse::A;
  MatrixDim M = 0;
  MatrixDim N = 0;
  std::vector<int64_t> AccumulatorValues;
  std::vector<int64_t> RHSValues;
  std::wstring PublicRule;
};

static bool isWaveAccumulateCaseValid(const WaveAccumulateCaseData &Case) {
  const size_t Elements = static_cast<size_t>(Case.M) * Case.N;
  return Case.M != 0 && Case.N != 0 &&
         (Case.RHSUse == MatrixUse::A || Case.RHSUse == MatrixUse::B) &&
         Case.AccumulatorValues.size() == Elements &&
         Case.RHSValues.size() == Elements &&
         toCapabilityDataType(Case.AccumulatorType).has_value() &&
         toCapabilityDataType(Case.RHSType).has_value() &&
         !Case.PublicRule.empty();
}

static const char WaveAccumulateShader[] = R"(
  #define USE_ACC 2
  #define SCOPE_WAVE 1
  #define LAYOUT_ROW_MAJOR 0

  ByteAddressBuffer AccumulatorInput : register(t0);
  ByteAddressBuffer RHSInput : register(t1);
  RWByteAddressBuffer Output : register(u2);

  [WaveSize(FORCED_WAVE_SIZE)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        ACCUMULATOR_COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE_WAVE)]]
      Accumulator;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Accumulator, AccumulatorInput, 0, ACCUMULATOR_STRIDE,
      LAYOUT_ROW_MAJOR, 128);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        RHS_COMP_TYPE, M_DIM, N_DIM, RHS_USE, SCOPE_WAVE)]]
      RHS;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      RHS, RHSInput, 0, RHS_STRIDE, LAYOUT_ROW_MAJOR, 128);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        ACCUMULATOR_COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE_WAVE)]]
      Result;
    __builtin_LinAlg_MatrixAccumulate(Result, Accumulator, RHS);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Result, Output, 0, ACCUMULATOR_STRIDE, LAYOUT_ROW_MAJOR, 128);
  }
)";

static std::optional<std::string>
buildWaveAccumulateCompilerArgs(const WaveAccumulateCaseData &Case,
                                UINT WaveSize) {
  if (!isWaveAccumulateCaseValid(Case) || WaveSize == 0)
    return std::nullopt;
  const MatrixParams Accumulator = makeWaveArithmeticParams(
      Case.AccumulatorType, Case.M, Case.N, MatrixUse::Accumulator, WaveSize);
  const MatrixParams RHS = makeWaveArithmeticParams(
      Case.RHSType, Case.M, Case.N, Case.RHSUse, WaveSize);

  std::stringstream SS;
  SS << "-HV 202x";
  SS << " -DACCUMULATOR_COMP_TYPE=" << static_cast<int>(Case.AccumulatorType);
  SS << " -DRHS_COMP_TYPE=" << static_cast<int>(Case.RHSType);
  SS << " -DRHS_USE=" << static_cast<int>(Case.RHSUse);
  SS << " -DM_DIM=" << Case.M;
  SS << " -DN_DIM=" << Case.N;
  SS << " -DACCUMULATOR_STRIDE=" << Accumulator.strideBytes();
  SS << " -DRHS_STRIDE=" << RHS.strideBytes();
  SS << " -DNUMTHREADS=" << WaveSize;
  SS << " -DFORCED_WAVE_SIZE=" << WaveSize;
  if (needs16BitTypes(Case.AccumulatorType) || needs16BitTypes(Case.RHSType))
    SS << " -enable-16bit-types";
  return SS.str();
}

static void runWaveAccumulateCase(ID3D12Device *Device,
                                  dxc::SpecificDllLoader &DxcSupport,
                                  const WaveAccumulateCaseData &Case,
                                  LPCWSTR CaseName, bool Verbose) {
  VERIFY_IS_TRUE(isWaveAccumulateCaseValid(Case));
  if (!isWaveAccumulateCaseValid(Case))
    return;

  MatrixParams CapabilityParams = makeWaveArithmeticParams(
      Case.AccumulatorType, Case.M, Case.N, MatrixUse::Accumulator, 1);
  bool Supported;
  UINT SelectedWaveSize;
  const linalg_test::MatrixRole RHSRole = Case.RHSUse == MatrixUse::A
                                              ? linalg_test::MatrixRole::A
                                              : linalg_test::MatrixRole::B;
  const HRESULT HR =
      queryMatrixConstructionSupport(Device, CapabilityParams,
                                     {{linalg_test::MatrixRole::Accumulator,
                                       Case.M, Case.N, Case.AccumulatorType},
                                      {RHSRole, Case.M, Case.N, Case.RHSType}},
                                     CaseName, Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          HR, Supported, linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability, CaseName))
    return;

  const MatrixParams Accumulator =
      makeWaveArithmeticParams(Case.AccumulatorType, Case.M, Case.N,
                               MatrixUse::Accumulator, SelectedWaveSize);
  const MatrixParams RHS = makeWaveArithmeticParams(
      Case.RHSType, Case.M, Case.N, Case.RHSUse, SelectedWaveSize);
  const std::optional<std::vector<BYTE>> AccumulatorBuffer =
      cpu_oracle::encodeLogicalMatrixBuffer(Accumulator,
                                            Case.AccumulatorValues);
  const std::optional<std::vector<BYTE>> RHSBuffer =
      cpu_oracle::encodeLogicalMatrixBuffer(RHS, Case.RHSValues);
  std::vector<int64_t> ExpectedValues(Case.AccumulatorValues.size());
  bool ExpectedValuesValid = true;
  for (size_t I = 0; I < ExpectedValues.size(); ++I) {
    if (!cpu_oracle::checkedAddInt64(Case.AccumulatorValues[I],
                                     Case.RHSValues[I], ExpectedValues[I])) {
      ExpectedValuesValid = false;
      break;
    }
  }
  const std::optional<std::vector<BYTE>> Expected =
      ExpectedValuesValid
          ? cpu_oracle::encodeLogicalMatrixBuffer(Accumulator, ExpectedValues)
          : std::optional<std::vector<BYTE>>();
  const std::optional<std::string> Args =
      buildWaveAccumulateCompilerArgs(Case, SelectedWaveSize);
  VERIFY_IS_TRUE(AccumulatorBuffer.has_value());
  VERIFY_IS_TRUE(RHSBuffer.has_value());
  VERIFY_IS_TRUE(Expected.has_value());
  VERIFY_IS_TRUE(Args.has_value());
  if (!AccumulatorBuffer.has_value() || !RHSBuffer.has_value() ||
      !Expected.has_value() || !Args.has_value())
    return;

  compileShader(DxcSupport, WaveAccumulateShader, "cs_6_10", *Args, Verbose);
  auto Op = createComputeOp(WaveAccumulateShader, "cs_6_10",
                            "SRV(t0), SRV(t1), UAV(u2)", Args->c_str());
  addSRVBuffer(Op.get(), "AccumulatorInput", AccumulatorBuffer->size(),
               "byname");
  addSRVBuffer(Op.get(), "RHSInput", RHSBuffer->size(), "byname");
  addUAVBuffer(Op.get(), "Output", Expected->size(), true);
  addRootView(Op.get(), 0, "AccumulatorInput");
  addRootView(Op.get(), 1, "RHSInput");
  addRootView(Op.get(), 2, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *) {
        const std::vector<BYTE> *Source = nullptr;
        if (strcmp(Name, "AccumulatorInput") == 0)
          Source = &*AccumulatorBuffer;
        else if (strcmp(Name, "RHSInput") == 0)
          Source = &*RHSBuffer;
        if (!Source)
          return;
        VERIFY_IS_TRUE(Data.size() == Source->size(),
                       "Wave matrix accumulate initializer size mismatch");
        std::copy(Source->begin(), Source->end(), Data.begin());
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  const cpu_oracle::BufferResultOracle Oracle =
      cpu_oracle::exactBufferResult(*Expected, Case.PublicRule);
  VERIFY_IS_TRUE(cpu_oracle::verifyBufferResult(OutData.data(), OutData.size(),
                                                Oracle, Verbose));
}

void DxilConf_SM610_LinAlg::MatAccum_Wave_8x32_F16_BUse_NonUniform() {
  WaveAccumulateCaseData Case = {};
  Case.AccumulatorType = ComponentType::F16;
  Case.RHSType = ComponentType::F16;
  Case.RHSUse = MatrixUse::B;
  Case.M = 8;
  Case.N = 32;
  Case.AccumulatorValues =
      makeMatrixArithmeticPattern(Case.M, Case.N, 2, 1, 7, 3);
  Case.RHSValues = makeMatrixArithmeticPattern(Case.M, Case.N, 1, 3, 5, 2);
  Case.PublicRule =
      L"Exact non-uniform F16 accumulator plus a B-use F16 matrix";
  runWaveAccumulateCase(D3DDevice, DxcSupport, Case,
                        L"MatAccum_Wave_8x32_F16_BUse_NonUniform",
                        VerboseLogging);
}

static const char MatVecMulShader[] = R"(
  #define USE_A 0
  #define SCOPE_THREAD 0

  ByteAddressBuffer MatrixInput : register(t0);
  ByteAddressBuffer VectorInput : register(t1);
  RWByteAddressBuffer Output : register(u2);

  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        MATRIX_COMP_TYPE, M_DIM, N_DIM, USE_A, SCOPE_THREAD)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, MatrixInput, 0, MATRIX_STRIDE, MATRIX_LAYOUT, 128);

    vector<INPUT_STORAGE_TYPE, INPUT_STORAGE_COUNT> InVec;
    for (uint I = 0; I < INPUT_STORAGE_COUNT; ++I) {
      InVec[I] =
        VectorInput.Load<INPUT_STORAGE_TYPE>(I * INPUT_STORAGE_SIZE);
    }

    vector<OUTPUT_TYPE, M_DIM> OutVec;
    __builtin_LinAlg_MatrixVectorMultiply(
      OutVec, Mat, OUTPUT_SIGNED, InVec, INPUT_INTERP);

    for (uint I = 0; I < M_DIM; ++I) {
      Output.Store<OUTPUT_TYPE>(I * OUTPUT_SIZE, OutVec[I]);
    }
  }
)";

static const char MatVecMulAddShader[] = R"(
  #define USE_A 0
  #define SCOPE_THREAD 0

  ByteAddressBuffer MatrixInput : register(t0);
  ByteAddressBuffer VectorInput : register(t1);
  ByteAddressBuffer BiasInput : register(t2);
  RWByteAddressBuffer Output : register(u3);

  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        MATRIX_COMP_TYPE, M_DIM, N_DIM, USE_A, SCOPE_THREAD)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Mat, MatrixInput, 0, MATRIX_STRIDE, MATRIX_LAYOUT, 128);

    vector<INPUT_STORAGE_TYPE, INPUT_STORAGE_COUNT> InVec;
    for (uint I = 0; I < INPUT_STORAGE_COUNT; ++I) {
      InVec[I] =
        VectorInput.Load<INPUT_STORAGE_TYPE>(I * INPUT_STORAGE_SIZE);
    }

    vector<BIAS_STORAGE_TYPE, BIAS_STORAGE_COUNT> BiasVec;
    for (uint I = 0; I < BIAS_STORAGE_COUNT; ++I) {
      BiasVec[I] = BiasInput.Load<BIAS_STORAGE_TYPE>(I * BIAS_STORAGE_SIZE);
    }

    vector<OUTPUT_TYPE, M_DIM> OutVec;
    __builtin_LinAlg_MatrixVectorMultiplyAdd(
      OutVec, Mat, OUTPUT_SIGNED, InVec, INPUT_INTERP, BiasVec, BIAS_INTERP);

    for (uint I = 0; I < M_DIM; ++I) {
      Output.Store<OUTPUT_TYPE>(I * OUTPUT_SIZE, OutVec[I]);
    }
  }
)";

struct MatVecCaseData {
  MatrixParams Matrix = {};
  ComponentType VectorInputType = ComponentType::Invalid;
  ComponentType InputInterpretation = ComponentType::Invalid;
  ComponentType BiasInputType = ComponentType::Invalid;
  ComponentType ResultType = ComponentType::Invalid;
  bool OutputSigned = true;
  std::vector<int64_t> MatrixValues;
  std::vector<int64_t> VectorValues;
  std::vector<int64_t> BiasValues;
  std::wstring PublicRule;

  bool hasBias() const { return BiasInputType != ComponentType::Invalid; }
};

static bool isMatVecCaseValid(const MatVecCaseData &Case) {
  const size_t MatrixElementCount =
      static_cast<size_t>(Case.Matrix.M) * Case.Matrix.N;
  if (Case.Matrix.M == 0 || Case.Matrix.N == 0 ||
      Case.Matrix.Use != MatrixUse::A ||
      Case.Matrix.Scope != MatrixScope::Thread ||
      (Case.Matrix.Layout != LinalgMatrixLayout::RowMajor &&
       Case.Matrix.Layout != LinalgMatrixLayout::ColumnMajor) ||
      Case.Matrix.NumThreads != 1 ||
      Case.MatrixValues.size() != MatrixElementCount ||
      Case.VectorValues.size() != Case.Matrix.N || Case.PublicRule.empty())
    return false;

  if (Case.hasBias() != !Case.BiasValues.empty() ||
      (Case.hasBias() && (Case.BiasValues.size() != Case.Matrix.M ||
                          Case.BiasInputType != Case.ResultType)))
    return false;

  if (cpu_oracle::isPackedVectorComponent(Case.VectorInputType) &&
      (Case.VectorInputType != Case.Matrix.CompType ||
       Case.InputInterpretation != Case.VectorInputType))
    return false;
  if (!cpu_oracle::isPackedVectorComponent(Case.VectorInputType) &&
      Case.InputInterpretation != Case.Matrix.CompType)
    return false;

  const char *ResultTypeName = cpu_oracle::hlslElementTypeName(Case.ResultType);
  const bool ExpectedOutputSigned = Case.ResultType != ComponentType::U32;
  return Case.OutputSigned == ExpectedOutputSigned &&
         cpu_oracle::hlslVectorStorageTypeName(Case.VectorInputType) !=
             nullptr &&
         ResultTypeName != nullptr &&
         (!Case.hasBias() ||
          cpu_oracle::hlslVectorStorageTypeName(Case.BiasInputType) != nullptr);
}

static std::vector<int64_t>
calculateMatVecExpected(const MatVecCaseData &Case) {
  std::vector<int64_t> Expected(Case.Matrix.M, 0);
  for (MatrixDim Row = 0; Row < Case.Matrix.M; ++Row) {
    for (MatrixDim Column = 0; Column < Case.Matrix.N; ++Column) {
      Expected[Row] +=
          Case.MatrixValues[static_cast<size_t>(Row) * Case.Matrix.N + Column] *
          Case.VectorValues[Column];
    }
    if (Case.hasBias())
      Expected[Row] += Case.BiasValues[Row];
  }
  return Expected;
}

static std::optional<std::string>
buildMatVecCompilerArgs(const MatVecCaseData &Case) {
  if (!isMatVecCaseValid(Case))
    return std::nullopt;

  const char *InputStorageType =
      cpu_oracle::hlslVectorStorageTypeName(Case.VectorInputType);
  const char *OutputType = cpu_oracle::hlslElementTypeName(Case.ResultType);
  const char *BiasStorageType =
      Case.hasBias() ? cpu_oracle::hlslVectorStorageTypeName(Case.BiasInputType)
                     : nullptr;
  if (!InputStorageType || !OutputType || (Case.hasBias() && !BiasStorageType))
    return std::nullopt;

  std::stringstream SS;
  SS << "-HV 202x";
  SS << " -DMATRIX_COMP_TYPE=" << static_cast<int>(Case.Matrix.CompType);
  SS << " -DM_DIM=" << Case.Matrix.M;
  SS << " -DN_DIM=" << Case.Matrix.N;
  SS << " -DMATRIX_STRIDE=" << Case.Matrix.strideBytes();
  SS << " -DMATRIX_LAYOUT=" << static_cast<int>(Case.Matrix.Layout);
  SS << " -DNUMTHREADS=" << Case.Matrix.NumThreads;
  SS << " -DINPUT_STORAGE_TYPE=" << InputStorageType;
  SS << " -DINPUT_STORAGE_COUNT="
     << cpu_oracle::vectorStorageCount(Case.VectorInputType, Case.Matrix.N);
  SS << " -DINPUT_STORAGE_SIZE="
     << cpu_oracle::vectorStorageElementSize(Case.VectorInputType);
  // Native vectors carry their source type in DXIL; the immediate identifies
  // the matrix-format conversion target. Packed vectors use the same type for
  // storage interpretation and matrix data.
  SS << " -DINPUT_INTERP=" << static_cast<int>(Case.InputInterpretation);
  SS << " -DOUTPUT_TYPE=" << OutputType;
  SS << " -DOUTPUT_SIZE="
     << static_cast<unsigned>(elementSize(Case.ResultType));
  SS << " -DOUTPUT_SIGNED=" << Case.OutputSigned;
  if (Case.hasBias()) {
    SS << " -DBIAS_STORAGE_TYPE=" << BiasStorageType;
    SS << " -DBIAS_STORAGE_COUNT="
       << cpu_oracle::vectorStorageCount(Case.BiasInputType, Case.Matrix.M);
    SS << " -DBIAS_STORAGE_SIZE="
       << cpu_oracle::vectorStorageElementSize(Case.BiasInputType);
    SS << " -DBIAS_INTERP=" << static_cast<int>(Case.BiasInputType);
  }

  if (needs16BitTypes(Case.Matrix.CompType) ||
      needs16BitTypes(Case.VectorInputType) ||
      needs16BitTypes(Case.BiasInputType) || needs16BitTypes(Case.ResultType))
    SS << " -enable-16bit-types";
  return SS.str();
}

static void runMatVecCase(ID3D12Device *Device,
                          dxc::SpecificDllLoader &DxcSupport,
                          const MatVecCaseData &Case, bool Verbose) {
  const std::optional<std::vector<BYTE>> MatrixBuffer =
      cpu_oracle::encodeLogicalMatrixBuffer(Case.Matrix, Case.MatrixValues);
  const std::optional<std::vector<BYTE>> VectorBuffer =
      cpu_oracle::encodeExactComponents(Case.VectorInputType,
                                        Case.VectorValues);
  const std::optional<std::vector<BYTE>> BiasBuffer =
      Case.hasBias() ? cpu_oracle::encodeExactComponents(Case.BiasInputType,
                                                         Case.BiasValues)
                     : std::optional<std::vector<BYTE>>();
  const std::optional<std::vector<BYTE>> Expected =
      cpu_oracle::encodeExactComponents(Case.ResultType,
                                        calculateMatVecExpected(Case));
  const std::optional<std::string> Args = buildMatVecCompilerArgs(Case);
  VERIFY_IS_TRUE(MatrixBuffer.has_value());
  VERIFY_IS_TRUE(VectorBuffer.has_value());
  VERIFY_IS_TRUE(!Case.hasBias() || BiasBuffer.has_value());
  VERIFY_IS_TRUE(Expected.has_value());
  VERIFY_IS_TRUE(Args.has_value());
  if (!MatrixBuffer.has_value() || !VectorBuffer.has_value() ||
      (Case.hasBias() && !BiasBuffer.has_value()) || !Expected.has_value() ||
      !Args.has_value())
    return;

  const char *Shader = Case.hasBias() ? MatVecMulAddShader : MatVecMulShader;
  const char *RootSignature = Case.hasBias()
                                  ? "SRV(t0), SRV(t1), SRV(t2), UAV(u3)"
                                  : "SRV(t0), SRV(t1), UAV(u2)";
  compileShader(DxcSupport, Shader, "cs_6_10", *Args, Verbose);

  auto Op = createComputeOp(Shader, "cs_6_10", RootSignature, Args->c_str());
  addSRVBuffer(Op.get(), "MatrixInput", MatrixBuffer->size(), "byname");
  addSRVBuffer(Op.get(), "VectorInput", VectorBuffer->size(), "byname");
  if (Case.hasBias())
    addSRVBuffer(Op.get(), "BiasInput", BiasBuffer->size(), "byname");
  addUAVBuffer(Op.get(), "Output", Expected->size(), true);
  addRootView(Op.get(), 0, "MatrixInput");
  addRootView(Op.get(), 1, "VectorInput");
  if (Case.hasBias()) {
    addRootView(Op.get(), 2, "BiasInput");
    addRootView(Op.get(), 3, "Output");
  } else {
    addRootView(Op.get(), 2, "Output");
  }

  auto Result =
      runShaderOp(Device, DxcSupport, std::move(Op),
                  [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *) {
                    const std::vector<BYTE> *Source = nullptr;
                    if (strcmp(Name, "MatrixInput") == 0)
                      Source = &*MatrixBuffer;
                    else if (strcmp(Name, "VectorInput") == 0)
                      Source = &*VectorBuffer;
                    else if (Case.hasBias() && strcmp(Name, "BiasInput") == 0)
                      Source = &*BiasBuffer;
                    if (!Source)
                      return;
                    VERIFY_IS_TRUE(Data.size() == Source->size(),
                                   "MatVec resource initializer size mismatch");
                    std::copy(Source->begin(), Source->end(), Data.begin());
                  });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  if (Verbose) {
    const BYTE *Bytes = static_cast<const BYTE *>(OutData.data());
    for (size_t I = 0; I < OutData.size(); ++I)
      hlsl_test::LogCommentFmt(L"  MatVec output[%zu]=0x%02x", I, Bytes[I]);
  }
  cpu_oracle::BufferResultOracle Oracle =
      cpu_oracle::exactBufferResult(std::move(*Expected), Case.PublicRule);
  VERIFY_IS_TRUE(cpu_oracle::verifyBufferResult(OutData.data(), OutData.size(),
                                                Oracle, Verbose));
}

static void runCapabilityCheckedMatVec(
    ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
    const MatVecCaseData &Case, linalg_test::CapabilityRequirement Requirement,
    LPCWSTR CaseName, bool Verbose) {
  bool Supported;
  const HRESULT HR = queryThreadVectorMatrixMultiplySupport(
      Device, Case.Matrix, Case.VectorInputType, Case.BiasInputType,
      Case.ResultType, CaseName, Supported);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(HR, Supported, Requirement);
  if (!applyApplicability(Applicability, CaseName))
    return;
  runMatVecCase(Device, DxcSupport, Case, Verbose);
}

static MatrixParams makeThreadMatVecParams(ComponentType MatrixType,
                                           MatrixDim M, MatrixDim N,
                                           LinalgMatrixLayout Layout) {
  MatrixParams Params = {};
  Params.CompType = MatrixType;
  Params.M = M;
  Params.N = N;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Thread;
  Params.Layout = Layout;
  Params.NumThreads = 1;
  Params.Enable16Bit = needs16BitTypes(MatrixType);
  return Params;
}

static MatVecCaseData
makeUniformMatVecCase(const MatrixParams &Params, int FillValue,
                      bool OutputSigned, ComponentType InputInterp,
                      ComponentType BiasInterp = ComponentType::Invalid) {
  MatVecCaseData Case = {};
  Case.Matrix = Params;
  Case.Matrix.Use = MatrixUse::A;
  Case.VectorInputType = Params.CompType;
  Case.InputInterpretation = InputInterp;
  Case.BiasInputType = BiasInterp;
  Case.ResultType = Params.CompType;
  Case.OutputSigned = OutputSigned;
  Case.MatrixValues.assign(static_cast<size_t>(Params.M) * Params.N, FillValue);
  Case.VectorValues.assign(Params.N, FillValue);
  if (Case.hasBias())
    Case.BiasValues.assign(Params.M, FillValue);
  Case.PublicRule =
      Case.hasBias()
          ? L"Exact uniform matrix-vector dot products plus independent bias"
          : L"Exact uniform matrix-vector dot products";
  return Case;
}

static MatVecCaseData makeNonUniformF16MatVecCase(LinalgMatrixLayout Layout) {
  MatVecCaseData Case = {};
  Case.Matrix = makeThreadMatVecParams(ComponentType::F16, 4, 8, Layout);
  Case.VectorInputType = ComponentType::F16;
  Case.InputInterpretation = ComponentType::F16;
  Case.ResultType = ComponentType::F16;
  Case.OutputSigned = true;
  Case.MatrixValues = {
      1,  0, -1, 2, -2, 3, -3, 1,  0, 1,  2, -1, 3, -2, 1,  -3,
      -1, 2, 0,  1, -2, 1, 3,  -1, 2, -1, 1, 0,  1, -3, -2, 3,
  };
  Case.VectorValues = {1, -2, 3, -1, 2, -3, 1, 2};
  Case.PublicRule =
      Layout == LinalgMatrixLayout::RowMajor
          ? L"Exact non-uniform F16 RowMajor matrix-vector dot products"
          : L"Exact non-uniform F16 ColumnMajor matrix-vector dot products";
  return Case;
}

static MatVecCaseData makeSInt8MatVecCase(ComponentType VectorInputType) {
  MatVecCaseData Case = {};
  Case.Matrix = makeThreadMatVecParams(ComponentType::I8, 4, 8,
                                       LinalgMatrixLayout::RowMajor);
  Case.VectorInputType = VectorInputType;
  Case.InputInterpretation = ComponentType::I8;
  Case.ResultType = ComponentType::I32;
  Case.OutputSigned = true;
  Case.MatrixValues = {
      1, -2, 3, -4, 5, -6, 7, -8, -1, 2,  -3, 4,  -5, 6,  -7, 8,
      1, 1,  1, 1,  1, 1,  1, 1,  -8, -7, -6, -5, -4, -3, -2, -1,
  };
  Case.VectorValues = {1, -1, 2, -2, 3, -3, 4, -4};
  Case.PublicRule =
      VectorInputType == ComponentType::I8
          ? L"Exact packed SInt8 vector times SInt8 matrix dot products"
          : L"Exact native F32 vector conversion for SInt8 matrix dot products";
  return Case;
}

static MatVecCaseData makeUInt8MatVecCase() {
  MatVecCaseData Case = {};
  Case.Matrix = makeThreadMatVecParams(ComponentType::U8, 4, 8,
                                       LinalgMatrixLayout::RowMajor);
  Case.VectorInputType = ComponentType::U8;
  Case.InputInterpretation = ComponentType::U8;
  Case.ResultType = ComponentType::I32;
  Case.OutputSigned = true;
  Case.MatrixValues = {
      255, 1, 2,   3, 4,   5, 6,   7, 128, 127, 1, 1,   1, 1,   1, 1,
      200, 0, 200, 0, 200, 0, 200, 0, 0,   200, 0, 200, 0, 200, 0, 200,
  };
  Case.VectorValues = {1, 2, 3, 4, 5, 6, 7, 8};
  Case.PublicRule =
      L"Exact packed UInt8 vector times UInt8 matrix dot products";
  return Case;
}

static MatVecCaseData makeUInt32MatVecCase() {
  MatVecCaseData Case = {};
  Case.Matrix = makeThreadMatVecParams(ComponentType::U32, 4, 8,
                                       LinalgMatrixLayout::RowMajor);
  Case.VectorInputType = ComponentType::U32;
  Case.InputInterpretation = ComponentType::U32;
  Case.ResultType = ComponentType::U32;
  Case.OutputSigned = false;
  Case.MatrixValues = {
      2147483648LL, 0, 0,   0, 0,   0, 0,   0, 1, 2,   3, 4,   5, 6,   7, 8,
      100,          0, 100, 0, 100, 0, 100, 0, 0, 200, 0, 200, 0, 200, 0, 200,
  };
  Case.VectorValues = {1, 1, 1, 1, 1, 1, 1, 1};
  Case.PublicRule =
      L"Exact native UInt32 matrix-vector results with unsigned output";
  return Case;
}

static void runMatVecMul(ID3D12Device *Device,
                         dxc::SpecificDllLoader &DxcSupport,
                         const MatrixParams &Params, bool Verbose,
                         int FillValue, bool OutputSigned,
                         ComponentType InputInterp) {
  runMatVecCase(
      Device, DxcSupport,
      makeUniformMatVecCase(Params, FillValue, OutputSigned, InputInterp),
      Verbose);
}

static void runMatVecMulAdd(ID3D12Device *Device,
                            dxc::SpecificDllLoader &DxcSupport,
                            const MatrixParams &Params, bool Verbose,
                            int FillValue, bool OutputSigned,
                            ComponentType InputInterp,
                            ComponentType BiasInterp) {
  runMatVecCase(Device, DxcSupport,
                makeUniformMatVecCase(Params, FillValue, OutputSigned,
                                      InputInterp, BiasInterp),
                Verbose);
}

void DxilConf_SM610_LinAlg::MatVecMul_Thread_16x16_F16() {
  MatrixParams Params = makeThreadMatVecParams(ComponentType::F16, 16, 16,
                                               LinalgMatrixLayout::RowMajor);
  runMatVecMul(D3DDevice, DxcSupport, Params, VerboseLogging,
               /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F16);
}

void DxilConf_SM610_LinAlg::MatVecMul_Thread_4x8_F32() {
  MatrixParams Params = makeThreadMatVecParams(ComponentType::F32, 4, 8,
                                               LinalgMatrixLayout::RowMajor);
  runMatVecMul(D3DDevice, DxcSupport, Params, VerboseLogging,
               /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F32);
}

void DxilConf_SM610_LinAlg::MatVecMul_Thread_4x8_F16_NonUniform() {
  MatVecCaseData Case =
      makeNonUniformF16MatVecCase(LinalgMatrixLayout::RowMajor);
  runCapabilityCheckedMatVec(D3DDevice, DxcSupport, Case,
                             linalg_test::CapabilityRequirement::Mandatory,
                             L"MatVecMul_Thread_4x8_F16_NonUniform",
                             VerboseLogging);
}

void DxilConf_SM610_LinAlg::MatVecMul_Thread_4x8_F16_ColumnMajor() {
  MatVecCaseData Case =
      makeNonUniformF16MatVecCase(LinalgMatrixLayout::ColumnMajor);
  runCapabilityCheckedMatVec(
      D3DDevice, DxcSupport, Case,
      linalg_test::CapabilityRequirement::CapabilityGated,
      L"MatVecMul_Thread_4x8_F16_ColumnMajor", VerboseLogging);
}

void DxilConf_SM610_LinAlg::MatVecMul_Thread_4x8_I8_Interpreted() {
  MatVecCaseData Case = makeSInt8MatVecCase(ComponentType::I8);
  runCapabilityCheckedMatVec(D3DDevice, DxcSupport, Case,
                             linalg_test::CapabilityRequirement::Mandatory,
                             L"MatVecMul_Thread_4x8_I8_Interpreted",
                             VerboseLogging);
}

void DxilConf_SM610_LinAlg::MatVecMul_Thread_4x8_U8_Interpreted() {
  MatVecCaseData Case = makeUInt8MatVecCase();
  runCapabilityCheckedMatVec(D3DDevice, DxcSupport, Case,
                             linalg_test::CapabilityRequirement::Mandatory,
                             L"MatVecMul_Thread_4x8_U8_Interpreted",
                             VerboseLogging);
}

void DxilConf_SM610_LinAlg::MatVecMul_Thread_4x8_F32_ToI8() {
  MatVecCaseData Case = makeSInt8MatVecCase(ComponentType::F32);
  runCapabilityCheckedMatVec(D3DDevice, DxcSupport, Case,
                             linalg_test::CapabilityRequirement::Mandatory,
                             L"MatVecMul_Thread_4x8_F32_ToI8", VerboseLogging);
}

void DxilConf_SM610_LinAlg::MatVecMul_Thread_4x8_U32_UnsignedOutput() {
  MatVecCaseData Case = makeUInt32MatVecCase();
  runCapabilityCheckedMatVec(
      D3DDevice, DxcSupport, Case,
      linalg_test::CapabilityRequirement::CapabilityGated,
      L"MatVecMul_Thread_4x8_U32_UnsignedOutput", VerboseLogging);
}

void DxilConf_SM610_LinAlg::MatVecMulAdd_Thread_16x16_F16() {
  MatrixParams Params = makeThreadMatVecParams(ComponentType::F16, 16, 16,
                                               LinalgMatrixLayout::RowMajor);
  runMatVecMulAdd(D3DDevice, DxcSupport, Params, VerboseLogging,
                  /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F16,
                  ComponentType::F16);
}

void DxilConf_SM610_LinAlg::MatVecMulAdd_Thread_4x8_F32() {
  MatrixParams Params = makeThreadMatVecParams(ComponentType::F32, 4, 8,
                                               LinalgMatrixLayout::RowMajor);
  runMatVecMulAdd(D3DDevice, DxcSupport, Params, VerboseLogging,
                  /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F32,
                  ComponentType::F32);
}

void DxilConf_SM610_LinAlg::MatVecMulAdd_Thread_4x8_F16_IndependentBias() {
  MatVecCaseData Case =
      makeNonUniformF16MatVecCase(LinalgMatrixLayout::RowMajor);
  Case.BiasInputType = ComponentType::F16;
  Case.BiasValues = {-5, 7, 3, -9};
  Case.PublicRule =
      L"Exact non-uniform F16 matrix-vector dot products plus independent bias";
  runCapabilityCheckedMatVec(D3DDevice, DxcSupport, Case,
                             linalg_test::CapabilityRequirement::Mandatory,
                             L"MatVecMulAdd_Thread_4x8_F16_IndependentBias",
                             VerboseLogging);
}

struct OuterProductCaseData {
  ComponentType InputType = ComponentType::Invalid;
  ComponentType ResultType = ComponentType::Invalid;
  MatrixDim M = 0;
  MatrixDim N = 0;
  std::vector<int64_t> VectorAValues;
  std::vector<int64_t> VectorBValues;
  std::wstring PublicRule;
};

static bool isOuterProductCaseValid(const OuterProductCaseData &Case) {
  return Case.M != 0 && Case.N != 0 && Case.VectorAValues.size() == Case.M &&
         Case.VectorBValues.size() == Case.N &&
         toCapabilityDataType(Case.InputType).has_value() &&
         toCapabilityDataType(Case.ResultType).has_value() &&
         cpu_oracle::hlslElementTypeName(Case.InputType) &&
         cpu_oracle::hlslElementTypeName(Case.ResultType) &&
         !Case.PublicRule.empty();
}

static std::optional<std::vector<int64_t>>
calculateOuterProductExpected(const OuterProductCaseData &Case) {
  if (!isOuterProductCaseValid(Case))
    return std::nullopt;

  std::vector<int64_t> Expected(static_cast<size_t>(Case.M) * Case.N);
  for (MatrixDim Row = 0; Row < Case.M; ++Row) {
    for (MatrixDim Column = 0; Column < Case.N; ++Column) {
      const size_t Index = static_cast<size_t>(Row) * Case.N + Column;
      if (!cpu_oracle::checkedMultiplyInt64(Case.VectorAValues[Row],
                                            Case.VectorBValues[Column],
                                            Expected[Index]))
        return std::nullopt;
    }
  }
  return Expected;
}

static const char OuterProductShader[] = R"(
  #define USE_ACC 2
  #define SCOPE_THREAD 0

  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);

  [numthreads(1, 1, 1)]
  void main() {
    vector<INPUT_ELEM_TYPE, M_DIM> VecA;
    for (uint I = 0; I < M_DIM; ++I) {
      VecA[I] = Input.Load<INPUT_ELEM_TYPE>(I * INPUT_ELEM_SIZE);
    }

    vector<INPUT_ELEM_TYPE, N_DIM> VecB;
    uint VecBOffset = M_DIM * INPUT_ELEM_SIZE;
    for (uint I = 0; I < N_DIM; ++I) {
      VecB[I] =
        Input.Load<INPUT_ELEM_TYPE>(VecBOffset + I * INPUT_ELEM_SIZE);
    }

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        RESULT_COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE_THREAD)]]
      Mat;
    __builtin_LinAlg_MatrixOuterProduct(Mat, VecA, VecB);

    __builtin_LinAlg_MatrixAccumulateToDescriptor(
      Mat, Output, 0, 0, OUTPUT_LAYOUT, 0);
  }
)";

static std::optional<std::string>
buildOuterProductCompilerArgs(const OuterProductCaseData &Case) {
  if (!isOuterProductCaseValid(Case))
    return std::nullopt;

  std::stringstream SS;
  SS << "-HV 202x";
  SS << " -DINPUT_ELEM_TYPE="
     << cpu_oracle::hlslElementTypeName(Case.InputType);
  SS << " -DRESULT_COMP_TYPE=" << static_cast<int>(Case.ResultType);
  SS << " -DINPUT_ELEM_SIZE=" << static_cast<int>(elementSize(Case.InputType));
  SS << " -DOUTPUT_LAYOUT="
     << static_cast<int>(LinalgMatrixLayout::OuterProductOptimal);
  SS << " -DM_DIM=" << Case.M;
  SS << " -DN_DIM=" << Case.N;
  if (needs16BitTypes(Case.InputType) || needs16BitTypes(Case.ResultType))
    SS << " -enable-16bit-types";
  return SS.str();
}

static void runOuterProductCase(ID3D12Device *Device,
                                dxc::SpecificDllLoader &DxcSupport,
                                const OuterProductCaseData &Case,
                                LPCWSTR CaseName, bool Verbose) {
  VERIFY_IS_TRUE(isOuterProductCaseValid(Case));
  if (!isOuterProductCaseValid(Case))
    return;

  bool Supported;
  const HRESULT HR = queryThreadOuterProductSupport(
      Device, Case.InputType, Case.ResultType, CaseName, Supported);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          HR, Supported, linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability, CaseName))
    return;

  const std::optional<std::vector<BYTE>> VectorA =
      cpu_oracle::encodeExactComponents(Case.InputType, Case.VectorAValues);
  const std::optional<std::vector<BYTE>> VectorB =
      cpu_oracle::encodeExactComponents(Case.InputType, Case.VectorBValues);
  const std::optional<std::vector<int64_t>> ExpectedValues =
      calculateOuterProductExpected(Case);
  const std::optional<std::vector<BYTE>> Expected =
      ExpectedValues.has_value()
          ? cpu_oracle::encodeExactComponents(Case.ResultType, *ExpectedValues)
          : std::optional<std::vector<BYTE>>();
  const std::optional<std::string> Args = buildOuterProductCompilerArgs(Case);
  const std::optional<linalg_test::DataType> DataType =
      toCapabilityDataType(Case.ResultType);
  VERIFY_IS_TRUE(VectorA.has_value());
  VERIFY_IS_TRUE(VectorB.has_value());
  VERIFY_IS_TRUE(Expected.has_value());
  VERIFY_IS_TRUE(Args.has_value());
  VERIFY_IS_TRUE(DataType.has_value());
  if (!VectorA.has_value() || !VectorB.has_value() || !Expected.has_value() ||
      !Args.has_value() || !DataType.has_value())
    return;

  std::vector<BYTE> Input;
  Input.reserve(VectorA->size() + VectorB->size());
  Input.insert(Input.end(), VectorA->begin(), VectorA->end());
  Input.insert(Input.end(), VectorB->begin(), VectorB->end());

  const UINT OptimalBufferSize = getLinAlgMatrixByteSize(
      Device, Case.M, Case.N, *DataType,
      linalg_test::MatrixLayout::OuterProductOptimal, /*Stride=*/0);
  const UINT RowMajorStride =
      static_cast<UINT>(Case.N * elementSize(Case.ResultType));
  const UINT RowMajorBufferSize = getLinAlgMatrixByteSize(
      Device, Case.M, Case.N, *DataType, linalg_test::MatrixLayout::RowMajor,
      RowMajorStride);
  VERIFY_IS_TRUE(OptimalBufferSize != 0);
  VERIFY_IS_TRUE(RowMajorBufferSize == Expected->size());
  if (OptimalBufferSize == 0 || RowMajorBufferSize != Expected->size())
    return;
  const std::vector<BYTE> InitialOptimal(OptimalBufferSize, 0);

  compileShader(DxcSupport, OuterProductShader, "cs_6_10", *Args, Verbose);
  auto Op = createComputeOp(OuterProductShader, "cs_6_10", "UAV(u0), UAV(u1)",
                            Args->c_str());
  addUAVBuffer(Op.get(), "Input", Input.size(), false, "byname");
  addUAVBuffer(Op.get(), "Output", OptimalBufferSize, false, "byname");
  addUAVBuffer(Op.get(), "OutputRowMajor", RowMajorBufferSize, true);
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *) {
        const std::vector<BYTE> *Source = nullptr;
        if (strcmp(Name, "Input") == 0)
          Source = &Input;
        else if (strcmp(Name, "Output") == 0)
          Source = &InitialOptimal;
        if (!Source)
          return;
        VERIFY_IS_TRUE(Data.size() == Source->size(),
                       "OuterProduct initializer size mismatch");
        std::copy(Source->begin(), Source->end(), Data.begin());
      },
      [OptimalBufferSize, RowMajorBufferSize, RowMajorStride, DataType,
       Case](ID3D12GraphicsCommandList *List, st::ShaderOpTest *Test) {
        ID3D12Resource *OptimalBuffer = nullptr;
        ID3D12Resource *RowMajorBuffer = nullptr;
        Test->GetResource("Output", &OptimalBuffer);
        Test->GetResource("OutputRowMajor", &RowMajorBuffer);
        recordLinAlgMatrixConversion(
            List, OptimalBuffer, OptimalBufferSize, RowMajorBuffer,
            RowMajorBufferSize, Case.M, Case.N, *DataType,
            linalg_test::MatrixLayout::OuterProductOptimal, /*SrcStride=*/0,
            linalg_test::MatrixLayout::RowMajor, RowMajorStride);
      });

  MappedData OutData;
  Result->Test->GetReadBackData("OutputRowMajor", &OutData);
  const cpu_oracle::BufferResultOracle Oracle =
      cpu_oracle::exactBufferResult(*Expected, Case.PublicRule);
  VERIFY_IS_TRUE(cpu_oracle::verifyBufferResult(OutData.data(), OutData.size(),
                                                Oracle, Verbose));
}

void DxilConf_SM610_LinAlg::OuterProduct_Thread_16x16_F16() {
  OuterProductCaseData Case = {};
  Case.InputType = ComponentType::F16;
  Case.ResultType = ComponentType::F16;
  Case.M = 16;
  Case.N = 16;
  Case.VectorAValues.assign(Case.M, 2);
  Case.VectorBValues.assign(Case.N, 2);
  Case.PublicRule = L"Exact uniform F16 Thread OuterProduct";
  runOuterProductCase(D3DDevice, DxcSupport, Case,
                      L"OuterProduct_Thread_16x16_F16", VerboseLogging);
}

void DxilConf_SM610_LinAlg::OuterProduct_Thread_8x16_F16_NonUniform() {
  OuterProductCaseData Case = {};
  Case.InputType = ComponentType::F16;
  Case.ResultType = ComponentType::F16;
  Case.M = 8;
  Case.N = 16;
  Case.VectorAValues = makeMatrixArithmeticPattern(/*M=*/1, Case.M, 0, 2, 7, 3);
  Case.VectorBValues = makeMatrixArithmeticPattern(/*M=*/1, Case.N, 0, 3, 5, 2);
  Case.PublicRule = L"Exact non-uniform F16 Thread OuterProduct";
  runOuterProductCase(D3DDevice, DxcSupport, Case,
                      L"OuterProduct_Thread_8x16_F16_NonUniform",
                      VerboseLogging);
}

void DxilConf_SM610_LinAlg::OuterProduct_Thread_4x8_F32_NonUniform() {
  OuterProductCaseData Case = {};
  Case.InputType = ComponentType::F32;
  Case.ResultType = ComponentType::F32;
  Case.M = 4;
  Case.N = 8;
  Case.VectorAValues = makeMatrixArithmeticPattern(/*M=*/1, Case.M, 0, 3, 7, 2);
  Case.VectorBValues = makeMatrixArithmeticPattern(/*M=*/1, Case.N, 0, 2, 5, 2);
  Case.PublicRule = L"Exact non-uniform F32 Thread OuterProduct";
  runOuterProductCase(D3DDevice, DxcSupport, Case,
                      L"OuterProduct_Thread_4x8_F32_NonUniform",
                      VerboseLogging);
}

static const char QueryAccumLayoutShader[] = R"(
  #define USE_A 0
  #define USE_B 1
  #define USE_ACC 2
  #define SCOPE_WAVE 1
  #define LAYOUT_ROW_MAJOR 0

  RWByteAddressBuffer Output : register(u0);

  [WaveSize(FORCED_WAVE_SIZE)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    uint Layout = __builtin_LinAlg_MatrixQueryAccumulatorLayout();

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE_WAVE)]]
      Accumulator;
    __builtin_LinAlg_FillMatrix(Accumulator, 2.0);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        COMP_TYPE, M_DIM, N_DIM, USE_A, SCOPE_WAVE)]]
      MatrixA;
    __builtin_LinAlg_FillMatrix(MatrixA, 3.0);

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(
        COMP_TYPE, M_DIM, N_DIM, USE_B, SCOPE_WAVE)]]
      MatrixB;
    __builtin_LinAlg_FillMatrix(MatrixB, 7.0);

    if (Layout == USE_A) {
      __builtin_LinAlgMatrix
        [[__LinAlgMatrix_Attributes(
          COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE_WAVE)]]
        Result;
      __builtin_LinAlg_MatrixAccumulate(Result, Accumulator, MatrixA);
      __builtin_LinAlg_MatrixStoreToDescriptor(
        Result, Output, 0, STRIDE, LAYOUT_ROW_MAJOR, 128);
    } else {
      __builtin_LinAlgMatrix
        [[__LinAlgMatrix_Attributes(
          COMP_TYPE, M_DIM, N_DIM, USE_ACC, SCOPE_WAVE)]]
        Result;
      __builtin_LinAlg_MatrixAccumulate(Result, Accumulator, MatrixB);
      __builtin_LinAlg_MatrixStoreToDescriptor(
        Result, Output, 0, STRIDE, LAYOUT_ROW_MAJOR, 128);
    }
    if (WaveGetLaneIndex() == 0)
      Output.Store<uint>(LAYOUT_OFFSET, Layout);
  }
)";

static void runQueryAccumLayout(ID3D12Device *Device,
                                dxc::SpecificDllLoader &DxcSupport,
                                bool Verbose) {
  MatrixParams Params = makeWaveArithmeticParams(ComponentType::F16, 4, 8,
                                                 MatrixUse::Accumulator, 1);
  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT HR = queryMatrixConstructionSupport(
      Device, Params,
      {{linalg_test::MatrixRole::Accumulator, Params.M, Params.N,
        ComponentType::F16},
       {linalg_test::MatrixRole::A, Params.M, Params.N, ComponentType::F16},
       {linalg_test::MatrixRole::B, Params.M, Params.N, ComponentType::F16}},
      L"QueryAccumLayout", Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          HR, Supported, linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability, L"QueryAccumLayout"))
    return;

  Params.NumThreads = static_cast<int>(SelectedWaveSize);
  const size_t MatrixBytes = Params.totalBytes();
  const size_t BufferSize = MatrixBytes + sizeof(uint32_t);
  std::stringstream ArgsStream;
  ArgsStream << "-HV 202x";
  ArgsStream << " -DCOMP_TYPE=" << static_cast<int>(Params.CompType);
  ArgsStream << " -DM_DIM=" << Params.M;
  ArgsStream << " -DN_DIM=" << Params.N;
  ArgsStream << " -DSTRIDE=" << Params.strideBytes();
  ArgsStream << " -DNUMTHREADS=" << SelectedWaveSize;
  ArgsStream << " -DFORCED_WAVE_SIZE=" << SelectedWaveSize;
  ArgsStream << " -DLAYOUT_OFFSET=" << MatrixBytes;
  ArgsStream << " -enable-16bit-types";
  const std::string Args = ArgsStream.str();

  compileShader(DxcSupport, QueryAccumLayoutShader, "cs_6_10", Args, Verbose);

  auto Op = createComputeOp(QueryAccumLayoutShader, "cs_6_10", "UAV(u0)",
                            Args.c_str());
  addUAVBuffer(Op.get(), "Output", BufferSize, true);
  addRootView(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  VERIFY_IS_TRUE(OutData.size() == BufferSize);
  if (OutData.size() != BufferSize)
    return;

  uint32_t Layout;
  std::memcpy(&Layout, static_cast<const BYTE *>(OutData.data()) + MatrixBytes,
              sizeof(Layout));
  VERIFY_IS_TRUE(Layout == static_cast<uint32_t>(MatrixUse::A) ||
                 Layout == static_cast<uint32_t>(MatrixUse::B));
  if (Layout != static_cast<uint32_t>(MatrixUse::A) &&
      Layout != static_cast<uint32_t>(MatrixUse::B))
    return;

  const int64_t ExpectedValue =
      Layout == static_cast<uint32_t>(MatrixUse::A) ? 5 : 9;
  const std::vector<int64_t> ExpectedValues(Params.totalElements(),
                                            ExpectedValue);
  std::optional<std::vector<BYTE>> Expected =
      cpu_oracle::encodeLogicalMatrixBuffer(Params, ExpectedValues);
  VERIFY_IS_TRUE(Expected.has_value());
  if (!Expected.has_value())
    return;
  const std::vector<BYTE> LayoutBytes =
      cpu_oracle::encodeNativeVector<uint32_t>({Layout});
  Expected->insert(Expected->end(), LayoutBytes.begin(), LayoutBytes.end());
  const cpu_oracle::BufferResultOracle Oracle = cpu_oracle::exactBufferResult(
      std::move(*Expected),
      L"AccumulatorLayout selects the matching A-use or B-use accumulation "
      L"path");
  VERIFY_IS_TRUE(cpu_oracle::verifyBufferResult(OutData.data(), OutData.size(),
                                                Oracle, Verbose));
  if (Verbose)
    hlsl_test::LogCommentFmt(L"AccumulatorLayout = %u", Layout);
}

void DxilConf_SM610_LinAlg::QueryAccumLayout() {
  runQueryAccumLayout(D3DDevice, DxcSupport, VerboseLogging);
}

static const char LoadMemoryShader[] = R"(
  RWByteAddressBuffer Input : register(u0);
  RWByteAddressBuffer Output : register(u1);
  groupshared ELEM_TYPE GsData[M_DIM * N_DIM];

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    for (uint Index = threadID; Index < M_DIM * N_DIM;
         Index += NUMTHREADS) {
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
                          const MatrixParams &Params, bool Verbose) {
  const size_t NumElements = Params.totalElements();
  const size_t BufferSize = Params.totalBytes();

  std::stringstream ExtraDefs;
  ExtraDefs << " -DOFFSET=" << 0;

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
    if (GetGroupWaveIndex() == 0) {
      __builtin_LinAlgMatrix
        [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
        Mat;
      __builtin_LinAlg_FillMatrix(Mat, FILL_VALUE);

      __builtin_LinAlg_MatrixStoreToMemory(
        Mat, GsData, OFFSET / ELEM_SIZE, STRIDE / ELEM_SIZE, LAYOUT);
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint Index = threadID; Index < M_DIM * N_DIM;
         Index += NUMTHREADS) {
      Output.Store<ELEM_TYPE>(Index * ELEM_SIZE, GsData[Index]);
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
  STREAM_FLOAT(ExtraDefs, "FILL_VALUE", FillValue);

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
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runStoreMemory(D3DDevice, DxcSupport, Params, VerboseLogging,
                 /*FillValue=*/7.0f);
}

static const char AccumulateMemoryShader[] = R"(
  RWByteAddressBuffer Output : register(u0);
  groupshared ELEM_TYPE GsData[M_DIM * N_DIM];

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    for (uint Index = threadID; Index < M_DIM * N_DIM;
         Index += NUMTHREADS) {
      GsData[Index] = FILL_VALUE;
    }

    GroupMemoryBarrierWithGroupSync();

    if (GetGroupWaveIndex() == 0) {
      __builtin_LinAlgMatrix
        [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
        Mat;
      __builtin_LinAlg_FillMatrix(Mat, FILL_VALUE);

      __builtin_LinAlg_MatrixAccumulateToMemory(
        Mat, GsData, OFFSET / ELEM_SIZE, STRIDE / ELEM_SIZE, LAYOUT);
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint Index = threadID; Index < M_DIM * N_DIM;
         Index += NUMTHREADS) {
      Output.Store<ELEM_TYPE>(Index * ELEM_SIZE, GsData[Index]);
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
  STREAM_FLOAT(ExtraDefs, "FILL_VALUE", FillValue);

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
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runAccumulateMemory(D3DDevice, DxcSupport, Params, VerboseLogging,
                      /*FillValue=*/7.0f);
}

struct GroupSharedMemorySpec {
  cpu_oracle::MatrixBufferLayout Layout;
};

static const char GroupSharedTransferShader[] = R"(
  ByteAddressBuffer SourceInit : register(t0);
  ByteAddressBuffer DestinationInit : register(t1);
  RWByteAddressBuffer Output : register(u2);

  groupshared ELEM_TYPE SourceData[SRC_ELEMENTS];
  groupshared ELEM_TYPE DestinationData[DST_ELEMENTS];

  void TransferMatrix() {
    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_MatrixLoadFromMemory(
      Mat, SourceData, SRC_OFFSET, SRC_STRIDE, SRC_LAYOUT);
    __builtin_LinAlg_MatrixStoreToMemory(
      Mat, DestinationData, DST_OFFSET, DST_STRIDE, DST_LAYOUT);
  }

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 64)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    for (uint Index = threadID; Index < SRC_ELEMENTS;
         Index += NUMTHREADS) {
      SourceData[Index] =
        SourceInit.Load<ELEM_TYPE>(Index * ELEM_SIZE);
    }
    for (uint Index = threadID; Index < DST_ELEMENTS;
         Index += NUMTHREADS) {
      DestinationData[Index] =
        DestinationInit.Load<ELEM_TYPE>(Index * ELEM_SIZE);
    }

    GroupMemoryBarrierWithGroupSync();

    #if SCOPE == 1
    if (GetGroupWaveIndex() == 0)
      TransferMatrix();
    #elif SCOPE == 2
    TransferMatrix();
    #else
    #error Group-shared matrix transfer requires Wave or ThreadGroup scope
    #endif

    GroupMemoryBarrierWithGroupSync();

    for (uint Index = threadID; Index < DST_ELEMENTS;
         Index += NUMTHREADS) {
      Output.Store<ELEM_TYPE>(Index * ELEM_SIZE, DestinationData[Index]);
    }
  }
)";

static bool getGroupSharedBufferDescription(const MatrixParams &Params,
                                            const GroupSharedMemorySpec &Spec,
                                            size_t &BufferSize,
                                            UINT &NumElements) {
  const size_t ElementBytes = elementSize(Params.CompType);
  std::optional<size_t> RequiredBytes = cpu_oracle::getMatrixBufferSize(
      Params.CompType, Params.M, Params.N, Spec.Layout);
  if (!RequiredBytes.has_value() ||
      Spec.Layout.OffsetBytes % ElementBytes != 0 ||
      Spec.Layout.StrideBytes % ElementBytes != 0 ||
      *RequiredBytes % ElementBytes != 0 ||
      *RequiredBytes / ElementBytes > (std::numeric_limits<UINT>::max)())
    return false;

  BufferSize = *RequiredBytes;
  NumElements = static_cast<UINT>(BufferSize / ElementBytes);
  return true;
}

static void
runGroupSharedTransfer(ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
                       const MatrixParams &Params,
                       const GroupSharedMemorySpec &Source,
                       const GroupSharedMemorySpec &Destination, bool Verbose,
                       std::optional<UINT> ForcedWaveSize = std::nullopt) {
  using namespace cpu_oracle;

  if (!Device ||
      (Params.Scope != MatrixScope::Wave &&
       Params.Scope != MatrixScope::ThreadGroup) ||
      Params.Use != MatrixUse::A) {
    VERIFY_IS_TRUE(false, "Invalid group-shared transfer parameters");
    return;
  }

  size_t SourceBufferSize;
  UINT SourceElements;
  size_t DestinationBufferSize;
  UINT DestinationElements;
  if (!getGroupSharedBufferDescription(Params, Source, SourceBufferSize,
                                       SourceElements) ||
      !getGroupSharedBufferDescription(
          Params, Destination, DestinationBufferSize, DestinationElements)) {
    VERIFY_IS_TRUE(false, "Invalid group-shared buffer description");
    return;
  }

  std::optional<TypedMatrix> SourceValues = makeSequentialMatrix(
      Params.CompType, Params.M, Params.N, /*StartingValue=*/1);
  std::optional<TypedMatrix> DestinationValues = makeSequentialMatrix(
      Params.CompType, Params.M, Params.N, /*StartingValue=*/1);
  std::optional<std::vector<BYTE>> SourceBuffer =
      makeFilledComponentBuffer(Params.CompType, SourceBufferSize, 91);
  std::optional<std::vector<BYTE>> DestinationInitial =
      makeFilledComponentBuffer(Params.CompType, DestinationBufferSize, 90);
  if (!SourceValues.has_value() || !DestinationValues.has_value() ||
      !SourceBuffer.has_value() || !DestinationInitial.has_value() ||
      !writeMatrixBuffer(*SourceValues, Source.Layout, *SourceBuffer)) {
    VERIFY_IS_TRUE(false, "Failed to build group-shared transfer inputs");
    return;
  }

  std::vector<BYTE> Expected = *DestinationInitial;
  if (!writeMatrixBuffer(*DestinationValues, Destination.Layout, Expected)) {
    VERIFY_IS_TRUE(false, "Failed to build group-shared transfer expectation");
    return;
  }

  std::stringstream ExtraDefs;
  ExtraDefs << " -DSRC_ELEMENTS=" << SourceElements;
  ExtraDefs << " -DSRC_OFFSET="
            << Source.Layout.OffsetBytes / elementSize(Params.CompType);
  ExtraDefs << " -DSRC_STRIDE="
            << Source.Layout.StrideBytes / elementSize(Params.CompType);
  ExtraDefs << " -DSRC_LAYOUT=" << static_cast<UINT>(Source.Layout.Layout);
  ExtraDefs << " -DDST_ELEMENTS=" << DestinationElements;
  ExtraDefs << " -DDST_OFFSET="
            << Destination.Layout.OffsetBytes / elementSize(Params.CompType);
  ExtraDefs << " -DDST_STRIDE="
            << Destination.Layout.StrideBytes / elementSize(Params.CompType);
  ExtraDefs << " -DDST_LAYOUT=" << static_cast<UINT>(Destination.Layout.Layout);
  if (ForcedWaveSize.has_value())
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << *ForcedWaveSize;

  const std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());
  compileShader(DxcSupport, GroupSharedTransferShader, "cs_6_10", Args,
                Verbose);

  auto Op = createComputeOp(GroupSharedTransferShader, "cs_6_10",
                            "SRV(t0), SRV(t1), UAV(u2)", Args.c_str());
  addSRVBuffer(Op.get(), "SourceInit", SourceBuffer->size(), "byname");
  addSRVBuffer(Op.get(), "DestinationInit", DestinationInitial->size(),
               "byname");
  addUAVBuffer(Op.get(), "Output", Expected.size(), true);
  addRootView(Op.get(), 0, "SourceInit");
  addRootView(Op.get(), 1, "DestinationInit");
  addRootView(Op.get(), 2, "Output");

  auto Result =
      runShaderOp(Device, DxcSupport, std::move(Op),
                  [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *) {
                    if (strcmp(Name, "SourceInit") == 0)
                      Data = *SourceBuffer;
                    else if (strcmp(Name, "DestinationInit") == 0)
                      Data = *DestinationInitial;
                  });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  BufferResultOracle Oracle =
      exactBufferResult(std::move(Expected),
                        L"Exact group-shared load/store layout and addressing");
  VERIFY_IS_TRUE(
      verifyBufferResult(OutData.data(), OutData.size(), Oracle, Verbose));
}

static void runBidirectionalGroupSharedTransfer(
    ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
    const MatrixParams &Params, const GroupSharedMemorySpec &Target,
    const GroupSharedMemorySpec &Canonical, bool Verbose,
    std::optional<UINT> ForcedWaveSize = std::nullopt) {
  hlsl_test::LogCommentFmt(L"Group-shared transfer: target to canonical");
  runGroupSharedTransfer(Device, DxcSupport, Params, Target, Canonical, Verbose,
                         ForcedWaveSize);
  hlsl_test::LogCommentFmt(L"Group-shared transfer: canonical to target");
  runGroupSharedTransfer(Device, DxcSupport, Params, Canonical, Target, Verbose,
                         ForcedWaveSize);
}

void DxilConf_SM610_LinAlg::
    LoadStoreMemory_Wave_4x8_F16_RowMajorOffsetPadded() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryMatrixConstructionSupport(
      D3DDevice, Params, {{linalg_test::MatrixRole::A, Params.M, Params.N}},
      L"LoadStoreMemory_Wave_4x8_F16_RowMajorOffsetPadded", Supported,
      SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability,
                          L"LoadStoreMemory_Wave_4x8_F16_RowMajorOffsetPadded "
                          L"MatrixConstruction"))
    return;

  const GroupSharedMemorySpec Target = {{LinalgMatrixLayout::RowMajor,
                                         /*OffsetBytes=*/8,
                                         /*StrideBytes=*/24}};
  const GroupSharedMemorySpec Canonical = {{LinalgMatrixLayout::RowMajor,
                                            /*OffsetBytes=*/0,
                                            /*StrideBytes=*/16}};
  runBidirectionalGroupSharedTransfer(D3DDevice, DxcSupport, Params, Target,
                                      Canonical, VerboseLogging,
                                      SelectedWaveSize);
}

void DxilConf_SM610_LinAlg::
    LoadStoreMemory_Wave_4x8_F32_ColumnMajorOffsetPadded() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::ColumnMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = false;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryMatrixConstructionSupport(
      D3DDevice, Params, {{linalg_test::MatrixRole::A, Params.M, Params.N}},
      L"LoadStoreMemory_Wave_4x8_F32_ColumnMajorOffsetPadded", Supported,
      SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(
          Applicability,
          L"LoadStoreMemory_Wave_4x8_F32_ColumnMajorOffsetPadded "
          L"MatrixConstruction"))
    return;

  const GroupSharedMemorySpec Target = {{LinalgMatrixLayout::ColumnMajor,
                                         /*OffsetBytes=*/16,
                                         /*StrideBytes=*/24}};
  const GroupSharedMemorySpec Canonical = {{LinalgMatrixLayout::RowMajor,
                                            /*OffsetBytes=*/0,
                                            /*StrideBytes=*/32}};
  runBidirectionalGroupSharedTransfer(D3DDevice, DxcSupport, Params, Target,
                                      Canonical, VerboseLogging,
                                      SelectedWaveSize);
}

void DxilConf_SM610_LinAlg::LoadStoreMemory_ThreadGroup_4x8_F16() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::ThreadGroup;
  Params.Layout = LinalgMatrixLayout::ColumnMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryMatrixConstructionSupport(
      D3DDevice, Params, {{linalg_test::MatrixRole::A, Params.M, Params.N}},
      L"LoadStoreMemory_ThreadGroup_4x8_F16", Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(
          Applicability,
          L"LoadStoreMemory_ThreadGroup_4x8_F16 MatrixConstruction"))
    return;

  const GroupSharedMemorySpec Target = {{LinalgMatrixLayout::ColumnMajor,
                                         /*OffsetBytes=*/8,
                                         /*StrideBytes=*/12}};
  const GroupSharedMemorySpec Canonical = {{LinalgMatrixLayout::RowMajor,
                                            /*OffsetBytes=*/0,
                                            /*StrideBytes=*/16}};
  runBidirectionalGroupSharedTransfer(D3DDevice, DxcSupport, Params, Target,
                                      Canonical, VerboseLogging,
                                      SelectedWaveSize);
}

static const char GroupSharedAccumulateShader[] = R"(
  ByteAddressBuffer Initial : register(t0);
  RWByteAddressBuffer Output : register(u1);
  groupshared ELEM_TYPE GsData[MEM_ELEMENTS];

  #ifdef FORCED_WAVE_SIZE
  [WaveSize(FORCED_WAVE_SIZE)]
  #else
  [WaveSize(4, 64)]
  #endif
  [numthreads(NUMTHREADS, 1, 1)]
  void main(uint threadID : SV_GroupIndex) {
    for (uint Index = threadID; Index < MEM_ELEMENTS;
         Index += NUMTHREADS) {
      GsData[Index] = Initial.Load<ELEM_TYPE>(Index * ELEM_SIZE);
    }

    GroupMemoryBarrierWithGroupSync();

    if (GetGroupWaveIndex() == 0) {
      __builtin_LinAlgMatrix
        [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
        Mat;
      __builtin_LinAlg_FillMatrix(Mat, 0);
      for (uint I = 0; I < __builtin_LinAlg_MatrixLength(Mat); ++I) {
        uint2 Coord = __builtin_LinAlg_MatrixGetCoordinate(Mat, I);
        __builtin_LinAlg_MatrixSetElement(
          Mat, Mat, I,
          (ELEM_TYPE)(ACCUMULATE_START + Coord.x * N_DIM + Coord.y));
      }
      __builtin_LinAlg_MatrixAccumulateToMemory(
        Mat, GsData, MEM_OFFSET, MEM_STRIDE, MEM_LAYOUT);
    }

    GroupMemoryBarrierWithGroupSync();

    for (uint Index = threadID; Index < MEM_ELEMENTS;
         Index += NUMTHREADS) {
      Output.Store<ELEM_TYPE>(Index * ELEM_SIZE, GsData[Index]);
    }
  }
)";

static void runGroupSharedAccumulate(
    ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
    const MatrixParams &Params, const GroupSharedMemorySpec &Memory,
    uint32_t InitialValue, uint32_t AccumulateStartingValue, bool Verbose,
    std::optional<UINT> ForcedWaveSize = std::nullopt) {
  using namespace cpu_oracle;

  if (!Device || Params.Scope != MatrixScope::Wave ||
      Params.Use != MatrixUse::Accumulator ||
      InitialValue >
          (std::numeric_limits<uint32_t>::max)() - AccumulateStartingValue) {
    VERIFY_IS_TRUE(false, "Invalid group-shared accumulate parameters");
    return;
  }

  size_t BufferSize;
  UINT NumElements;
  if (!getGroupSharedBufferDescription(Params, Memory, BufferSize,
                                       NumElements)) {
    VERIFY_IS_TRUE(false, "Invalid group-shared accumulate buffer");
    return;
  }

  std::optional<TypedMatrix> InitialMatrix =
      makeSequentialMatrix(Params.CompType, Params.M, Params.N, InitialValue,
                           /*Increment=*/false);
  std::optional<TypedMatrix> ExpectedMatrix = makeSequentialMatrix(
      Params.CompType, Params.M, Params.N,
      InitialValue + AccumulateStartingValue, /*Increment=*/true);
  std::optional<std::vector<BYTE>> Initial =
      makeFilledComponentBuffer(Params.CompType, BufferSize, 90);
  std::optional<std::vector<BYTE>> Expected =
      makeFilledComponentBuffer(Params.CompType, BufferSize, 90);
  if (!InitialMatrix.has_value() || !ExpectedMatrix.has_value() ||
      !Initial.has_value() || !Expected.has_value() ||
      !writeMatrixBuffer(*InitialMatrix, Memory.Layout, *Initial) ||
      !writeMatrixBuffer(*ExpectedMatrix, Memory.Layout, *Expected)) {
    VERIFY_IS_TRUE(false, "Failed to build group-shared accumulate oracle");
    return;
  }

  std::stringstream ExtraDefs;
  ExtraDefs << " -DMEM_ELEMENTS=" << NumElements;
  ExtraDefs << " -DMEM_OFFSET="
            << Memory.Layout.OffsetBytes / elementSize(Params.CompType);
  ExtraDefs << " -DMEM_STRIDE="
            << Memory.Layout.StrideBytes / elementSize(Params.CompType);
  ExtraDefs << " -DMEM_LAYOUT=" << static_cast<UINT>(Memory.Layout.Layout);
  ExtraDefs << " -DACCUMULATE_START=" << AccumulateStartingValue;
  if (ForcedWaveSize.has_value())
    ExtraDefs << " -DFORCED_WAVE_SIZE=" << *ForcedWaveSize;

  const std::string Args = buildCompilerArgs(Params, ExtraDefs.str().c_str());
  compileShader(DxcSupport, GroupSharedAccumulateShader, "cs_6_10", Args,
                Verbose);

  auto Op = createComputeOp(GroupSharedAccumulateShader, "cs_6_10",
                            "SRV(t0), UAV(u1)", Args.c_str());
  addSRVBuffer(Op.get(), "Initial", Initial->size(), "byname");
  addUAVBuffer(Op.get(), "Output", Expected->size(), true);
  addRootView(Op.get(), 0, "Initial");
  addRootView(Op.get(), 1, "Output");

  auto Result =
      runShaderOp(Device, DxcSupport, std::move(Op),
                  [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *) {
                    if (strcmp(Name, "Initial") == 0)
                      Data = *Initial;
                  });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  BufferResultOracle Oracle = exactBufferResult(
      std::move(*Expected), L"Exact Wave group-shared atomic accumulation");
  VERIFY_IS_TRUE(
      verifyBufferResult(OutData.data(), OutData.size(), Oracle, Verbose));
}

void DxilConf_SM610_LinAlg::
    AccumulateMemory_Wave_4x8_F16_RowMajorOffsetPadded() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F16;
  Params.M = 4;
  Params.N = 8;
  Params.Use = MatrixUse::Accumulator;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;

  bool Supported;
  UINT SelectedWaveSize;
  const HRESULT QueryResult = queryAtomicAccumulateSupport(
      D3DDevice, Params, linalg_test::AtomicDestination::GroupShared,
      L"AccumulateMemory_Wave_4x8_F16_RowMajorOffsetPadded", Supported,
      SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability,
                          L"AccumulateMemory_Wave_4x8_F16_RowMajorOffsetPadded "
                          L"AtomicAccumulateStore"))
    return;

  const GroupSharedMemorySpec Memory = {{LinalgMatrixLayout::RowMajor,
                                         /*OffsetBytes=*/8,
                                         /*StrideBytes=*/24}};
  runGroupSharedAccumulate(D3DDevice, DxcSupport, Params, Memory,
                           /*InitialValue=*/12,
                           /*AccumulateStartingValue=*/1, VerboseLogging,
                           SelectedWaveSize);
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

static const char ConvertI16ToI32Shader[] = R"(
  #define CT_I16 2
  #define CT_I32 4

  RWByteAddressBuffer Output : register(u0);

  [numthreads(1, 1, 1)]
  void main() {
    vector<int16_t, 8> InVec = {
      -32768, -1024, -1, 0, 1, 1024, 12345, 32767
    };
    vector<int, 8> OutVec;
    __builtin_LinAlg_Convert(OutVec, InVec, CT_I16, CT_I32);
    for (uint I = 0; I < 8; ++I)
      Output.Store<int>(I * sizeof(int), OutVec[I]);
  }
)";

static const char ConvertF32ToI16Shader[] = R"(
  #define CT_I16 2
  #define CT_F32 9

  RWByteAddressBuffer Output : register(u0);

  [numthreads(1, 1, 1)]
  void main() {
    vector<float, 8> InVec = {
      -40000.0F, -32768.5F, -2.5F, -1.5F,
      1.5F, 2.5F, 32767.5F, 40000.0F
    };
    vector<int16_t, 8> OutVec;
    __builtin_LinAlg_Convert(OutVec, InVec, CT_F32, CT_I16);
    for (uint I = 0; I < 8; ++I)
      Output.Store<int16_t>(I * sizeof(int16_t), OutVec[I]);
  }
)";

static const char ConvertF16FP8RoundTripShader[] = R"(
  #define CT_F16 8

  RWByteAddressBuffer Output : register(u0);

  [numthreads(1, 1, 1)]
  void main() {
    vector<half, 8> InVec = {
      0.0, 1.0, -1.0, 1.5, -1.5, 3.0, 6.0, -6.0
    };
    vector<uint, 2> PackedBits;
    vector<uint, 2> PackedRoundTrip;
    vector<half, 8> RoundTrip;
    __builtin_LinAlg_Convert(PackedBits, InVec, CT_F16, FP8_TYPE);
    __builtin_LinAlg_Convert(PackedRoundTrip, InVec, CT_F16, FP8_TYPE);
    __builtin_LinAlg_Convert(RoundTrip, PackedRoundTrip, FP8_TYPE, CT_F16);

    Output.Store<uint>(0, PackedBits.x);
    Output.Store<uint>(4, PackedBits.y);
    for (uint I = 0; I < 8; ++I)
      Output.Store<half>(8 + I * sizeof(half), RoundTrip[I]);
  }
)";

static void runExactConvertShader(ID3D12Device *Device,
                                  dxc::SpecificDllLoader &DxcSupport,
                                  const char *Shader, const std::string &Args,
                                  std::vector<BYTE> Expected,
                                  const std::wstring &PublicRule,
                                  bool Verbose) {
  compileShader(DxcSupport, Shader, "cs_6_10", Args, Verbose);

  auto Op = createComputeOp(Shader, "cs_6_10", "UAV(u0)", Args.c_str());
  addUAVBuffer(Op.get(), "Output", Expected.size(), true);
  addRootView(Op.get(), 0, "Output");

  auto Result = runShaderOp(Device, DxcSupport, std::move(Op));
  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  if (Verbose) {
    const BYTE *Bytes = static_cast<const BYTE *>(OutData.data());
    for (size_t I = 0; I < OutData.size(); ++I)
      hlsl_test::LogCommentFmt(L"  output[%zu]=0x%02x", I, Bytes[I]);
  }

  cpu_oracle::BufferResultOracle Oracle =
      cpu_oracle::exactBufferResult(std::move(Expected), PublicRule);
  VERIFY_IS_TRUE(cpu_oracle::verifyBufferResult(OutData.data(), OutData.size(),
                                                Oracle, Verbose));
}

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
  runConvert(D3DDevice, DxcSupport, VerboseLogging);
}

void DxilConf_SM610_LinAlg::Convert_I16_ToI32_Exact() {
  runExactConvertShader(
      D3DDevice, DxcSupport, ConvertI16ToI32Shader,
      "-HV 202x -enable-16bit-types",
      cpu_oracle::encodeNativeVector<int32_t>(
          {-32768, -1024, -1, 0, 1, 1024, 12345, 32767}),
      L"Integer widening preserves exactly representable values",
      VerboseLogging);
}

void DxilConf_SM610_LinAlg::Convert_F32_ToI16_RTNE_Saturate() {
  runExactConvertShader(
      D3DDevice, DxcSupport, ConvertF32ToI16Shader,
      "-HV 202x -enable-16bit-types",
      cpu_oracle::encodeNativeVector<int16_t>(
          {-32768, -32768, -2, -2, 2, 2, 32767, 32767}),
      L"Floating-to-integer conversion uses RTNE and saturation",
      VerboseLogging);
}

void DxilConf_SM610_LinAlg::Convert_F16_FP8_RoundTrip() {
  MatrixParams Params = {};
  Params.M = 1;
  Params.N = 1;
  Params.Use = MatrixUse::A;
  Params.Scope = MatrixScope::Wave;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;

  const std::vector<float> Values = {
      0.0f, 1.0f, -1.0f, 1.5f, -1.5f, 3.0f, 6.0f, -6.0f,
  };

  size_t ExecutedFormats = 0;
  auto RunFormat = [&](ComponentType CompType, const char *CompilerArgs,
                       LPCWSTR CaseName) -> bool {
    Params.CompType = CompType;
    bool Supported;
    UINT SelectedWaveSize;
    // D3D12 has no Convert query, so use shape-independent construction
    // support as the closest available component-type capability proxy.
    const HRESULT QueryResult = queryMatrixConstructionSupport(
        D3DDevice, Params,
        {{linalg_test::MatrixRole::A, 0, 0, CompType,
          /*RequireShape=*/false}},
        CaseName, Supported, SelectedWaveSize);
    const linalg_test::Applicability Applicability =
        linalg_test::classifyApplicability(
            QueryResult, Supported,
            linalg_test::CapabilityRequirement::CapabilityGated);
    if (Applicability == linalg_test::Applicability::Fail)
      return applyApplicability(Applicability, CaseName);
    if (Applicability == linalg_test::Applicability::NotApplicable) {
      // Each format is an optional subcase; the method is not applicable only
      // when neither format can execute.
      hlsl_test::LogCommentFmt(
          L"Skipping %s because the component type is not advertised",
          CaseName);
      return true;
    }

    std::optional<std::vector<BYTE>> Expected =
        cpu_oracle::encodeExactFP8RoundTrip(Values, CompType);
    VERIFY_IS_TRUE(Expected.has_value(),
                   "Unable to derive exact FP8 conversion oracle");
    if (!Expected.has_value())
      return false;

    ++ExecutedFormats;
    runExactConvertShader(D3DDevice, DxcSupport, ConvertF16FP8RoundTripShader,
                          CompilerArgs, std::move(*Expected),
                          std::wstring(CaseName) +
                              L" exact packed encoding and F16 round trip",
                          VerboseLogging);
    return true;
  };

  if (!RunFormat(ComponentType::F8_E4M3FN,
                 "-HV 202x -enable-16bit-types -DFP8_TYPE=21", L"F8_E4M3FN"))
    return;
  if (!RunFormat(ComponentType::F8_E5M2,
                 "-HV 202x -enable-16bit-types -DFP8_TYPE=22", L"F8_E5M2"))
    return;

  if (ExecutedFormats == 0)
    applyApplicability(linalg_test::Applicability::NotApplicable,
                       L"Convert_F16_FP8_RoundTrip");
}

static const char VectorAccumulateDescriptorShader[] = R"(
  ByteAddressBuffer Input : register(t0);
  RWByteAddressBuffer Output : register(u1);

  [numthreads(1, 1, 1)]
  void main() {
    vector<ELEM_TYPE, VECTOR_LENGTH> InVec;
    for (uint I = 0; I < VECTOR_LENGTH; ++I) {
      InVec[I] = Input.Load<ELEM_TYPE>(I * ELEM_SIZE);
    }
    __builtin_LinAlg_VectorAccumulateToDescriptor(
      Output, 0, 64, InVec);
  }
)";

struct VectorAccumulateCaseData {
  ComponentType CompType = ComponentType::Invalid;
  std::vector<int64_t> InputValues;
  std::vector<int64_t> InitialValues;
  std::wstring PublicRule;
};

static bool isVectorAccumulateCaseValid(const VectorAccumulateCaseData &Case) {
  return !Case.InputValues.empty() &&
         Case.InputValues.size() == Case.InitialValues.size() &&
         toCapabilityDataType(Case.CompType).has_value() &&
         cpu_oracle::hlslElementTypeName(Case.CompType) &&
         !Case.PublicRule.empty();
}

static std::optional<std::vector<int64_t>>
calculateVectorAccumulateExpected(const VectorAccumulateCaseData &Case) {
  if (!isVectorAccumulateCaseValid(Case))
    return std::nullopt;
  std::vector<int64_t> Expected(Case.InputValues.size());
  for (size_t I = 0; I < Expected.size(); ++I) {
    if (!cpu_oracle::checkedAddInt64(Case.InitialValues[I], Case.InputValues[I],
                                     Expected[I]))
      return std::nullopt;
  }
  return Expected;
}

static std::optional<std::string>
buildVectorAccumulateCompilerArgs(const VectorAccumulateCaseData &Case) {
  if (!isVectorAccumulateCaseValid(Case))
    return std::nullopt;
  std::stringstream SS;
  SS << "-HV 202x";
  SS << " -DELEM_TYPE=" << cpu_oracle::hlslElementTypeName(Case.CompType);
  SS << " -DELEM_SIZE=" << static_cast<int>(elementSize(Case.CompType));
  SS << " -DVECTOR_LENGTH=" << Case.InputValues.size();
  if (needs16BitTypes(Case.CompType))
    SS << " -enable-16bit-types";
  return SS.str();
}

static void runVectorAccumulateDescriptor(ID3D12Device *Device,
                                          dxc::SpecificDllLoader &DxcSupport,
                                          const VectorAccumulateCaseData &Case,
                                          LPCWSTR CaseName, bool Verbose) {
  VERIFY_IS_TRUE(isVectorAccumulateCaseValid(Case));
  if (!isVectorAccumulateCaseValid(Case))
    return;

  bool Supported;
  const HRESULT HR = queryVectorAccumulateDescriptorSupport(
      Device, Case.CompType, CaseName, Supported);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          HR, Supported, linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(Applicability, CaseName))
    return;

  const std::optional<std::vector<BYTE>> Input =
      cpu_oracle::encodeExactComponents(Case.CompType, Case.InputValues);
  const std::optional<std::vector<BYTE>> Initial =
      cpu_oracle::encodeExactComponents(Case.CompType, Case.InitialValues);
  const std::optional<std::vector<int64_t>> ExpectedValues =
      calculateVectorAccumulateExpected(Case);
  const std::optional<std::vector<BYTE>> Expected =
      ExpectedValues.has_value()
          ? cpu_oracle::encodeExactComponents(Case.CompType, *ExpectedValues)
          : std::optional<std::vector<BYTE>>();
  const std::optional<std::string> Args =
      buildVectorAccumulateCompilerArgs(Case);
  VERIFY_IS_TRUE(Input.has_value());
  VERIFY_IS_TRUE(Initial.has_value());
  VERIFY_IS_TRUE(Expected.has_value());
  VERIFY_IS_TRUE(Args.has_value());
  if (!Input.has_value() || !Initial.has_value() || !Expected.has_value() ||
      !Args.has_value())
    return;

  compileShader(DxcSupport, VectorAccumulateDescriptorShader, "cs_6_10", *Args,
                Verbose);
  auto Op = createComputeOp(VectorAccumulateDescriptorShader, "cs_6_10",
                            "SRV(t0), UAV(u1)", Args->c_str());
  addSRVBuffer(Op.get(), "Input", Input->size(), "byname");
  addUAVBuffer(Op.get(), "Output", Initial->size(), true, "byname");
  addRootView(Op.get(), 0, "Input");
  addRootView(Op.get(), 1, "Output");

  auto Result = runShaderOp(
      Device, DxcSupport, std::move(Op),
      [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *) {
        const std::vector<BYTE> *Source = nullptr;
        if (strcmp(Name, "Input") == 0)
          Source = &*Input;
        else if (strcmp(Name, "Output") == 0)
          Source = &*Initial;
        if (!Source)
          return;
        VERIFY_IS_TRUE(Data.size() == Source->size(),
                       "Vector accumulation initializer size mismatch");
        std::copy(Source->begin(), Source->end(), Data.begin());
      });

  MappedData OutData;
  Result->Test->GetReadBackData("Output", &OutData);
  const cpu_oracle::BufferResultOracle Oracle =
      cpu_oracle::exactBufferResult(*Expected, Case.PublicRule);
  VERIFY_IS_TRUE(cpu_oracle::verifyBufferResult(OutData.data(), OutData.size(),
                                                Oracle, Verbose));
}

void DxilConf_SM610_LinAlg::VectorAccumulateDescriptor_Thread_F16() {
  VectorAccumulateCaseData Case = {};
  Case.CompType = ComponentType::F16;
  Case.InputValues = {1, 2, 3, 4};
  Case.InitialValues = {0, 0, 0, 0};
  Case.PublicRule = L"Exact F16 vector descriptor accumulation";
  runVectorAccumulateDescriptor(D3DDevice, DxcSupport, Case,
                                L"VectorAccumulateDescriptor_Thread_F16",
                                VerboseLogging);
}

void DxilConf_SM610_LinAlg::
    VectorAccumulateDescriptor_Thread_F16_Length8_NonZero() {
  VectorAccumulateCaseData Case = {};
  Case.CompType = ComponentType::F16;
  Case.InputValues = {-3, 2, 5, -1, 4, 0, -2, 6};
  Case.InitialValues = {10, 11, 12, 13, 14, 15, 16, 17};
  Case.PublicRule =
      L"Exact non-contended F16 vector accumulation onto non-zero values";
  runVectorAccumulateDescriptor(
      D3DDevice, DxcSupport, Case,
      L"VectorAccumulateDescriptor_Thread_F16_Length8_NonZero", VerboseLogging);
}

void DxilConf_SM610_LinAlg::
    VectorAccumulateDescriptor_Thread_F32_Length8_NonZero() {
  VectorAccumulateCaseData Case = {};
  Case.CompType = ComponentType::F32;
  Case.InputValues = {6, -2, 0, 3, -5, 4, 1, -1};
  Case.InitialValues = {20, 21, 22, 23, 24, 25, 26, 27};
  Case.PublicRule =
      L"Exact non-contended F32 vector accumulation onto non-zero values";
  runVectorAccumulateDescriptor(
      D3DDevice, DxcSupport, Case,
      L"VectorAccumulateDescriptor_Thread_F32_Length8_NonZero", VerboseLogging);
}

} // namespace LinAlg
