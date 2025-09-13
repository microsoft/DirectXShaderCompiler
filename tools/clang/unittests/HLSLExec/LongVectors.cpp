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
#define OP(GROUP, SYMBOL, ARITY, INTRINSIC, OPERATOR, INPUT_SET_1,             \
           INPUT_SET_2, INPUT_SET_3)                                           \
  SYMBOL,
#include "LongVectorOps.def"
};

template <OpType OP> struct OpTraits;

#define OP(GROUP, SYMBOL, ARITY, INTRINSIC, OPERATOR, INPUT_SET_1,             \
           INPUT_SET_2, INPUT_SET_3)                                           \
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

  HLK_TEST(Mad, uint16_t, Vector, "eb7d60e7-c32b-41d5-93fe-bbeac5f2f934");
  HLK_TEST(Mad, uint16_t, ScalarOp3, "828d57e0-e0db-471d-ad3c-b35268f712b5");
  HLK_TEST(Mad, uint32_t, Vector, "bbaabc74-e41e-4a7d-a78c-f91177afc071");
  HLK_TEST(Mad, uint32_t, ScalarOp2, "7c8ef25b-d5a2-4a3e-a6c6-ec83e26a4b81");
  HLK_TEST(Mad, uint64_t, Vector, "7020262a-f74c-4dd2-9952-45275c08de50");
  HLK_TEST(Mad, uint64_t, ScalarOp3, "392dd509-f531-450a-bc0d-c8f08dcf399f");
  HLK_TEST(Mad, int16_t, Vector, "b62bd6a3-e564-42ce-93b1-21295647b9d9");
  HLK_TEST(Mad, int16_t, ScalarOp2, "dae04e2d-c929-4b7d-8a23-bdca862cdc02");
  HLK_TEST(Mad, int32_t, Vector, "c692e0d6-1973-4a18-b013-ab769ee177a3");
  HLK_TEST(Mad, int32_t, ScalarOp2, "18005439-0a84-4972-9b5d-8d214a6bdae8");
  HLK_TEST(Mad, int64_t, Vector, "440995bd-47be-410c-a3cd-25dac2b66218");
  HLK_TEST(Mad, int64_t, ScalarOp3, "2af43011-b71b-4f80-9a4e-52835391aa26");
  HLK_TEST(Mad, HLSLHalf_t, Vector, "61fd5c80-ff89-40c8-9e7d-d8f8498c6d34");
  HLK_TEST(Mad, HLSLHalf_t, ScalarOp2, "1e29537e-3dcd-4e88-9772-ca01afb80788");
  HLK_TEST(SmoothStep, HLSLHalf_t, Vector,
           "9afc0bf6-4da8-4cd8-9a9c-e400e0718498");
  HLK_TEST(SmoothStep, HLSLHalf_t, ScalarOp2,
           "ccb6720c-2f00-4a78-897d-48532ee5ed16");
  HLK_TEST(Mad, float, Vector, "2f30def6-0539-4636-8788-7648e0000049");
  HLK_TEST(Mad, float, ScalarOp2, "b61b898e-aadd-4d34-b43b-157a8ce9be28");
  HLK_TEST(SmoothStep, float, Vector, "0128e0a2-6464-4238-83ef-0760d680d210");
  HLK_TEST(SmoothStep, float, ScalarOp3,
           "ae3ce5f0-f331-464d-836c-bf0a74903b01");
  HLK_TEST(Fma, double, Vector, "70c54dcf-c62c-459a-95d5-4e8850baddb9");
  HLK_TEST(Fma, double, ScalarOp2, "6d5c74fc-ee50-445f-bc28-0c6666cbd754");
  HLK_TEST(Mad, double, Vector, "1813aa84-26d8-401a-9204-86323042a43d");
  HLK_TEST(Mad, double, ScalarOp2, "c5954f98-7f26-47ef-ae09-80d0526b3a9f");
  HLK_TEST(Add, HLSLBool_t, ScalarOp2, "0b763d04-9f24-404f-9307-09e2ba2b43d1");
  HLK_TEST(Add, HLSLBool_t, Vector, "31142b2a-f263-4ea2-81ab-6bec7d95f7c6");
  HLK_TEST(Subtract, HLSLBool_t, ScalarOp2,
           "e11509fc-b420-45ee-8d5f-880ac318ebbf");
  HLK_TEST(Subtract, HLSLBool_t, Vector,
           "32a7c9ea-906a-4677-be27-f82a6a97e724");
  HLK_TEST(Add, int16_t, ScalarOp2, "99063d10-04ea-4f3e-acd9-b1db8298db60");
  HLK_TEST(Add, int16_t, Vector, "ec82f079-b138-4dc2-a7e4-ad0b9623d5f2");
  HLK_TEST(Subtract, int16_t, ScalarOp2,
           "5deadf0c-fc3a-419e-997f-4f48903d9273");
  HLK_TEST(Subtract, int16_t, Vector, "0c67cb7c-c7ae-4ef5-851f-654181b464da");
  HLK_TEST(Multiply, int16_t, ScalarOp2,
           "617bf684-9196-4d14-aa86-936da7e83bf9");
  HLK_TEST(Multiply, int16_t, Vector, "7abcba64-f915-4d93-b266-709d2e461f12");
  HLK_TEST(Divide, int16_t, ScalarOp2, "d16a0b8c-d7ac-481d-ba72-a5c2493a076e");
  HLK_TEST(Divide, int16_t, Vector, "966ab38d-d1e3-42b6-9ada-e8dbc8a28c27");
  HLK_TEST(Modulus, int16_t, ScalarOp2, "e6564f25-a23a-4336-be12-92c682c0c49a");
  HLK_TEST(Modulus, int16_t, Vector, "95559fa0-bbd7-4912-8ea5-a04555f57f84");
  HLK_TEST(Min, int16_t, ScalarOp2, "0f6f20e8-1ed1-4fb0-8a8b-2765ec5c015a");
  HLK_TEST(Min, int16_t, Vector, "5dc11432-821c-4869-aa73-054494a965d5");
  HLK_TEST(Max, int16_t, ScalarOp2, "5268e59f-e6bc-417d-9570-79b8bfe09c74");
  HLK_TEST(Max, int16_t, Vector, "efdb5064-9ef6-41d0-89a4-743ee2f39516");
  HLK_TEST(Add, int32_t, ScalarOp2, "05d70ab1-29b5-423f-8d00-5ecbe5e34d1d");
  HLK_TEST(Add, int32_t, Vector, "a75d168f-546e-4b49-88c9-a5249c1054cf");
  HLK_TEST(Subtract, int32_t, ScalarOp2,
           "b12ad9d4-05cb-432f-aa8f-e34aaa963f2e");
  HLK_TEST(Subtract, int32_t, Vector, "4cc5196f-f3f2-411b-96ba-4ce4be8fc084");
  HLK_TEST(Multiply, int32_t, ScalarOp2,
           "fe238e74-4738-484d-8784-c1d2fbd1ca1d");
  HLK_TEST(Multiply, int32_t, Vector, "a00939f6-9b5e-482f-ba5d-450b5a69a4f3");
  HLK_TEST(Divide, int32_t, ScalarOp2, "d88b41de-384b-4960-b58c-2106b8eb10bf");
  HLK_TEST(Divide, int32_t, Vector, "97e9fa69-c5b9-4c8e-8a23-c4805c16c5a1");
  HLK_TEST(Modulus, int32_t, ScalarOp2, "e3dd6e62-d333-44f7-949b-235337161276");
  HLK_TEST(Modulus, int32_t, Vector, "ed748432-b245-4f26-b06b-537e6284e889");
  HLK_TEST(Min, int32_t, ScalarOp2, "54ef9574-5e2c-4cbd-8b26-2f8c0ffebd6b");
  HLK_TEST(Min, int32_t, Vector, "ba0286fc-e653-45c4-be44-d4e649b2cca7");
  HLK_TEST(Max, int32_t, ScalarOp2, "1ca94bc4-1852-489c-a165-a0d1298ccfa7");
  HLK_TEST(Max, int32_t, Vector, "79726dc0-bb2a-48b6-bf13-edc60a3564a6");
  HLK_TEST(Add, int64_t, ScalarOp2, "9f0832fc-03b4-44f9-a2ef-326d270b6c9a");
  HLK_TEST(Add, int64_t, Vector, "813688e4-d33c-4266-ac90-820e3276d615");
  HLK_TEST(Subtract, int64_t, ScalarOp2,
           "4c47ffcf-7415-479f-913f-94a07c4ede67");
  HLK_TEST(Subtract, int64_t, Vector, "1fc82ce6-863b-4e2a-9053-74f16023efeb");
  HLK_TEST(Multiply, int64_t, ScalarOp2,
           "e952aec9-9320-4ae4-a983-50a811fb0027");
  HLK_TEST(Multiply, int64_t, Vector, "fea9911a-b77e-471a-a136-9c4a752d6fe2");
  HLK_TEST(Divide, int64_t, ScalarOp2, "5f2f239d-8a26-446a-8944-c56a0fbde25f");
  HLK_TEST(Divide, int64_t, Vector, "884bc286-bd88-4ced-ad66-0ee0bde1a569");
  HLK_TEST(Modulus, int64_t, ScalarOp2, "ded33606-5d00-4c44-86dc-4ae82cc10509");
  HLK_TEST(Modulus, int64_t, Vector, "4485fb0f-61cf-4e8e-b599-a76614d0fc26");
  HLK_TEST(Min, int64_t, ScalarOp2, "e8a496f7-e0c0-47c9-9acb-0c2c306fa0ce");
  HLK_TEST(Min, int64_t, Vector, "51496fd5-34af-4513-953e-3040bd729180");
  HLK_TEST(Max, int64_t, ScalarOp2, "1caa5864-c6c0-4ac2-869e-7f53290302ed");
  HLK_TEST(Max, int64_t, Vector, "7bfe1ed5-79eb-4a9a-b139-ed1129b3cf36");
  HLK_TEST(Add, uint16_t, ScalarOp2, "cfe84eac-399b-4fdf-8e65-7cbea73ffd83");
  HLK_TEST(Add, uint16_t, Vector, "653d1af6-c153-4a0a-ba00-d611531254d8");
  HLK_TEST(Subtract, uint16_t, ScalarOp2,
           "5cfcb35b-6e86-4dd2-94e0-bee23fea3852");
  HLK_TEST(Subtract, uint16_t, Vector, "df22e337-d4fe-439e-bd20-c9cfed7fa3cb");
  HLK_TEST(Multiply, uint16_t, ScalarOp2,
           "7a813e95-a7cb-4f9e-9321-f7d441c3d92c");
  HLK_TEST(Multiply, uint16_t, Vector, "63cf685d-2c5f-4d02-8344-f887b4a7608b");
  HLK_TEST(Divide, uint16_t, ScalarOp2, "6cd31845-635d-464e-ace6-0a83bc4abe75");
  HLK_TEST(Divide, uint16_t, Vector, "4f91958c-992d-453c-b4de-63ccccac5cd3");
  HLK_TEST(Modulus, uint16_t, ScalarOp2,
           "62125ad3-dabc-4d98-8fe0-6de0b24b9138");
  HLK_TEST(Modulus, uint16_t, Vector, "a8c3096b-85d3-4de5-880e-6437368176af");
  HLK_TEST(Min, uint16_t, ScalarOp2, "e6ef7645-6118-4f3f-9f95-ab985409e698");
  HLK_TEST(Min, uint16_t, Vector, "8d569585-c838-4885-88b1-4e5acdf40ef9");
  HLK_TEST(Max, uint16_t, ScalarOp2, "8f38bd33-a378-4e32-bece-792827c3a73a");
  HLK_TEST(Max, uint16_t, Vector, "f6a4b8bb-0eba-4b45-b51a-7ea5476a01f7");
  HLK_TEST(Add, uint32_t, ScalarOp2, "a68a0dba-75cc-452d-bc54-a1cfcb99d4d4");
  HLK_TEST(Add, uint32_t, Vector, "bc8be68c-6498-4654-946a-199e20c1de07");
  HLK_TEST(Subtract, uint32_t, ScalarOp2,
           "adfd829d-21ec-4829-8064-1cb44573bda2");
  HLK_TEST(Subtract, uint32_t, Vector, "3f0f609f-dac6-426a-8374-4cb415eb6e76");
  HLK_TEST(Multiply, uint32_t, ScalarOp2,
           "3a6d6f8c-6851-4626-b485-e9435a35d492");
  HLK_TEST(Multiply, uint32_t, Vector, "d7f88184-8048-4be5-bc6c-5ca2c8dd3c3f");
  HLK_TEST(Divide, uint32_t, ScalarOp2, "838b577a-ffb9-46c2-8ce6-4f2aa86f2761");
  HLK_TEST(Divide, uint32_t, Vector, "e0e2db7c-b26e-4ee4-9906-66ea40536ee6");
  HLK_TEST(Modulus, uint32_t, ScalarOp2,
           "41bd2196-72c3-4556-9d34-67bc34140ab7");
  HLK_TEST(Modulus, uint32_t, Vector, "b3dc846a-1c69-4e33-ba9a-d998f29df457");
  HLK_TEST(Min, uint32_t, ScalarOp2, "a4709b03-642b-41cd-8c45-3e940a188f9b");
  HLK_TEST(Min, uint32_t, Vector, "41791e84-4116-44e2-bb22-1aa1821c7f26");
  HLK_TEST(Max, uint32_t, ScalarOp2, "0ebdffe0-5b6f-4118-a6ec-9914ba056a9c");
  HLK_TEST(Max, uint32_t, Vector, "a2d7ba56-3ea1-4560-943b-79ef092d34f3");
  HLK_TEST(Add, uint64_t, ScalarOp2, "418502a9-6af4-4901-8f31-8a4ca3245ee6");
  HLK_TEST(Add, uint64_t, Vector, "4355af2f-4f32-4759-98b6-b9d6b0e3bf15");
  HLK_TEST(Subtract, uint64_t, ScalarOp2,
           "83720855-632e-43f1-99db-decca8298f53");
  HLK_TEST(Subtract, uint64_t, Vector, "f1ad509b-d0ca-4cc8-b33f-9a8895443ab7");
  HLK_TEST(Multiply, uint64_t, ScalarOp2,
           "0879b20b-b214-4c7b-9c59-cba9e629ec11");
  HLK_TEST(Multiply, uint64_t, Vector, "d55972f8-155f-491d-9bfd-030e61b0ac20");
  HLK_TEST(Divide, uint64_t, ScalarOp2, "7cc3d7a1-d631-4e0f-aebb-a9ca31001982");
  HLK_TEST(Divide, uint64_t, Vector, "b7fb6133-e7e6-4a89-b45e-ec3a342d7b95");
  HLK_TEST(Modulus, uint64_t, ScalarOp2,
           "9a09760d-9db4-4a37-911e-ed58b2962890");
  HLK_TEST(Modulus, uint64_t, Vector, "8d0b56d5-452a-499d-aefa-62d1642e0c14");
  HLK_TEST(Min, uint64_t, ScalarOp2, "b2bb7787-91e3-4378-ae61-740d152f577c");
  HLK_TEST(Min, uint64_t, Vector, "30bf28f9-b2ff-4592-9ea8-7673a333a5b0");
  HLK_TEST(Max, uint64_t, ScalarOp2, "134b161e-c188-45a7-9433-03cb1e5e516c");
  HLK_TEST(Max, uint64_t, Vector, "8da765fb-9cb4-4762-b4fa-dded08a7bdef");
  HLK_TEST(Add, HLSLHalf_t, ScalarOp2, "e4cf06a6-0831-480f-8f9f-4a61996ceb7a");
  HLK_TEST(Add, HLSLHalf_t, Vector, "d47576fa-74b1-4da8-b67b-889bd9cee31f");
  HLK_TEST(Subtract, HLSLHalf_t, ScalarOp2,
           "7e339e4d-9cc2-499d-b36d-68adc5408cc0");
  HLK_TEST(Subtract, HLSLHalf_t, Vector,
           "b532c269-3c21-4fd4-aabe-e60eec6a6df2");
  HLK_TEST(Multiply, HLSLHalf_t, ScalarOp2,
           "89924df1-cd13-4e2f-bd0f-d3e54927bfdc");
  HLK_TEST(Multiply, HLSLHalf_t, Vector,
           "e30a29d3-8eaa-4c3e-af88-f6abc6686393");
  HLK_TEST(Divide, HLSLHalf_t, ScalarOp2,
           "eadc0bf5-0f67-4cab-a28a-816033ab9895");
  HLK_TEST(Divide, HLSLHalf_t, Vector, "52048471-82cd-4fe3-89a4-4332a1cf77f2");
  HLK_TEST(Modulus, HLSLHalf_t, ScalarOp2,
           "c2ed6573-2a9a-4b25-8936-335b1ba48789");
  HLK_TEST(Modulus, HLSLHalf_t, Vector, "300b66ca-0572-4070-846e-f13751c838c7");
  HLK_TEST(Min, HLSLHalf_t, ScalarOp2, "b95a613b-7a3c-466c-b949-452b528fc2f1");
  HLK_TEST(Min, HLSLHalf_t, Vector, "2fac70ee-4165-4a5f-80ad-100d6f95cf41");
  HLK_TEST(Max, HLSLHalf_t, ScalarOp2, "4997d108-835d-4660-9129-f1d98fdb6079");
  HLK_TEST(Max, HLSLHalf_t, Vector, "5ba2589b-d78e-486a-a6a8-e6112a87c4c3");
  HLK_TEST(Ldexp, HLSLHalf_t, Vector, "ffe7eb1d-a2cb-4f31-b1e9-997f3de153d3");
  HLK_TEST(Ldexp, HLSLHalf_t, ScalarOp2,
           "b1ad3c06-6f72-4707-ab50-c403779b3cd3");
  HLK_TEST(Add, float, ScalarOp2, "a4f20e92-2a87-47c1-9dce-2aabae278c13");
  HLK_TEST(Add, float, Vector, "dcc106ce-12e0-4c3b-9320-7745027b0dc4");
  HLK_TEST(Subtract, float, ScalarOp2, "d83a2cd3-24be-48d5-994c-6a7bea990d1e");
  HLK_TEST(Subtract, float, Vector, "9399b231-c5c2-492b-b115-af35c954cbe7");
  HLK_TEST(Multiply, float, ScalarOp2, "6695d246-0de6-4d4f-bea5-d6d62fe842a2");
  HLK_TEST(Multiply, float, Vector, "ddcf60a9-7909-4a74-b17f-96240b40de2a");
  HLK_TEST(Divide, float, ScalarOp2, "e4ad6720-63ca-46a1-abcd-1d7f269bc26a");
  HLK_TEST(Divide, float, Vector, "62fdda0c-c1e2-4e23-a861-f8c1e7f6ab03");
  HLK_TEST(Modulus, float, ScalarOp2, "85544288-d72e-4a0e-859a-2be2b36ef795");
  HLK_TEST(Modulus, float, Vector, "be28c633-e034-4fd4-b417-9fe18001c370");
  HLK_TEST(Min, float, ScalarOp2, "afa3e8b5-9a6f-49bd-8c27-c898eb4c3550");
  HLK_TEST(Min, float, Vector, "a5319faa-6dba-4c5d-84c8-4da1b4b2ee77");
  HLK_TEST(Max, float, ScalarOp2, "8bceb8a7-d774-4992-97b7-f8c988cb5912");
  HLK_TEST(Max, float, Vector, "7e50d1d7-7ae4-455b-9023-29d21daf4af9");
  HLK_TEST(Ldexp, float, Vector, "46a14777-956c-40ff-9718-874a73db058b");
  HLK_TEST(Ldexp, float, ScalarOp2, "338aacfe-922e-45e6-9f5a-d458caebe353");
  HLK_TEST(Add, double, ScalarOp2, "141f30a3-b45d-4a52-acdb-b48b2bc2fbfb");
  HLK_TEST(Add, double, Vector, "93bce5cf-604f-46a0-8124-c38a47c824a1");
  HLK_TEST(Subtract, double, ScalarOp2, "059fbc6e-83aa-463d-8a72-d75ea4fdb19c");
  HLK_TEST(Subtract, double, Vector, "50680265-7c22-49ca-adcd-b6e1df0e913d");
  HLK_TEST(Multiply, double, ScalarOp2, "0b70e2ce-ac1d-45c4-a982-cec80044704f");
  HLK_TEST(Multiply, double, Vector, "e62b8de1-6b48-415f-88e5-5ea116f09b40");
  HLK_TEST(Divide, double, ScalarOp2, "638bed29-8600-48bf-9c00-fee381326f7b");
  HLK_TEST(Divide, double, Vector, "ff73f97e-a835-448a-aa3d-9c41ffe56fcc");
  HLK_TEST(Min, double, ScalarOp2, "ec42a826-7637-447c-a14d-1661a9223ee3");
  HLK_TEST(Min, double, Vector, "ff4a187a-e410-4ce4-b50f-c488a7158a06");
  HLK_TEST(Max, double, ScalarOp2, "38a46477-9d51-4874-920c-6b7c08463d52");
  HLK_TEST(Max, double, Vector, "a395f842-2e32-4bec-8635-b458e7faee46");
  HLK_TEST(Initialize, HLSLBool_t, Vector,
           "3203d5bd-2b83-4328-a04e-4befd87119b3");
  HLK_TEST(Initialize, int16_t, Vector, "c89bb550-6d1f-47e9-827a-b49817348110");
  HLK_TEST(Initialize, int32_t, Vector, "c82729f5-d0a9-49aa-a15a-8aa06e8f04f6");
  HLK_TEST(Initialize, int64_t, Vector, "20cb73c1-7a62-49e7-b07e-96c2bc5ac873");
  HLK_TEST(Initialize, uint16_t, Vector,
           "ae9aab35-f962-41db-8bab-d81f422f40d9");
  HLK_TEST(Initialize, uint32_t, Vector,
           "33ac44bf-2f80-48a4-9574-83ef9e549bca");
  HLK_TEST(Initialize, uint64_t, Vector,
           "c180c191-0c56-479b-96d1-fb656ad9447a");
  HLK_TEST(Initialize, HLSLHalf_t, Vector,
           "6291fe76-2b95-45c5-8ea2-4cb4a47e0a52");
  HLK_TEST(Initialize, float, Vector, "fe56569a-ba15-4ba5-b671-575f05538c73");
  HLK_TEST(Initialize, double, Vector, "c091a797-1252-40aa-a53d-661c5875c8a5");
  HLK_TEST(Acos, HLSLHalf_t, Vector, "5735cddd-66ed-403d-a6e1-8a9ebef9a6e2");
  HLK_TEST(Asin, HLSLHalf_t, Vector, "507e7d7b-acf8-41e9-b489-971a239472db");
  HLK_TEST(Atan, HLSLHalf_t, Vector, "0de04aec-f6db-4832-8e2e-58b4f3a1ce0a");
  HLK_TEST(Cos, HLSLHalf_t, Vector, "47aa9184-b577-4be9-bdff-03434a5bd43f");
  HLK_TEST(Cosh, HLSLHalf_t, Vector, "6573b255-11b4-4aab-82ae-c224166c068b");
  HLK_TEST(Sin, HLSLHalf_t, Vector, "512bd9ca-bfca-4e41-afe6-714fb5d87f58");
  HLK_TEST(Sinh, HLSLHalf_t, Vector, "58fa1a29-4d99-4e16-81a2-f6cfa6b90775");
  HLK_TEST(Tan, HLSLHalf_t, Vector, "03cd7071-4864-499e-9d1a-ccb82af456d8");
  HLK_TEST(Tanh, HLSLHalf_t, Vector, "153c7ccd-03f6-42d0-b6b5-9bb347eaabce");
  HLK_TEST(Acos, float, Vector, "5cb07a7e-d1f4-44af-b638-edd6f9ba2177");
  HLK_TEST(Asin, float, Vector, "0d301505-a35a-40de-8698-07f5eaf4f16e");
  HLK_TEST(Atan, float, Vector, "1d6274be-a88e-418d-8f9c-d7af3d45a620");
  HLK_TEST(Cos, float, Vector, "e44cec7a-bfa7-4afe-821a-423fe0451b81");
  HLK_TEST(Cosh, float, Vector, "1dcbb60b-143b-4e26-bef5-5708be9a817c");
  HLK_TEST(Sin, float, Vector, "1a3d20bc-b835-47cb-a647-ae894f31c325");
  HLK_TEST(Sinh, float, Vector, "2ea08f3e-9daa-4e0f-93a6-2fecf8943498");
  HLK_TEST(Tan, float, Vector, "12d097d6-94e9-4bed-bd3c-6634f088a65e");
  HLK_TEST(Tanh, float, Vector, "05e9f20e-83a7-4d65-9160-8aab4792edf0");
  HLK_TEST(AsFloat16, int16_t, Vector, "4dd29b6a-39cc-4d5d-86ea-cd0aa027686e");
  HLK_TEST(AsInt16, int16_t, Vector, "2874f989-3f2d-4afb-ab01-96223ac36574");
  HLK_TEST(AsUint16, int16_t, Vector, "6a169c65-d90b-4cb6-8f86-83310afb1a4d");
  HLK_TEST(AsFloat, int32_t, Vector, "e5e0a25b-90d1-4e93-9cd9-2a5f3a95fd71");
  HLK_TEST(AsInt, int32_t, Vector, "89bcde1c-f22d-44e4-95a9-08e205a6d902");
  HLK_TEST(AsUint, int32_t, Vector, "6a2f32fc-b91a-49b6-98b0-5b8544e05070");
  HLK_TEST(AsFloat16, uint16_t, Vector, "dac6896d-8d8c-4f50-a098-7432c501d765");
  HLK_TEST(AsInt16, uint16_t, Vector, "af88a2d8-eff2-43f2-959d-02950ef5755f");
  HLK_TEST(AsUint16, uint16_t, Vector, "c55206de-2277-4253-b32e-c857f5af929e");
  HLK_TEST(AsFloat, uint32_t, Vector, "e424dfcb-ff4b-41bc-a991-2c31d3ca242b");
  HLK_TEST(AsInt, uint32_t, Vector, "d5364ed0-f52d-4579-8405-da1dcec0eba9");
  HLK_TEST(AsUint, uint32_t, Vector, "a08b9506-e7e0-4cca-86c6-42cf12a3ab23");
  HLK_TEST(AsDouble, uint32_t, Vector, "8c040027-e4b9-4283-b55d-98f1dd4631ab");
  HLK_TEST(AsDouble, uint32_t, ScalarOp2,
           "afadfbd0-1f28-438f-8a75-a83d5556f415");
  HLK_TEST(AsFloat16, HLSLHalf_t, Vector,
           "392e3f98-1f46-430a-bbcf-6b8ae68652c5");
  HLK_TEST(AsInt16, HLSLHalf_t, Vector, "dd67f22d-a259-4801-88a1-7de42398e727");
  HLK_TEST(AsUint16, HLSLHalf_t, Vector,
           "3af078f6-514c-4b19-aecb-daa2480d6f41");
  HLK_TEST(AsUint_SplitDouble, double, Vector,
           "ada8958b-9d52-4fe2-9380-441ed7328caf");
  HLK_TEST(Abs, int16_t, Vector, "29762a0b-d0c3-4546-b63a-fd7a268be8f1");
  HLK_TEST(Sign, int16_t, Vector, "85c16ce7-47e8-45f8-9023-7f8a10ef91c5");
  HLK_TEST(Abs, int32_t, Vector, "ce9753d7-aa4c-4319-81ae-2479e8afe76f");
  HLK_TEST(Sign, int32_t, Vector, "13ee4ec3-15cf-4ecb-9e2a-492ae52a7622");
  HLK_TEST(Abs, int64_t, Vector, "85dfc7bc-defb-429a-a25e-c687bc406040");
  HLK_TEST(Sign, int64_t, Vector, "d55a11cf-852a-4091-adc3-bd711c1c305c");
  HLK_TEST(Abs, uint16_t, Vector, "34b87e2b-5a71-483c-83a0-23d27026e939");
  HLK_TEST(Sign, uint16_t, Vector, "1b630589-e6cf-4b24-a894-a160c5af4336");
  HLK_TEST(Abs, uint32_t, Vector, "0fee2346-9e37-4ab6-ab4f-24eeb58633e2");
  HLK_TEST(Sign, uint32_t, Vector, "eea76fb5-6722-45a7-854c-83730bb66bcc");
  HLK_TEST(Abs, uint64_t, Vector, "6d76bb96-1ca3-4cd4-8a80-93eea0f3f217");
  HLK_TEST(Sign, uint64_t, Vector, "6d814a95-a04d-46f7-a064-bbfc1699554d");
  HLK_TEST(Abs, HLSLHalf_t, Vector, "64573bef-ec71-4e20-8525-328951367f29");
  HLK_TEST(Ceil, HLSLHalf_t, Vector, "b35292e5-c2ca-4fd3-bc2b-2f1da57a4b5a");
  HLK_TEST(Exp, HLSLHalf_t, Vector, "3ff8a9ee-0987-4757-bcb4-b1fa9d0738dc");
  HLK_TEST(Floor, HLSLHalf_t, Vector, "57940ab6-8b3a-4025-87be-ad8d6a06d275");
  HLK_TEST(Frac, HLSLHalf_t, Vector, "36f4f782-e571-47d0-b4aa-3441216e5d2c");
  HLK_TEST(Log, HLSLHalf_t, Vector, "794dde41-e89a-4c3c-9b6f-23dd658cd427");
  HLK_TEST(Rcp, HLSLHalf_t, Vector, "7a24c5c9-da3b-4375-b71a-a46f99efc867");
  HLK_TEST(Round, HLSLHalf_t, Vector, "8989cffb-cf70-442b-a4de-5e3341d42b54");
  HLK_TEST(Rsqrt, HLSLHalf_t, Vector, "328e8854-bd09-4ab0-9535-64bdc34bf5a4");
  HLK_TEST(Sign, HLSLHalf_t, Vector, "c2a690ad-8411-4203-aeca-588ba52127d8");
  HLK_TEST(Sqrt, HLSLHalf_t, Vector, "6a7ef04a-25d1-4d39-bba0-39cab4d2a044");
  HLK_TEST(Trunc, HLSLHalf_t, Vector, "0a0bf60c-aa27-400b-add3-0455a48be88b");
  HLK_TEST(Exp2, HLSLHalf_t, Vector, "069343bb-af6e-4731-81a3-2577c76bb43c");
  HLK_TEST(Log10, HLSLHalf_t, Vector, "56101521-4a66-4862-b7c1-2d90c027a2b8");
  HLK_TEST(Log2, HLSLHalf_t, Vector, "6c120353-b79b-4f69-8c88-dd2c33915d6a");
  HLK_TEST(Abs, float, Vector, "ab34eb79-431d-4bdd-b26d-3f6eedf3e6f0");
  HLK_TEST(Ceil, float, Vector, "693e9ec3-c91a-47c1-b771-6da761ca4368");
  HLK_TEST(Exp, float, Vector, "a13b8bb9-d104-45d9-ab6c-e58b23c8b69e");
  HLK_TEST(Floor, float, Vector, "58b6b608-9610-4902-a031-e89421ba50d5");
  HLK_TEST(Frac, float, Vector, "96ebbcaa-9443-4c0a-98a2-fa5ac4d55ff9");
  HLK_TEST(Log, float, Vector, "6c9dc8d6-2dba-4b93-a59d-2c9bc092ee79");
  HLK_TEST(Rcp, float, Vector, "ebaefda1-f2bf-4414-857b-306de0717360");
  HLK_TEST(Round, float, Vector, "e0191850-3bea-4012-9ff9-52583051b91c");
  HLK_TEST(Rsqrt, float, Vector, "5ad0993b-4ff2-494c-af61-e304b4e89e7c");
  HLK_TEST(Sign, float, Vector, "96046f66-3313-48b2-9d76-d9bb0c77022f");
  HLK_TEST(Sqrt, float, Vector, "eabda77b-30a6-49df-94d7-755132e91528");
  HLK_TEST(Trunc, float, Vector, "c68582da-6a52-46aa-8cb4-fcfdc04d6fd3");
  HLK_TEST(Exp2, float, Vector, "919d82c4-dc69-430b-9817-8bd6a6238fe5");
  HLK_TEST(Log10, float, Vector, "03162d2e-1e4b-462f-91bb-aa4e866a346f");
  HLK_TEST(Log2, float, Vector, "19978052-b38c-45b7-b544-fbe15505aa47");
  HLK_TEST(Frexp, float, Vector, "99a1307d-7e5d-4656-913f-7391a7f2081c");
  HLK_TEST(Abs, double, Vector, "ff0fadf6-87f1-4bf1-9164-5d90cb2a49b6");
  HLK_TEST(Sign, double, Vector, "89362c0b-9678-4d7f-8461-faff042de684");

private:
  bool Initialized = false;
  bool VerboseLogging = false;
};

} // namespace LongVector
