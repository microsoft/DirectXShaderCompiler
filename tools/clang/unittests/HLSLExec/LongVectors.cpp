#include "LongVectors.h"
#include "HlslExecTestUtils.h"
#include "LongVectorTestData.h"
#include <iomanip>
#include <type_traits>

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

// A helper to create and fill the expected vector with computed values.
// Also helps us factor out the generic fill loop via a passed in ComputeFn.
template <typename T, typename ComputeFnT>
VariantVector generateExpectedVector(size_t Count, ComputeFnT ComputeFn) {

  std::vector<T> Values;

  for (size_t Index = 0; Index < Count; ++Index)
    Values.push_back(ComputeFn(Index));

  return Values;
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
std::vector<OUT_TYPE> runTest(OP_TYPE OpType, const InputSets<T, ARITY> &Inputs,
                              size_t ExpectedOutputSize,
                              uint16_t ScalarInputFlags,
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
void runAndVerify(OP_TYPE OpType, const InputSets<T, ARITY> &Inputs,
                  const std::vector<OUT_TYPE> &Expected,
                  uint16_t ScalarInputFlags, std::string ExtraDefines,
                  const ValidationConfig &ValidationConfig) {

  bool WasSkipped = true;
  std::vector<OUT_TYPE> Actual =
      runTest<OUT_TYPE>(OpType, Inputs, Expected.size(), ScalarInputFlags,
                        ExtraDefines, WasSkipped);
  if (WasSkipped)
    return;

  bool VerboseLogging = false;
  VERIFY_IS_TRUE(doVectorsMatch(Actual, Expected, ValidationConfig.Tolerance,
                                ValidationConfig.Type, VerboseLogging));
}

template <typename T, typename OUT_TYPE, typename OP_TYPE>
void dispatchUnaryTest(const TAEFTestDataValues &TAEFTestData,
                       const ValidationConfig &ValidationConfig, OP_TYPE OpType,
                       size_t VectorSize, OUT_TYPE (*Calc)(T),
                       std::string ExtraDefines) {

  InputSets<T, 1> Inputs = buildTestInputs<T, 1>(TAEFTestData, VectorSize);

  std::vector<OUT_TYPE> Expected;
  Expected.reserve(Inputs[0].size());

  for (size_t I = 0; I < Inputs[0].size(); ++I) {
    Expected.push_back(Calc(Inputs[0][I]));
  }

  runAndVerify(OpType, Inputs, Expected, TAEFTestData.ScalarInputFlags,
               ExtraDefines, ValidationConfig);
}

template <typename T, typename OUT_TYPE, typename OP_TYPE>
void dispatchBinaryTest(const TAEFTestDataValues &TAEFTestData,
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

  runAndVerify(OpType, Inputs, Expected, TAEFTestData.ScalarInputFlags, "",
               ValidationConfig);
}

//
// TrigonometricTest
//

template <typename T>
void dispatchTrigonometricTest(const TAEFTestDataValues &TAEFTestData,
                               ValidationConfig ValidationConfig,
                               TrigonometricOpType OpType, size_t VectorSize) {
#define DISPATCH(OP, NAME)                                                     \
  case OP:                                                                     \
    return dispatchUnaryTest<T>(TAEFTestData, ValidationConfig, OP,            \
                                VectorSize, TrigonometricOperation<T>::NAME,   \
                                "")

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

void dispatchTest(const TAEFTestDataValues &TAEFTestData,
                  TrigonometricOpType OpType, size_t VectorSize) {

  // All trigonometric ops are floating point types.
  // These trig functions are defined to have a max absolute error of 0.0008
  // as per the D3D functional specs. An example with this spec for sin and
  // cos is available here:
  // https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#22.10.20

  if (TAEFTestData.DataType == DataTypeName<HLSLHalf_t>())
    return dispatchTrigonometricTest<HLSLHalf_t>(
        TAEFTestData, ValidationConfig::Epsilon(0.0010f), OpType, VectorSize);

  if (TAEFTestData.DataType == DataTypeName<float>())
    return dispatchTrigonometricTest<float>(
        TAEFTestData, ValidationConfig::Epsilon(0.0008f), OpType, VectorSize);

  LOG_ERROR_FMT_THROW(
      L"DataType '%s' not supported for trigonometric operations.",
      (const wchar_t *)TAEFTestData.DataType);
}

//
// AsTypeOp
//

void dispatchAsUintSplitDoubleTest(const TAEFTestDataValues &TAEFTestData,
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
  runAndVerify(AsTypeOpType_AsUint_SplitDouble, Inputs, Expected,
               TAEFTestData.ScalarInputFlags, " -DFUNC_ASUINT_SPLITDOUBLE=1",
               ValidationConfig);
}

void dispatchTest(const TAEFTestDataValues &TAEFTestData, AsTypeOpType OpType,
                  size_t VectorSize) {

  // Different AsType* operations are supported for different data types, so we
  // dispatch on operation first.

#define DISPATCH(TYPE, FN)                                                     \
  if (TAEFTestData.DataType == DataTypeName<TYPE>())                           \
  return dispatchUnaryTest<TYPE>(TAEFTestData, ValidationConfig{}, OpType,     \
                                 VectorSize, FN<TYPE>, "")

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
      return dispatchAsUintSplitDoubleTest(TAEFTestData, VectorSize);
    break;

  case AsTypeOpType_AsDouble:
    if (TAEFTestData.DataType == DataTypeName<uint32_t>())
      return dispatchBinaryTest<uint32_t>(TAEFTestData, ValidationConfig{},
                                          AsTypeOpType_AsDouble, VectorSize,
                                          asDouble);
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

void dispatchTest(const TAEFTestDataValues &TAEFTestData, UnaryOpType OpType,
                  size_t VectorSize) {
#define DISPATCH(TYPE, FUNC, EXTRA_DEFINES)                                    \
  if (TAEFTestData.DataType == DataTypeName<TYPE>())                           \
  return dispatchUnaryTest(TAEFTestData, ValidationConfig{}, OpType,           \
                           VectorSize, FUNC, EXTRA_DEFINES)

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
void dispatchUnaryMathOpTest(const TAEFTestDataValues &TAEFTestData,
                             UnaryMathOpType OpType, size_t VectorSize,
                             OUT_TYPE (*Calc)(T)) {

  ValidationConfig ValidationConfig;

  if (isFloatingPointType<T>()) {
    ValidationConfig = ValidationConfig::Ulp(1.0);
  }

  dispatchUnaryTest(TAEFTestData, ValidationConfig, OpType, VectorSize, Calc,
                    "");
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

void dispatchFrexpTest(const TAEFTestDataValues &TAEFTestData,
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
}

void dispatchTest(const TAEFTestDataValues &TAEFTestData,
                  UnaryMathOpType OpType, size_t VectorSize) {
#define DISPATCH(TYPE, FUNC)                                                   \
  if (TAEFTestData.DataType == DataTypeName<TYPE>())                           \
  return dispatchUnaryMathOpTest(TAEFTestData, OpType, VectorSize,             \
                                 UnaryMathOps<TYPE>::FUNC)

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
      return dispatchFrexpTest(TAEFTestData, VectorSize);
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
void dispatchBinaryMathOpTest(const TAEFTestDataValues &TAEFTestData,
                              BinaryMathOpType OpType, size_t VectorSize,
                              OUT_TYPE (*Calc)(T, T)) {

  ValidationConfig ValidationConfig;

  if (isFloatingPointType<T>()) {
    ValidationConfig = ValidationConfig::Ulp(1.0);
  }

  dispatchBinaryTest(TAEFTestData, ValidationConfig, OpType, VectorSize, Calc);
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

void dispatchTest(const TAEFTestDataValues &TAEFTestData,
                  BinaryMathOpType OpType, size_t VectorSize) {

#define DISPATCH(TYPE, FUNC)                                                   \
  if (TAEFTestData.DataType == DataTypeName<TYPE>())                           \
  return dispatchBinaryMathOpTest(TAEFTestData, OpType, VectorSize,            \
                                  BinaryMathOps<TYPE>::FUNC)

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
//
//
template <typename OP_TYPE> void dispatchTest() {
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
    dispatchTest(*TAEFTestData, OpType, VectorSize);
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

  dispatchTest<TrigonometricOpType>();
}

TEST_F(OpTest, unaryOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  dispatchTest<UnaryOpType>();
}

TEST_F(OpTest, asTypeOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  dispatchTest<AsTypeOpType>();
}

TEST_F(OpTest, unaryMathOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  dispatchTest<UnaryMathOpType>();
}

TEST_F(OpTest, binaryMathOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  dispatchTest<BinaryMathOpType>();
}

TEST_F(OpTest, ternaryMathOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  // dispatchTest<TernaryMathOpType>();
}

// Generic dispatch that dispatchs all DataTypes recognized in these tests
template <typename OpT>
void OpTest::dispatchTestByDataType(const OpTypeMetaData<OpT> &OpTypeMd,
                                    std::wstring DataType,
                                    TableParameterHandler &Handler) {
  switch (Hash_djb2a(DataType)) {
  case Hash_djb2a(L"bool"):
    dispatchTestByVectorLength<HLSLBool_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"int16"):
    dispatchTestByVectorLength<int16_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"int32"):
    dispatchTestByVectorLength<int32_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"int64"):
    dispatchTestByVectorLength<int64_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint16"):
    dispatchTestByVectorLength<uint16_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint32"):
    dispatchTestByVectorLength<uint32_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint64"):
    dispatchTestByVectorLength<uint64_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float16"):
    dispatchTestByVectorLength<HLSLHalf_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float32"):
    dispatchTestByVectorLength<float>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float64"):
    dispatchTestByVectorLength<double>(OpTypeMd, Handler);
    return;
  default:
    LOG_ERROR_FMT_THROW(L"Unrecognized DataType: %ls for OpType: %ls.",
                        DataType.c_str(), OpTypeMd.OpTypeString.c_str());
  }
}

// Unary math ops don't support HLSLBool_t. If we included a dispatcher for
// them by allowing the generic dispatchTestByDataType then we would get
// compile errors for a bunch of the templated std lib functions we call to
// compute unary math ops. This is easier and cleaner than guarding against in
// at that point.
void OpTest::dispatchUnaryMathOpTestByDataType(
    const OpTypeMetaData<UnaryMathOpType> &OpTypeMd, std::wstring DataType,
    TableParameterHandler &Handler) {

  switch (Hash_djb2a(DataType)) {
  case Hash_djb2a(L"int16"):
    dispatchTestByVectorLength<int16_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"int32"):
    dispatchTestByVectorLength<int32_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"int64"):
    dispatchTestByVectorLength<int64_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint16"):
    dispatchTestByVectorLength<uint16_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint32"):
    dispatchTestByVectorLength<uint32_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint64"):
    dispatchTestByVectorLength<uint64_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float16"):
    dispatchTestByVectorLength<HLSLHalf_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float32"):
    dispatchTestByVectorLength<float>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float64"):
    dispatchTestByVectorLength<double>(OpTypeMd, Handler);
    return;
  default:
    LOG_ERROR_FMT_THROW(L"Invalid UnaryMathOpType DataType: %ls.",
                        DataType.c_str());
  }
}

// Specialized dispatch for Trigonometric op tests (tan, sin, etc)
// Trig ops only support fp16, fp32, and fp64. So we don't want to
// to generate code paths for any other types. Emit a runtime error via
// LOG_ERROR_FMT_THROW if someone accidentally trys to add support for
// a different DataType.
void OpTest::dispatchTrigonometricOpTestByDataType(
    const OpTypeMetaData<TrigonometricOpType> &OpTypeMd, std::wstring DataType,
    TableParameterHandler &Handler) {

  if (DataType == L"float16")
    dispatchTestByVectorLength<HLSLHalf_t>(OpTypeMd, Handler);
  else if (DataType == L"float32")
    dispatchTestByVectorLength<float>(OpTypeMd, Handler);
  else if (DataType == L"float64")
    dispatchTestByVectorLength<double>(OpTypeMd, Handler);
  else
    LOG_ERROR_FMT_THROW(
        L"Trigonometric ops are only supported for floating point types. "
        L"DataType: %ls is not recognized.",
        DataType.c_str());
}

template <typename T, typename OpT>
void OpTest::dispatchTestByVectorLength(const OpTypeMetaData<OpT> &OpTypeMd,
                                        TableParameterHandler &Handler) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  auto TestConfig = makeTestConfig<T>(OpTypeMd);
  TestConfig->setVerboseLogging(VerboseLogging);
  auto OperandCount = TestConfig->getNumOperands();

  std::wstring Name = L"InputValueSetName";
  for (size_t I = 0; I < OperandCount; I++) {
    auto NameI = Name + std::to_wstring(I + 1);
    std::wstring InputValueSetName(
        Handler.GetTableParamByName(NameI.c_str())->m_str);
    if (!InputValueSetName.empty())
      TestConfig->setInputValueSetKey(InputValueSetName, I);
  }

  // Manual override to test a specific vector size. Convenient for debugging
  // issues.
  size_t InputSizeToTestOverride = 0;
  WEX::TestExecution::RuntimeParameters::TryGetValue(L"LongVectorInputSize",
                                                     InputSizeToTestOverride);

  std::vector<size_t> InputVectorSizes;
  if (InputSizeToTestOverride)
    InputVectorSizes.push_back(InputSizeToTestOverride);
  else
    InputVectorSizes = {3, 4, 5, 16, 17, 35, 100, 256, 1024};

  for (auto SizeToTest : InputVectorSizes) {
    // We could create a new config for each test case with the new length, but
    // that feels wasteful. Instead, we just update the length to test.
    TestConfig->setLengthToTest(SizeToTest);
    testBaseMethod<T>(TestConfig);
  }
}

template <typename T>
void OpTest::testBaseMethod(std::unique_ptr<TestConfig<T>> &TestConfig) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  CComPtr<ID3D12Device> D3DDevice;
  if (!createDevice(&D3DDevice, ExecTestUtils::D3D_SHADER_MODEL_6_9, false)) {
#ifdef _HLK_CONF
    LOG_ERROR_FMT_THROW(
        L"Device does not support SM 6.9. Can't run these tests.");
#else
    WEX::Logging::Log::Comment(
        "Device does not support SM 6.9. Can't run these tests.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
#endif
  }

  TestInputs<T> Inputs = TestInputs<T>();
  TestConfig->fillInputs(Inputs);

  TestConfig->computeExpectedValues(Inputs);

  if (VerboseLogging) {
    logLongVector(Inputs.InputVector1, L"InputVector1");
    if (Inputs.InputVector2.has_value())
      logLongVector(Inputs.InputVector2.value(), L"InputVector2");
    if (Inputs.InputVector3.has_value())
      logLongVector(Inputs.InputVector3.value(), L"InputVector3");
  }

  // We have to construct the string outside of the lambda. Otherwise it's
  // cleaned up when the lambda finishes executing but before the shader runs.
  std::string CompilerOptionsString = TestConfig->getCompilerOptionsString();

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
        hlsl_test::LogCommentFmt(L"RunShaderOpTest CallBack. Resource Name: %S",
                                 Name);

        // This callback is called once for each resource defined for
        // "LongVectorOp" in ShaderOpArith.xml. All callbacks are fired for each
        // resource. We determine whether they are applicable to the test case
        // when they run.

        // Process the callback for the OutputVector resource.
        if (0 == _stricmp(Name, "OutputVector")) {
          // We only need to set the compiler options string once. So this is a
          // convenient place to do it.
          ShaderOp->Shaders.at(0).Arguments = CompilerOptionsString.c_str();

          return;
        }

        // Process the callback for the InputVector1 resource.
        if (0 == _stricmp(Name, "InputVector1")) {
          fillShaderBufferFromLongVectorData(ShaderData, Inputs.InputVector1);
          return;
        }

        // Process the callback for the InputVector2 resource.
        if (0 == _stricmp(Name, "InputVector2")) {
          if (Inputs.InputVector2.has_value())
            fillShaderBufferFromLongVectorData(ShaderData,
                                               Inputs.InputVector2.value());
          return;
        }

        // Process the callback for the InputVector3 resource.
        if (0 == _stricmp(Name, "InputVector3")) {
          if (Inputs.InputVector3.has_value())
            fillShaderBufferFromLongVectorData(ShaderData,
                                               Inputs.InputVector3.value());
          return;
        }

        LOG_ERROR_FMT_THROW(
            L"RunShaderOpTest CallBack. Unexpected Resource Name: %S", Name);
      });

  // The TestConfig object handles the logic for extracting the shader output
  // based on the op type.
  VERIFY_SUCCEEDED(TestConfig->verifyOutput(TestResult));
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

// Returns the compiler options string to be used for the shader compilation.
// Reference ShaderOpArith.xml and the 'LongVectorOp' shader source to see how
// the defines are used in the shader code.
template <typename T>
std::string TestConfig<T>::getCompilerOptionsString() const {

  std::stringstream CompilerOptions("");

  if (is16BitType<T>())
    CompilerOptions << " -enable-16bit-types";

  CompilerOptions << " -DTYPE=" << getHLSLInputTypeString();
  CompilerOptions << " -DNUM=" << LengthToTest;

  CompilerOptions << " -DOPERATOR=";
  if (Operator)
    CompilerOptions << *Operator;

  CompilerOptions << " -DFUNC=";
  if (Intrinsic)
    CompilerOptions << *Intrinsic;

  // For most of the ops this string is std::nullopt.
  if (SpecialDefines)
    CompilerOptions << " " << *SpecialDefines;

  CompilerOptions << " -DOUT_TYPE=" << getHLSLOutputTypeString();

  CompilerOptions << " -DBASIC_OP_TYPE=" << getBasicOpTypeHexString();

  CompilerOptions << " -DOPERAND_IS_SCALAR_FLAGS=";
  CompilerOptions << "0x" << std::hex << ScalarInputFlags;

  return CompilerOptions.str();
}

template <typename T>
std::string TestConfig<T>::getBasicOpTypeHexString() const {

  if (BasicOpType == BasicOpType_Unary)
    return "0x1";
  if (BasicOpType == BasicOpType_Binary)
    return "0x2";
  if (BasicOpType == BasicOpType_Ternary)
    return "0x3";

  LOG_ERROR_FMT_THROW(L"Invalid BasicOpType: %d",
                      static_cast<int>(BasicOpType));
  return "0x0";
}

template <typename T> size_t TestConfig<T>::getNumOperands() const {
  if (BasicOpType == BasicOpType_Unary)
    return 1;

  if (BasicOpType == BasicOpType_Binary)
    return 2;

  if (BasicOpType == BasicOpType_Ternary)
    return 3;

  LOG_ERROR_FMT_THROW(L"Invalid BasicOpType: %d",
                      static_cast<int>(BasicOpType));
  return 0;
}

template <typename T>
std::vector<T> TestConfig<T>::getInputValueSet(size_t ValueSetIndex) const {
  if (BasicOpType == BasicOpType_Unary && ValueSetIndex == 0)
    return getInputValueSetByKey<T>(InputValueSetKeys[ValueSetIndex]);

  if (BasicOpType == BasicOpType_Binary && ValueSetIndex <= 1)
    return getInputValueSetByKey<T>(InputValueSetKeys[ValueSetIndex]);

  if (BasicOpType == BasicOpType_Ternary && ValueSetIndex <= 2)
    return getInputValueSetByKey<T>(InputValueSetKeys[ValueSetIndex]);

  LOG_ERROR_FMT_THROW(L"Invalid ValueSetIndex: %d for OpType: %ls",
                      ValueSetIndex, OpTypeName.c_str());
  return std::vector<T>();
}

template <typename T>
std::string TestConfig<T>::getHLSLOutputTypeString() const {
  // std::visit allows us to dispatch a call to getHLSLTypeString<T>() with the
  // the current underlying element type of ExpectedVector.
  return std::visit(
      [](const auto &Vec) {
        using ElementType = typename std::decay_t<decltype(Vec)>::value_type;
        return getHLSLTypeString<ElementType>();
      },
      ExpectedVector);
}

template <typename T>
bool TestConfig<T>::verifyOutput(
    const std::shared_ptr<st::ShaderOpTestResult> &TestResult) {

  // std::visit allows us to dispatch a call to the private version of
  // verifyOutput using a std::vector<T> that matches the type currently held in
  // ExpectedVector. This works because ExpectedVector is a std::variant of
  // vector types, and the lambda receives the active type at runtime. It's
  // important that the TestConfig instance has correctly assigned the expected
  // output type to ExpectedVector. By default, this is std::vector<T>,
  // but ops like AsTypeOpType must override it. For example,
  // AsTypeOpType_AsFloat16 sets ExpectedVector to std::vector<HLSLHalf_t>.
  return std::visit(
      [this, &TestResult](const auto &Vec) {
        using ElementType = typename std::decay_t<decltype(Vec)>::value_type;
        return this->verifyOutput<ElementType>(TestResult, Vec);
      },
      ExpectedVector);
}

// Private version of verifyOutput. Called by the public version of verifyOutput
// which resolves OutT based on the ExpectedVector type. Most intrinsics will
// have an OutT that matches the input type being tested (which is T). But some,
// such as the 'AsType' ops, i.e 'AsUint' have an OutT that doesn't match T.
template <typename T>    // Primary template from TestConfig
template <typename OutT> // Secondary template for verifyOutput
bool TestConfig<T>::verifyOutput(
    const std::shared_ptr<st::ShaderOpTestResult> &TestResult,
    const std::vector<OutT> &ExpectedVector) {

  WEX::Logging::Log::Comment(WEX::Common::String().Format(
      L"verifyOutput with OpType: %ls ExpectedVector<%S>", OpTypeName.c_str(),
      typeid(OutT).name()));

  DXASSERT(!ExpectedVector.empty(),
           "Programmer Error: ExpectedVector is empty.");

  MappedData ShaderOutData;
  TestResult->Test->GetReadBackData("OutputVector", &ShaderOutData);

  const size_t OutputVectorSize = ExpectedVector.size();

  std::vector<OutT> ActualValues;
  fillLongVectorDataFromShaderBuffer(ShaderOutData, ActualValues,
                                     OutputVectorSize);

  return doVectorsMatch(ActualValues, ExpectedVector, Tolerance, ValidationType,
                        VerboseLogging);
}

// Generic fillInput. Fill the inputs based on the OpType and the
// ScalarInputFlags.
template <typename T>
void TestConfig<T>::fillInputs(TestInputs<T> &Inputs) const {

  auto fillVecFromValueSet = [this](std::vector<T> &Vec, size_t ValueSetIndex,
                                    size_t Count) {
    std::vector<T> ValueSet = getInputValueSet(ValueSetIndex);
    for (size_t Index = 0; Index < Count; Index++)
      Vec.push_back(ValueSet[Index % ValueSet.size()]);
  };

  size_t ValueSetIndex = 0;

  fillVecFromValueSet(Inputs.InputVector1, ValueSetIndex++, LengthToTest);

  if (BasicOpType == BasicOpType_Unary)
    return;

  DXASSERT_NOMSG(BasicOpType == BasicOpType_Binary ||
                 BasicOpType == BasicOpType_Ternary);

  const size_t Input2Length =
      (ScalarInputFlags & SCALAR_INPUT_FLAGS_OPERAND_2_IS_SCALAR)
          ? 1
          : LengthToTest;
  Inputs.InputVector2 = std::vector<T>();
  fillVecFromValueSet(*Inputs.InputVector2, ValueSetIndex++, Input2Length);

  if (BasicOpType == BasicOpType_Binary)
    return;

  DXASSERT_NOMSG(BasicOpType == BasicOpType_Ternary);

  const size_t Input3Length =
      (ScalarInputFlags & SCALAR_INPUT_FLAGS_OPERAND_3_IS_SCALAR)
          ? 1
          : LengthToTest;
  Inputs.InputVector3 = std::vector<T>();
  fillVecFromValueSet(*Inputs.InputVector3, ValueSetIndex++, Input3Length);
}

template <typename T>
AsTypeOpTestConfig<T>::AsTypeOpTestConfig(
    const OpTypeMetaData<AsTypeOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  BasicOpType = BasicOpType_Unary;

  switch (OpType) {
  case AsTypeOpType_AsFloat16: {
    auto ComputeFunc = [this](const T &Val) { return asFloat16(Val); };
    InitUnaryOpValueComputer<HLSLHalf_t>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsFloat: {
    auto ComputeFunc = [this](const T &Val) { return asFloat(Val); };
    InitUnaryOpValueComputer<float>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsInt: {
    auto ComputeFunc = [this](const T &Val) { return asInt(Val); };
    InitUnaryOpValueComputer<int32_t>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsInt16: {
    auto ComputeFunc = [this](const T &Val) { return asInt16(Val); };
    InitUnaryOpValueComputer<int16_t>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsUint: {
    auto ComputeFunc = [this](const T &Val) { return asUint(Val); };
    InitUnaryOpValueComputer<uint32_t>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsUint_SplitDouble: {
    SpecialDefines = " -DFUNC_ASUINT_SPLITDOUBLE=1";
    break;
  }
  case AsTypeOpType_AsUint16: {
    auto ComputeFunc = [this](const T &Val) { return asUint16(Val); };
    InitUnaryOpValueComputer<uint16_t>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsDouble: {
    BasicOpType = BasicOpType_Binary;
    auto ComputeFunc = [this](const T &A, const T &B) {
      return asDouble(A, B);
    };
    InitBinaryOpValueComputer<double>(ComputeFunc);
    break;
  }
  default:
    LOG_ERROR_FMT_THROW(L"Unsupported AsTypeOpType: %ls", OpTypeName.c_str());
  }
}

template <typename T>
void TestConfig<T>::computeExpectedValues(const TestInputs<T> &Inputs) {

  // Either a ExpectedValueComputer member should be set or the deriving class
  // should have overridden computeExpectedValues.
  if (!ExpectedValueComputer)
    LOG_ERROR_FMT_THROW(
        L"Programmer Error: ExpectedValueComputer is not set for OpType: %ls.",
        OpTypeName.c_str());

  ExpectedVector = ExpectedValueComputer->computeExpectedValues(Inputs);
}

template <typename T>
void AsTypeOpTestConfig<T>::computeExpectedValues(const TestInputs<T> &Inputs) {

  if (BasicOpType != BasicOpType_Unary && BasicOpType != BasicOpType_Binary)
    LOG_ERROR_FMT_THROW(L"Programmer Error: computeExpectedValue called with "
                        L"unexpected BasicOpType: %d",
                        static_cast<int>(BasicOpType));

  if (ExpectedValueComputer)
    ExpectedVector = ExpectedValueComputer->computeExpectedValues(Inputs);
  else
    // Only SplitDouble has special handling. All other ops will have an
    // ExpectedValueComputer set.
    computeExpectedValues_SplitDouble(Inputs.InputVector1);
}

template <typename T>
void AsTypeOpTestConfig<T>::computeExpectedValues_SplitDouble(
    const std::vector<T> &InputVector) {

  DXASSERT_NOMSG(OpType == AsTypeOpType_AsUint_SplitDouble);

  // SplitDouble is a special case. We fill the first half of the expected
  // vector with the expected low bits of each input double and the second
  // half with the high bits of each input double. Doing things this way
  // helps keep the rest of the generic logic in the LongVector test code
  // simple.
  std::vector<uint32_t> Values;
  Values.resize(InputVector.size() * 2);

  uint32_t LowBits, HighBits;
  const size_t InputSize = InputVector.size();

  for (size_t Index = 0; Index < InputSize; ++Index) {
    splitDouble(InputVector[Index], LowBits, HighBits);
    Values[Index] = LowBits;
    Values[Index + InputSize] = HighBits;
  }

  ExpectedVector = std::move(Values);
}

template <typename T>
TrigonometricOpTestConfig<T>::TrigonometricOpTestConfig(
    const OpTypeMetaData<TrigonometricOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  static_assert(
      isFloatingPointType<T>(),
      "Trigonometric ops are only supported for floating point types.");

  BasicOpType = BasicOpType_Unary;

  // All trigonometric ops are floating point types.
  // These trig functions are defined to have a max absolute error of 0.0008
  // as per the D3D functional specs. An example with this spec for sin and
  // cos is available here:
  // https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#22.10.20
  ValidationType = ValidationType_Epsilon;
  if (std::is_same_v<T, HLSLHalf_t>)
    Tolerance = 0.0010f;
  else if (std::is_same_v<T, float>)
    Tolerance = 0.0008f;

  auto ComputeFunc = [this](const T &A) {
    return this->computeExpectedValue(A);
  };
  InitUnaryOpValueComputer<T>(ComputeFunc);
}

// computeExpectedValue Trigonometric
template <typename T>
T TrigonometricOpTestConfig<T>::computeExpectedValue(const T &A) const {

  switch (OpType) {
  case TrigonometricOpType_Acos:
    return std::acos(A);
  case TrigonometricOpType_Asin:
    return std::asin(A);
  case TrigonometricOpType_Atan:
    return std::atan(A);
  case TrigonometricOpType_Cos:
    return std::cos(A);
  case TrigonometricOpType_Cosh:
    return std::cosh(A);
  case TrigonometricOpType_Sin:
    return std::sin(A);
  case TrigonometricOpType_Sinh:
    return std::sinh(A);
  case TrigonometricOpType_Tan:
    return std::tan(A);
  case TrigonometricOpType_Tanh:
    return std::tanh(A);
  default:
    LOG_ERROR_FMT_THROW(L"Unknown TrigonometricOpType: %ls",
                        OpTypeName.c_str());
    return T();
  }
}

template <typename T>
UnaryOpTestConfig<T>::UnaryOpTestConfig(
    const OpTypeMetaData<UnaryOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  BasicOpType = BasicOpType_Unary;

  switch (OpType) {
  case UnaryOpType_Initialize:
    SpecialDefines = " -DFUNC_INITIALIZE=1";
    break;
  default:
    LOG_ERROR_FMT_THROW(L"Unsupported UnaryOpType: %ls", OpTypeName.c_str());
  }

  auto ComputeFunc = [this](const T &A) {
    return this->computeExpectedValue(A);
  };
  InitUnaryOpValueComputer<T>(ComputeFunc);
}

template <typename T>
T UnaryOpTestConfig<T>::computeExpectedValue(const T &A) const {
  if (OpType != UnaryOpType_Initialize) {
    LOG_ERROR_FMT_THROW(L"computeExpectedValue(const T &A, "
                        L"UnaryOpType OpType) called on an "
                        L"unrecognized unary op: %ls",
                        OpTypeName.c_str());
    return T();
  }

  return T(A);
}

template <typename T>
UnaryMathOpTestConfig<T>::UnaryMathOpTestConfig(
    const OpTypeMetaData<UnaryMathOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  BasicOpType = BasicOpType_Unary;

  if (isFloatingPointType<T>()) {
    Tolerance = 1;
    ValidationType = ValidationType_Ulp;
  }

  switch (OpType) {
  case UnaryMathOpType_Sign: {
    // Sign has overridden special logic.
    auto ComputeFunc = [this](const T &A) { return this->sign(A); };
    InitUnaryOpValueComputer<int32_t>(ComputeFunc);
    break;
  }
  case UnaryMathOpType_Frexp:
    // Don't initialize a ValueComputer, Frexp has special logic for handling
    // its output
    SpecialDefines = " -DFUNC_FREXP=1";
    break;
  default: {
    auto ComputeFunc = [this](const T &A) {
      return this->computeExpectedValue(A);
    };
    InitUnaryOpValueComputer<T>(ComputeFunc);
  }
  }
}

template <typename T>
void UnaryMathOpTestConfig<T>::computeExpectedValues(
    const TestInputs<T> &Inputs) {

  // Base case
  if (ExpectedValueComputer) {
    ExpectedVector = ExpectedValueComputer->computeExpectedValues(Inputs);
    return;
  }

  computeExpectedValues_Frexp(Inputs.InputVector1);
}

// Frexp has a return value as well as an output paramater. So we handle it
// with special logic. Frexp is only supported for fp32 values.
template <typename T>
void UnaryMathOpTestConfig<T>::computeExpectedValues_Frexp(
    const std::vector<T> &InputVector) {

  DXASSERT_NOMSG(OpType == UnaryMathOpType_Frexp);

  std::vector<float> Values;

  // Expected values size is doubled. In the first half we store the Mantissas
  // and in the second half we store the Exponents. This way we can leverage the
  // existing logic which verify expected values in a single vector. We just
  // need to make sure that we organize the output in the same way in the shader
  // and when we read it back.
  const size_t InputSize = InputVector.size();
  Values.resize(InputSize * 2);
  float Exp = 0;
  float Man = 0;

  for (size_t Index = 0; Index < InputSize; ++Index) {
    Man = frexp(InputVector[Index], &Exp);
    Values[Index] = Man;
    Values[Index + InputSize] = Exp;
  }

  ExpectedVector = std::move(Values);
}

template <typename T>
T UnaryMathOpTestConfig<T>::computeExpectedValue(const T &A) const {

  if constexpr (std::is_integral<T>::value) {
    // Abs and Sign are the only UnaryMathOps thats support integral types.
    // Sign always returns int32 values, so its handled elsewhere.
    DXASSERT_NOMSG(OpType == UnaryMathOpType_Abs);
    return abs(A);
  }

  if constexpr (!isFloatingPointType<T>()) {
    LOG_ERROR_FMT_THROW(L"Programmer error: UnaryMathOpType OpType: %ls only "
                        L"supports floating point types",
                        OpTypeName.c_str());
    return T();
  }

  // Most of the std math functions here are only defined for floating point
  // types. If we don't use a mechanism to ensure that we're only using floating
  // point types then the compiler will complain about implicit conversions.
  if constexpr (isFloatingPointType<T>()) {
    // A bunch of the std match functions here are  wrapped in () to avoid
    // collisions with the macro defitions for various functions in windows.h
    switch (OpType) {
    case UnaryMathOpType_Abs:
      return abs(A);
    case UnaryMathOpType_Ceil:
      return (std::ceil)(A);
    case UnaryMathOpType_Floor:
      // float only
      return (std::floor)(A);
    case UnaryMathOpType_Trunc:
      // float only
      return (std::trunc)(A);
    case UnaryMathOpType_Round:
      // float only
      return (std::round)(A);
    case UnaryMathOpType_Frac:
      // std::frac is not a standard C++ function, but we can implement it as
      return A - T((std::floor)(A));
    case UnaryMathOpType_Sqrt:
      return (std::sqrt)(A);
    case UnaryMathOpType_Rsqrt:
      // std::rsqrt is not a standard C++ function, but we can implement it as
      return T(1.0) / T((std::sqrt)(A));
    case UnaryMathOpType_Exp:
      return (std::exp)(A);
    case UnaryMathOpType_Exp2:
      return (std::exp2)(A);
    case UnaryMathOpType_Log:
      return (std::log)(A);
    case UnaryMathOpType_Log2:
      return (std::log2)(A);
    case UnaryMathOpType_Log10:
      return (std::log10)(A);
    case UnaryMathOpType_Rcp:
      // std::.rcp is not a standard C++ function, but we can implement it as
      return T(1.0) / A;
    default:
      LOG_ERROR_FMT_THROW(L"computeExpectedValue(const T &A)"
                          L"called on an unrecognized unary math op: %ls",
                          OpTypeName.c_str());
      return T();
    }
  }
}

template <typename T>
BinaryMathOpTestConfig<T>::BinaryMathOpTestConfig(
    const OpTypeMetaData<BinaryMathOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  if (isFloatingPointType<T>()) {
    Tolerance = 1;
    ValidationType = ValidationType_Ulp;
  }

  BasicOpType = BasicOpType_Binary;

  auto ComputeFunc = [this](const T &A, const T &B) {
    return this->computeExpectedValue(A, B);
  };
  InitBinaryOpValueComputer<T>(ComputeFunc);
}

template <typename T>
T BinaryMathOpTestConfig<T>::computeExpectedValue(const T &A,
                                                  const T &B) const {

  switch (OpType) {
  case BinaryMathOpType_Multiply:
    return A * B;
  case BinaryMathOpType_Add:
    return A + B;
  case BinaryMathOpType_Subtract:
    return A - B;
  case BinaryMathOpType_Divide:
    return A / B;
  case BinaryMathOpType_Modulus:
    return mod(A, B);
  case BinaryMathOpType_Min:
    // std::max and std::min are wrapped in () to avoid collisions with the //
    // macro defintions for min and max in windows.h
    return (std::min)(A, B);
  case BinaryMathOpType_Max:
    return (std::max)(A, B);
  case BinaryMathOpType_Ldexp:
    return ldexp(A, B);
  default:
    LOG_ERROR_FMT_THROW(L"Unknown BinaryMathOpType: %ls", OpTypeName.c_str());
    return T();
  }
}

template <typename T>
TernaryMathOpTestConfig<T>::TernaryMathOpTestConfig(
    const OpTypeMetaData<TernaryMathOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  if (isFloatingPointType<T>()) {
    Tolerance = 1;
    ValidationType = ValidationType_Ulp;
  }

  BasicOpType = BasicOpType_Ternary;

  switch (OpType) {
  case TernaryMathOpType_Fma:
  case TernaryMathOpType_Mad:
  case TernaryMathOpType_SmoothStep:
    break;
  default:
    LOG_ERROR_FMT_THROW(L"Invalid TernaryMathOpType: %ls", OpTypeName.c_str());
  }

  auto ComputeFunc = [this](const T &A, const T &B, const T &C) {
    return this->computeExpectedValue(A, B, C);
  };
  InitTernaryOpValueComputer<T>(ComputeFunc);
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
std::vector<OUT_TYPE> runTest(OP_TYPE OpType, const InputSets<T, ARITY> &Inputs,
                              size_t ExpectedOutputSize,
                              uint16_t ScalarInputFlags,
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

  // TODO: reinstate VerboseLogging flag?
  bool VerboseLogging = false;
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

}; // namespace LongVector
