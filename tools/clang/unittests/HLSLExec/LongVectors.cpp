#include "LongVectors.h"
#include "LongVectorTestData.h"

#include "HlslExecTestUtils.h"
#include "TableParameterHandler.h"
#include "dxc/Support/Global.h"

#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace LongVector {

template <typename OpT, size_t Length>
const OpTypeMetaData<OpT> &
getOpType(const OpTypeMetaData<OpT> (&Values)[Length],
          const std::wstring &OpTypeString) {
  for (size_t I = 0; I < Length; I++) {
    if (Values[I].OpTypeString == OpTypeString)
      return Values[I];
  }

  LOG_ERROR_FMT_THROW(L"Invalid OpType string: %ls", OpTypeString.c_str());

  // We need to return something to satisfy the compiler. We can't annotate
  // LOG_ERROR_FMT_THROW with [[noreturn]] because the TAEF VERIFY_* macros that
  // it uses are re-mapped on Unix to not throw exceptions, so they naturally
  // return. If we hit this point it is a programmer error when implementing a
  // test. Specifically, an entry for this OpTypeString is missing in the
  // static OpTypeStringToOpMetaData array. Or something has been
  // corrupted. Test execution is invalid at this point. Usin std::abort() keeps
  // the compiler happy about no return path. And LOG_ERROR_FMT_THROW will still
  // provide a useful error message via gtest logging on Unix systems.
  std::abort();
}

// Helper to fill the test data from the shader buffer based on type. Convenient
// to be used when copying HLSL*_t types so we can use the underlying type.
template <typename T>
void fillLongVectorDataFromShaderBuffer(const MappedData &ShaderBuffer,
                                        std::vector<T> &TestData,
                                        size_t NumElements) {

  if constexpr (std::is_same_v<T, HLSLHalf_t>) {
    auto ShaderBufferPtr =
        static_cast<const DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      // HLSLHalf_t has a DirectX::PackedVector::HALF based constructor.
      TestData.push_back(ShaderBufferPtr[I]);
    return;
  }

  if constexpr (std::is_same_v<T, HLSLBool_t>) {
    auto ShaderBufferPtr = static_cast<const int32_t *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      // HLSLBool_t has a int32_t based constructor.
      TestData.push_back(ShaderBufferPtr[I]);
    return;
  }

  auto ShaderBufferPtr = static_cast<const T *>(ShaderBuffer.data());
  for (size_t I = 0; I < NumElements; I++)
    TestData.push_back(ShaderBufferPtr[I]);
  return;
}

template <typename T>
bool doValuesMatch(T A, T B, float Tolerance, ValidationType) {
  if (Tolerance == 0.0f)
    return A == B;

  T Diff = A > B ? A - B : B - A;
  return Diff <= Tolerance;
}

bool doValuesMatch(HLSLBool_t A, HLSLBool_t B, float, ValidationType) {
  return A == B;
}

bool doValuesMatch(HLSLHalf_t A, HLSLHalf_t B, float Tolerance,
                   ValidationType ValidationType) {
  switch (ValidationType) {
  case ValidationType_Epsilon:
    return CompareHalfEpsilon(A.Val, B.Val, Tolerance);
  case ValidationType_Ulp:
    return CompareHalfULP(A.Val, B.Val, Tolerance);
  default:
    WEX::Logging::Log::Error(
        L"Invalid ValidationType. Expecting Epsilon or ULP.");
    return false;
  }
}

bool doValuesMatch(float A, float B, float Tolerance,
                   ValidationType ValidationType) {
  switch (ValidationType) {
  case ValidationType_Epsilon:
    return CompareFloatEpsilon(A, B, Tolerance);
  case ValidationType_Ulp: {
    // Tolerance is in ULPs. Convert to int for the comparison.
    const int IntTolerance = static_cast<int>(Tolerance);
    return CompareFloatULP(A, B, IntTolerance);
  };
  default:
    WEX::Logging::Log::Error(
        L"Invalid ValidationType. Expecting Epsilon or ULP.");
    return false;
  }
}

bool doValuesMatch(double A, double B, float Tolerance,
                   ValidationType ValidationType) {
  switch (ValidationType) {
  case ValidationType_Epsilon:
    return CompareDoubleEpsilon(A, B, Tolerance);
  case ValidationType_Ulp: {
    // Tolerance is in ULPs. Convert to int64_t for the comparison.
    const int64_t IntTolerance = static_cast<int64_t>(Tolerance);
    return CompareDoubleULP(A, B, IntTolerance);
  };
  default:
    WEX::Logging::Log::Error(
        L"Invalid ValidationType. Expecting Epsilon or ULP.");
    return false;
  }
}

template <typename T>
bool doVectorsMatch(const std::vector<T> &ActualValues,
                    const std::vector<T> &ExpectedValues, float Tolerance,
                    ValidationType ValidationType, bool VerboseLogging) {

  DXASSERT(
      ActualValues.size() == ExpectedValues.size(),
      "Programmer error: Actual and Expected vectors must be the same size.");

  if (VerboseLogging) {
    logLongVector(ActualValues, L"ActualValues");
    logLongVector(ExpectedValues, L"ExpectedValues");
  }

  // Stash mismatched indexes for easy failure logging later
  std::vector<size_t> MismatchedIndexes;
  for (size_t I = 0; I < ActualValues.size(); I++) {
    if (!doValuesMatch(ActualValues[I], ExpectedValues[I], Tolerance,
                       ValidationType))
      MismatchedIndexes.push_back(I);
  }

  if (MismatchedIndexes.empty())
    return true;

  if (!MismatchedIndexes.empty()) {
    for (size_t Index : MismatchedIndexes) {
      std::wstringstream Wss(L"");
      Wss << std::setprecision(15); // Set precision for floating point types
      Wss << L"Mismatch at Index: " << Index;
      Wss << L" Actual Value:" << ActualValues[Index] << ",";
      Wss << L" Expected Value:" << ExpectedValues[Index];
      WEX::Logging::Log::Error(Wss.str().c_str());
    }
  }

  return false;
}

template <typename T>
void logLongVector(const std::vector<T> &Values, const std::wstring &Name) {
  WEX::Logging::Log::Comment(
      WEX::Common::String().Format(L"LongVector Name: %s", Name.c_str()));

  const size_t LoggingWidth = 40;

  std::wstringstream Wss(L"");
  Wss << L"LongVector Values: ";
  Wss << L"[";
  const size_t NumElements = Values.size();
  for (size_t I = 0; I < NumElements; I++) {
    if (I % LoggingWidth == 0 && I != 0)
      Wss << L"\n ";
    Wss << Values[I];
    if (I != NumElements - 1)
      Wss << L", ";
  }
  Wss << L" ]";

  WEX::Logging::Log::Comment(Wss.str().c_str());
}

template <typename T> std::string getHLSLTypeString() {
  if (std::is_same_v<T, HLSLBool_t>)
    return "bool";
  if (std::is_same_v<T, HLSLHalf_t>)
    return "half";
  if (std::is_same_v<T, float>)
    return "float";
  if (std::is_same_v<T, double>)
    return "double";
  if (std::is_same_v<T, int16_t>)
    return "int16_t";
  if (std::is_same_v<T, int32_t>)
    return "int";
  if (std::is_same_v<T, int64_t>)
    return "int64_t";
  if (std::is_same_v<T, uint16_t>)
    return "uint16_t";
  if (std::is_same_v<T, uint32_t>)
    return "uint32_t";
  if (std::is_same_v<T, uint64_t>)
    return "uint64_t";

  LOG_ERROR_FMT_THROW(L"Unsupported type: %S", typeid(T).name());
  return "UnknownType";
}

template <typename T, size_t ARITY>
using InputSets = std::array<std::vector<T>, ARITY>;

template <typename OP_TYPE> struct OpTypeTraits;

template <> struct OpTypeTraits<TrigonometricOpType> {
  static constexpr size_t Arity = 1;

  static TrigonometricOpType GetOpType(const wchar_t *OpTypeString) {
    return getTrigonometricOpType(OpTypeString).OpType;
  }
};

template <> struct OpTypeTraits<UnaryOpType> {
  static constexpr size_t Arity = 1;

  static UnaryOpType GetOpType(const wchar_t *OpTypeString) {
    return getUnaryOpType(OpTypeString).OpType;
  }
};

template <> struct OpTypeTraits<AsTypeOpType> {
  static constexpr size_t Arity = 1;

  static AsTypeOpType GetOpType(const wchar_t *OpTypeString) {
    return getAsTypeOpType(OpTypeString).OpType;
  }
};

template <> struct OpTypeTraits<UnaryMathOpType> {
  static constexpr size_t Arity = 1;

  static UnaryMathOpType GetOpType(const wchar_t *OpTypeString) {
    return getUnaryMathOpType(OpTypeString).OpType;
  }
};

template <> struct OpTypeTraits<BinaryMathOpType> {
  static constexpr size_t Arity = 2;

  static BinaryMathOpType GetOpType(const wchar_t *OpTypeString) {
    return getBinaryMathOpType(OpTypeString).OpType;
  }
};

template <> struct OpTypeTraits<TernaryMathOpType> {
  static constexpr size_t Arity = 3;

  static TernaryMathOpType GetOpType(const wchar_t *OpTypeString) {
    return getTernaryMathOpType(OpTypeString).OpType;
  }
};

static uint16_t GetScalarInputFlags() {
  using WEX::Common::String;
  using WEX::TestExecution::TestData;

  String ScalarInputFlagsString;
  if (FAILED(
          TestData::TryGetValue(L"ScalarInputFlags", ScalarInputFlagsString)))
    return 0;

  if (ScalarInputFlagsString.IsEmpty())
    return 0;

  uint16_t ScalarInputFlags;
  VERIFY_IS_TRUE(IsHexString(ScalarInputFlagsString, &ScalarInputFlags));
  return ScalarInputFlags;
}

static WEX::Common::String getInputValueSetName(size_t Index) {
  using WEX::Common::String;
  using WEX::TestExecution::TestData;

  DXASSERT(Index >= 0 && Index <= 9, "Only single digit indices supported");

  String ParameterName = L"InputValueSetName";
  ParameterName.Append((wchar_t)(L'1' + Index));

  String ValueSetName;
  if (FAILED(TestData::TryGetValue(ParameterName, ValueSetName))) {
    String Name = L"DefaultInputValueSet";
    Name.Append((wchar_t)(L'1' + Index));
    return Name;
  }

  return ValueSetName;
}

//
// asFloat
//

template <typename T> float asFloat(T);
template <> float asFloat(float A) { return float(A); }
template <> float asFloat(int32_t A) { return bit_cast<float>(A); }
template <> float asFloat(uint32_t A) { return bit_cast<float>(A); }

//
// asFloat16
//
template <typename T> HLSLHalf_t asFloat16(T);
template <> HLSLHalf_t asFloat16<HLSLHalf_t>(HLSLHalf_t A) {
  return HLSLHalf_t(A.Val);
}
template <> HLSLHalf_t asFloat16(int16_t A) {
  return HLSLHalf_t(bit_cast<DirectX::PackedVector::HALF>(A));
}
template <> HLSLHalf_t asFloat16(uint16_t A) {
  return HLSLHalf_t(bit_cast<DirectX::PackedVector::HALF>(A));
}

//
// asInt
//

template <typename T> int32_t asInt(T);
template <> int32_t asInt(float A) { return bit_cast<int32_t>(A); }
template <> int32_t asInt(int32_t A) { return A; }
template <> int32_t asInt(uint32_t A) { return bit_cast<int32_t>(A); }

//
// asInt16
//

template <typename T> int16_t asInt16(T);
template <> int16_t asInt16(HLSLHalf_t A) { return bit_cast<int16_t>(A.Val); }
template <> int16_t asInt16(int16_t A) { return A; }
template <> int16_t asInt16(uint16_t A) { return bit_cast<int16_t>(A); }

//
// asUint16
//

template <typename T> uint16_t asUint16(T);
template <> uint16_t asUint16(HLSLHalf_t A) {
  return bit_cast<uint16_t>(A.Val);
}
template <> uint16_t asUint16(uint16_t A) { return A; }
template <> uint16_t asUint16(int16_t A) { return bit_cast<uint16_t>(A); }

//
// asUint
//

template <typename T> unsigned int asUint(T);
template <> unsigned int asUint(unsigned int A) { return A; }
template <> unsigned int asUint(float A) { return bit_cast<unsigned int>(A); }
template <> unsigned int asUint(int A) { return bit_cast<unsigned int>(A); }

//
// splitDouble
//

static void splitDouble(const double A, uint32_t &LowBits, uint32_t &HighBits) {
  uint64_t Bits = 0;
  std::memcpy(&Bits, &A, sizeof(Bits));
  LowBits = static_cast<uint32_t>(Bits & 0xFFFFFFFF);
  HighBits = static_cast<uint32_t>(Bits >> 32);
}

//
// asDouble
//

static double asDouble(const uint32_t LowBits, const uint32_t HighBits) {
  uint64_t Bits = (static_cast<uint64_t>(HighBits) << 32) | LowBits;
  double Result;
  std::memcpy(&Result, &Bits, sizeof(Result));
  return Result;
}

template <typename T> struct TrigonometricOperation {
  static T acos(T Val) { return std::acos(Val); }
  static T asin(T Val) { return std::asin(Val); }
  static T atan(T Val) { return std::atan(Val); }
  static T cos(T Val) { return std::cos(Val); }
  static T cosh(T Val) { return std::cosh(Val); }
  static T sin(T Val) { return std::sin(Val); }
  static T sinh(T Val) { return std::sinh(Val); }
  static T tan(T Val) { return std::tan(Val); }
  static T tanh(T Val) { return std::tanh(Val); }
};

template <typename T> const wchar_t *DataTypeName() {
  static_assert(false && "Missing data type name");
}

#define DATA_TYPE_NAME(TYPE, NAME)                                             \
  template <> const wchar_t *DataTypeName<TYPE>() { return NAME; }

DATA_TYPE_NAME(HLSLBool_t, L"bool");
DATA_TYPE_NAME(int16_t, L"int16");
DATA_TYPE_NAME(int32_t, L"int32");
DATA_TYPE_NAME(int64_t, L"int64");
DATA_TYPE_NAME(uint16_t, L"uint16");
DATA_TYPE_NAME(uint32_t, L"uint32");
DATA_TYPE_NAME(uint64_t, L"uint64");
DATA_TYPE_NAME(HLSLHalf_t, L"float16");
DATA_TYPE_NAME(float, L"float32");
DATA_TYPE_NAME(double, L"float64");

#undef DATA_TYPE_NAME

struct TAEFTestDataValues {
  using String = WEX::Common::String;

  String DataType;
  String OpTypeEnum;
  String InputValueSetNames[3];
  uint16_t ScalarInputFlags = 0;
  size_t LongVectorInputSize = 0;

  static std::optional<TAEFTestDataValues> CreateFromTestData() {
    using WEX::TestExecution::TestData;
    TAEFTestDataValues Values;

    if (FAILED(TestData::TryGetValue(L"DataType", Values.DataType)) &&
        FAILED(TestData::TryGetValue(L"DataTypeIn", Values.DataType))) {
      LOG_ERROR_FMT_THROW(L"TestData missing 'DataType' or 'DataTypeIn'.");
      return std::nullopt;
    }

    if (FAILED(TestData::TryGetValue(L"OpTypeEnum", Values.OpTypeEnum))) {
      LOG_ERROR_FMT_THROW(L"TestData missing 'OpTypeEnum'.");
      return std::nullopt;
    }

    for (size_t I = 0; I < std::size(Values.InputValueSetNames); ++I) {
      Values.InputValueSetNames[I] = getInputValueSetName(I);
    }

    Values.ScalarInputFlags = GetScalarInputFlags();

    TestData::TryGetValue(L"LongVectorInputSize", Values.LongVectorInputSize);

    return Values;
  }
};

template <typename DATA_TYPE>
std::vector<DATA_TYPE> buildTestInput(const wchar_t *InputValueSetName,
                                      size_t SizeToTest) {
  // TODO: remove the need to build up a RawValueSet, only to use that to build
  // ValueSet.
  std::vector<DATA_TYPE> RawValueSet =
      getInputValueSetByKey<DATA_TYPE>(InputValueSetName);

  std::vector<DATA_TYPE> ValueSet;
  ValueSet.reserve(SizeToTest);
  for (size_t I = 0; I < SizeToTest; ++I)
    ValueSet.push_back(RawValueSet[I % RawValueSet.size()]);

  return ValueSet;
}

template <typename T, size_t ARITY>
InputSets<T, ARITY> buildTestInputs(const TAEFTestDataValues &TAEFTestData,
                                    size_t SizeToTest) {
  InputSets<T, ARITY> Inputs;
  for (size_t I = 0; I < ARITY; ++I) {

    uint16_t OperandScalarFlag = 1 << I;
    bool IsOperandScalar = TAEFTestData.ScalarInputFlags & OperandScalarFlag;

    if (TAEFTestData.InputValueSetNames[I].IsEmpty()) {
      LOG_ERROR_FMT_THROW(
          L"Expected parameter 'InputValueSetName%d' not found.", I + 1);
      continue;
    }

    Inputs[I] = buildTestInput<T>(TAEFTestData.InputValueSetNames[I],
                                  IsOperandScalar ? 1 : SizeToTest);
  }

  return Inputs;
}

template <typename OUT_TYPE, typename T, size_t ARITY, typename OP_TYPE>
std::vector<OUT_TYPE>
runTest(bool VerboseLogging, OP_TYPE OpType, const InputSets<T, ARITY> &Inputs,
        size_t ExpectedOutputSize, uint16_t ScalarInputFlags,
        std::string ExtraDefines, bool &WasSkipped);

struct ValidationConfig {
  float Tolerance = 0.0f;
  ValidationType Type = ValidationType_Epsilon;

  static ValidationConfig Epsilon(float Tolerance) {
    return ValidationConfig{Tolerance, ValidationType_Epsilon};
  }

  static ValidationConfig Ulp(float Tolerance) {
    return ValidationConfig{Tolerance, ValidationType_Ulp};
  }
};

template <typename T, typename OUT_TYPE, typename OP_TYPE, size_t ARITY>
void runAndVerify(bool VerboseLogging, OP_TYPE OpType,
                  const InputSets<T, ARITY> &Inputs,
                  const std::vector<OUT_TYPE> &Expected,
                  uint16_t ScalarInputFlags, std::string ExtraDefines,
                  const ValidationConfig &ValidationConfig) {

  bool WasSkipped = true;
  std::vector<OUT_TYPE> Actual =
      runTest<OUT_TYPE>(VerboseLogging, OpType, Inputs, Expected.size(),
                        ScalarInputFlags, ExtraDefines, WasSkipped);
  if (WasSkipped)
    return;

  VERIFY_IS_TRUE(doVectorsMatch(Actual, Expected, ValidationConfig.Tolerance,
                                ValidationConfig.Type, VerboseLogging));
}

template <typename T, typename OUT_TYPE, typename OP_TYPE>
void dispatchUnaryTest(bool VerboseLogging,
                       const TAEFTestDataValues &TAEFTestData,
                       const ValidationConfig &ValidationConfig, OP_TYPE OpType,
                       size_t VectorSize, OUT_TYPE (*Calc)(T),
                       std::string ExtraDefines) {

  InputSets<T, 1> Inputs = buildTestInputs<T, 1>(TAEFTestData, VectorSize);

  std::vector<OUT_TYPE> Expected;
  Expected.reserve(Inputs[0].size());

  for (size_t I = 0; I < Inputs[0].size(); ++I) {
    Expected.push_back(Calc(Inputs[0][I]));
  }

  runAndVerify(VerboseLogging, OpType, Inputs, Expected,
               TAEFTestData.ScalarInputFlags, ExtraDefines, ValidationConfig);
}

template <typename T, typename OUT_TYPE, typename OP_TYPE>
void dispatchBinaryTest(bool VerboseLogging,
                        const TAEFTestDataValues &TAEFTestData,
                        const ValidationConfig &ValidationConfig,
                        OP_TYPE OpType, size_t VectorSize,
                        OUT_TYPE (*Calc)(T, T)) {
  InputSets<T, 2> Inputs = buildTestInputs<T, 2>(TAEFTestData, VectorSize);

  std::vector<OUT_TYPE> Expected;
  Expected.reserve(Inputs[0].size());

  for (size_t I = 0; I < Inputs[0].size(); ++I) {
    size_t Index1 = (TAEFTestData.ScalarInputFlags & (1 << 1)) ? 0 : I;
    Expected.push_back(Calc(Inputs[0][I], Inputs[1][Index1]));
  }

  runAndVerify(VerboseLogging, OpType, Inputs, Expected,
               TAEFTestData.ScalarInputFlags, "", ValidationConfig);
}

//
// TrigonometricTest
//

template <typename T>
void dispatchTrigonometricTest(bool VerboseLogging,
                               const TAEFTestDataValues &TAEFTestData,
                               ValidationConfig ValidationConfig,
                               TrigonometricOpType OpType, size_t VectorSize) {
#define DISPATCH(OP, NAME)                                                     \
  case OP:                                                                     \
    return dispatchUnaryTest<T>(VerboseLogging, TAEFTestData,                  \
                                ValidationConfig, OP, VectorSize,              \
                                TrigonometricOperation<T>::NAME, "")

  switch (OpType) {
    DISPATCH(TrigonometricOpType_Acos, acos);
    DISPATCH(TrigonometricOpType_Asin, asin);
    DISPATCH(TrigonometricOpType_Atan, atan);
    DISPATCH(TrigonometricOpType_Cos, cos);
    DISPATCH(TrigonometricOpType_Cosh, cosh);
    DISPATCH(TrigonometricOpType_Sin, sin);
    DISPATCH(TrigonometricOpType_Sinh, sinh);
    DISPATCH(TrigonometricOpType_Tan, tan);
    DISPATCH(TrigonometricOpType_Tanh, tanh);
  case TrigonometricOpType_EnumValueCount:
    break;
  }

#undef DISPATCH

  LOG_ERROR_FMT_THROW(L"Unexpected TrigonometricOpType: %d.", OpType);
}

void dispatchTest(bool VerboseLogging, const TAEFTestDataValues &TAEFTestData,
                  TrigonometricOpType OpType, size_t VectorSize) {

  // All trigonometric ops are floating point types.
  // These trig functions are defined to have a max absolute error of 0.0008
  // as per the D3D functional specs. An example with this spec for sin and
  // cos is available here:
  // https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#22.10.20

  if (TAEFTestData.DataType == DataTypeName<HLSLHalf_t>())
    return dispatchTrigonometricTest<HLSLHalf_t>(
        VerboseLogging, TAEFTestData, ValidationConfig::Epsilon(0.0010f),
        OpType, VectorSize);

  if (TAEFTestData.DataType == DataTypeName<float>())
    return dispatchTrigonometricTest<float>(VerboseLogging, TAEFTestData,
                                            ValidationConfig::Epsilon(0.0008f),
                                            OpType, VectorSize);

  LOG_ERROR_FMT_THROW(
      L"DataType '%s' not supported for trigonometric operations.",
      (const wchar_t *)TAEFTestData.DataType);
}

//
// AsTypeOp
//

void dispatchAsUintSplitDoubleTest(bool VerboseLogging,
                                   const TAEFTestDataValues &TAEFTestData,
                                   size_t VectorSize) {

  InputSets<double, 1> Inputs =
      buildTestInputs<double, 1>(TAEFTestData, VectorSize);

  std::vector<uint32_t> Expected;
  Expected.resize(Inputs.size() * 2);

  for (size_t I = 0; I < Inputs.size(); ++I) {
    uint32_t Low, High;
    splitDouble(Expected[I], Low, High);
    Expected[I] = Low;
    Expected[I + Inputs.size()] = High;
  }

  ValidationConfig ValidationConfig{};
  runAndVerify(VerboseLogging, AsTypeOpType_AsUint_SplitDouble, Inputs,
               Expected, TAEFTestData.ScalarInputFlags,
               " -DFUNC_ASUINT_SPLITDOUBLE=1", ValidationConfig);
}

void dispatchTest(bool VerboseLogging, const TAEFTestDataValues &TAEFTestData,
                  AsTypeOpType OpType, size_t VectorSize) {

  // Different AsType* operations are supported for different data types, so we
  // dispatch on operation first.

#define DISPATCH(TYPE, FN)                                                     \
  if (TAEFTestData.DataType == DataTypeName<TYPE>())                           \
  return dispatchUnaryTest<TYPE>(VerboseLogging, TAEFTestData,                 \
                                 ValidationConfig{}, OpType, VectorSize,       \
                                 FN<TYPE>, "")

  switch (OpType) {
  case AsTypeOpType_AsFloat:
    DISPATCH(float, asFloat);
    DISPATCH(int32_t, asFloat);
    DISPATCH(uint32_t, asFloat);
    break;

  case AsTypeOpType_AsInt:
    DISPATCH(float, asInt);
    DISPATCH(int32_t, asInt);
    DISPATCH(uint32_t, asInt);
    break;

  case AsTypeOpType_AsUint:
    DISPATCH(int32_t, asUint);
    DISPATCH(uint32_t, asUint);
    break;

  case AsTypeOpType_AsFloat16:
    DISPATCH(HLSLHalf_t, asFloat16);
    DISPATCH(int16_t, asFloat16);
    DISPATCH(uint16_t, asFloat16);
    break;

  case AsTypeOpType_AsInt16:
    DISPATCH(HLSLHalf_t, asInt16);
    DISPATCH(int16_t, asInt16);
    DISPATCH(uint16_t, asInt16);
    break;

  case AsTypeOpType_AsUint16:
    DISPATCH(HLSLHalf_t, asUint16);
    DISPATCH(int16_t, asUint16);
    DISPATCH(uint16_t, asUint16);
    break;

    break;

  case AsTypeOpType_AsUint_SplitDouble:
    if (TAEFTestData.DataType == DataTypeName<double>())
      return dispatchAsUintSplitDoubleTest(VerboseLogging, TAEFTestData,
                                           VectorSize);
    break;

  case AsTypeOpType_AsDouble:
    if (TAEFTestData.DataType == DataTypeName<uint32_t>())
      return dispatchBinaryTest<uint32_t>(
          VerboseLogging, TAEFTestData, ValidationConfig{},
          AsTypeOpType_AsDouble, VectorSize, asDouble);
    break;

  case AsTypeOpType_EnumValueCount:
    break;
  }

#undef DISPATCH

  LOG_ERROR_FMT_THROW(L"DataType '%s' not supported for AsTypeOp '%s'",
                      (const wchar_t *)TAEFTestData.DataType,
                      (const wchar_t *)TAEFTestData.OpTypeEnum);
}

//
// UnaryOp
//

template <typename T> T Initialize(T V) { return V; }

void dispatchTest(bool VerboseLogging, const TAEFTestDataValues &TAEFTestData,
                  UnaryOpType OpType, size_t VectorSize) {
#define DISPATCH(TYPE, FUNC, EXTRA_DEFINES)                                    \
  if (TAEFTestData.DataType == DataTypeName<TYPE>())                           \
  return dispatchUnaryTest(VerboseLogging, TAEFTestData, ValidationConfig{},   \
                           OpType, VectorSize, FUNC, EXTRA_DEFINES)

#define DISPATCH_INITIALIZE(TYPE)                                              \
  DISPATCH(TYPE, Initialize<TYPE>, " -DFUNC_INITIALIZE=1")

  switch (OpType) {
  case UnaryOpType_Initialize:
    DISPATCH_INITIALIZE(HLSLBool_t);
    DISPATCH_INITIALIZE(int16_t);
    DISPATCH_INITIALIZE(int32_t);
    DISPATCH_INITIALIZE(int64_t);
    DISPATCH_INITIALIZE(uint16_t);
    DISPATCH_INITIALIZE(uint32_t);
    DISPATCH_INITIALIZE(uint64_t);
    DISPATCH_INITIALIZE(HLSLHalf_t);
    DISPATCH_INITIALIZE(float);
    DISPATCH_INITIALIZE(double);
    break;
  case UnaryOpType_EnumValueCount:
    break;
  }

#undef DISPATCH_INITIALIZE
#undef DISPATCH

  LOG_ERROR_FMT_THROW(L"DataType '%s' not supported for UnaryOpType '%s'",
                      (const wchar_t *)TAEFTestData.DataType,
                      (const wchar_t *)TAEFTestData.OpTypeEnum);
}

//
// UnaryMathOp
//

template <typename T, typename OUT_TYPE>
void dispatchUnaryMathOpTest(bool VerboseLogging,
                             const TAEFTestDataValues &TAEFTestData,
                             UnaryMathOpType OpType, size_t VectorSize,
                             OUT_TYPE (*Calc)(T)) {

  ValidationConfig ValidationConfig;

  if (isFloatingPointType<T>()) {
    ValidationConfig = ValidationConfig::Ulp(1.0);
  }

  dispatchUnaryTest(VerboseLogging, TAEFTestData, ValidationConfig, OpType,
                    VectorSize, Calc, "");
}

template <typename T> struct UnaryMathOps {
  static T Abs(T V) {
    if constexpr (std::is_unsigned_v<T>)
      return V;
    else
      return static_cast<T>(std::abs(V));
  }

  static int32_t Sign(T V) {
    return V > static_cast<T>(0) ? 1 : (V < static_cast<T>(0) ? -1 : 0);
  }

  static T Ceil(T V) { return std::ceil(V); }
  static T Floor(T V) { return std::floor(V); }
  static T Trunc(T V) { return std::trunc(V); }
  static T Round(T V) { return std::round(V); }
  static T Frac(T V) { return V - static_cast<T>(std::floor(V)); }
  static T Sqrt(T V) { return std::sqrt(V); }

  static T Rsqrt(T V) {
    return static_cast<T>(1.0) / static_cast<T>(std::sqrt(V));
  }

  static T Exp(T V) { return std::exp(V); }
  static T Exp2(T V) { return std::exp2(V); }
  static T Log(T V) { return std::log(V); }
  static T Log2(T V) { return std::log2(V); }
  static T Log10(T V) { return std::log10(V); }
  static T Rcp(T V) { return static_cast<T>(1.0) / V; }
};

void dispatchFrexpTest(bool VerboseLogging,
                       const TAEFTestDataValues &TAEFTestData,
                       size_t VectorSize) {
  // Frexp has a return value as well as an output paramater. So we handle it
  // with special logic. Frexp is only supported for fp32 values.

  InputSets<float, 1> Inputs =
      buildTestInputs<float, 1>(TAEFTestData, VectorSize);

  std::vector<float> Expected;

  // Expected values size is doubled. In the first half we store the Mantissas
  // and in the second half we store the Exponents. This way we can leverage the
  // existing logic which verify expected values in a single vector. We just
  // need to make sure that we organize the output in the same way in the shader
  // and when we read it back.

  Expected.resize(VectorSize * 2);

  for (size_t I = 0; I < VectorSize; ++I) {
    int Exp = 0;
    float Man = std::frexp(Inputs[0][I], &Exp);

    // std::frexp returns a signed mantissa. But the HLSL implmentation returns
    // an unsigned mantissa.
    Man = std::abs(Man);

    Expected[I] = Man;

    // std::frexp returns the exponent as an int, but HLSL stores it as a float.
    // However, the HLSL exponents fractional component is always 0. So it can
    // conversion between float and int is safe.
    Expected[I + VectorSize] = static_cast<float>(Exp);
  }

  runAndVerify(VerboseLogging, UnaryMathOpType_Frexp, Inputs, Expected,
               TAEFTestData.ScalarInputFlags, " -DFUNC_FREXP=1", ValidationConfig{});
}

void dispatchTest(bool VerboseLogging, const TAEFTestDataValues &TAEFTestData,
                  UnaryMathOpType OpType, size_t VectorSize) {
#define DISPATCH(TYPE, FUNC)                                                   \
  if (TAEFTestData.DataType == DataTypeName<TYPE>())                           \
  return dispatchUnaryMathOpTest(VerboseLogging, TAEFTestData, OpType,         \
                                 VectorSize, UnaryMathOps<TYPE>::FUNC)

  switch (OpType) {
  case UnaryMathOpType_Abs:
    DISPATCH(HLSLHalf_t, Abs);
    DISPATCH(float, Abs);
    DISPATCH(double, Abs);
    DISPATCH(int16_t, Abs);
    DISPATCH(int32_t, Abs);
    DISPATCH(int64_t, Abs);
    DISPATCH(uint16_t, Abs);
    DISPATCH(uint32_t, Abs);
    DISPATCH(uint64_t, Abs);
    break;

  case UnaryMathOpType_Sign:
    DISPATCH(HLSLHalf_t, Sign);
    DISPATCH(float, Sign);
    DISPATCH(double, Sign);
    DISPATCH(int16_t, Sign);
    DISPATCH(int32_t, Sign);
    DISPATCH(int64_t, Sign);
    DISPATCH(uint16_t, Sign);
    DISPATCH(uint32_t, Sign);
    DISPATCH(uint64_t, Sign);
    break;

  case UnaryMathOpType_Ceil:
    DISPATCH(HLSLHalf_t, Ceil);
    DISPATCH(float, Ceil);
    break;

  case UnaryMathOpType_Floor:
    DISPATCH(HLSLHalf_t, Floor);
    DISPATCH(float, Floor);
    break;

  case UnaryMathOpType_Trunc:
    DISPATCH(HLSLHalf_t, Trunc);
    DISPATCH(float, Trunc);
    break;

  case UnaryMathOpType_Round:
    DISPATCH(HLSLHalf_t, Round);
    DISPATCH(float, Round);
    break;

  case UnaryMathOpType_Frac:
    DISPATCH(HLSLHalf_t, Frac);
    DISPATCH(float, Frac);
    break;

  case UnaryMathOpType_Sqrt:
    DISPATCH(HLSLHalf_t, Sqrt);
    DISPATCH(float, Sqrt);
    break;

  case UnaryMathOpType_Rsqrt:
    DISPATCH(HLSLHalf_t, Rsqrt);
    DISPATCH(float, Rsqrt);
    break;

  case UnaryMathOpType_Exp:
    DISPATCH(HLSLHalf_t, Exp);
    DISPATCH(float, Exp);
    break;

  case UnaryMathOpType_Exp2:
    DISPATCH(HLSLHalf_t, Exp2);
    DISPATCH(float, Exp2);
    break;

  case UnaryMathOpType_Log:
    DISPATCH(HLSLHalf_t, Log);
    DISPATCH(float, Log);
    break;

  case UnaryMathOpType_Log2:
    DISPATCH(HLSLHalf_t, Log2);
    DISPATCH(float, Log2);
    break;

  case UnaryMathOpType_Log10:
    DISPATCH(HLSLHalf_t, Log10);
    DISPATCH(float, Log10);
    break;

  case UnaryMathOpType_Rcp:
    DISPATCH(HLSLHalf_t, Rcp);
    DISPATCH(float, Rcp);
    break;

  case UnaryMathOpType_Frexp:
    if (TAEFTestData.DataType == DataTypeName<float>())
      return dispatchFrexpTest(VerboseLogging, TAEFTestData, VectorSize);
    break;

  case UnaryMathOpType_EnumValueCount:
    break;
  }

#undef DISPATCH

  LOG_ERROR_FMT_THROW(L"DataType '%s' not supported for UnaryOpType '%s'",
                      (const wchar_t *)TAEFTestData.DataType,
                      (const wchar_t *)TAEFTestData.OpTypeEnum);
}

//
// BinaryMathOp
//

template <typename T, typename OUT_TYPE>
void dispatchBinaryMathOpTest(bool VerboseLogging,
                              const TAEFTestDataValues &TAEFTestData,
                              BinaryMathOpType OpType, size_t VectorSize,
                              OUT_TYPE (*Calc)(T, T)) {

  ValidationConfig ValidationConfig;

  if (isFloatingPointType<T>()) {
    ValidationConfig = ValidationConfig::Ulp(1.0);
  }

  dispatchBinaryTest(VerboseLogging, TAEFTestData, ValidationConfig, OpType,
                     VectorSize, Calc);
}

template <typename T> struct BinaryMathOps {
  static T Multiply(T A, T B) { return A * B; }
  static T Add(T A, T B) { return A + B; }
  static T Subtract(T A, T B) { return A - B; }
  static T Divide(T A, T B) { return A / B; }

  static T FmodModulus(T A, T B) {
    static_assert(isFloatingPointType<T>());
    return std::fmod(A, B);
  }

  static T OperatorModulus(T A, T B) {
    // note: as well as integral types, HLSLHalf_t go through this code path
    return A % B;
  }

  // std::max and std::min are wrapped in () to avoid collisions with the macro
  // defintions for min and max in windows.h

  static T Min(T A, T B) { return (std::min)(A, B); }
  static T Max(T A, T B) { return (std::max)(A, B); }

  static T Ldexp(T A, T B) { return A * static_cast<T>(std::pow(2.0f, B)); }
};

void dispatchTest(bool VerboseLogging, const TAEFTestDataValues &TAEFTestData,
                  BinaryMathOpType OpType, size_t VectorSize) {

#define DISPATCH(TYPE, FUNC)                                                   \
  if (TAEFTestData.DataType == DataTypeName<TYPE>())                           \
  return dispatchBinaryMathOpTest(VerboseLogging, TAEFTestData, OpType,        \
                                  VectorSize, BinaryMathOps<TYPE>::FUNC)

  switch (OpType) {
  case BinaryMathOpType_Multiply:
    DISPATCH(HLSLHalf_t, Multiply);
    DISPATCH(float, Multiply);
    DISPATCH(double, Multiply);
    DISPATCH(int16_t, Multiply);
    DISPATCH(int32_t, Multiply);
    DISPATCH(int64_t, Multiply);
    DISPATCH(uint16_t, Multiply);
    DISPATCH(uint32_t, Multiply);
    DISPATCH(uint64_t, Multiply);
    break;

  case BinaryMathOpType_Add:
    DISPATCH(HLSLBool_t, Add);
    DISPATCH(HLSLHalf_t, Add);
    DISPATCH(float, Add);
    DISPATCH(double, Add);
    DISPATCH(int16_t, Add);
    DISPATCH(int32_t, Add);
    DISPATCH(int64_t, Add);
    DISPATCH(uint16_t, Add);
    DISPATCH(uint32_t, Add);
    DISPATCH(uint64_t, Add);
    break;

  case BinaryMathOpType_Subtract:
    DISPATCH(HLSLBool_t, Subtract);
    DISPATCH(HLSLHalf_t, Subtract);
    DISPATCH(float, Subtract);
    DISPATCH(double, Subtract);
    DISPATCH(int16_t, Subtract);
    DISPATCH(int32_t, Subtract);
    DISPATCH(int64_t, Subtract);
    DISPATCH(uint16_t, Subtract);
    DISPATCH(uint32_t, Subtract);
    DISPATCH(uint64_t, Subtract);
    break;

  case BinaryMathOpType_Divide:
    DISPATCH(HLSLHalf_t, Divide);
    DISPATCH(float, Divide);
    DISPATCH(double, Divide);
    DISPATCH(int16_t, Divide);
    DISPATCH(int32_t, Divide);
    DISPATCH(int64_t, Divide);
    DISPATCH(uint16_t, Divide);
    DISPATCH(uint32_t, Divide);
    DISPATCH(uint64_t, Divide);
    break;

  case BinaryMathOpType_Modulus:
    DISPATCH(HLSLHalf_t, OperatorModulus);
    DISPATCH(float, FmodModulus);
    DISPATCH(int16_t, OperatorModulus);
    DISPATCH(int32_t, OperatorModulus);
    DISPATCH(int64_t, OperatorModulus);
    DISPATCH(uint16_t, OperatorModulus);
    DISPATCH(uint32_t, OperatorModulus);
    DISPATCH(uint64_t, OperatorModulus);
    break;

  case BinaryMathOpType_Min:
    DISPATCH(HLSLHalf_t, Min);
    DISPATCH(float, Min);
    DISPATCH(double, Min);
    DISPATCH(int16_t, Min);
    DISPATCH(int32_t, Min);
    DISPATCH(int64_t, Min);
    DISPATCH(uint16_t, Min);
    DISPATCH(uint32_t, Min);
    DISPATCH(uint64_t, Min);
    break;

  case BinaryMathOpType_Max:
    DISPATCH(HLSLHalf_t, Max);
    DISPATCH(float, Max);
    DISPATCH(double, Max);
    DISPATCH(int16_t, Max);
    DISPATCH(int32_t, Max);
    DISPATCH(int64_t, Max);
    DISPATCH(uint16_t, Max);
    DISPATCH(uint32_t, Max);
    DISPATCH(uint64_t, Max);
    break;

  case BinaryMathOpType_Ldexp:
    DISPATCH(HLSLHalf_t, Ldexp);
    DISPATCH(float, Ldexp);
    break;

  case BinaryMathOpType_EnumValueCount:
    break;
  }

#undef DISPATCH

  LOG_ERROR_FMT_THROW(L"DataType '%s' not supported for BinaryMathOpType '%s'",
                      (const wchar_t *)TAEFTestData.DataType,
                      (const wchar_t *)TAEFTestData.OpTypeEnum);
}

//
// TernaryMathOp
//

template <typename T, typename OUT_TYPE>
void dispatchTernaryMathOpTest(bool VerboseLogging,
                               const TAEFTestDataValues &TAEFTestData,
                               TernaryMathOpType OpType, size_t VectorSize,
                               OUT_TYPE (*Calc)(T, T, T)) {

  ValidationConfig ValidationConfig;

  if (isFloatingPointType<T>()) {
    ValidationConfig = ValidationConfig::Ulp(1.0);
  }

  InputSets<T, 3> Inputs = buildTestInputs<T, 3>(TAEFTestData, VectorSize);

  std::vector<OUT_TYPE> Expected;
  Expected.reserve(Inputs[0].size());

  for (size_t I = 0; I < Inputs[0].size(); ++I) {
    size_t Index1 = (TAEFTestData.ScalarInputFlags & (1 << 1)) ? 0 : I;
    size_t Index2 = (TAEFTestData.ScalarInputFlags & (1 << 2)) ? 0 : I;
    Expected.push_back(
        Calc(Inputs[0][I], Inputs[1][Index1], Inputs[2][Index2]));
  }

  runAndVerify(VerboseLogging, OpType, Inputs, Expected,
               TAEFTestData.ScalarInputFlags, "", ValidationConfig);
}

namespace TernaryMathOps {

template <typename T> T Fma(T, T, T);
template <> double Fma(double A, double B, double C) { return A * B + C; }

template <typename T> T Mad(T A, T B, T C) { return A * B + C; }

template <typename T> T SmoothStep(T Min, T Max, T X) {
  DXASSERT_NOMSG(Min < Max);

  if (X <= Min)
    return T(0);
  if (X >= Max)
    return T(1);

  T NormalizedX = (X - Min) / (Max - Min);
  NormalizedX = std::clamp(NormalizedX, T(0), T(1));
  return NormalizedX * NormalizedX * (T(3) - T(2) * NormalizedX);
}

} // namespace TernaryMathOps

void dispatchTest(bool VerboseLogging, const TAEFTestDataValues &TAEFTestData,
                  TernaryMathOpType OpType, size_t VectorSize) {

#define DISPATCH(TYPE, FUNC)                                                   \
  if (TAEFTestData.DataType == DataTypeName<TYPE>())                           \
  return dispatchTernaryMathOpTest(VerboseLogging, TAEFTestData, OpType,       \
                                   VectorSize, TernaryMathOps::FUNC<TYPE>)

  switch (OpType) {
  case TernaryMathOpType_Fma:
    DISPATCH(double, Fma);
    break;

  case TernaryMathOpType_Mad:
    DISPATCH(HLSLHalf_t, Mad);
    DISPATCH(float, Mad);
    DISPATCH(double, Mad);
    DISPATCH(int16_t, Mad);
    DISPATCH(int32_t, Mad);
    DISPATCH(int64_t, Mad);
    DISPATCH(uint16_t, Mad);
    DISPATCH(uint32_t, Mad);
    DISPATCH(uint64_t, Mad);
    break;

  case TernaryMathOpType_SmoothStep:
    DISPATCH(HLSLHalf_t, SmoothStep);
    DISPATCH(double, SmoothStep);
    break;

  case TernaryMathOpType_EnumValueCount:
    break;
  }

  LOG_ERROR_FMT_THROW(L"DataType '%s' not supported for TernaryMathOpType '%s'",
                      (const wchar_t *)TAEFTestData.DataType,
                      (const wchar_t *)TAEFTestData.OpTypeEnum);
} // namespace TernaryMathOps

//
//
//
template <typename OP_TYPE> void dispatchTest(bool VerboseLogging) {
  std::optional<TAEFTestDataValues> TAEFTestData =
      TAEFTestDataValues::CreateFromTestData();
  if (!TAEFTestData)
    return;

  OP_TYPE OpType = OpTypeTraits<OP_TYPE>::GetOpType(TAEFTestData->OpTypeEnum);

  std::vector<size_t> InputVectorSizes;
  if (TAEFTestData->LongVectorInputSize)
    InputVectorSizes.push_back(TAEFTestData->LongVectorInputSize);
  else
    InputVectorSizes = {3, 4, 5, 16, 17, 35, 100, 256, 1024};

  for (size_t VectorSize : InputVectorSizes) {
    dispatchTest(VerboseLogging, *TAEFTestData, OpType, VectorSize);
  }
}

// These are helper arrays to be used with the TableParameterHandler that parses
// the LongVectorOpTable.xml file for us.
static TableParameter UnaryOpParameters[] = {
    {L"DataType", TableParameter::STRING, true},
    {L"OpTypeEnum", TableParameter::STRING, true},
    {L"InputValueSetName1", TableParameter::STRING, false},
};

static TableParameter BinaryOpParameters[] = {
    {L"DataType", TableParameter::STRING, true},
    {L"OpTypeEnum", TableParameter::STRING, true},
    {L"InputValueSetName1", TableParameter::STRING, false},
    {L"InputValueSetName2", TableParameter::STRING, false},
    {L"ScalarInputFlags", TableParameter::STRING, false},
};

static TableParameter TernaryOpParameters[] = {
    {L"DataType", TableParameter::STRING, true},
    {L"OpTypeEnum", TableParameter::STRING, true},
    {L"InputValueSetName1", TableParameter::STRING, false},
    {L"InputValueSetName2", TableParameter::STRING, false},
    {L"InputValueSetName3", TableParameter::STRING, false},
    {L"ScalarInputFlags", TableParameter::STRING, false},
};

static TableParameter AsTypeOpParameters[] = {
    // DataTypeOut is determined at runtime based on the OpType.
    // For example...AsUint has an output type of uint32_t.
    {L"DataTypeIn", TableParameter::STRING, true},
    {L"OpTypeEnum", TableParameter::STRING, true},
    {L"InputValueSetName1", TableParameter::STRING, false},
    {L"InputValueSetName2", TableParameter::STRING, false},
    {L"ScalarInputFlags", TableParameter::STRING, false},
};

bool OpTest::classSetup() {
  // Run this only once.
  if (!Initialized) {
    Initialized = true;

    HMODULE Runtime = LoadLibraryW(L"d3d12.dll");
    if (Runtime == NULL)
      return false;
    // Do not: FreeLibrary(hRuntime);
    // If we actually free the library, it defeats the purpose of
    // enableAgilitySDK and enableExperimentalMode.

    HRESULT HR;
    HR = enableAgilitySDK(Runtime);

    if (FAILED(HR))
      hlsl_test::LogCommentFmt(L"Unable to enable Agility SDK - 0x%08x.", HR);
    else if (HR == S_FALSE)
      hlsl_test::LogCommentFmt(L"Agility SDK not enabled.");
    else
      hlsl_test::LogCommentFmt(L"Agility SDK enabled.");

    HR = enableExperimentalMode(Runtime);
    if (FAILED(HR))
      hlsl_test::LogCommentFmt(
          L"Unable to enable shader experimental mode - 0x%08x.", HR);
    else if (HR == S_FALSE)
      hlsl_test::LogCommentFmt(L"Experimental mode not enabled.");
    else
      hlsl_test::LogCommentFmt(L"Experimental mode enabled.");

    HR = enableDebugLayer();
    if (FAILED(HR))
      hlsl_test::LogCommentFmt(L"Unable to enable debug layer - 0x%08x.", HR);
    else if (HR == S_FALSE)
      hlsl_test::LogCommentFmt(L"Debug layer not enabled.");
    else
      hlsl_test::LogCommentFmt(L"Debug layer enabled.");

    WEX::TestExecution::RuntimeParameters::TryGetValue(L"VerboseLogging",
                                                       VerboseLogging);
    if (VerboseLogging)
      WEX::Logging::Log::Comment(L"Verbose logging is enabled for this test.");
    else
      WEX::Logging::Log::Comment(L"Verbose logging is disabled for this test.");
  }

  return true;
}

TEST_F(OpTest, trigonometricOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  dispatchTest<TrigonometricOpType>(VerboseLogging);
}

TEST_F(OpTest, unaryOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  dispatchTest<UnaryOpType>(VerboseLogging);
}

TEST_F(OpTest, asTypeOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  dispatchTest<AsTypeOpType>(VerboseLogging);
}

TEST_F(OpTest, unaryMathOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  dispatchTest<UnaryMathOpType>(VerboseLogging);
}

TEST_F(OpTest, binaryMathOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  dispatchTest<BinaryMathOpType>(VerboseLogging);
}

TEST_F(OpTest, ternaryMathOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  dispatchTest<TernaryMathOpType>(VerboseLogging);
}

// Helper to fill the shader buffer based on type. Convenient to be used when
// copying HLSL*_t types so we can copy the underlying type directly instead of
// the struct.
template <typename T>
void fillShaderBufferFromLongVectorData(std::vector<BYTE> &ShaderBuffer,
                                        const std::vector<T> &TestData) {

  // Note: DataSize for HLSLHalf_t and HLSLBool_t may be larger than the
  // underlying type in some cases. Thats fine. Resize just makes sure we have
  // enough space.
  const size_t NumElements = TestData.size();
  const size_t DataSize = sizeof(T) * NumElements;
  ShaderBuffer.resize(DataSize);

  if constexpr (std::is_same_v<T, HLSLHalf_t>) {
    auto ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      ShaderBufferPtr[I] = TestData[I].Val;
    return;
  }

  if constexpr (std::is_same_v<T, HLSLBool_t>) {
    auto ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      ShaderBufferPtr[I] = TestData[I].Val;
    return;
  }

  auto ShaderBufferPtr = reinterpret_cast<T *>(ShaderBuffer.data());
  for (size_t I = 0; I < NumElements; I++)
    ShaderBufferPtr[I] = TestData[I];
  return;
}

template <typename T, typename OUT_TYPE, size_t ARITY, typename OP_TYPE>
std::string getCompilerOptionsString(OP_TYPE OpType, size_t VectorSize,
                                     uint16_t ScalarInputFlags,
                                     std::string ExtraDefines) {
  OpTypeMetaData<OP_TYPE> OpTypeMetaData = getOpTypeMetaData(OpType);

  std::stringstream CompilerOptions;

  if (is16BitType<T>())
    CompilerOptions << " -enable-16bit-types";

  CompilerOptions << " -DTYPE=" << getHLSLTypeString<T>();
  CompilerOptions << " -DNUM=" << VectorSize;

  CompilerOptions << " -DOPERATOR=";
  if (OpTypeMetaData.Operator)
    CompilerOptions << *OpTypeMetaData.Operator;

  CompilerOptions << " -DFUNC=";
  if (OpTypeMetaData.Intrinsic)
    CompilerOptions << *OpTypeMetaData.Intrinsic;

  // For most of the ops this string is std::nullopt.
  if (!ExtraDefines.empty())
    CompilerOptions << " " << ExtraDefines;

  CompilerOptions << " -DOUT_TYPE=" << getHLSLTypeString<OUT_TYPE>();

  CompilerOptions << " -DBASIC_OP_TYPE=0x" << std::hex << ARITY;

  CompilerOptions << " -DOPERAND_IS_SCALAR_FLAGS=";
  CompilerOptions << "0x" << std::hex << ScalarInputFlags;

  return CompilerOptions.str();
}

template <typename OUT_TYPE, typename T, size_t ARITY, typename OP_TYPE>
std::vector<OUT_TYPE>
runTest(bool VerboseLogging, OP_TYPE OpType, const InputSets<T, ARITY> &Inputs,
        size_t ExpectedOutputSize, uint16_t ScalarInputFlags,
        std::string ExtraDefines, bool &WasSkipped) {

  CComPtr<ID3D12Device> D3DDevice;
  if (!createDevice(&D3DDevice, ExecTestUtils::D3D_SHADER_MODEL_6_9, false)) {
#ifdef _HLK_CONF
    LOG_ERROR_FMT_THROW(
        L"Device does not support SM 6.9. Can't run these tests.");
#else
    WEX::Logging::Log::Comment(
        "Device does not support SM 6.9. Can't run these tests.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    WasSkipped = true;
    return {};
#endif
  }

  if (VerboseLogging) {
    for (size_t I = 0; I < ARITY; ++I) {
      std::wstring Name = L"InputVector";
      Name += (wchar_t)(L'1' + I);
      logLongVector(Inputs[I], Name);
    }
  }

  // We have to construct the string outside of the lambda. Otherwise it's
  // cleaned up when the lambda finishes executing but before the shader runs.
  std::string CompilerOptionsString =
      getCompilerOptionsString<T, OUT_TYPE, ARITY>(
          OpType, Inputs[0].size(), ScalarInputFlags, std::move(ExtraDefines));

  dxc::SpecificDllLoader DxilDllLoader;

  // The name of the shader we want to use in ShaderOpArith.xml. Could also add
  // logic to set this name in ShaderOpArithTable.xml so we can use different
  // shaders for different tests.
  LPCSTR ShaderName = "LongVectorOp";
  // ShaderOpArith.xml defines the input/output resources and the shader source.
  CComPtr<IStream> TestXML;
  readHlslDataIntoNewStream(L"ShaderOpArith.xml", &TestXML, DxilDllLoader);

  // RunShaderOpTest is a helper function that handles resource creation
  // and setup. It also handles the shader compilation and execution. It takes a
  // callback that is called when the shader is compiled, but before it is
  // executed.
  std::shared_ptr<st::ShaderOpTestResult> TestResult = st::RunShaderOpTest(
      D3DDevice, DxilDllLoader, TestXML, ShaderName,
      [&](LPCSTR Name, std::vector<BYTE> &ShaderData, st::ShaderOp *ShaderOp) {
        if (VerboseLogging)
          hlsl_test::LogCommentFmt(
              L"RunShaderOpTest CallBack. Resource Name: %S", Name);

        // This callback is called once for each resource defined for
        // "LongVectorOp" in ShaderOpArith.xml. All callbacks are fired for each
        // resource. We determine whether they are applicable to the test case
        // when they run.

        // Process the callback for the OutputVector resource.
        if (_stricmp(Name, "OutputVector") == 0) {
          // We only need to set the compiler options string once. So this is a
          // convenient place to do it.
          ShaderOp->Shaders.at(0).Arguments = CompilerOptionsString.c_str();

          return;
        }

        for (size_t I = 0; I < 3; ++I) {
          std::string BufferName = "InputVector";
          BufferName += (char)('1' + I);
          if (_stricmp(Name, BufferName.c_str()) == 0) {
            if (I < ARITY)
              fillShaderBufferFromLongVectorData(ShaderData, Inputs[I]);
            return;
          }
        }

        LOG_ERROR_FMT_THROW(
            L"RunShaderOpTest CallBack. Unexpected Resource Name: %S", Name);
      });

  // Extract the data from the shader result
  MappedData ShaderOutData;
  TestResult->Test->GetReadBackData("OutputVector", &ShaderOutData);

  std::vector<OUT_TYPE> OutData;
  fillLongVectorDataFromShaderBuffer(ShaderOutData, OutData,
                                     ExpectedOutputSize);

  WasSkipped = false;
  return OutData;
}

} // namespace LongVector