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
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 4;
  Params.CompType = ComponentType::I32;
  VERIFY_IS_TRUE(buildCompilerArgs(Params).find(" -DELEM_TYPE=int") !=
                 std::string::npos);
  Params.CompType = ComponentType::U32;
  VERIFY_IS_TRUE(buildCompilerArgs(Params).find(" -DELEM_TYPE=uint") !=
                 std::string::npos);
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
  TEST_METHOD(ElementGetOOB_Wave_4x8_F32);
  TEST_METHOD(ElementSet_Wave_16x16_F16);
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

  [WaveSize(4, 64)]
  [numthreads(NUMTHREADS, 1, 1)]
  void main() {
    if (GetGroupWaveIndex() != 0)
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

static const char AccumulateDescriptorShader[] = R"(
  #define USE_ACC 2

  ByteAddressBuffer Input : register(t0);
  RWByteAddressBuffer Output : register(u1);

  [WaveSize(4, 64)]
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
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 64;
  Params.Enable16Bit = true;
  runAccumulateDescriptor(D3DDevice, DxcSupport, Params, 12, VerboseLogging);
}

struct MatrixConstructionRequirement {
  linalg_test::MatrixRole Role;
  MatrixDim Rows;
  MatrixDim Columns;
};

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
    if (Requirement.Rows == 0 || Requirement.Columns == 0)
      return E_INVALIDARG;
  }

  std::optional<linalg_test::DataType> DataType =
      toCapabilityDataType(Params.CompType);
  if (!DataType.has_value())
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

    linalg_test::MatrixConstructionSupport Construction;
    HR = linalg_test::queryMatrixConstruction(Device, {*DataType, WaveSize},
                                              Construction);
    if (FAILED(HR))
      return HR;

    bool AllSupported = true;
    for (const MatrixConstructionRequirement &Requirement : Requirements) {
      if (!Construction.supports(Requirement.Role, Requirement.Rows,
                                 Requirement.Columns)) {
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
      hlsl_test::LogCommentFmt(L"  role=%s, rows=%u, columns=%u",
                               matrixRoleName(Requirement.Role),
                               Requirement.Rows, Requirement.Columns);
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
      [[__LinAlgMatrix_Attributes(COMP_TYPE, DST_M_DIM, DST_N_DIM, USE, SCOPE)]]
      Dst;

    __builtin_LinAlg_MatrixLoadFromDescriptor(
      Src, Input, 0, SRC_STRIDE, LAYOUT, 128);
    __builtin_LinAlg_CopyConvertMatrix(Dst, Src, TRANSPOSE);
    __builtin_LinAlg_MatrixStoreToDescriptor(
      Dst, Output, 0, DST_STRIDE, LAYOUT, 128);
  }
)";

static HRESULT queryCopyConvertSupport(ID3D12Device *Device,
                                       const MatrixParams &Params,
                                       bool Transpose, bool &Supported,
                                       UINT &SelectedWaveSize) {
  Supported = false;
  SelectedWaveSize = 0;
  if (Params.Use != MatrixUse::A)
    return E_INVALIDARG;

  MatrixParams Destination = Params;
  if (Transpose) {
    Destination.M = Params.N;
    Destination.N = Params.M;
  }

  return queryMatrixConstructionSupport(
      Device, Params,
      {
          {linalg_test::MatrixRole::A, Params.M, Params.N},
          {linalg_test::MatrixRole::A, Destination.M, Destination.N},
      },
      L"CopyConvert source and destination", Supported, SelectedWaveSize);
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
  const HRESULT QueryResult = queryCopyConvertSupport(
      D3DDevice, Params, /*Transpose=*/true, Supported, SelectedWaveSize);
  const linalg_test::Applicability Applicability =
      linalg_test::classifyApplicability(
          QueryResult, Supported,
          linalg_test::CapabilityRequirement::CapabilityGated);
  if (!applyApplicability(
          Applicability,
          L"CopyConvert_Wave_4x8_F32_Transpose MatrixConstruction"))
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
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 1;
  Params.Enable16Bit = true;
  runMatVecMul(D3DDevice, DxcSupport, Params, VerboseLogging,
               /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F16);
}

void DxilConf_SM610_LinAlg::MatVecMul_Thread_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Scope = MatrixScope::Thread;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 1;
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
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 1;
  Params.Enable16Bit = true;
  runMatVecMulAdd(D3DDevice, DxcSupport, Params, VerboseLogging,
                  /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F16,
                  ComponentType::F16);
}

void DxilConf_SM610_LinAlg::MatVecMulAdd_Thread_4x8_F32() {
  MatrixParams Params = {};
  Params.CompType = ComponentType::F32;
  Params.M = 4;
  Params.N = 8;
  Params.Scope = MatrixScope::Thread;
  Params.Layout = LinalgMatrixLayout::RowMajor;
  Params.NumThreads = 1;
  runMatVecMulAdd(D3DDevice, DxcSupport, Params, VerboseLogging,
                  /*FillValue=*/2, /*OutputSigned=*/true, ComponentType::F32,
                  ComponentType::F32);
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
      Params.Layout == LinalgMatrixLayout::OuterProductOptimal,
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
  Params.Layout = LinalgMatrixLayout::OuterProductOptimal;
  Params.NumThreads = 1;
  Params.Enable16Bit = true;
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

    if (GetGroupWaveIndex() != 0)
      return;

    __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(COMP_TYPE, M_DIM, N_DIM, USE, SCOPE)]]
      Mat;
    __builtin_LinAlg_FillMatrix(Mat, FILL_VALUE);

    __builtin_LinAlg_MatrixAccumulateToMemory(
      Mat, GsData, OFFSET / ELEM_SIZE, STRIDE / ELEM_SIZE, LAYOUT);

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
  runVectorAccumulateDescriptor(D3DDevice, DxcSupport, VerboseLogging);
}

} // namespace LinAlg
