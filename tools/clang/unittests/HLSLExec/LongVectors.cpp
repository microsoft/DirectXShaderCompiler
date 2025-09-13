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
// Input Sets
//

enum class InputSet {
#define INPUT_SET(SYMBOL, NAME) SYMBOL,
#include "LongVectorOps.def"
};

const wchar_t *getInputSetName(InputSet InputSet) {
  switch (InputSet) {
#define INPUT_SET(SYMBOL, NAME)                                                \
  case InputSet::SYMBOL:                                                       \
    return L##NAME;
#include "LongVectorOps.def"
  }
  DXASSERT_NOMSG(false);
  return L"<Invalid Input Set>";
}

//
// Operation Types
//

enum class OpType {
#define OP(SYMBOL, ARITY, INTRINSIC, OPERATOR, INPUT_SET_1, INPUT_SET_2,       \
           INPUT_SET_3)                                                        \
  SYMBOL,
#include "LongVectorOps.def"
};

template <OpType OP> struct OpTraits;

#define OP(SYMBOL, ARITY, INTRINSIC, OPERATOR, INPUT_SET_1, INPUT_SET_2,       \
           INPUT_SET_3)                                                        \
  template <> struct OpTraits<OpType::SYMBOL> {                                \
    static constexpr size_t Arity = ARITY;                                     \
    static constexpr const char *Intrinsic = INTRINSIC;                        \
    static constexpr const char *Operator = OPERATOR;                          \
    static constexpr InputSet InputSets[3] = {                                 \
        InputSet::INPUT_SET_1, InputSet::INPUT_SET_2, InputSet::INPUT_SET_3};  \
  };
#include "LongVectorOps.def"

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

template <typename T, OpType OP> struct TestConfig {
  using String = WEX::Common::String;

  bool VerboseLogging;
  uint16_t ScalarInputFlags;

  size_t LongVectorInputSize = 0;

  TestConfig(bool VerboseLogging, uint16_t ScalarInputFlags)
      : VerboseLogging(VerboseLogging), ScalarInputFlags(ScalarInputFlags) {
    using WEX::TestExecution::RuntimeParameters;

    RuntimeParameters::TryGetValue(L"LongVectorInputSize", LongVectorInputSize);
  }
};

template <typename T, size_t ARITY>
using InputSets = std::array<std::vector<T>, ARITY>;

template <OpType OP, typename T, typename OUT_TYPE>
std::string getCompilerOptionsString(size_t VectorSize,
                                     uint16_t ScalarInputFlags,
                                     std::string ExtraDefines) {
  using OpTraits = OpTraits<OP>;

  std::stringstream CompilerOptions;

  if (is16BitType<T>())
    CompilerOptions << " -enable-16bit-types";

  CompilerOptions << " -DTYPE=" << getHLSLTypeString<T>();
  CompilerOptions << " -DNUM=" << VectorSize;

  CompilerOptions << " -DOPERATOR=";
  CompilerOptions << OpTraits::Operator;

  CompilerOptions << " -DFUNC=";
  CompilerOptions << OpTraits::Intrinsic;

  CompilerOptions << " " << ExtraDefines;

  CompilerOptions << " -DOUT_TYPE=" << getHLSLTypeString<OUT_TYPE>();

  CompilerOptions << " -DBASIC_OP_TYPE=0x" << std::hex << OpTraits::Arity;

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

template <typename OUT_TYPE, typename T, OpType OP, size_t ARITY>
std::optional<std::vector<OUT_TYPE>>
runTest(const TestConfig<T, OP> &Config, const InputSets<T, ARITY> &Inputs,
        size_t ExpectedOutputSize, std::string ExtraDefines) {

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
  std::string CompilerOptionsString = getCompilerOptionsString<OP, T, OUT_TYPE>(
      Inputs[0].size(), Config.ScalarInputFlags, std::move(ExtraDefines));

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
std::vector<T> buildTestInput(InputSet InputSet, size_t SizeToTest) {
  const std::vector<T> &RawValueSet =
      TestData<T>::Data.at(getInputSetName(InputSet));

  std::vector<T> ValueSet;
  ValueSet.reserve(SizeToTest);
  for (size_t I = 0; I < SizeToTest; ++I)
    ValueSet.push_back(RawValueSet[I % RawValueSet.size()]);

  return ValueSet;
}

template <typename T, OpType OP>
InputSets<T, OpTraits<OP>::Arity> buildTestInputs(size_t VectorSize,
                                                  uint16_t ScalarInputFlags) {
  constexpr size_t Arity = OpTraits<OP>::Arity;

  InputSets<T, Arity> Inputs;

  for (size_t I = 0; I < Arity; ++I) {
    uint16_t OperandScalarFlag = 1 << I;
    bool IsOperandScalar = ScalarInputFlags & OperandScalarFlag;
    Inputs[I] = buildTestInput<T>(OpTraits<OP>::InputSets[I],
                                  IsOperandScalar ? 1 : VectorSize);
  }

  return Inputs;
}

#if 0
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
#endif

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
void runAndVerify(const TestConfig<T, OP> &Config,
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

#endif

// asDouble

static double asDouble(const uint32_t LowBits, const uint32_t HighBits) {
  uint64_t Bits = (static_cast<uint64_t>(HighBits) << 32) | LowBits;
  double Result;
  std::memcpy(&Result, &Bits, sizeof(Result));
  return Result;
}

#if 0

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

//
// dispatchTest
//

template <OpType OP, typename T> struct Op;

template <OpType OP, typename T, typename Enable = void>
struct OpValidationConfig {
  static ValidationConfig get() { return ValidationConfig(); }
};

template <OpType OP, typename Enable = void> struct OpExtraDefines {
  static const char *get() { return ""; }
};

template <OpType OP, typename T>
struct OpValidationConfig<OP, T,
                          typename std::enable_if_t<isFloatingPointType<T>()>> {
  static ValidationConfig get() { return ValidationConfig::Ulp(1.0); }
};

template <> struct Op<OpType::AsDouble, uint32_t> {
  using OutT = double;

  double operator()(uint32_t Low, uint32_t High) { return asDouble(Low, High); }
};

template <> struct Op<OpType::Fma, double> {
  using OutT = double;

  double operator()(double A, double B, double C) { return A * B + C; }
};

template <typename T> struct Op<OpType::Mad, T> {
  using OutT = T;

  T operator()(T A, T B, T C) { return A * B + C; }
};

template <typename T> struct Op<OpType::SmoothStep, T> {
  using OutT = T;

  T operator()(T Min, T Max, T X) {
    DXASSERT_NOMSG(Min < Max);

    if (X <= Min)
      return 0.0;
    if (X >= Max)
      return 1.0;

    T NormalizedX = (X - Min) / (Max - Min);
    NormalizedX = std::clamp(NormalizedX, T(0.0), T(1.0));
    return NormalizedX * NormalizedX * (T(3.0) - T(2.0) * NormalizedX);
  }
};

// Binary Math Op specializations
template <typename T> struct Op<OpType::Add, T> {
  using OutT = T;
  T operator()(T A, T B) { return A + B; }
};

template <typename T> struct Op<OpType::Subtract, T> {
  using OutT = T;
  T operator()(T A, T B) { return A - B; }
};

template <typename T> struct Op<OpType::Multiply, T> {
  using OutT = T;
  T operator()(T A, T B) { return A * B; }
};

template <typename T> struct Op<OpType::Divide, T> {
  using OutT = T;
  T operator()(T A, T B) { return A / B; }
};

template <typename T, typename Enable = void> struct ModulusImpl;

// Integral types
template <typename T>
struct ModulusImpl<T, typename std::enable_if_t<!std::is_same_v<T, float>>> {
  static T eval(T A, T B) { return B == 0 ? T() : A % B; }
};

// Floating point types (float/double)
template <typename T>
struct ModulusImpl<T, typename std::enable_if_t<std::is_same_v<T, float>>> {
  static T eval(T A, T B) { return std::fmod(A, B); }
};

// HLSLHalf_t specialization: implement via float conversion
template <> struct ModulusImpl<HLSLHalf_t, void> {
  static HLSLHalf_t eval(HLSLHalf_t A, HLSLHalf_t B) {
    // Rely on available operators/constructors; perform in float for accuracy.
    float Af = static_cast<float>(A);
    float Bf = static_cast<float>(B);
    float R = std::fmod(Af, Bf);
    return HLSLHalf_t(R);
  }
};

template <typename T> struct Op<OpType::Modulus, T> {
  using OutT = T;
  T operator()(T A, T B) { return ModulusImpl<T>::eval(A, B); }
};

template <typename T> struct Op<OpType::Min, T> {
  using OutT = T;
  T operator()(T A, T B) { return std::min(A, B); }
};

template <typename T> struct Op<OpType::Max, T> {
  using OutT = T;
  T operator()(T A, T B) { return std::max(A, B); }
};

// Ldexp only defined/used for floating/half/double types in tests
template <typename T, typename Enable = void> struct LdexpImpl;

template <typename T>
struct LdexpImpl<T, typename std::enable_if_t<std::is_floating_point_v<T>>> {
  static T eval(T A, T B) { return A * static_cast<T>(std::pow(2.0, B)); }
};

template <> struct LdexpImpl<HLSLHalf_t, void> {
  static HLSLHalf_t eval(HLSLHalf_t A, HLSLHalf_t B) {
    float Af = static_cast<float>(A);
    float Bf = static_cast<float>(B);
    float R = Af * std::pow(2.0f, Bf);
    return HLSLHalf_t(R);
  }
};

template <typename T> struct Op<OpType::Ldexp, T> {
  using OutT = T;
  T operator()(T A, T B) { return LdexpImpl<T>::eval(A, B); }
};

template <OpType OP, typename T, size_t ARITY>
std::vector<T> buildExpected(const InputSets<T, ARITY> &Inputs);

template <OpType OP, typename T, typename OUT_TYPE = typename Op<OP, T>::OutT>
std::vector<OUT_TYPE> buildExpected(Op<OP, T> Op, const InputSets<T, 2> &Inputs,
                                    size_t ScalarInputFlags) {
  std::vector<OUT_TYPE> Expected;
  Expected.reserve(Inputs[0].size());

  for (size_t I = 0; I < Inputs[0].size(); ++I) {
    size_t Index1 = (ScalarInputFlags & (1 << 1)) ? 0 : I;
    Expected.push_back(Op(Inputs[0][I], Inputs[1][Index1]));
  }

  return Expected;
}

template <OpType OP, typename T, typename OUT_TYPE = typename Op<OP, T>::OutT>
std::vector<OUT_TYPE> buildExpected(Op<OP, T> Op, const InputSets<T, 3> &Inputs,
                                    size_t ScalarInputFlags) {
  std::vector<OUT_TYPE> Expected;
  Expected.reserve(Inputs[0].size());

  for (size_t I = 0; I < Inputs[0].size(); ++I) {
    size_t Index1 = (ScalarInputFlags & (1 << 1)) ? 0 : I;
    size_t Index2 = (ScalarInputFlags & (1 << 2)) ? 0 : I;
    Expected.push_back(Op(Inputs[0][I], Inputs[1][Index1], Inputs[2][Index2]));
  }

  return Expected;
}

template <typename T, OpType OP>
void dispatchTest(const TestConfig<T, OP> &Config) {
  std::vector<size_t> InputVectorSizes;
  if (Config.LongVectorInputSize)
    InputVectorSizes.push_back(Config.LongVectorInputSize);
  else
    InputVectorSizes = {3, 4, 5, 16, 17, 35, 100, 256, 1024};

  Op<OP, T> Op;

  for (size_t VectorSize : InputVectorSizes) {
    InputSets<T, OpTraits<OP>::Arity> Inputs =
        buildTestInputs<T, OP>(VectorSize, Config.ScalarInputFlags);

    auto Expected = buildExpected(Op, Inputs, Config.ScalarInputFlags);

    runAndVerify(Config, Inputs, Expected, OpExtraDefines<OP>::get(),
                 OpValidationConfig<OP, T>::get());
  }
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

  template <typename T, OpType OP> void runTest(uint16_t ScalarInputFlags) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    dispatchTest(TestConfig<T, OP>(VerboseLogging, ScalarInputFlags));
  }

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

#define METHOD_NAME(Op, DataType, Variant)                                     \
  CONCAT(Op##_##DataType, VARIANT_NAME(Variant))

#define TEST_NAME(Op, DataType, Variant) KITS_TESTNAME #DataType " - " #Variant

#define HLK_TEST_METHOD(MethodName, Description, Specification, TestName,      \
                        Guid, Op, Variant, DataType)                           \
  TEST_METHOD(MethodName) {                                                    \
    BEGIN_TEST_METHOD_PROPERTIES()                                             \
    TEST_METHOD_PROPERTY("Kits.Description", Description)                      \
    TEST_METHOD_PROPERTY("Kits.Specification", Specification)                  \
    TEST_METHOD_PROPERTY("Kits.TestName", TestName)                            \
    TEST_METHOD_PROPERTY("Kits.TestId", Guid)                                  \
    END_TEST_METHOD_PROPERTIES();                                              \
    runTest<DataType, OpType ::Op>(Variant);                                   \
  }

#define HLK_TEST(Op, DataType, Variant, GUID)                                  \
  HLK_TEST_METHOD(METHOD_NAME(Op, DataType, Variant),                          \
                  DESCRIPTION(KITS_DESCRIPTION, Op, DataType, Variant),        \
                  KITS_SPECIFICATION, TEST_NAME(Op, DataType, Variant), GUID,  \
                  Op, VARIANT_VALUE_##Variant, DataType)

#define KITS_DESCRIPTION "Verifies the vectorized DXIL instruction"
#define KITS_SPECIFICATION                                                     \
  "Device.Graphics.WDDMXXX.AdapterRender.D3D12.DXILCore.ShaderModel69."        \
  "CoreRequirement"

#define KITS_TESTNAME "D3D12 - Shader Model 6.9 - vectorized DXIL - "

  HLK_TEST(AsDouble, uint32_t, Vector, "0d09d921-05e0-41b8-9e48-d5828cd54f1d");
  HLK_TEST(AsDouble, uint32_t, ScalarOp2,
           "2a744949-9a64-4d1a-ac46-eb4dec05676c");

  // Ternary Ops
  HLK_TEST(Mad, uint16_t, Vector, "296dae2f-4823-4c64-874c-d7e8b1e5e53b");
  HLK_TEST(Mad, uint16_t, ScalarOp3, "1adf820c-86d7-483e-86b4-d5398a8b8a32");
  HLK_TEST(Mad, uint32_t, Vector, "df66f9cd-268e-4509-90c5-a7f39fea2757");
  HLK_TEST(Mad, uint32_t, ScalarOp2, "4208fb2e-df8b-4c65-8889-f4acbb04a263");
  HLK_TEST(Mad, uint64_t, Vector, "1459088f-e647-4f7c-8cc7-a6673e2277ae");
  HLK_TEST(Mad, uint64_t, ScalarOp3, "cc002bb6-9e55-460a-a381-8ff259cd103f");
  HLK_TEST(Mad, int16_t, Vector, "5e2f0dd1-4c4e-4b3e-9c5c-4d1b6e9b6f10");
  HLK_TEST(Mad, int16_t, ScalarOp2, "e1c8a2f7-9b1c-4c5f-8f0d-2fb0c5d3e6a4");
  HLK_TEST(Mad, int32_t, Vector, "3a4d7c2b-2e5a-4f0a-9b1d-6c7e8f9a0b1c");
  HLK_TEST(Mad, int32_t, ScalarOp2, "9b8a7c6d-5e4f-4d3c-8b2a-1c0d9e8f7a6b");
  HLK_TEST(Mad, int64_t, Vector, "0f1e2d3c-4b5a-6978-8899-aabbccddeeff");
  HLK_TEST(Mad, int64_t, ScalarOp3, "11223344-5566-4788-99aa-bbccddeeff00");
  HLK_TEST(Mad, HLSLHalf_t, Vector, "7c6b5a49-3847-2615-9d8c-7b6a59483726");
  HLK_TEST(Mad, HLSLHalf_t, ScalarOp2, "d4c3b2a1-0f9e-8d7c-6b5a-493827161504");
  HLK_TEST(SmoothStep, HLSLHalf_t, Vector,
           "2468ace0-1357-49b1-8d2f-3a5c7e9f1b3d");
  HLK_TEST(SmoothStep, HLSLHalf_t, ScalarOp2,
           "13579bdf-2468-4ace-9bdf-246813579bdf");
  HLK_TEST(Mad, float, Vector, "89abcdef-0123-4567-89ab-cdef01234567");
  HLK_TEST(Mad, float, ScalarOp2, "fedcba98-7654-3210-fedc-ba9876543210");
  HLK_TEST(SmoothStep, float, Vector, "4b3a2918-1706-5f4e-3d2c-1b0a99887766");
  HLK_TEST(SmoothStep, float, ScalarOp3,
           "66778899-aabb-4ccd-affe-112233445566");
  HLK_TEST(Fma, double, Vector, "a1b2c3d4-e5f6-47a8-90ab-cdef12345678");
  HLK_TEST(Fma, double, ScalarOp2, "0a9b8c7d-6e5f-4d3c-2b1a-998877665544");
  HLK_TEST(Mad, double, Vector, "10293847-5647-4839-9abc-def012345678");
  HLK_TEST(Mad, double, ScalarOp2, "88776655-4433-4211-a0b9-8c7d6e5f4d3c");

  // Binary Math Ops
  HLK_TEST(Add, double, ScalarOp2, "fc5b16e2-e398-4b3b-af58-7586ad949043");
  HLK_TEST(Add, double, Vector, "9002df61-5dc9-4cc9-834d-94cb85893894");
  HLK_TEST(Add, float, ScalarOp2, "9dd248ea-0fe4-45bf-ae60-560f4ec343ac");
  HLK_TEST(Add, float, Vector, "cdcee025-ab25-4c1d-843a-9f627f725ed3");
  HLK_TEST(Add, HLSLBool_t, ScalarOp2, "8b3b11c0-3d72-45ef-b9dc-d6f9ad6a8fef");
  HLK_TEST(Add, HLSLBool_t, Vector, "6711ab2b-52d7-4d22-b78c-f7f03503c231");
  HLK_TEST(Add, HLSLHalf_t, ScalarOp2, "7e63613a-6fcf-46ad-b53e-9eca1d2a0bc5");
  HLK_TEST(Add, HLSLHalf_t, Vector, "86f9e8e3-f4ff-4e19-9056-92d64338e735");
  HLK_TEST(Add, int16_t, ScalarOp2, "2af4a24c-4b89-494d-bc7a-38c3924d48b0");
  HLK_TEST(Add, int16_t, Vector, "cb447b5c-dcbf-4d00-a76c-26202ab77cbc");
  HLK_TEST(Add, int32_t, ScalarOp2, "4f5e31de-b455-4be7-8a0f-323d40b140bf");
  HLK_TEST(Add, int32_t, Vector, "7636e751-6381-4f3d-b5e1-25d4a9abe8e1");
  HLK_TEST(Add, int64_t, ScalarOp2, "ae4fb81a-af11-4c8c-880c-90a0fcbb6d2e");
  HLK_TEST(Add, int64_t, Vector, "4d23fdb5-a8d6-41c1-986a-ddc9f4fb6a18");
  HLK_TEST(Add, uint16_t, ScalarOp2, "1820446d-6686-411e-bd65-cf1587b1c09d");
  HLK_TEST(Add, uint16_t, Vector, "8e1e9477-b6f1-4651-bb7a-791ce8b94fb3");
  HLK_TEST(Add, uint32_t, ScalarOp2, "50f5eecc-7921-4bea-9e2a-24cc8539ea08");
  HLK_TEST(Add, uint32_t, Vector, "672d59c6-e03b-48bd-9fe0-a323ccb884cf");
  HLK_TEST(Add, uint64_t, ScalarOp2, "e2191b64-4e39-462f-97c6-79e5b2f1ebf3");
  HLK_TEST(Add, uint64_t, Vector, "b6eaf5b3-3a8d-423c-b389-3378d3f2e3c4");
  HLK_TEST(Divide, double, ScalarOp2, "a14b0a69-2784-4099-8f0a-02b15d6ab3e1");
  HLK_TEST(Divide, double, Vector, "125f1c8d-9353-4f9c-b57d-2b289ceb5eb0");
  HLK_TEST(Divide, float, ScalarOp2, "285051cd-51d8-4434-a888-235827f08a55");
  HLK_TEST(Divide, float, Vector, "dbbac563-f9fa-41ca-ad65-4dd175766c06");
  HLK_TEST(Divide, HLSLHalf_t, ScalarOp2,
           "7bcb8465-28cc-496f-b5fa-468f8fcb35b8");
  HLK_TEST(Divide, HLSLHalf_t, Vector, "fbef4566-7c3e-4e78-be75-55b02edccec2");
  HLK_TEST(Divide, int16_t, ScalarOp2, "65f26857-5918-4995-9f2b-4155e5e1e7d7");
  HLK_TEST(Divide, int16_t, Vector, "3a9da95a-41f2-401b-afcf-58cb2fe90620");
  HLK_TEST(Divide, int32_t, ScalarOp2, "bc4847a3-aa0d-4790-8103-434cd0b3f13c");
  HLK_TEST(Divide, int32_t, Vector, "08876e71-e784-442b-8a61-f7ef1cefb7cb");
  HLK_TEST(Divide, int64_t, ScalarOp2, "fa70342d-db55-43a6-a85a-eca51f0687aa");
  HLK_TEST(Divide, int64_t, Vector, "7d2d1bce-cb8d-4663-9ed6-ff2a173a6a59");
  HLK_TEST(Divide, uint16_t, ScalarOp2, "76e3758e-01cf-4942-ab24-8c1c9df02e92");
  HLK_TEST(Divide, uint16_t, Vector, "810bdaa5-367a-40a2-9393-670df63222b5");
  HLK_TEST(Divide, uint32_t, ScalarOp2, "fe2f6c02-939c-4485-be43-aa9fbac860b9");
  HLK_TEST(Divide, uint32_t, Vector, "3dadb312-a228-4a99-abb2-c46479783315");
  HLK_TEST(Divide, uint64_t, ScalarOp2, "a8400ddc-c09e-4ae1-af86-1a5e529bcfb4");
  HLK_TEST(Divide, uint64_t, Vector, "0033036f-afd2-4ea8-b39a-15ebc94bd846");
  HLK_TEST(Ldexp, double, ScalarOp2, "879e5d73-111f-4bfd-941d-7e4b4030989a");
  HLK_TEST(Ldexp, double, Vector, "6d365a18-fae0-4efc-b22b-b0170589b2c7");
  HLK_TEST(Ldexp, float, ScalarOp2, "afa40b06-6db8-4528-a4e3-882d7b70c706");
  HLK_TEST(Ldexp, float, Vector, "9af9e99c-fa41-4cc6-8433-f6f171c4523f");
  HLK_TEST(Ldexp, HLSLHalf_t, ScalarOp2,
           "bc225589-8450-4645-acc6-951738ca2b4f");
  HLK_TEST(Ldexp, HLSLHalf_t, Vector, "cf3e13ab-a029-426c-bcc3-ffd5bb55eaa3");
  HLK_TEST(Max, double, ScalarOp2, "427a7ec9-706c-4985-9545-1f08c5370d94");
  HLK_TEST(Max, double, Vector, "8a8bbe9f-f75b-4404-bbbe-4ef635f5dc50");
  HLK_TEST(Max, float, ScalarOp2, "be120240-dada-4bb3-92cc-f19325ab1085");
  HLK_TEST(Max, float, Vector, "d5f7c684-fccf-49b3-84fc-84cb96a97112");
  HLK_TEST(Max, HLSLHalf_t, ScalarOp2, "ca118ccd-d6e5-4bcb-8da1-b6839c00fabc");
  HLK_TEST(Max, HLSLHalf_t, Vector, "0df0569b-b75d-405a-8c5f-091a63d7ab46");
  HLK_TEST(Max, int16_t, ScalarOp2, "4fff4bd1-cad0-4a73-91e8-79750f144171");
  HLK_TEST(Max, int16_t, Vector, "8fd63f6f-37a3-47c5-b440-279b79392004");
  HLK_TEST(Max, int32_t, ScalarOp2, "0094d003-1807-4201-a64d-e2a40b9447ca");
  HLK_TEST(Max, int32_t, Vector, "18c79aa4-6046-4189-b4d4-8e3381f8978e");
  HLK_TEST(Max, int64_t, ScalarOp2, "b14560de-3951-4308-b50d-6fd8da0c61b0");
  HLK_TEST(Max, int64_t, Vector, "9b003ad3-eed7-46b6-8e2b-b26df14cd9e0");
  HLK_TEST(Max, uint16_t, ScalarOp2, "7e9e55a0-d06a-47f1-b474-b130232c676e");
  HLK_TEST(Max, uint16_t, Vector, "2e700c36-8878-4ef0-8894-0b2545d4c71a");
  HLK_TEST(Max, uint32_t, ScalarOp2, "8886675c-63e9-427c-9794-b7c5c240f497");
  HLK_TEST(Max, uint32_t, Vector, "fe4dc347-e1eb-473b-ac45-fbba11401acf");
  HLK_TEST(Max, uint64_t, ScalarOp2, "58b4e38f-5a2e-43b8-9612-6522d7f3f177");
  HLK_TEST(Max, uint64_t, Vector, "6098a603-d0f7-4ec3-8fab-2453e6e767f3");
  HLK_TEST(Min, double, ScalarOp2, "c91c73f1-bc9a-4c28-a1f5-916f15d1ea91");
  HLK_TEST(Min, double, Vector, "48d82112-83cf-445e-9d30-849896fa66e7");
  HLK_TEST(Min, float, ScalarOp2, "4307ecea-cef5-426b-ba1c-095dc84beab5");
  HLK_TEST(Min, float, Vector, "2e415508-a053-4a14-a443-4d75e0c5d83d");
  HLK_TEST(Min, HLSLHalf_t, ScalarOp2, "5b269d4c-73c8-40a5-90ae-607a11f3f575");
  HLK_TEST(Min, HLSLHalf_t, Vector, "a5a05ae8-f535-485d-b435-12a230ad724b");
  HLK_TEST(Min, int16_t, ScalarOp2, "cb16bd20-4be5-408a-994b-8884a7e865be");
  HLK_TEST(Min, int16_t, Vector, "c176ed78-38f6-4109-b35a-cd4aa91f9351");
  HLK_TEST(Min, int32_t, ScalarOp2, "bfb20cf9-2463-4995-83d1-5565e96dadf0");
  HLK_TEST(Min, int32_t, Vector, "297ddfdd-0a28-4314-a148-9e5373c83dd5");
  HLK_TEST(Min, int64_t, ScalarOp2, "bddbbd2a-28d0-4ebf-8dc7-edc7766f6b9e");
  HLK_TEST(Min, int64_t, Vector, "386fb18f-36b7-4111-98c1-931e8d0b4c93");
  HLK_TEST(Min, uint16_t, ScalarOp2, "22b9167c-3e2a-4176-81bb-d55f4408b7ff");
  HLK_TEST(Min, uint16_t, Vector, "83940c59-590f-4947-8409-3112d8158a3a");
  HLK_TEST(Min, uint32_t, ScalarOp2, "aff848e7-fc7f-4389-aeb0-7afb69ace6ba");
  HLK_TEST(Min, uint32_t, Vector, "7d3dc677-14e9-491e-aa6d-ddd164cf837e");
  HLK_TEST(Min, uint64_t, ScalarOp2, "f2706239-f32a-48ff-8b22-ba7d53dc590d");
  HLK_TEST(Min, uint64_t, Vector, "65085c72-59c7-49f6-b06e-c36afcaa7682");
  HLK_TEST(Modulus, float, ScalarOp2, "9497f077-20c9-4d7f-b54d-59bab1e272c5");
  HLK_TEST(Modulus, float, Vector, "fac61889-472e-43ab-939a-2fea74e40579");
  HLK_TEST(Modulus, HLSLHalf_t, ScalarOp2,
           "3c37a29a-2a37-4ac3-864f-bc14cd222d08");
  HLK_TEST(Modulus, HLSLHalf_t, Vector, "9bb858cd-f9a3-40ff-ad61-6843cc50e42f");
  HLK_TEST(Modulus, int16_t, ScalarOp2, "fdb76fd6-c8a6-46e6-b3a7-b7a181ee5875");
  HLK_TEST(Modulus, int16_t, Vector, "5fd8d74b-0842-4eb1-bb95-77a0bd209d5c");
  HLK_TEST(Modulus, int32_t, ScalarOp2, "a588f3a1-6429-46d6-b578-9b8fcf7918f0");
  HLK_TEST(Modulus, int32_t, Vector, "f6285d1b-3394-470e-b780-6534d9693152");
  HLK_TEST(Modulus, int64_t, ScalarOp2, "e78322e4-ef26-4ec5-bb40-a108c86214f6");
  HLK_TEST(Modulus, int64_t, Vector, "7bf6c0a2-d1b6-4c8a-9a52-e6675fa18cff");
  HLK_TEST(Modulus, uint16_t, ScalarOp2,
           "81cd677e-43f7-460c-bad6-d14a0cbc6697");
  HLK_TEST(Modulus, uint16_t, Vector, "439a501f-e0b2-44f2-8bb3-1a951cbc5602");
  HLK_TEST(Modulus, uint32_t, ScalarOp2,
           "0977f3ea-2e93-4e19-b675-5393c642a80c");
  HLK_TEST(Modulus, uint32_t, Vector, "8ac45c14-4bae-4c91-8109-0359120aa95f");
  HLK_TEST(Modulus, uint64_t, ScalarOp2,
           "d51f3a79-b08b-4df5-9c26-e30cc94ec043");
  HLK_TEST(Modulus, uint64_t, Vector, "14366e55-41c0-4949-b883-6872bb2fe8dc");
  HLK_TEST(Multiply, double, ScalarOp2, "94528704-df73-4708-99f7-1fbb7a9ee455");
  HLK_TEST(Multiply, double, Vector, "59fddc0c-ccda-450e-8ba2-91f9ad798afe");
  HLK_TEST(Multiply, float, ScalarOp2, "fdaf7f64-e1df-4fa8-b735-a9461d0216a6");
  HLK_TEST(Multiply, float, Vector, "6e910978-a365-4e79-a9dd-8969f13c3704");
  HLK_TEST(Multiply, HLSLHalf_t, ScalarOp2,
           "86adfd21-a80d-4af4-a6de-1244c8a2c480");
  HLK_TEST(Multiply, HLSLHalf_t, Vector,
           "2f9169fb-6fa9-418c-ad9f-18fa75102649");
  HLK_TEST(Multiply, int16_t, ScalarOp2,
           "945270f7-df0f-4a4d-b770-1800150a7340");
  HLK_TEST(Multiply, int16_t, Vector, "9b07d135-863b-4d78-85d6-d7e0788d1ef5");
  HLK_TEST(Multiply, int32_t, ScalarOp2,
           "d42cbac7-beb8-4908-86af-3fe175513040");
  HLK_TEST(Multiply, int32_t, Vector, "3f010d7e-ebe7-4a1b-8474-2c3c41cdec7b");
  HLK_TEST(Multiply, int64_t, ScalarOp2,
           "04a11319-18ff-42a7-be0f-aa665696b281");
  HLK_TEST(Multiply, int64_t, Vector, "4dc8cabb-46cf-4af3-a44b-985a3cec804e");
  HLK_TEST(Multiply, uint16_t, ScalarOp2,
           "437fa9b7-53b8-4e7e-927f-302f4b28e947");
  HLK_TEST(Multiply, uint16_t, Vector, "ea7dc62b-00e1-4ee2-9afc-7980cdb32aa9");
  HLK_TEST(Multiply, uint32_t, ScalarOp2,
           "0f4b8e96-0c01-42b3-818a-4b2b93d02214");
  HLK_TEST(Multiply, uint32_t, Vector, "7ececacf-6f81-416e-8047-1e456f567206");
  HLK_TEST(Multiply, uint64_t, ScalarOp2,
           "2e32d339-9310-4799-a977-bfa91c1f195f");
  HLK_TEST(Multiply, uint64_t, Vector, "4e1f9a72-5637-4d2a-a176-c1a49e8876b8");
  HLK_TEST(Subtract, double, ScalarOp2, "e87606b7-db44-4a0c-9d77-f5ef235965a5");
  HLK_TEST(Subtract, double, Vector, "eee77704-a27b-4a12-914e-059651893979");
  HLK_TEST(Subtract, float, ScalarOp2, "643a8bb5-c0e3-45dd-b14e-15661cb613d7");
  HLK_TEST(Subtract, float, Vector, "b17076d0-d14a-45c8-a934-db5a00b5eeb7");
  HLK_TEST(Subtract, HLSLBool_t, ScalarOp2,
           "89f74c7e-7fd6-458c-8fdd-d1de28e2d801");
  HLK_TEST(Subtract, HLSLBool_t, Vector,
           "5332bf60-11b2-474d-a3f0-1454f2d5448e");
  HLK_TEST(Subtract, HLSLHalf_t, ScalarOp2,
           "930ed107-71a6-46e9-a424-b4f3f0cbada6");
  HLK_TEST(Subtract, HLSLHalf_t, Vector,
           "fa09851a-f269-4831-a297-84d7fcc721f8");
  HLK_TEST(Subtract, int16_t, ScalarOp2,
           "a8cd0c89-29b9-4eb6-9cf0-d630b4b08aff");
  HLK_TEST(Subtract, int16_t, Vector, "c65ede06-c0e4-4717-bf51-544879123885");
  HLK_TEST(Subtract, int32_t, ScalarOp2,
           "e6d49023-6180-4989-b30a-0f8e6dabb114");
  HLK_TEST(Subtract, int32_t, Vector, "b84ba4af-2a74-4f2b-8e26-e44c43831457");
  HLK_TEST(Subtract, int64_t, ScalarOp2,
           "0e714987-9bb4-4bc2-9ad5-febf8114c31e");
  HLK_TEST(Subtract, int64_t, Vector, "5f16783a-8a6b-4b02-89fa-ad4e13b6a10e");
  HLK_TEST(Subtract, uint16_t, ScalarOp2,
           "142b7ffd-4eab-498f-a8ad-8b44a75f916e");
  HLK_TEST(Subtract, uint16_t, Vector, "e7daddf7-dd8e-48c8-83d1-28f09e80aec8");
  HLK_TEST(Subtract, uint32_t, ScalarOp2,
           "3a5628bb-59f2-4bdc-9e75-fe93139fb2cf");
  HLK_TEST(Subtract, uint32_t, Vector, "7af8be49-0bb2-43c3-9d81-e1b6800bf67d");
  HLK_TEST(Subtract, uint64_t, ScalarOp2,
           "dab308b0-4948-44dc-8bad-5fa58e365cee");
  HLK_TEST(Subtract, uint64_t, Vector, "8b99573c-bda8-4928-9727-8604fa154b26");

private:
  bool Initialized = false;
  bool VerboseLogging = false;
};

} // namespace LongVector
