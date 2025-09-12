#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#define INLINE_TEST_METHOD_MARKUP
#include <WexTestClass.h>

#include "LongVectorTestData.h"

#include "ShaderOpTest.h"
#include "dxc/Support/Global.h"

#include "HlslExecTestUtils.h"

#include <array>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace LongVector {

//
// Data Types
//

template <typename T> const wchar_t *getDataTypeName() {
  static_assert(false && "Missing data type name");
}

template <typename T> const char *getHLSLTypeString() {
  static_assert(false && "Missing HLSL type string");
}

#define DATA_TYPE_NAME(TYPE, NAME, HLSL_STRING)                                \
  template <> const wchar_t *getDataTypeName<TYPE>() { return NAME; }          \
  template <> const char *getHLSLTypeString<TYPE>() { return HLSL_STRING; }

DATA_TYPE_NAME(HLSLBool_t, L"bool", "bool");
DATA_TYPE_NAME(int16_t, L"int16", "int16_t");
DATA_TYPE_NAME(int32_t, L"int32", "int");
DATA_TYPE_NAME(int64_t, L"int64", "int64_t");
DATA_TYPE_NAME(uint16_t, L"uint16", "uint16_t");
DATA_TYPE_NAME(uint32_t, L"uint32", "uint32_t");
DATA_TYPE_NAME(uint64_t, L"uint64", "uint64_t");
DATA_TYPE_NAME(HLSLHalf_t, L"float16", "half");
DATA_TYPE_NAME(float, L"float32", "float");
DATA_TYPE_NAME(double, L"float64", "double");

#undef DATA_TYPE_NAME

template <typename T> constexpr bool isFloatingPointType() {
  return std::is_same_v<T, float> || std::is_same_v<T, double> ||
         std::is_same_v<T, HLSLHalf_t>;
}

template <typename T> constexpr bool is16BitType() {
  return std::is_same_v<T, int16_t> || std::is_same_v<T, uint16_t> ||
         std::is_same_v<T, HLSLHalf_t>;
}

//
// Operation Types
//

// Helpful metadata struct so we can define some common properties for a test in
// a single place. Intrinsic and Operator are passed in with -D defines to
// the compiler and expanded as macros in the HLSL code. For a better
// understanding of expansion you can reference the shader source used in
// ShaderOpArith.xml under the 'LongVectorOp' entry.
//
// OpTypeString : This is populated by the TableParamaterHandler parsing the
// LongVectorOpTable.xml file. It's used to find the enum value in one of the
// arrays below. Such as binaryMathOpTypeStringToOpMetaData.
//
// OpType : Populated via the lookup with OpTypeString.
//
// Intrinsic : May be empty. Used to expand the intrinsic name in the
// compiled HLSL code via macro expansion. See getCompilerOptionsString() in
// LongVector.cpp in addition to the shader source.
//
// Operator : Used to expand the operator in the compiled HLSL code via macro
// expansion. May be empty. See getCompilerOptionsString() in LongVector.cpp and
// 'LongVectorOp' entry ShaderOpArith.xml. Expands to things like '+', '-',
// '*', etc.
template <typename T> struct OpTypeMetaData {
  std::wstring OpTypeString;
  T OpType;
  std::optional<std::string> Intrinsic = std::nullopt;
  std::optional<std::string> Operator = std::nullopt;
  uint16_t ScalarInputFlags = 0;
};

template <typename OP_TYPE, size_t N>
OP_TYPE getOpType(const OpTypeMetaData<OP_TYPE> (&Values)[N],
                  const wchar_t *OpTypeString) {
  for (size_t I = 0; I < N; ++I) {
    if (Values[I].OpTypeString == OpTypeString)
      return Values[I].OpType;
  }

  LOG_ERROR_FMT_THROW(L"Invalid OpType string: %ls", OpTypeString);

  // We need to return something to satisfy the compiler. We can't annotate
  // LOG_ERROR_FMT_THROW with [[noreturn]] because the TAEF VERIFY_* macros
  // that it uses are re-mapped on Unix to not throw exceptions, so they
  // naturally return. If we hit this point it is a programmer error when
  // implementing a test. Specifically, an entry for this OpTypeString is
  // missing in the static OpTypeStringToOpMetaData array. Or something has
  // been corrupted. Test execution is invalid at this point. Usin
  // std::abort() keeps the compiler happy about no return path. And
  // LOG_ERROR_FMT_THROW will still provide a useful error message via gtest
  // logging on Unix systems.
  std::abort();
}

template <typename OP_TYPE, size_t N>
const OpTypeMetaData<OP_TYPE> &
getOpTypeMetaData(const OpTypeMetaData<OP_TYPE> (&Values)[N], OP_TYPE OpType) {
  for (size_t I = 0; I < N; ++I) {
    if (Values[I].OpType == OpType)
      return Values[I];
  }
  LOG_ERROR_FMT_THROW(L"Invalid OpType: %d", OpType);
  std::abort();
}

template <typename OP_TYPE> OP_TYPE getOpType(const wchar_t *Name);

template <typename OP_TYPE>
OpTypeMetaData<OP_TYPE> getOpTypeMetaData(OP_TYPE OpType);

#define OP_TYPE_META_DATA(TYPE, ARRAY)                                         \
  template <> OpTypeMetaData<TYPE> getOpTypeMetaData(TYPE OpType) {            \
    return getOpTypeMetaData(ARRAY, OpType);                                   \
  }                                                                            \
  template <> TYPE getOpType(const wchar_t *Name) {                            \
    return getOpType(ARRAY, Name);                                             \
  }

// Helper to fill the test data from the shader buffer based on type.
// Convenient to be used when copying HLSL*_t types so we can use the
// underlying type.
template <typename T>
void fillLongVectorDataFromShaderBuffer(const MappedData &ShaderBuffer,
                                        std::vector<T> &TestData,
                                        size_t NumElements) {

  if constexpr (std::is_same_v<T, HLSLHalf_t>) {
    auto *ShaderBufferPtr =
        static_cast<const DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      TestData.push_back(HLSLHalf_t::FromHALF(ShaderBufferPtr[I]));
    return;
  }

  if constexpr (std::is_same_v<T, HLSLBool_t>) {
    auto *ShaderBufferPtr = static_cast<const int32_t *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      // HLSLBool_t has a int32_t based constructor.
      TestData.push_back(ShaderBufferPtr[I]);
    return;
  }

  auto *ShaderBufferPtr = static_cast<const T *>(ShaderBuffer.data());
  for (size_t I = 0; I < NumElements; I++)
    TestData.push_back(ShaderBufferPtr[I]);
  return;
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

enum class ValidationType {
  Epsilon,
  Ulp,
};

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
  case ValidationType::Epsilon:
    return CompareHalfEpsilon(A.Val, B.Val, Tolerance);
  case ValidationType::Ulp:
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
  case ValidationType::Epsilon:
    return CompareFloatEpsilon(A, B, Tolerance);
  case ValidationType::Ulp: {
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
  case ValidationType::Epsilon:
    return CompareDoubleEpsilon(A, B, Tolerance);
  case ValidationType::Ulp: {
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

template <typename T, typename OP_TYPE, OP_TYPE OP> struct TestConfig {
  using String = WEX::Common::String;

  bool VerboseLogging;
  uint16_t ScalarInputFlags;

  String InputValueSetNames[3];
  size_t LongVectorInputSize = 0;

  TestConfig(bool VerboseLogging, uint16_t ScalarInputFlags)
      : VerboseLogging(VerboseLogging), ScalarInputFlags(ScalarInputFlags) {
    using WEX::TestExecution::RuntimeParameters;
    using WEX::TestExecution::TestData;

    for (size_t I = 0; I < std::size(InputValueSetNames); ++I)
      InputValueSetNames[I] = getInputValueSetName(I);

    RuntimeParameters::TryGetValue(L"LongVectorInputSize", LongVectorInputSize);
  }
};

template <typename T, size_t ARITY>
using InputSets = std::array<std::vector<T>, ARITY>;

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

  CompilerOptions << " " << ExtraDefines;

  CompilerOptions << " -DOUT_TYPE=" << getHLSLTypeString<OUT_TYPE>();

  CompilerOptions << " -DBASIC_OP_TYPE=0x" << std::hex << ARITY;

  CompilerOptions << " -DOPERAND_IS_SCALAR_FLAGS=";
  CompilerOptions << "0x" << std::hex << ScalarInputFlags;

  return CompilerOptions.str();
}

// Helper to fill the shader buffer based on type. Convenient to be used when
// copying HLSL*_t types so we can copy the underlying type directly instead
// of the struct.
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
    auto *ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      ShaderBufferPtr[I] = TestData[I].Val;
    return;
  }

  if constexpr (std::is_same_v<T, HLSLBool_t>) {
    auto *ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      ShaderBufferPtr[I] = TestData[I].Val;
    return;
  }

  auto *ShaderBufferPtr = reinterpret_cast<T *>(ShaderBuffer.data());
  for (size_t I = 0; I < NumElements; I++)
    ShaderBufferPtr[I] = TestData[I];
}

template <typename OUT_TYPE, typename T, typename OP_TYPE, OP_TYPE OP,
          size_t ARITY>
std::optional<std::vector<OUT_TYPE>>
runTest(const TestConfig<T, OP_TYPE, OP> &Config,
        const InputSets<T, ARITY> &Inputs, size_t ExpectedOutputSize,
        std::string ExtraDefines) {

  CComPtr<ID3D12Device> D3DDevice;
  if (!createDevice(&D3DDevice, ExecTestUtils::D3D_SHADER_MODEL_6_9, false)) {
#ifdef _HLK_CONF
    LOG_ERROR_FMT_THROW(
        L"Device does not support SM 6.9. Can't run these tests.");
#else
    WEX::Logging::Log::Comment(
        "Device does not support SM 6.9. Can't run these tests.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return std::nullopt;
#endif
  }

  if (Config.VerboseLogging) {
    for (size_t I = 0; I < ARITY; ++I) {
      std::wstring Name = L"InputVector";
      Name += (wchar_t)(L'1' + I);
      logLongVector(Inputs[I], Name);
    }
  }

  // We have to construct the string outside of the lambda. Otherwise it's
  // cleaned up when the lambda finishes executing but before the shader runs.
  std::string CompilerOptionsString =
      getCompilerOptionsString<T, OUT_TYPE, ARITY>(OP, Inputs[0].size(),
                                                   Config.ScalarInputFlags,
                                                   std::move(ExtraDefines));

  dxc::SpecificDllLoader DxilDllLoader;

  // The name of the shader we want to use in ShaderOpArith.xml. Could also
  // add logic to set this name in ShaderOpArithTable.xml so we can use
  // different shaders for different tests.
  LPCSTR ShaderName = "LongVectorOp";
  // ShaderOpArith.xml defines the input/output resources and the shader
  // source.
  CComPtr<IStream> TestXML;
  readHlslDataIntoNewStream(L"ShaderOpArith.xml", &TestXML, DxilDllLoader);

  // RunShaderOpTest is a helper function that handles resource creation
  // and setup. It also handles the shader compilation and execution. It takes
  // a callback that is called when the shader is compiled, but before it is
  // executed.
  std::shared_ptr<st::ShaderOpTestResult> TestResult = st::RunShaderOpTest(
      D3DDevice, DxilDllLoader, TestXML, ShaderName,
      [&](LPCSTR Name, std::vector<BYTE> &ShaderData, st::ShaderOp *ShaderOp) {
        if (Config.VerboseLogging)
          hlsl_test::LogCommentFmt(
              L"RunShaderOpTest CallBack. Resource Name: %S", Name);

        // This callback is called once for each resource defined for
        // "LongVectorOp" in ShaderOpArith.xml. All callbacks are fired for
        // each resource. We determine whether they are applicable to the test
        // case when they run.

        // Process the callback for the OutputVector resource.
        if (_stricmp(Name, "OutputVector") == 0) {
          // We only need to set the compiler options string once. So this is
          // a convenient place to do it.
          ShaderOp->Shaders.at(0).Arguments = CompilerOptionsString.c_str();

          return;
        }

        // Process the callback for the InputVector[1-3] resources
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

  return OutData;
}

template <typename T>
std::vector<T> buildTestInput(const wchar_t *InputValueSetName,
                              size_t SizeToTest) {
  const std::vector<T> &RawValueSet = TestData<T>::Data.at(InputValueSetName);

  std::vector<T> ValueSet;
  ValueSet.reserve(SizeToTest);
  for (size_t I = 0; I < SizeToTest; ++I)
    ValueSet.push_back(RawValueSet[I % RawValueSet.size()]);

  return ValueSet;
}

template <size_t ARITY, typename T, typename OP_TYPE, OP_TYPE OP>
InputSets<T, ARITY> buildTestInputs(const TestConfig<T, OP_TYPE, OP> &Config,
                                    size_t SizeToTest) {
  InputSets<T, ARITY> Inputs;
  for (size_t I = 0; I < ARITY; ++I) {

    uint16_t OperandScalarFlag = 1 << I;
    bool IsOperandScalar = Config.ScalarInputFlags & OperandScalarFlag;

    if (Config.InputValueSetNames[I].IsEmpty()) {
      LOG_ERROR_FMT_THROW(
          L"Expected parameter 'InputValueSetName%d' not found.", I + 1);
      continue;
    }

    Inputs[I] = buildTestInput<T>(Config.InputValueSetNames[I],
                                  IsOperandScalar ? 1 : SizeToTest);
  }

  return Inputs;
}

struct ValidationConfig {
  float Tolerance = 0.0f;
  ValidationType Type = ValidationType::Epsilon;

  static ValidationConfig Epsilon(float Tolerance) {
    return ValidationConfig{Tolerance, ValidationType::Epsilon};
  }

  static ValidationConfig Ulp(float Tolerance) {
    return ValidationConfig{Tolerance, ValidationType::Ulp};
  }
};

template <typename T, typename OP_TYPE, OP_TYPE OP, typename OUT_TYPE,
          size_t ARITY>
void runAndVerify(const TestConfig<T, OP_TYPE, OP> &Config,
                  const InputSets<T, ARITY> &Inputs,
                  const std::vector<OUT_TYPE> &Expected,
                  std::string ExtraDefines,
                  const ValidationConfig &ValidationConfig) {

  std::optional<std::vector<OUT_TYPE>> Actual =
      runTest<OUT_TYPE>(Config, Inputs, Expected.size(), ExtraDefines);

  // If the test didn't run, don't verify anything.
  if (!Actual)
    return;

  VERIFY_IS_TRUE(doVectorsMatch(*Actual, Expected, ValidationConfig.Tolerance,
                                ValidationConfig.Type, Config.VerboseLogging));
}

#if 0
template <typename T, typename OP_TYPE, typename OUT_TYPE>
void dispatchUnaryTest(const TestConfig<T, OP_TYPE> &Config,
                       const ValidationConfig &ValidationConfig,
                       size_t VectorSize, OUT_TYPE (*Calc)(T),
                       std::string ExtraDefines) {

  InputSets<T, 1> Inputs = buildTestInputs<1>(Config, VectorSize);

  std::vector<OUT_TYPE> Expected;
  Expected.reserve(Inputs[0].size());

  for (size_t I = 0; I < Inputs[0].size(); ++I)
    Expected.push_back(Calc(Inputs[0][I]));

  runAndVerify(Config, Inputs, Expected, ExtraDefines, ValidationConfig);
}

template <typename T, typename OP_TYPE, typename OUT_TYPE>
void dispatchBinaryTest(const TestConfig<T, OP_TYPE> &Config,
                        const ValidationConfig &ValidationConfig,
                        size_t VectorSize, OUT_TYPE (*Calc)(T, T),
                        std::string ExtraDefines = "") {
  InputSets<T, 2> Inputs = buildTestInputs<T, 2>(Config, VectorSize);

  std::vector<OUT_TYPE> Expected;
  Expected.reserve(Inputs[0].size());

  for (size_t I = 0; I < Inputs[0].size(); ++I) {
    size_t Index1 = (Config.ScalarInputFlags & (1 << 1)) ? 0 : I;
    Expected.push_back(Calc(Inputs[0][I], Inputs[1][Index1]));
  }

  runAndVerify(Config, Inputs, Expected, ExtraDefines, ValidationConfig);
}
#endif

//
// TrigonometricTest
//
#if 0
enum class TrigonometricOpType {
  Acos,
  Asin,
  Atan,
  Cos,
  Cosh,
  Sin,
  Sinh,
  Tan,
  Tanh,
  EnumValueCount
};

static const OpTypeMetaData<TrigonometricOpType>
    trigonometricOpTypeStringToOpMetaData[] = {
        {L"TrigonometricOpType_Acos", TrigonometricOpType::Acos, "acos"},
        {L"TrigonometricOpType_Asin", TrigonometricOpType::Asin, "asin"},
        {L"TrigonometricOpType_Atan", TrigonometricOpType::Atan, "atan"},
        {L"TrigonometricOpType_Cos", TrigonometricOpType::Cos, "cos"},
        {L"TrigonometricOpType_Cosh", TrigonometricOpType::Cosh, "cosh"},
        {L"TrigonometricOpType_Sin", TrigonometricOpType::Sin, "sin"},
        {L"TrigonometricOpType_Sinh", TrigonometricOpType::Sinh, "sinh"},
        {L"TrigonometricOpType_Tan", TrigonometricOpType::Tan, "tan"},
        {L"TrigonometricOpType_Tanh", TrigonometricOpType::Tanh, "tanh"},
};

static_assert(
    _countof(trigonometricOpTypeStringToOpMetaData) ==
        (size_t)TrigonometricOpType::EnumValueCount,
    "trigonometricOpTypeStringToOpMetaData size mismatch. Did you add "
    "a new enum value?");

OP_TYPE_META_DATA(TrigonometricOpType, trigonometricOpTypeStringToOpMetaData);

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

template <typename T>
void dispatchTrigonometricTest(const TestConfig<T, TrigonometricOpType> &Config,
                               ValidationConfig ValidationConfig,
                               size_t VectorSize) {
#define DISPATCH(OP, NAME)                                                     \
  case OP:                                                                     \
    return dispatchUnaryTest<T>(Config, ValidationConfig, VectorSize,          \
                                TrigonometricOperation<T>::NAME, "")

  switch (Config.OpType) {
    DISPATCH(TrigonometricOpType::Acos, acos);
    DISPATCH(TrigonometricOpType::Asin, asin);
    DISPATCH(TrigonometricOpType::Atan, atan);
    DISPATCH(TrigonometricOpType::Cos, cos);
    DISPATCH(TrigonometricOpType::Cosh, cosh);
    DISPATCH(TrigonometricOpType::Sin, sin);
    DISPATCH(TrigonometricOpType::Sinh, sinh);
    DISPATCH(TrigonometricOpType::Tan, tan);
    DISPATCH(TrigonometricOpType::Tanh, tanh);
  case TrigonometricOpType::EnumValueCount:
    break;
  }

#undef DISPATCH

  DXASSERT(false, "Invalid OpType");
}

// All trigonometric ops are floating point types.
// These trig functions are defined to have a max absolute error of 0.0008
// as per the D3D functional specs. An example with this spec for sin and
// cos is available here:
// https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#22.10.20

template <typename T>
void dispatchTestByVectorSize(const TestConfig<T, TrigonometricOpType> &Config,
                              size_t VectorSize);

template <>
void dispatchTestByVectorSize(
    const TestConfig<HLSLHalf_t, TrigonometricOpType> &Config,
    size_t VectorSize) {
  return dispatchTrigonometricTest<HLSLHalf_t>(
      Config, ValidationConfig::Epsilon(0.0010f), VectorSize);
}

template <>
void dispatchTestByVectorSize(
    const TestConfig<float, TrigonometricOpType> &Config, size_t VectorSize) {
  return dispatchTrigonometricTest<float>(
      Config, ValidationConfig::Epsilon(0.0008f), VectorSize);
}

//
// AsTypeOp
//

enum class AsTypeOpType {
  AsFloat,
  AsFloat16,
  AsInt,
  AsInt16,
  AsUint,
  AsUint_SplitDouble,
  AsUint16,
  AsDouble,
  EnumValueCount
};

static const OpTypeMetaData<AsTypeOpType> asTypeOpTypeStringToOpMetaData[] = {
    {L"AsTypeOpType_AsFloat", AsTypeOpType::AsFloat, "asfloat"},
    {L"AsTypeOpType_AsFloat16", AsTypeOpType::AsFloat16, "asfloat16"},
    {L"AsTypeOpType_AsInt", AsTypeOpType::AsInt, "asint"},
    {L"AsTypeOpType_AsInt16", AsTypeOpType::AsInt16, "asint16"},
    {L"AsTypeOpType_AsUint", AsTypeOpType::AsUint, "asuint"},
    {L"AsTypeOpType_AsUint_SplitDouble", AsTypeOpType::AsUint_SplitDouble,
     "TestAsUintSplitDouble"},
    {L"AsTypeOpType_AsUint16", AsTypeOpType::AsUint16, "asuint16"},
    {L"AsTypeOpType_AsDouble", AsTypeOpType::AsDouble, "asdouble", ","},
};

static_assert(_countof(asTypeOpTypeStringToOpMetaData) ==
                  (size_t)AsTypeOpType::EnumValueCount,
              "asTypeOpTypeStringToOpMetaData size mismatch. Did you add "
              "a new enum value?");

OP_TYPE_META_DATA(AsTypeOpType, asTypeOpTypeStringToOpMetaData);

// We don't have std::bit_cast in C++17, so we define our own version.
template <typename ToT, typename FromT>
typename std::enable_if<sizeof(ToT) == sizeof(FromT) &&
                            std::is_trivially_copyable<FromT>::value &&
                            std::is_trivially_copyable<ToT>::value,
                        ToT>::type
bit_cast(const FromT &Src) {
  ToT Dst;
  std::memcpy(&Dst, &Src, sizeof(ToT));
  return Dst;
}

// asFloat

template <typename T> float asFloat(T);
template <> float asFloat(float A) { return float(A); }
template <> float asFloat(int32_t A) { return bit_cast<float>(A); }
template <> float asFloat(uint32_t A) { return bit_cast<float>(A); }

// asFloat16

template <typename T> HLSLHalf_t asFloat16(T);
template <> HLSLHalf_t asFloat16(HLSLHalf_t A) { return A; }
template <> HLSLHalf_t asFloat16(int16_t A) {
  return HLSLHalf_t::FromHALF(bit_cast<DirectX::PackedVector::HALF>(A));
}
template <> HLSLHalf_t asFloat16(uint16_t A) {
  return HLSLHalf_t::FromHALF(bit_cast<DirectX::PackedVector::HALF>(A));
}

// asInt

template <typename T> int32_t asInt(T);
template <> int32_t asInt(float A) { return bit_cast<int32_t>(A); }
template <> int32_t asInt(int32_t A) { return A; }
template <> int32_t asInt(uint32_t A) { return bit_cast<int32_t>(A); }

// asInt16

template <typename T> int16_t asInt16(T);
template <> int16_t asInt16(HLSLHalf_t A) { return bit_cast<int16_t>(A.Val); }
template <> int16_t asInt16(int16_t A) { return A; }
template <> int16_t asInt16(uint16_t A) { return bit_cast<int16_t>(A); }

// asUint16

template <typename T> uint16_t asUint16(T);
template <> uint16_t asUint16(HLSLHalf_t A) {
  return bit_cast<uint16_t>(A.Val);
}
template <> uint16_t asUint16(uint16_t A) { return A; }
template <> uint16_t asUint16(int16_t A) { return bit_cast<uint16_t>(A); }

// asUint

template <typename T> unsigned int asUint(T);
template <> unsigned int asUint(unsigned int A) { return A; }
template <> unsigned int asUint(float A) { return bit_cast<unsigned int>(A); }
template <> unsigned int asUint(int A) { return bit_cast<unsigned int>(A); }

// splitDouble

static void splitDouble(const double A, uint32_t &LowBits, uint32_t &HighBits) {
  uint64_t Bits = 0;
  std::memcpy(&Bits, &A, sizeof(Bits));
  LowBits = static_cast<uint32_t>(Bits & 0xFFFFFFFF);
  HighBits = static_cast<uint32_t>(Bits >> 32);
}

// asDouble

static double asDouble(const uint32_t LowBits, const uint32_t HighBits) {
  uint64_t Bits = (static_cast<uint64_t>(HighBits) << 32) | LowBits;
  double Result;
  std::memcpy(&Result, &Bits, sizeof(Result));
  return Result;
}

void dispatchAsUintSplitDoubleTest(
    const TestConfig<double, AsTypeOpType> &Config, size_t VectorSize) {
  DXASSERT(Config.OpType == AsTypeOpType::AsUint_SplitDouble,
           "Unexpected OpType");

  InputSets<double, 1> Inputs = buildTestInputs<1>(Config, VectorSize);

  std::vector<uint32_t> Expected;
  Expected.resize(Inputs.size() * 2);

  for (size_t I = 0; I < Inputs.size(); ++I) {
    uint32_t Low, High;
    splitDouble(Expected[I], Low, High);
    Expected[I] = Low;
    Expected[I + Inputs.size()] = High;
  }

  ValidationConfig ValidationConfig{};
  runAndVerify(Config, Inputs, Expected, " -DFUNC_ASUINT_SPLITDOUBLE=1",
               ValidationConfig);
}

template <typename T>
void dispatchTestByVectorSize(const TestConfig<T, AsTypeOpType> &Config,
                              size_t VectorSize) {

  // Different AsType* operations are supported for different data types, so
  // we dispatch on operation first.

#define DISPATCH(FN)                                                           \
  return dispatchUnaryTest(Config, ValidationConfig{}, VectorSize, FN<T>, "")

  switch (Config.OpType) {
  case AsTypeOpType::AsFloat:
    DISPATCH(asFloat);

  case AsTypeOpType::AsInt:
    DISPATCH(asInt);

  case AsTypeOpType::AsUint:
    DISPATCH(asUint);

  case AsTypeOpType::AsFloat16:
    DISPATCH(asFloat16);

  case AsTypeOpType::AsInt16:
    DISPATCH(asInt16);

  case AsTypeOpType::AsUint16:
    DISPATCH(asUint16);

  case AsTypeOpType::AsUint_SplitDouble:
    if constexpr (std::is_same_v<T, double>)
      return dispatchAsUintSplitDoubleTest(Config, VectorSize);
    break;

  case AsTypeOpType::AsDouble:
    if constexpr (std::is_same_v<T, uint32_t>)
      return dispatchBinaryTest(Config, ValidationConfig{}, VectorSize,
                                asDouble);
    break;

  case AsTypeOpType::EnumValueCount:
    break;
  }

#undef DISPATCH

  DXASSERT(false, "Invalid OpType");
}

// UnaryOp
//

enum class UnaryOpType { Initialize, EnumValueCount };

static const OpTypeMetaData<UnaryOpType> unaryOpTypeStringToOpMetaData[] = {
    {L"UnaryOpType_Initialize", UnaryOpType::Initialize, "TestInitialize"},
};

static_assert(_countof(unaryOpTypeStringToOpMetaData) ==
                  (size_t)UnaryOpType::EnumValueCount,
              "unaryOpTypeStringToOpMetaData size mismatch. Did you add "
              "a new enum value?");

OP_TYPE_META_DATA(UnaryOpType, unaryOpTypeStringToOpMetaData);

template <typename T> T Initialize(T V) { return V; }

template <typename T>
void dispatchTestByVectorSize(const TestConfig<T, UnaryOpType> &Config,
                              size_t VectorSize) {

  switch (Config.OpType) {
  case UnaryOpType::Initialize:
    return dispatchUnaryTest(Config, ValidationConfig{}, VectorSize,
                             Initialize<T>, " -DFUNC_INITIALIZE=1");

  case UnaryOpType::EnumValueCount:
    break;
  }

  DXASSERT(false, "Invalid OpType");
}

//
// UnaryMathOp
//

enum class UnaryMathOpType {
  Abs,
  Sign,
  Ceil,
  Floor,
  Trunc,
  Round,
  Frac,
  Sqrt,
  Rsqrt,
  Exp,
  Exp2,
  Log,
  Log2,
  Log10,
  Rcp,
  Frexp,
  EnumValueCount
};

static const OpTypeMetaData<UnaryMathOpType>
    unaryMathOpTypeStringToOpMetaData[] = {
        {L"UnaryMathOpType_Abs", UnaryMathOpType::Abs, "abs"},
        {L"UnaryMathOpType_Sign", UnaryMathOpType::Sign, "sign"},
        {L"UnaryMathOpType_Ceil", UnaryMathOpType::Ceil, "ceil"},
        {L"UnaryMathOpType_Floor", UnaryMathOpType::Floor, "floor"},
        {L"UnaryMathOpType_Trunc", UnaryMathOpType::Trunc, "trunc"},
        {L"UnaryMathOpType_Round", UnaryMathOpType::Round, "round"},
        {L"UnaryMathOpType_Frac", UnaryMathOpType::Frac, "frac"},
        {L"UnaryMathOpType_Sqrt", UnaryMathOpType::Sqrt, "sqrt"},
        {L"UnaryMathOpType_Rsqrt", UnaryMathOpType::Rsqrt, "rsqrt"},
        {L"UnaryMathOpType_Exp", UnaryMathOpType::Exp, "exp"},
        {L"UnaryMathOpType_Exp2", UnaryMathOpType::Exp2, "exp2"},
        {L"UnaryMathOpType_Log", UnaryMathOpType::Log, "log"},
        {L"UnaryMathOpType_Log2", UnaryMathOpType::Log2, "log2"},
        {L"UnaryMathOpType_Log10", UnaryMathOpType::Log10, "log10"},
        {L"UnaryMathOpType_Rcp", UnaryMathOpType::Rcp, "rcp"},
        {L"UnaryMathOpType_Frexp", UnaryMathOpType::Frexp, "TestFrexp"},
};

static_assert(_countof(unaryMathOpTypeStringToOpMetaData) ==
                  (size_t)UnaryMathOpType::EnumValueCount,
              "unaryMathOpTypeStringToOpMetaData size mismatch. Did you add "
              "a new enum value?");

OP_TYPE_META_DATA(UnaryMathOpType, unaryMathOpTypeStringToOpMetaData);

template <typename T, typename OUT_TYPE>
void dispatchUnaryMathOpTest(const TestConfig<T, UnaryMathOpType> &Config,
                             size_t VectorSize, OUT_TYPE (*Calc)(T)) {

  ValidationConfig ValidationConfig;

  if (isFloatingPointType<T>()) {
    ValidationConfig = ValidationConfig::Ulp(1.0);
  }

  dispatchUnaryTest(Config, ValidationConfig, VectorSize, Calc, "");
}

template <typename T> struct UnaryMathOps {
  static T Abs(T V) {
    if constexpr (std::is_unsigned_v<T>)
      return V;
    else
      return static_cast<T>(std::abs(V));
  }

  static int32_t Sign(T V) {
    const T Zero = T();

    if (V > Zero)
      return 1;

    if (V < Zero)
      return -1;

    return 0;
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

void dispatchFrexpTest(const TestConfig<float, UnaryMathOpType> &Config,
                       size_t VectorSize) {
  DXASSERT(Config.OpType == UnaryMathOpType::Frexp, "Unexpected OpType");

  // Frexp has a return value as well as an output paramater. So we handle it
  // with special logic. Frexp is only supported for fp32 values.

  InputSets<float, 1> Inputs = buildTestInputs<1>(Config, VectorSize);

  std::vector<float> Expected;

  // Expected values size is doubled. In the first half we store the Mantissas
  // and in the second half we store the Exponents. This way we can leverage
  // the existing logic which verify expected values in a single vector. We
  // just need to make sure that we organize the output in the same way in the
  // shader and when we read it back.

  Expected.resize(VectorSize * 2);

  for (size_t I = 0; I < VectorSize; ++I) {
    int Exp = 0;
    float Man = std::frexp(Inputs[0][I], &Exp);

    // std::frexp returns a signed mantissa. But the HLSL implmentation
    // returns an unsigned mantissa.
    Man = std::abs(Man);

    Expected[I] = Man;

    // std::frexp returns the exponent as an int, but HLSL stores it as a
    // float. However, the HLSL exponents fractional component is always 0. So
    // it can conversion between float and int is safe.
    Expected[I + VectorSize] = static_cast<float>(Exp);
  }

  runAndVerify(Config, Inputs, Expected, " -DFUNC_FREXP=1", ValidationConfig{});
}

template <typename T>
void dispatchTestByVectorSize(const TestConfig<T, UnaryMathOpType> &Config,
                              size_t VectorSize) {
#define DISPATCH(FUNC)                                                         \
  return dispatchUnaryMathOpTest(Config, VectorSize, UnaryMathOps<T>::FUNC)

  switch (Config.OpType) {
  case UnaryMathOpType::Abs:
    DISPATCH(Abs);
    break;

  case UnaryMathOpType::Sign:
    DISPATCH(Sign);
    break;

  case UnaryMathOpType::Ceil:
    DISPATCH(Ceil);
    break;

  case UnaryMathOpType::Floor:
    DISPATCH(Floor);
    break;

  case UnaryMathOpType::Trunc:
    DISPATCH(Trunc);
    break;

  case UnaryMathOpType::Round:
    DISPATCH(Round);
    break;

  case UnaryMathOpType::Frac:
    DISPATCH(Frac);
    break;

  case UnaryMathOpType::Sqrt:
    DISPATCH(Sqrt);
    break;

  case UnaryMathOpType::Rsqrt:
    DISPATCH(Rsqrt);
    break;

  case UnaryMathOpType::Exp:
    DISPATCH(Exp);
    break;

  case UnaryMathOpType::Exp2:
    DISPATCH(Exp2);
    break;

  case UnaryMathOpType::Log:
    DISPATCH(Log);
    break;

  case UnaryMathOpType::Log2:
    DISPATCH(Log2);
    break;

  case UnaryMathOpType::Log10:
    DISPATCH(Log10);
    break;

  case UnaryMathOpType::Rcp:
    DISPATCH(Rcp);
    break;

  case UnaryMathOpType::Frexp:
    if constexpr (std::is_same_v<T, float>)
      return dispatchFrexpTest(Config, VectorSize);
    break;

  case UnaryMathOpType::EnumValueCount:
    break;
  }

#undef DISPATCH

  DXASSERT(false, "Invalid OpType");
}

//
// BinaryOpType
//

enum class BinaryOpType {
  LogicalAnd,
  LogicalOr,
  TernaryAssignment_True,
  TernaryAssignment_False,
  EnumValueCount
};

static const OpTypeMetaData<BinaryOpType> binaryOpTypeStringToOpMetaData[] = {
    {L"BinaryOpType_Logical_And", BinaryOpType::LogicalAnd, "and", ","},
    {L"BinaryOpType_Logical_Or", BinaryOpType::LogicalOr, "or", ","},
    {L"BinaryOpType_TernaryAssignment_True",
     BinaryOpType::TernaryAssignment_True, "TestTernaryAssignment", ","},
    {L"BinaryOpType_TernaryAssignment_False",
     BinaryOpType::TernaryAssignment_False, "TestTernaryAssignment", ","},
};

static_assert(_countof(binaryOpTypeStringToOpMetaData) ==
                  (size_t)BinaryOpType::EnumValueCount,
              "binaryOpTypeStringToOpMetaData size mismatch. Did you "
              "add a new enum value?");

OP_TYPE_META_DATA(BinaryOpType, binaryOpTypeStringToOpMetaData);

template <typename T, typename OUT_TYPE>
void dispatchBinaryOpTest(const TestConfig<T, BinaryOpType> &Config,
                          size_t VectorSize, OUT_TYPE (*Calc)(T, T),
                          std::string ExtraDefines) {

  ValidationConfig ValidationConfig;

  if (isFloatingPointType<T>())
    ValidationConfig = ValidationConfig::Ulp(1.0);

  dispatchBinaryTest(Config, ValidationConfig, VectorSize, Calc, ExtraDefines);
}

template <typename T> struct BinaryOps {
  // Logical operations
  static T LogicalAnd(T A, T B) { return A && B; }
  static T LogicalOr(T A, T B) { return A || B; }

  // The ternary operator only allows scalars for the condition.
  // So it's easy to just treat it as a binary operation.
  static T TernaryTrue(T A, T B) { return true ? A : B; }
  static T TernaryFalse(T A, T B) { return false ? A : B; }
};

template <typename T>
void dispatchTestByVectorSize(const TestConfig<T, BinaryOpType> &Config,
                              size_t VectorSize) {

#define DISPATCH(FUNC)                                                         \
  return dispatchBinaryOpTest(Config, VectorSize, BinaryOps<T>::FUNC,          \
                              ExtraDefines)

  std::string ExtraDefines;

  switch (Config.OpType) {
  case BinaryOpType::TernaryAssignment_True:
    ExtraDefines = " -DTERNARY_CONDITION=1 -DFUNC_TERNARY_ASSIGNMENT=1";
    DISPATCH(TernaryTrue);
    break;

  case BinaryOpType::TernaryAssignment_False:
    ExtraDefines = " -DTERNARY_CONDITION=0 -DFUNC_TERNARY_ASSIGNMENT=1";
    DISPATCH(TernaryFalse);
    break;

  case BinaryOpType::LogicalAnd:
    DISPATCH(LogicalAnd);
    break;

  case BinaryOpType::LogicalOr:
    DISPATCH(LogicalOr);
    break;

  case BinaryOpType::EnumValueCount:
    break;
  }

#undef DISPATCH

  DXASSERT(false, "Invalid OpType");
}

// BinaryMathOp
//

enum class BinaryMathOpType {
  Multiply,
  Add,
  Subtract,
  Divide,
  Modulus,
  Min,
  Max,
  Ldexp,
  EnumValueCount
};

static const OpTypeMetaData<BinaryMathOpType>
    binaryMathOpTypeStringToOpMetaData[] = {
        {L"BinaryMathOpType_Add", BinaryMathOpType::Add, std::nullopt, "+"},
        {L"BinaryMathOpType_Multiply", BinaryMathOpType::Multiply, std::nullopt,
         "*"},
        {L"BinaryMathOpType_Subtract", BinaryMathOpType::Subtract, std::nullopt,
         "-"},
        {L"BinaryMathOpType_Divide", BinaryMathOpType::Divide, std::nullopt,
         "/"},
        {L"BinaryMathOpType_Modulus", BinaryMathOpType::Modulus, std::nullopt,
         "%"},
        {L"BinaryMathOpType_Min", BinaryMathOpType::Min, "min", ","},
        {L"BinaryMathOpType_Max", BinaryMathOpType::Max, "max", ","},
        {L"BinaryMathOpType_Ldexp", BinaryMathOpType::Ldexp, "ldexp", ","},
};

static_assert(_countof(binaryMathOpTypeStringToOpMetaData) ==
                  (size_t)BinaryMathOpType::EnumValueCount,
              "binaryMathOpTypeStringToOpMetaData size mismatch. Did you "
              "add a new enum value?");

OP_TYPE_META_DATA(BinaryMathOpType, binaryMathOpTypeStringToOpMetaData);

template <typename T, typename OUT_TYPE>
void dispatchBinaryMathOpTest(const TestConfig<T, BinaryMathOpType> &Config,
                              size_t VectorSize, OUT_TYPE (*Calc)(T, T)) {

  ValidationConfig ValidationConfig;

  if (isFloatingPointType<T>())
    ValidationConfig = ValidationConfig::Ulp(1.0);

  dispatchBinaryTest(Config, ValidationConfig, VectorSize, Calc);
}

template <typename T> struct BinaryMathOps {
  static T Multiply(T A, T B) { return A * B; }
  static T Add(T A, T B) { return A + B; }
  static T Subtract(T A, T B) { return A - B; }
  static T Divide(T A, T B) { return A / B; }

  static T Modulus(T A, T B) {
    // Only float uses fmod.  Everything else - including HLSLHalf_t uses the
    // operator.
    if constexpr (std::is_same_v<T, float>)
      return std::fmod(A, B);
    else
      return A % B;
  }

  static T Min(T A, T B) { return std::min(A, B); }
  static T Max(T A, T B) { return std::max(A, B); }

  static T Ldexp(T A, T B) { return A * static_cast<T>(std::pow(2.0f, B)); }
};

template <typename T>
void dispatchTestByVectorSize(const TestConfig<T, BinaryMathOpType> &Config,
                              size_t VectorSize) {

#define DISPATCH(FUNC)                                                         \
  return dispatchBinaryMathOpTest(Config, VectorSize, BinaryMathOps<T>::FUNC)

  switch (Config.OpType) {
  case BinaryMathOpType::Multiply:
    DISPATCH(Multiply);

  case BinaryMathOpType::Add:
    DISPATCH(Add);

  case BinaryMathOpType::Subtract:
    DISPATCH(Subtract);

  case BinaryMathOpType::Divide:
    DISPATCH(Divide);

  case BinaryMathOpType::Modulus:
    DISPATCH(Modulus);

  case BinaryMathOpType::Min:
    DISPATCH(Min);

  case BinaryMathOpType::Max:
    DISPATCH(Max);

  case BinaryMathOpType::Ldexp:
    DISPATCH(Ldexp);
    break;

  case BinaryMathOpType::EnumValueCount:
    break;
  }

#undef DISPATCH

  DXASSERT(false, "Invalid OpType");
}

// BinaryComparisonOp
//

enum class BinaryComparisonOpType {
  LessThan,
  LessEqual,
  GreaterThan,
  GreaterEqual,
  Equal,
  NotEqual,
  EnumValueCount
};

static const OpTypeMetaData<BinaryComparisonOpType>
    binaryComparisonOpTypeStringToOpMetaData[] = {
        {L"BinaryComparisonOpType_LessThan", BinaryComparisonOpType::LessThan,
         std::nullopt, "<"},
        {L"BinaryComparisonOpType_LessEqual", BinaryComparisonOpType::LessEqual,
         std::nullopt, "<="},
        {L"BinaryComparisonOpType_GreaterThan",
         BinaryComparisonOpType::GreaterThan, std::nullopt, ">"},
        {L"BinaryComparisonOpType_GreaterEqual",
         BinaryComparisonOpType::GreaterEqual, std::nullopt, ">="},
        {L"BinaryComparisonOpType_Equal", BinaryComparisonOpType::Equal,
         std::nullopt, "=="},
        {L"BinaryComparisonOpType_NotEqual", BinaryComparisonOpType::NotEqual,
         std::nullopt, "!="},
};

static_assert(_countof(binaryComparisonOpTypeStringToOpMetaData) ==
                  (size_t)BinaryComparisonOpType::EnumValueCount,
              "binaryComparisonOpTypeStringToOpMetaData size mismatch. Did "
              "you add a new enum value?");

OP_TYPE_META_DATA(BinaryComparisonOpType,
                  binaryComparisonOpTypeStringToOpMetaData);

template <typename T, typename OUT_TYPE>
void dispatchBinaryComparisonOpTest(
    const TestConfig<T, BinaryComparisonOpType> &Config, size_t VectorSize,
    OUT_TYPE (*Calc)(T, T)) {
  ValidationConfig ValidationConfig{};
  dispatchBinaryTest(Config, ValidationConfig, VectorSize, Calc);
}

template <typename T> struct BinaryComparisonOps {
  static HLSLBool_t Equal(T A, T B) { return A == B; }
  static HLSLBool_t NotEqual(T A, T B) { return A != B; }
  static HLSLBool_t LessThan(T A, T B) { return A < B; }
  static HLSLBool_t LessEqual(T A, T B) { return A <= B; }
  static HLSLBool_t GreaterThan(T A, T B) { return A > B; }
  static HLSLBool_t GreaterEqual(T A, T B) { return A >= B; }
};

template <typename T>
void dispatchTestByVectorSize(
    const TestConfig<T, BinaryComparisonOpType> &Config, size_t VectorSize) {

#define DISPATCH(FUNC)                                                         \
  return dispatchBinaryTest(Config, VectorSize, BinaryComparisonOps<T>::FUNC)

  switch (Config.OpType) {
  case BinaryComparisonOpType::LessThan:
    DISPATCH(LessThan);

  case BinaryComparisonOpType::LessEqual:
    DISPATCH(LessEqual);

  case BinaryComparisonOpType::GreaterThan:
    DISPATCH(GreaterThan);

  case BinaryComparisonOpType::GreaterEqual:
    DISPATCH(GreaterEqual);

  case BinaryComparisonOpType::Equal:
    DISPATCH(Equal);

  case BinaryComparisonOpType::NotEqual:
    DISPATCH(NotEqual);

  case BinaryComparisonOpType::EnumValueCount:
    break;
  }

#undef DISPATCH

  DXASSERT(false, "Invalid OpType");
}

//
// BitwiseOpType
//

enum class BitwiseOpType {
  And,
  Or,
  Xor,
  Not,
  LeftShift,
  RightShift,
  EnumValueCount
};

static const OpTypeMetaData<BitwiseOpType> bitwiseOpTypeStringToOpMetaData[] = {
    {L"BitwiseOpType_And", BitwiseOpType::And, std::nullopt, "&"},
    {L"BitwiseOpType_Or", BitwiseOpType::Or, std::nullopt, "|"},
    {L"BitwiseOpType_Xor", BitwiseOpType::Xor, std::nullopt, "^"},
    {L"BitwiseOpType_Not", BitwiseOpType::Not, "TestUnaryOperator", "~"},
    {L"BitwiseOpType_LeftShift", BitwiseOpType::LeftShift, std::nullopt, "<<"},
    {L"BitwiseOpType_RightShift", BitwiseOpType::RightShift, std::nullopt,
     ">>"},
};

static_assert(_countof(bitwiseOpTypeStringToOpMetaData) ==
                  (size_t)BitwiseOpType::EnumValueCount,
              "bitwiseOpTypeStringToOpMetaData size mismatch. Did you "
              "add a new enum value?");

OP_TYPE_META_DATA(BitwiseOpType, bitwiseOpTypeStringToOpMetaData);

template <typename T, typename OUT_TYPE>
void dispatchBitwiseOpTest(const TestConfig<T, BitwiseOpType> &Config,
                           size_t VectorSize, OUT_TYPE (*Calc)(T, T)) {
  ValidationConfig ValidationConfig{};
  dispatchBinaryTest(Config, ValidationConfig, VectorSize, Calc);
}

template <typename T> struct BitwiseOps {
  static T And(T A, T B) { return A & B; }
  static T Or(T A, T B) { return A | B; }
  static T Xor(T A, T B) { return A ^ B; }
  static T LeftShift(T A, T B) { return A << B; }
  static T RightShift(T A, T B) { return A >> B; }
  static T Not(T A) { return ~A; }
};

template <typename T>
void dispatchTestByVectorSize(const TestConfig<T, BitwiseOpType> &Config,
                              size_t VectorSize) {

#define DISPATCH(FUNC)                                                         \
  return dispatchBinaryTest(Config, VectorSize, BitwiseOps<T>::FUNC)

#define DISPATCH_NOT(FUNC)                                                     \
  return dispatchUnaryTest(Config, VectorSize, BitwiseOps<T>::FUNC,            \
                           "-DFUNC_UNARY_OPERATOR=1")

  switch (Config.OpType) {
  case BitwiseOpType::And:
    DISPATCH(And);

  case BitwiseOpType::Or:
    DISPATCH(Or);

  case BitwiseOpType::Xor:
    DISPATCH(Xor);

  case BitwiseOpType::Not:
    DISPATCH_NOT(Not);

  case BitwiseOpType::LeftShift:
    DISPATCH(LeftShift);

  case BitwiseOpType::RightShift:
    DISPATCH(RightShift);

  case BitwiseOpType::EnumValueCount:
    break;
  }

#undef DISPATCH
#undef DISPATCH_NOT

  DXASSERT(false, "Invalid OpType");
}
#endif

// TernaryMathOpType
//

enum class TernaryMathOpType { Fma, Mad, SmoothStep, EnumValueCount };

static const OpTypeMetaData<TernaryMathOpType>
    ternaryMathOpTypeStringToOpMetaData[] = {
        {L"TernaryMathOpType_Fma", TernaryMathOpType::Fma, "fma"},
        {L"TernaryMathOpType_Mad", TernaryMathOpType::Mad, "mad"},
        {L"TernaryMathOpType_SmoothStep", TernaryMathOpType::SmoothStep,
         "smoothstep"},
};

static_assert(_countof(ternaryMathOpTypeStringToOpMetaData) ==
                  (size_t)TernaryMathOpType::EnumValueCount,
              "ternaryMathOpTypeStringToOpMetaData size mismatch. Did you add "
              "a new enum value?");

OP_TYPE_META_DATA(TernaryMathOpType, ternaryMathOpTypeStringToOpMetaData);

template <typename T, TernaryMathOpType OP, typename OUT_TYPE>
void dispatchTernaryMathOpTest(
    const TestConfig<T, TernaryMathOpType, OP> &Config, size_t VectorSize,
    OUT_TYPE (*Calc)(T, T, T)) {

  ValidationConfig ValidationConfig;

  if (isFloatingPointType<T>())
    ValidationConfig = ValidationConfig::Ulp(1.0);

  InputSets<T, 3> Inputs = buildTestInputs<3>(Config, VectorSize);

  std::vector<OUT_TYPE> Expected;
  Expected.reserve(Inputs[0].size());

  for (size_t I = 0; I < Inputs[0].size(); ++I) {
    size_t Index1 = (Config.ScalarInputFlags & (1 << 1)) ? 0 : I;
    size_t Index2 = (Config.ScalarInputFlags & (1 << 2)) ? 0 : I;
    Expected.push_back(
        Calc(Inputs[0][I], Inputs[1][Index1], Inputs[2][Index2]));
  }

  runAndVerify(Config, Inputs, Expected, "", ValidationConfig);
}

namespace TernaryMathOps {

template <typename T> T Fma(T, T, T);
template <> double Fma(double A, double B, double C) { return A * B + C; }

template <typename T> T Mad(T A, T B, T C) { return A * B + C; }

template <typename T> T SmoothStep(T Min, T Max, T X) {
  DXASSERT_NOMSG(Min < Max);

  if (X <= Min)
    return 0.0;
  if (X >= Max)
    return 1.0;

  T NormalizedX = (X - Min) / (Max - Min);
  NormalizedX = std::clamp(NormalizedX, T(0.0), T(1.0));
  return NormalizedX * NormalizedX * (T(3.0) - T(2.0) * NormalizedX);
}

} // namespace TernaryMathOps

template <typename T, TernaryMathOpType OP>
void dispatchTestByVectorSize(
    const TestConfig<T, TernaryMathOpType, OP> &Config, size_t VectorSize) {
  (void)Config;
  (void)VectorSize;
  static_assert(false, "Unsupported configuration");
}

template <>
void dispatchTestByVectorSize(
    const TestConfig<double, TernaryMathOpType, TernaryMathOpType::Fma> &Config,
    size_t VectorSize) {
  dispatchTernaryMathOpTest(Config, VectorSize, TernaryMathOps::Fma<double>);
}

template <typename T>
typename std::enable_if_t<!std::is_same_v<T, HLSLBool_t>, void>
dispatchTestByVectorSize(
    const TestConfig<T, TernaryMathOpType, TernaryMathOpType::Mad> &Config,
    size_t VectorSize) {
  dispatchTernaryMathOpTest(Config, VectorSize, TernaryMathOps::Mad<T>);
}

template <typename T>
typename std::enable_if_t<
    std::is_same_v<T, float> || std::is_same_v<T, HLSLHalf_t>, void>
dispatchTestByVectorSize(
    const TestConfig<T, TernaryMathOpType, TernaryMathOpType::SmoothStep>
        &Config,
    size_t VectorSize) {
  dispatchTernaryMathOpTest(Config, VectorSize, TernaryMathOps::SmoothStep<T>);
}

//
// dispatchTest
//

template <typename T, typename OP_TYPE, OP_TYPE OP>
void dispatchTest(const TestConfig<T, OP_TYPE, OP> &Config) {
  std::vector<size_t> InputVectorSizes;
  if (Config.LongVectorInputSize)
    InputVectorSizes.push_back(Config.LongVectorInputSize);
  else
    InputVectorSizes = {3, 4, 5, 16, 17, 35, 100, 256, 1024};

  for (size_t VectorSize : InputVectorSizes)
    dispatchTestByVectorSize(Config, VectorSize);
}

// TAEF test entry points

class OpTest {
public:
  TEST_CLASS(OpTest);

  TEST_CLASS_SETUP(classSetup) {
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
        WEX::Logging::Log::Comment(
            L"Verbose logging is enabled for this test.");
      else
        WEX::Logging::Log::Comment(
            L"Verbose logging is disabled for this test.");
    }

    return true;
  }

  template <typename T, typename OP_TYPE, OP_TYPE OP>
  void runTest(uint16_t ScalarInputFlags) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    dispatchTest(TestConfig<T, OP_TYPE, OP>(VerboseLogging, ScalarInputFlags));
  }

  // #define OP_TESTS(name, type, table)                                            \
//   TEST_METHOD(name) {                                                          \
//     BEGIN_TEST_METHOD_PROPERTIES()                                             \
//     TEST_METHOD_PROPERTY(L"DataSource", L"Table:LongVectorOpTable.xml#" table) \
//     END_TEST_METHOD_PROPERTIES();                                              \
//     runTest<type>();                                                           \
//   }

  // OP_TESTS(unaryMathOpTest, UnaryOpType, L"UnaryMathOpTable");
  // OP_TESTS(binaryOpTest, BinaryOpType, L"BinaryOpTable");
  // OP_TESTS(binaryMathOpTest, BinaryMathOpType, L"BinaryMathOpTable");
  // OP_TESTS(binaryComparisonOpTest, BinaryComparisonOpType,
  //          L"BinaryComparisonOpTable");
  // OP_TESTS(bitwiseOpTest, BitwiseOpType, L"bitwiseOpTable");
  // OP_TESTS(ternaryMathOpTest, TernaryMathOpType, L"TernaryMathOpTable");
  // OP_TESTS(trigonometricOpTest, TrigonometricOpType,
  // L"TrigonometricOpTable"); OP_TESTS(unaryOpTest, UnaryOpType,
  // L"UnaryOpTable"); OP_TESTS(asTypeOpTest, AsTypeOpType, L"AsTypeOpTable");

#define DESCRIPTION(Base, Op, DataType, Variant)                               \
  Base " " #Op " (" #DataType " " #Variant ")"

#define VARIANT_NAME_Vector
#define VARIANT_NAME_ScalarOp2 _ScalarOp2
#define VARIANT_NAME_ScalarOp3 _ScalarOp3

#define VARIANT_VALUE_Vector 0
#define VARIANT_VALUE_ScalarOp2 2
#define VARIANT_VALUE_ScalarOp3 4

#define VARIANT_NAME(v) VARIANT_NAME_##v

#define CONCAT(a, b) CONCAT_I(a, b)
#define CONCAT_I(a, b) a##b

#define METHOD_NAME(OpType, Op, DataType, Variant)                             \
  CONCAT(OpType##_##Op##_##DataType, VARIANT_NAME(Variant))

#define TEST_NAME(Op, DataType, Variant) KITS_TESTNAME #DataType " - " #Variant

#define HLK_TEST_METHOD(MethodName, Description, Specification, TestName,      \
                        Guid, OpType, Op, Variant, DataType)                   \
  TEST_METHOD(MethodName) {                                                    \
    BEGIN_TEST_METHOD_PROPERTIES()                                             \
    TEST_METHOD_PROPERTY("Kits.Description", Description)                      \
    TEST_METHOD_PROPERTY("Kits.Specification", Specification)                  \
    TEST_METHOD_PROPERTY("Kits.TestName", TestName)                            \
    TEST_METHOD_PROPERTY("Kits.TestId", Guid)                                  \
    END_TEST_METHOD_PROPERTIES();                                              \
    runTest<DataType, OpType, OpType ::Op>(Variant);                           \
  }

#define HLK_TEST(OpType, Op, DataType, Variant, GUID)                          \
  HLK_TEST_METHOD(METHOD_NAME(OpType, Op, DataType, Variant),                  \
                  DESCRIPTION(KITS_DESCRIPTION, Op, DataType, Variant),        \
                  KITS_SPECIFICATION, TEST_NAME(Op, DataType, Variant), GUID,  \
                  OpType, Op, VARIANT_VALUE_##Variant, DataType)

#define KITS_DESCRIPTION "Verifies the vectorized DXIL instruction"
#define KITS_SPECIFICATION                                                     \
  "Device.Graphics.WDDMXXX.AdapterRender.D3D12.DXILCore.ShaderModel69."        \
  "CoreRequirement"

#define KITS_TESTNAME "D3D12 - Shader Model 6.9 - vectorized DXIL - "

  HLK_TEST(TernaryMathOpType, Mad, uint16_t, Vector,
           "296dae2f-4823-4c64-874c-d7e8b1e5e53b");
  HLK_TEST(TernaryMathOpType, Mad, uint16_t, ScalarOp3,
           "1adf820c-86d7-483e-86b4-d5398a8b8a32");
  HLK_TEST(TernaryMathOpType, Mad, uint32_t, Vector,
           "df66f9cd-268e-4509-90c5-a7f39fea2757");
  HLK_TEST(TernaryMathOpType, Mad, uint32_t, ScalarOp2,
           "4208fb2e-df8b-4c65-8889-f4acbb04a263");
  HLK_TEST(TernaryMathOpType, Mad, uint64_t, Vector,
           "1459088f-e647-4f7c-8cc7-a6673e2277ae");
  HLK_TEST(TernaryMathOpType, Mad, uint64_t, ScalarOp3,
           "cc002bb6-9e55-460a-a381-8ff259cd103f");
  HLK_TEST(TernaryMathOpType, Mad, int16_t, Vector,
           "5e2f0dd1-4c4e-4b3e-9c5c-4d1b6e9b6f10");
  HLK_TEST(TernaryMathOpType, Mad, int16_t, ScalarOp2,
           "e1c8a2f7-9b1c-4c5f-8f0d-2fb0c5d3e6a4");
  HLK_TEST(TernaryMathOpType, Mad, int32_t, Vector,
           "3a4d7c2b-2e5a-4f0a-9b1d-6c7e8f9a0b1c");
  HLK_TEST(TernaryMathOpType, Mad, int32_t, ScalarOp2,
           "9b8a7c6d-5e4f-4d3c-8b2a-1c0d9e8f7a6b");
  HLK_TEST(TernaryMathOpType, Mad, int64_t, Vector,
           "0f1e2d3c-4b5a-6978-8899-aabbccddeeff");
  HLK_TEST(TernaryMathOpType, Mad, int64_t, ScalarOp3,
           "11223344-5566-4788-99aa-bbccddeeff00");
  HLK_TEST(TernaryMathOpType, Mad, HLSLHalf_t, Vector,
           "7c6b5a49-3847-2615-9d8c-7b6a59483726");
  HLK_TEST(TernaryMathOpType, Mad, HLSLHalf_t, ScalarOp2,
           "d4c3b2a1-0f9e-8d7c-6b5a-493827161504");
  HLK_TEST(TernaryMathOpType, SmoothStep, HLSLHalf_t, Vector,
           "2468ace0-1357-49b1-8d2f-3a5c7e9f1b3d");
  HLK_TEST(TernaryMathOpType, SmoothStep, HLSLHalf_t, ScalarOp2,
           "13579bdf-2468-4ace-9bdf-246813579bdf");
  HLK_TEST(TernaryMathOpType, Mad, float, Vector,
           "89abcdef-0123-4567-89ab-cdef01234567");
  HLK_TEST(TernaryMathOpType, Mad, float, ScalarOp2,
           "fedcba98-7654-3210-fedc-ba9876543210");
  HLK_TEST(TernaryMathOpType, SmoothStep, float, Vector,
           "4b3a2918-1706-5f4e-3d2c-1b0a99887766");
  HLK_TEST(TernaryMathOpType, SmoothStep, float, ScalarOp3,
           "66778899-aabb-4ccd-affe-112233445566");
  HLK_TEST(TernaryMathOpType, Fma, double, Vector,
           "a1b2c3d4-e5f6-47a8-90ab-cdef12345678");
  HLK_TEST(TernaryMathOpType, Fma, double, ScalarOp2,
           "0a9b8c7d-6e5f-4d3c-2b1a-998877665544");
  HLK_TEST(TernaryMathOpType, Mad, double, Vector,
           "10293847-5647-4839-9abc-def012345678");
  HLK_TEST(TernaryMathOpType, Mad, double, ScalarOp2,
           "88776655-4433-4211-a0b9-8c7d6e5f4d3c");

private:
  bool Initialized = false;
  bool VerboseLogging = false;
};

} // namespace LongVector
