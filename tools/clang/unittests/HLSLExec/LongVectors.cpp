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
#include <bitset>
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

template <typename T> constexpr bool is16BitType() {
  return std::is_same_v<T, int16_t> || std::is_same_v<T, uint16_t> ||
         std::is_same_v<T, HLSLHalf_t>;
}

struct DataType {
  const char *HLSLTypeString;
  bool Is16Bit;
};

template <typename T> const DataType &getDataType() {
  static_assert(false && "Unknown data type");
}

#define DATA_TYPE(TYPE, HLSL_STRING)                                           \
  template <> const DataType &getDataType<TYPE>() {                            \
    static DataType DataType{HLSL_STRING, is16BitType<TYPE>()};                \
    return DataType;                                                           \
  }

DATA_TYPE(HLSLBool_t, "bool")
DATA_TYPE(int16_t, "int16_t")
DATA_TYPE(int32_t, "int")
DATA_TYPE(int64_t, "int64_t")
DATA_TYPE(uint16_t, "uint16_t")
DATA_TYPE(uint32_t, "uint32_t")
DATA_TYPE(uint64_t, "uint64_t")
DATA_TYPE(HLSLHalf_t, "half")
DATA_TYPE(float, "float")
DATA_TYPE(double, "double")

#undef DATA_TYPE

template <typename T> constexpr bool isFloatingPointType() {
  return std::is_same_v<T, float> || std::is_same_v<T, double> ||
         std::is_same_v<T, HLSLHalf_t>;
}

//
// Operation Types
//

enum class OpType : unsigned {
#define OP(GROUP, SYMBOL, ARITY, INTRINSIC, OPERATOR, DEFINES, INPUT_SET_1,    \
           INPUT_SET_2, INPUT_SET_3)                                           \
  SYMBOL,
#include "LongVectorOps.def"
  NumOpTypes
};

struct Operation {
  size_t Arity;
  const char *Intrinsic;
  const char *Operator;
  const char *ExtraDefines;
  InputSet InputSets[3];
};

static constexpr Operation Operations[] = {

#define OP(GROUP, SYMBOL, ARITY, INTRINSIC, OPERATOR, DEFINES, INPUT_SET_1,    \
           INPUT_SET_2, INPUT_SET_3)                                           \
  {ARITY,                                                                      \
   INTRINSIC,                                                                  \
   OPERATOR,                                                                   \
   DEFINES,                                                                    \
   {InputSet::INPUT_SET_1, InputSet::INPUT_SET_2, InputSet::INPUT_SET_3}},
#include "LongVectorOps.def"
};

constexpr const Operation &getOperation(OpType Op) {
  if (Op < OpType::NumOpTypes)
    return Operations[unsigned(Op)];
  std::abort();
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
  hlsl_test::LogCommentFmt(L"LongVector Name: %s", Name.c_str());

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

  hlsl_test::LogCommentFmt(Wss.str().c_str());
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
    hlsl_test::LogErrorFmt(
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
    hlsl_test::LogErrorFmt(
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
    hlsl_test::LogErrorFmt(
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
      hlsl_test::LogErrorFmt(Wss.str().c_str());
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

std::string getCompilerOptionsString(const Operation &Operation,
                                     const DataType &OpDataType,
                                     const DataType &OutDataType,
                                     size_t VectorSize) {
  std::stringstream CompilerOptions;

  if (OpDataType.Is16Bit || OutDataType.Is16Bit)
    CompilerOptions << " -enable-16bit-types";

  CompilerOptions << " -DTYPE=" << OpDataType.HLSLTypeString;
  CompilerOptions << " -DNUM=" << VectorSize;

  CompilerOptions << " -DOPERATOR=";
  CompilerOptions << Operation.Operator;

  CompilerOptions << " -DFUNC=";
  CompilerOptions << Operation.Intrinsic;

  CompilerOptions << " " << Operation.ExtraDefines;

  CompilerOptions << " -DOUT_TYPE=" << OutDataType.HLSLTypeString;

  CompilerOptions << " -DBASIC_OP_TYPE=0x" << std::hex << Operation.Arity;

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

//
// Run the test.  Return std::nullopt if the test was skipped, otherwise returns
// the output buffer that was populated by the shader.
//
template <typename T> using InputSets = std::vector<std::vector<T>>;

template <typename OUT_TYPE, typename T>
std::optional<std::vector<OUT_TYPE>>
runTest(ID3D12Device *D3DDevice, bool VerboseLogging,
        const Operation &Operation, const InputSets<T> &Inputs,
        size_t ExpectedOutputSize) {
  DXASSERT_NOMSG(Inputs.size() == Operation.Arity);

  if (VerboseLogging) {
    for (size_t I = 0; I < Operation.Arity; ++I) {
      std::wstring Name = L"InputVector";
      Name += (wchar_t)(L'1' + I);
      logLongVector(Inputs[I], Name);
    }
  }

  const DataType &OpDataType = getDataType<T>();
  const DataType &OutDataType = getDataType<OUT_TYPE>();

  // We have to construct the string outside of the lambda. Otherwise it's
  // cleaned up when the lambda finishes executing but before the shader runs.
  std::string CompilerOptionsString = getCompilerOptionsString(
      Operation, OpDataType, OutDataType, Inputs[0].size());

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
        if (VerboseLogging)
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
            if (I < Operation.Arity)
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
  const std::vector<T> &RawValueSet = getInputSet<T>(InputSet);

  std::vector<T> ValueSet;
  ValueSet.reserve(SizeToTest);
  for (size_t I = 0; I < SizeToTest; ++I)
    ValueSet.push_back(RawValueSet[I % RawValueSet.size()]);

  return ValueSet;
}

template <typename T>
InputSets<T> buildTestInputs(size_t VectorSize, const InputSet OpInputSets[3],
                             size_t Arity) {
  InputSets<T> Inputs;

  for (size_t I = 0; I < Arity; ++I)
    Inputs.push_back(buildTestInput<T>(OpInputSets[I], VectorSize));

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

template <typename T, typename OUT_TYPE>
void runAndVerify(ID3D12Device *D3DDevice, bool VerboseLogging,
                  const Operation &Operation, const InputSets<T> &Inputs,
                  const std::vector<OUT_TYPE> &Expected,
                  const ValidationConfig &ValidationConfig) {

  std::optional<std::vector<OUT_TYPE>> Actual = runTest<OUT_TYPE>(
      D3DDevice, VerboseLogging, Operation, Inputs, Expected.size());

  // If the test didn't run, don't verify anything.
  if (!Actual)
    return;

  VERIFY_IS_TRUE(doVectorsMatch(*Actual, Expected, ValidationConfig.Tolerance,
                                ValidationConfig.Type, VerboseLogging));
}

//
// Op definitions.  The main goal of this is to specify the validation
// configuration and how to build the Expected results for a given Op.
//
// Most Ops have a 1:1 mapping of input to output, and so can use the generic
// ExpectedBuilder.
//
// Ops that differ from this pattern can specialize ExpectedBuilder as
// necessary.
//

// Op - specializations are expected to have a ValidationConfig member and an
// appropriate overloaded function call operator.
template <OpType OP, typename T, size_t Arity> struct Op;

// ExpectedBuilder - specializations are expected to have buildExpectedData
// member functions.
template <OpType OP, typename T> struct ExpectedBuilder;

// Default Validation configuration - ULP for floating point types, exact
// matches for everything else.
template <typename T> struct DefaultValidation {
  ValidationConfig ValidationConfig;

  DefaultValidation() {
    if constexpr (isFloatingPointType<T>())
      ValidationConfig = ValidationConfig::Ulp(1.0f);
  }
};

// Strict Validation - require exact matches for all types
struct StrictValidation {
  ValidationConfig ValidationConfig;
};

// Macros to build up common patterns of Op definitions

#define OP_1(OP, VALIDATION, IMPL)                                             \
  template <typename T> struct Op<OP, T, 1> : VALIDATION {                     \
    T operator()(T A) { return IMPL; }                                         \
  }

#define OP_2(OP, VALIDATION, IMPL)                                             \
  template <typename T> struct Op<OP, T, 2> : VALIDATION {                     \
    T operator()(T A, T B) { return IMPL; }                                    \
  }

#define OP_3(OP, VALIDATION, IMPL)                                             \
  template <typename T> struct Op<OP, T, 3> : VALIDATION {                     \
    T operator()(T A, T B, T C) { return IMPL; }                               \
  }

#define DEFAULT_OP_1(OP, IMPL) OP_1(OP, DefaultValidation<T>, IMPL)
#define DEFAULT_OP_2(OP, IMPL) OP_2(OP, DefaultValidation<T>, IMPL)
#define DEFAULT_OP_3(OP, IMPL) OP_3(OP, DefaultValidation<T>, IMPL)

//
// TernaryMath
//

DEFAULT_OP_3(OpType::Mad, (A * B + C));
DEFAULT_OP_3(OpType::Fma, (A * B + C));

//
// BinaryMath
//

DEFAULT_OP_2(OpType::Add, (A + B));
DEFAULT_OP_2(OpType::Subtract, (A - B));
DEFAULT_OP_2(OpType::Multiply, (A * B));
DEFAULT_OP_2(OpType::Divide, (A / B));

template <typename T> struct Op<OpType::Modulus, T, 2> : DefaultValidation<T> {
  T operator()(T A, T B) {
    if constexpr (std::is_same_v<T, float>)
      return std::fmod(A, B);
    else
      return A % B;
  }
};

DEFAULT_OP_2(OpType::Min, (std::min(A, B)));
DEFAULT_OP_2(OpType::Max, (std::max(A, B)));
DEFAULT_OP_2(OpType::Ldexp, (A * static_cast<T>(std::pow(2.0f, B))));

//
// Bitwise
//

template <typename T> T Saturate(T A) {
  if (A < static_cast<T>(0.0f))
    return static_cast<T>(0.0f);
  if (A > static_cast<T>(1.0f))
    return static_cast<T>(1.0f);
  return A;
}

template <typename T> T ReverseBits(T A) {
  T Result = 0;
  const size_t NumBits = sizeof(T) * 8;
  for (size_t I = 0; I < NumBits; I++) {
    Result <<= 1;
    Result |= (A & 1);
    A >>= 1;
  }
  return Result;
}

template <typename T> uint32_t CountBits(T A) {
  return static_cast<uint32_t>(std::bitset<sizeof(T) * 8>(A).count());
}

// General purpose bit scan from the MSB. Based on the value of LookingForZero
// returns the index of the first high/low bit found.
template <typename T> uint32_t ScanFromMSB(T A, bool LookingForZero) {
  if (A == 0)
    return ~0;

  constexpr uint32_t NumBits = sizeof(T) * 8;
  for (int32_t I = NumBits - 1; I >= 0; --I) {
    bool BitSet = (A & (static_cast<T>(1) << I)) != 0;
    if (BitSet != LookingForZero)
      return static_cast<uint32_t>(I);
  }
  return ~0;
}

template <typename T>
typename std::enable_if<std::is_signed<T>::value, uint32_t>::type
FirstBitHigh(T A) {
  const bool IsNegative = A < 0;
  return ScanFromMSB(A, IsNegative);
}

template <typename T>
typename std::enable_if<!std::is_signed<T>::value, uint32_t>::type
FirstBitHigh(T A) {
  return ScanFromMSB(A, false);
}

template <typename T> uint32_t FirstBitLow(T A) {
  const uint32_t NumBits = sizeof(T) * 8;

  if (A == 0)
    return ~0;

  for (uint32_t I = 0; I < NumBits; ++I) {
    if (A & (static_cast<T>(1) << I))
      return static_cast<T>(I);
  }

  return ~0;
}

DEFAULT_OP_2(OpType::And, (A & B));
DEFAULT_OP_2(OpType::Or, (A | B));
DEFAULT_OP_2(OpType::Xor, (A ^ B));
DEFAULT_OP_2(OpType::LeftShift, (A << B));
DEFAULT_OP_2(OpType::RightShift, (A >> B));
DEFAULT_OP_1(OpType::Saturate, (Saturate(A)));
DEFAULT_OP_1(OpType::ReverseBits, (ReverseBits(A)));

#define BITWISE_OP(OP, IMPL)                                                   \
  template <typename T> struct Op<OP, T, 1> : StrictValidation {               \
    uint32_t operator()(T A) { return IMPL; }                                  \
  }

BITWISE_OP(OpType::CountBits, (CountBits(A)));
BITWISE_OP(OpType::FirstBitHigh, (FirstBitHigh(A)));
BITWISE_OP(OpType::FirstBitLow, (FirstBitLow(A)));

#undef BITWISE_OP

//
// Unary
//

DEFAULT_OP_1(OpType::Initialize, (A));

//
// Cast
//

#define CAST_OP(OP, TYPE, IMPL)                                                \
  template <typename T> struct Op<OP, T, 1> : StrictValidation {               \
    TYPE operator()(T A) { return IMPL; }                                      \
  };

template <typename T> HLSLBool_t CastToBool(T A) { return (bool)A; }
template <> HLSLBool_t CastToBool(HLSLHalf_t A) { return (bool)((float)A); }

template <typename T> HLSLHalf_t CastToFloat16(T A) {
  return HLSLHalf_t(float(A));
}

template <typename T> float CastToFloat32(T A) { return (float)A; }

template <typename T> double CastToFloat64(T A) { return (double)A; }
template <> double CastToFloat64(HLSLHalf_t A) { return (double)((float)A); }

template <typename T> int16_t CastToInt16(T A) { return (int16_t)A; }
template <> int16_t CastToInt16(HLSLHalf_t A) { return (int16_t)((float)A); }

template <typename T> int32_t CastToInt32(T A) { return (int32_t)A; }
template <> int32_t CastToInt32(HLSLHalf_t A) { return (int32_t)((float)A); }

template <typename T> int64_t CastToInt64(T A) { return (int64_t)A; }
template <> int64_t CastToInt64(HLSLHalf_t A) { return (int64_t)((float)A); }

template <typename T> uint16_t CastToUint16(T A) { return (uint16_t)A; }
template <> uint16_t CastToUint16(HLSLHalf_t A) { return (uint16_t)((float)A); }

template <typename T> uint32_t CastToUint32(T A) { return (uint32_t)A; }
template <> uint32_t CastToUint32(HLSLHalf_t A) { return (uint32_t)((float)A); }

template <typename T> uint64_t CastToUint64(T A) { return (uint64_t)A; }
template <> uint64_t CastToUint64(HLSLHalf_t A) { return (uint64_t)((float)A); }

CAST_OP(OpType::CastToBool, HLSLBool_t, (CastToBool(A)));
CAST_OP(OpType::CastToInt16, int16_t, (CastToInt16(A)));
CAST_OP(OpType::CastToInt32, int32_t, (CastToInt32(A)));
CAST_OP(OpType::CastToInt64, int64_t, (CastToInt64(A)));
CAST_OP(OpType::CastToUint16, uint16_t, (CastToUint16(A)));
CAST_OP(OpType::CastToUint32, uint32_t, (CastToUint32(A)));
CAST_OP(OpType::CastToUint64, uint64_t, (CastToUint64(A)));
CAST_OP(OpType::CastToUint16_FromFP, uint16_t, (CastToUint16(A)));
CAST_OP(OpType::CastToUint32_FromFP, uint32_t, (CastToUint32(A)));
CAST_OP(OpType::CastToUint64_FromFP, uint64_t, (CastToUint64(A)));
CAST_OP(OpType::CastToFloat16, HLSLHalf_t, (CastToFloat16(A)));
CAST_OP(OpType::CastToFloat32, float, (CastToFloat32(A)));
CAST_OP(OpType::CastToFloat64, double, (CastToFloat64(A)));

#undef CAST_OP

//
// Trigonometric
//

// All trigonometric ops are floating point types. These trig functions are
// defined to have a max absolute error of 0.0008 as per the D3D functional
// specs. An example with this spec for sin and cos is available here:
// https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#22.10.20

struct TrigonometricValidation {
  ValidationConfig ValidationConfig = ValidationConfig::Epsilon(0.0008f);
};

#define TRIG_OP(OP, IMPL)                                                      \
  template <typename T> struct Op<OP, T, 1> : TrigonometricValidation {        \
    T operator()(T A) { return IMPL; }                                         \
  }

TRIG_OP(OpType::Acos, (std::acos(A)));
TRIG_OP(OpType::Asin, (std::asin(A)));
TRIG_OP(OpType::Atan, (std::atan(A)));
TRIG_OP(OpType::Cos, (std::cos(A)));
TRIG_OP(OpType::Cosh, (std::cosh(A)));
TRIG_OP(OpType::Sin, (std::sin(A)));
TRIG_OP(OpType::Sinh, (std::sinh(A)));
TRIG_OP(OpType::Tan, (std::tan(A)));
TRIG_OP(OpType::Tanh, (std::tanh(A)));

#undef TRIG_OP

//
// AsType
//

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

#define AS_TYPE_OP(OP, TYPE, IMPL)                                             \
  template <typename T> struct Op<OP, T, 1> : StrictValidation {               \
    TYPE operator()(T A) { return IMPL; }                                      \
  };

// asFloat16

template <typename T> HLSLHalf_t asFloat16(T);
template <> HLSLHalf_t asFloat16(HLSLHalf_t A) { return A; }
template <> HLSLHalf_t asFloat16(int16_t A) {
  return HLSLHalf_t::FromHALF(bit_cast<DirectX::PackedVector::HALF>(A));
}
template <> HLSLHalf_t asFloat16(uint16_t A) {
  return HLSLHalf_t::FromHALF(bit_cast<DirectX::PackedVector::HALF>(A));
}

AS_TYPE_OP(OpType::AsFloat16, HLSLHalf_t, (asFloat16(A)));

// asInt16

template <typename T> int16_t asInt16(T);
template <> int16_t asInt16(HLSLHalf_t A) { return bit_cast<int16_t>(A.Val); }
template <> int16_t asInt16(int16_t A) { return A; }
template <> int16_t asInt16(uint16_t A) { return bit_cast<int16_t>(A); }

AS_TYPE_OP(OpType::AsInt16, int16_t, (asInt16(A)));

// asUint16

template <typename T> uint16_t asUint16(T);
template <> uint16_t asUint16<HLSLHalf_t>(HLSLHalf_t A) {
  return bit_cast<uint16_t>(A.Val);
}
template <> uint16_t asUint16(uint16_t A) { return A; }
template <> uint16_t asUint16(int16_t A) { return bit_cast<uint16_t>(A); }

AS_TYPE_OP(OpType::AsUint16, uint16_t, (asUint16(A)));

// asFloat

template <typename T> float asFloat(T);
template <> float asFloat(float A) { return float(A); }
template <> float asFloat(int32_t A) { return bit_cast<float>(A); }
template <> float asFloat(uint32_t A) { return bit_cast<float>(A); }

AS_TYPE_OP(OpType::AsFloat, float, (asFloat(A)));

// asInt

template <typename T> int32_t asInt(T);
template <> int32_t asInt(float A) { return bit_cast<int32_t>(A); }
template <> int32_t asInt(int32_t A) { return A; }
template <> int32_t asInt(uint32_t A) { return bit_cast<int32_t>(A); }

AS_TYPE_OP(OpType::AsInt, int32_t, (asInt(A)));

// asUint

template <typename T> unsigned int asUint(T);
template <> unsigned int asUint(unsigned int A) { return A; }
template <> unsigned int asUint(float A) { return bit_cast<unsigned int>(A); }
template <> unsigned int asUint(int A) { return bit_cast<unsigned int>(A); }

AS_TYPE_OP(OpType::AsUint, uint32_t, (asUint(A)));

// asDouble

template <> struct Op<OpType::AsDouble, uint32_t, 2> : StrictValidation {
  double operator()(uint32_t LowBits, uint32_t HighBits) {
    uint64_t Bits = (static_cast<uint64_t>(HighBits) << 32) | LowBits;
    double Result;
    std::memcpy(&Result, &Bits, sizeof(Result));
    return Result;
  }
};

// splitDouble
//
// splitdouble is special because it's a function that takes a double and
// outputs two values. To handle this special case we override various bits of
// the testing machinary.

template <>
struct Op<OpType::AsUint_SplitDouble, double, 1> : StrictValidation {};

// Specialized version of ExpectedBuilder for the splitdouble case. The
// expected output for this has all the Low values followed by all the High
// values.
template <> struct ExpectedBuilder<OpType::AsUint_SplitDouble, double> {
  static std::vector<uint32_t>
  buildExpected(Op<OpType::AsUint_SplitDouble, double, 1>,
                const InputSets<double> &Inputs) {
    DXASSERT_NOMSG(Inputs.size() == 1);

    size_t VectorSize = Inputs[0].size();

    std::vector<uint32_t> Expected;
    Expected.resize(VectorSize * 2);

    for (size_t I = 0; I < VectorSize; ++I) {
      uint32_t Low, High;
      splitDouble(Inputs[0][I], Low, High);
      Expected[I] = Low;
      Expected[I + VectorSize] = High;
    }

    return Expected;
  }

  static void splitDouble(const double A, uint32_t &LowBits,
                          uint32_t &HighBits) {
    uint64_t Bits = 0;
    std::memcpy(&Bits, &A, sizeof(Bits));
    LowBits = static_cast<uint32_t>(Bits & 0xFFFFFFFF);
    HighBits = static_cast<uint32_t>(Bits >> 32);
  }
};

//
// Unary Math
//

template <typename T> T UnaryMathAbs(T A) {
  if constexpr (std::is_unsigned_v<T>)
    return A;
  else
    return static_cast<T>(std::abs(A));
}

DEFAULT_OP_1(OpType::Abs, (UnaryMathAbs(A)));

// Sign is special because the return type doesn't match the input type.
template <typename T> struct Op<OpType::Sign, T, 1> : DefaultValidation<T> {
  int32_t operator()(T A) {
    const T Zero = T();

    if (A > Zero)
      return 1;
    if (A < Zero)
      return -1;
    return 0;
  }
};

DEFAULT_OP_1(OpType::Ceil, (std::ceil(A)));
DEFAULT_OP_1(OpType::Exp, (std::exp(A)));
DEFAULT_OP_1(OpType::Floor, (std::floor(A)));
DEFAULT_OP_1(OpType::Frac, (A - static_cast<T>(std::floor(A))));
DEFAULT_OP_1(OpType::Log, (std::log(A)));
DEFAULT_OP_1(OpType::Rcp, (static_cast<T>(1.0) / A));
DEFAULT_OP_1(OpType::Round, (std::round(A)));
DEFAULT_OP_1(OpType::Rsqrt,
             (static_cast<T>(1.0) / static_cast<T>(std::sqrt(A))));
DEFAULT_OP_1(OpType::Sqrt, (std::sqrt(A)));
DEFAULT_OP_1(OpType::Trunc, (std::trunc(A)));
DEFAULT_OP_1(OpType::Exp2, (std::exp2(A)));
DEFAULT_OP_1(OpType::Log10, (std::log10(A)));
DEFAULT_OP_1(OpType::Log2, (std::log2(A)));

// Frexp has a return value as well as an output paramater. So we handle it
// with special logic. Frexp is only supported for fp32 values.
template <> struct Op<OpType::Frexp, float, 1> : DefaultValidation<float> {};

template <> struct ExpectedBuilder<OpType::Frexp, float> {
  static std::vector<float> buildExpected(Op<OpType::Frexp, float, 1>,
                                          const InputSets<float> &Inputs) {
    DXASSERT_NOMSG(Inputs.size() == 1);

    // Expected values size is doubled. In the first half we store the
    // Mantissas and in the second half we store the Exponents. This way we
    // can leverage the existing logic which verify expected values in a
    // single vector. We just need to make sure that we organize the output in
    // the same way in the shader and when we read it back.

    size_t VectorSize = Inputs[0].size();

    std::vector<float> Expected;
    Expected.resize(VectorSize * 2);

    for (size_t I = 0; I < VectorSize; ++I) {
      int Exp = 0;
      float Man = std::frexp(Inputs[0][I], &Exp);

      // std::frexp returns a signed mantissa. But the HLSL implmentation
      // returns an unsigned mantissa.
      Man = std::abs(Man);

      Expected[I] = Man;

      // std::frexp returns the exponent as an int, but HLSL stores it as a
      // float. However, the HLSL exponents fractional component is always 0.
      // So it can conversion between float and int is safe.
      Expected[I + VectorSize] = static_cast<float>(Exp);
    }

    return Expected;
  }
};

//
// Binary Comparison
//

#define BINARY_COMPARISON_OP(OP, IMPL)                                         \
  template <typename T> struct Op<OP, T, 2> : StrictValidation {               \
    HLSLBool_t operator()(T A, T B) { return IMPL; }                           \
  };

BINARY_COMPARISON_OP(OpType::LessThan, (A < B));
BINARY_COMPARISON_OP(OpType::LessEqual, (A <= B));
BINARY_COMPARISON_OP(OpType::GreaterThan, (A > B));
BINARY_COMPARISON_OP(OpType::GreaterEqual, (A >= B));
BINARY_COMPARISON_OP(OpType::Equal, (A == B));
BINARY_COMPARISON_OP(OpType::NotEqual, (A != B));

//
// Binary Logical
//

DEFAULT_OP_2(OpType::Logical_And, (A && B));
DEFAULT_OP_2(OpType::Logical_Or, (A || B));

// Ternary Logical
//

OP_3(OpType::Select, StrictValidation, (static_cast<bool>(A) ? B : C));

//
// Reduction
//

#define REDUCTION_OP(OP, STDFUNC)                                              \
  template <typename T> struct Op<OP, T, 1> : StrictValidation {};             \
  template <typename T> struct ExpectedBuilder<OP, T> {                        \
    static std::vector<HLSLBool_t> buildExpected(Op<OP, T, 1>,                 \
                                                 const InputSets<T> &Inputs) { \
      const bool Res = STDFUNC(Inputs[0].begin(), Inputs[0].end(),             \
                               [](T A) { return A != static_cast<T>(0); });    \
      return std::vector<HLSLBool_t>{Res};                                     \
    }                                                                          \
  };

REDUCTION_OP(OpType::Any_Mixed, (std::any_of));
REDUCTION_OP(OpType::Any_NoZero, (std::any_of));
REDUCTION_OP(OpType::Any_Zero, (std::any_of));

REDUCTION_OP(OpType::All_Mixed, (std::all_of));
REDUCTION_OP(OpType::All_NoZero, (std::all_of));
REDUCTION_OP(OpType::All_Zero, (std::all_of));

#undef REDUCTION_OP

template <typename T> struct Op<OpType::Dot, T, 2> : DefaultValidation<T> {};
template <typename T> struct ExpectedBuilder<OpType::Dot, T> {
  static std::vector<T> buildExpected(Op<OpType::Dot, T, 2>,
                                      const InputSets<T> &Inputs) {
    T DotProduct = T();

    for (size_t I = 0; I < Inputs[0].size(); ++I) {
      DotProduct += Inputs[0][I] * Inputs[1][I];
    }

    std::vector<T> Expected;
    Expected.push_back(DotProduct);
    return Expected;
  }
};

template <typename T>
struct Op<OpType::ShuffleVector, T, 1> : DefaultValidation<T> {};
template <typename T> struct ExpectedBuilder<OpType::ShuffleVector, T> {
  static std::vector<T> buildExpected(Op<OpType::ShuffleVector, T, 1>,
                                      const InputSets<T> &Inputs) {
    std::vector<T> Expected(Inputs[0].size(), Inputs[0][0]);
    return Expected;
  }
};

//
// dispatchTest
//

template <OpType OP, typename T> struct ExpectedBuilder {

  static auto buildExpected(Op<OP, T, 1> Op, const InputSets<T> &Inputs) {
    DXASSERT_NOMSG(Inputs.size() == 1);

    std::vector<decltype(Op(T()))> Expected;
    Expected.reserve(Inputs[0].size());

    for (size_t I = 0; I < Inputs[0].size(); ++I)
      Expected.push_back(Op(Inputs[0][I]));

    return Expected;
  }

  static auto buildExpected(Op<OP, T, 2> Op, const InputSets<T> &Inputs) {
    DXASSERT_NOMSG(Inputs.size() == 2);

    std::vector<decltype(Op(T(), T()))> Expected;
    Expected.reserve(Inputs[0].size());

    for (size_t I = 0; I < Inputs[0].size(); ++I)
      Expected.push_back(Op(Inputs[0][I], Inputs[1][I]));

    return Expected;
  }

  static auto buildExpected(Op<OP, T, 3> Op, const InputSets<T> &Inputs) {
    DXASSERT_NOMSG(Inputs.size() == 3);

    std::vector<decltype(Op(T(), T(), T()))> Expected;
    Expected.reserve(Inputs[0].size());

    for (size_t I = 0; I < Inputs[0].size(); ++I)
      Expected.push_back(Op(Inputs[0][I], Inputs[1][I], Inputs[2][I]));

    return Expected;
  }
};

template <typename T, OpType OP>
void dispatchTest(ID3D12Device *D3DDevice, bool VerboseLogging,
                  size_t OverrideInputSize) {
  std::vector<size_t> InputVectorSizes;
  if (OverrideInputSize)
    InputVectorSizes.push_back(OverrideInputSize);
  else
    InputVectorSizes = {3, 4, 5, 16, 17, 35, 100, 256, 1024};

  constexpr const Operation &Operation = getOperation(OP);

  Op<OP, T, Operation.Arity> Op;

  for (size_t VectorSize : InputVectorSizes) {
    std::vector<std::vector<T>> Inputs =
        buildTestInputs<T>(VectorSize, Operation.InputSets, Operation.Arity);

    auto Expected = ExpectedBuilder<OP, T>::buildExpected(Op, Inputs);

    runAndVerify(D3DDevice, VerboseLogging, Operation, Inputs, Expected,
                 Op.ValidationConfig);
  }
}

} // namespace LongVector

using namespace LongVector;

// TAEF test entry points
#define HLK_TEST(Op, DataType)                                                 \
  TEST_METHOD(Op##_##DataType) { runTest<DataType, OpType::Op>(); }

class DxilConf_SM69_Vectorized {
public:
  BEGIN_TEST_CLASS(DxilConf_SM69_Vectorized)
  TEST_CLASS_PROPERTY("Kits.TestName",
                      "D3D12 - Shader Model 6.9 - Vectorized DXIL - Core Tests")
  TEST_CLASS_PROPERTY("Kits.TestId", "81db1ff8-5bc5-48a1-8d7b-600fc600a677")
  TEST_CLASS_PROPERTY("Kits.Description",
                      "Validates required SM 6.9 vectorized DXIL operations")
  TEST_CLASS_PROPERTY(
      "Kits.Specification",
      "Device.Graphics.D3D12.DXILCore.ShaderModel69.CoreRequirement")
  END_TEST_CLASS()

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
        hlsl_test::LogCommentFmt(L"Verbose logging is enabled for this test.");
      else
        hlsl_test::LogCommentFmt(L"Verbose logging is disabled for this test.");

      WEX::TestExecution::RuntimeParameters::TryGetValue(L"InputSize",
                                                         OverrideInputSize);

      bool IsRITP = false;
      WEX::TestExecution::RuntimeParameters::TryGetValue(L"RITP", IsRITP);

      if (IsRITP) {
        if (!OverrideInputSize)
          // Help keep test runtime down for RITP runs
          OverrideInputSize = 10;
        else
          hlsl_test::LogWarningFmt(
              L"RITP is enabled but InputSize is also set. Will use the"
              L"InputSize value: %d.",
              OverrideInputSize);
      }

      // Only skip unsupported tests for RITP runs.
      const bool SkipUnsupported = IsRITP;
      createDevice(&D3DDevice, ExecTestUtils::D3D_SHADER_MODEL_6_9,
                   SkipUnsupported);
    }

    return true;
  }

  template <typename T, OpType OP> void runTest() {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    // It's possible a previous test case caused a device removal. If it did we
    // need to try and create a new device.
    if (!D3DDevice || D3DDevice->GetDeviceRemovedReason() != S_OK)
      VERIFY_IS_TRUE(
          createDevice(&D3DDevice, ExecTestUtils::D3D_SHADER_MODEL_6_9, false));

    dispatchTest<T, OP>(D3DDevice, VerboseLogging, OverrideInputSize);
  }

  // TernaryMath

  HLK_TEST(Mad, uint16_t);
  HLK_TEST(Mad, uint32_t);
  HLK_TEST(Mad, uint64_t);
  HLK_TEST(Mad, int16_t);
  HLK_TEST(Mad, int32_t);
  HLK_TEST(Mad, int64_t);
  HLK_TEST(Mad, HLSLHalf_t);
  HLK_TEST(Mad, float);
  HLK_TEST(Fma, double);
  HLK_TEST(Mad, double);

  // BinaryMath

  HLK_TEST(Add, HLSLBool_t);
  HLK_TEST(Subtract, HLSLBool_t);
  HLK_TEST(Add, int16_t);
  HLK_TEST(Subtract, int16_t);
  HLK_TEST(Multiply, int16_t);
  HLK_TEST(Divide, int16_t);
  HLK_TEST(Modulus, int16_t);
  HLK_TEST(Min, int16_t);
  HLK_TEST(Max, int16_t);
  HLK_TEST(Add, int32_t);
  HLK_TEST(Subtract, int32_t);
  HLK_TEST(Multiply, int32_t);
  HLK_TEST(Divide, int32_t);
  HLK_TEST(Modulus, int32_t);
  HLK_TEST(Min, int32_t);
  HLK_TEST(Max, int32_t);
  HLK_TEST(Add, int64_t);
  HLK_TEST(Subtract, int64_t);
  HLK_TEST(Multiply, int64_t);
  HLK_TEST(Divide, int64_t);
  HLK_TEST(Modulus, int64_t);
  HLK_TEST(Min, int64_t);
  HLK_TEST(Max, int64_t);
  HLK_TEST(Add, uint16_t);
  HLK_TEST(Subtract, uint16_t);
  HLK_TEST(Multiply, uint16_t);
  HLK_TEST(Divide, uint16_t);
  HLK_TEST(Modulus, uint16_t);
  HLK_TEST(Min, uint16_t);
  HLK_TEST(Max, uint16_t);
  HLK_TEST(Add, uint32_t);
  HLK_TEST(Subtract, uint32_t);
  HLK_TEST(Multiply, uint32_t);
  HLK_TEST(Divide, uint32_t);
  HLK_TEST(Modulus, uint32_t);
  HLK_TEST(Min, uint32_t);
  HLK_TEST(Max, uint32_t);
  HLK_TEST(Add, uint64_t);
  HLK_TEST(Subtract, uint64_t);
  HLK_TEST(Multiply, uint64_t);
  HLK_TEST(Divide, uint64_t);
  HLK_TEST(Modulus, uint64_t);
  HLK_TEST(Min, uint64_t);
  HLK_TEST(Max, uint64_t);
  HLK_TEST(Add, HLSLHalf_t);
  HLK_TEST(Subtract, HLSLHalf_t);
  HLK_TEST(Multiply, HLSLHalf_t);
  HLK_TEST(Divide, HLSLHalf_t);
  HLK_TEST(Modulus, HLSLHalf_t);
  HLK_TEST(Min, HLSLHalf_t);
  HLK_TEST(Max, HLSLHalf_t);
  HLK_TEST(Ldexp, HLSLHalf_t);
  HLK_TEST(Add, float);
  HLK_TEST(Subtract, float);
  HLK_TEST(Multiply, float);
  HLK_TEST(Divide, float);
  HLK_TEST(Modulus, float);
  HLK_TEST(Min, float);
  HLK_TEST(Max, float);
  HLK_TEST(Ldexp, float);
  HLK_TEST(Add, double);
  HLK_TEST(Subtract, double);
  HLK_TEST(Multiply, double);
  HLK_TEST(Divide, double);
  HLK_TEST(Min, double);
  HLK_TEST(Max, double);

  // Bitwise

  HLK_TEST(And, uint16_t);
  HLK_TEST(Or, uint16_t);
  HLK_TEST(Xor, uint16_t);
  HLK_TEST(ReverseBits, uint16_t);
  HLK_TEST(CountBits, uint16_t);
  HLK_TEST(FirstBitHigh, uint16_t);
  HLK_TEST(FirstBitLow, uint16_t);
  HLK_TEST(LeftShift, uint16_t);
  HLK_TEST(RightShift, uint16_t);
  HLK_TEST(And, uint32_t);
  HLK_TEST(Or, uint32_t);
  HLK_TEST(Xor, uint32_t);
  HLK_TEST(LeftShift, uint32_t);
  HLK_TEST(RightShift, uint32_t);
  HLK_TEST(ReverseBits, uint32_t);
  HLK_TEST(CountBits, uint32_t);
  HLK_TEST(FirstBitHigh, uint32_t);
  HLK_TEST(FirstBitLow, uint32_t);
  HLK_TEST(And, uint64_t);
  HLK_TEST(Or, uint64_t);
  HLK_TEST(Xor, uint64_t);
  HLK_TEST(LeftShift, uint64_t);
  HLK_TEST(RightShift, uint64_t);
  HLK_TEST(ReverseBits, uint64_t);
  HLK_TEST(CountBits, uint64_t);
  HLK_TEST(FirstBitHigh, uint64_t);
  HLK_TEST(FirstBitLow, uint64_t);
  HLK_TEST(And, int16_t);
  HLK_TEST(Or, int16_t);
  HLK_TEST(Xor, int16_t);
  HLK_TEST(LeftShift, int16_t);
  HLK_TEST(RightShift, int16_t);
  HLK_TEST(ReverseBits, int16_t);
  HLK_TEST(CountBits, int16_t);
  HLK_TEST(FirstBitHigh, int16_t);
  HLK_TEST(FirstBitLow, int16_t);
  HLK_TEST(And, int32_t);
  HLK_TEST(Or, int32_t);
  HLK_TEST(Xor, int32_t);
  HLK_TEST(LeftShift, int32_t);
  HLK_TEST(RightShift, int32_t);
  HLK_TEST(ReverseBits, int32_t);
  HLK_TEST(CountBits, int32_t);
  HLK_TEST(FirstBitHigh, int32_t);
  HLK_TEST(FirstBitLow, int32_t);
  HLK_TEST(And, int64_t);
  HLK_TEST(Or, int64_t);
  HLK_TEST(Xor, int64_t);
  HLK_TEST(LeftShift, int64_t);
  HLK_TEST(RightShift, int64_t);
  HLK_TEST(ReverseBits, int64_t);
  HLK_TEST(CountBits, int64_t);
  HLK_TEST(FirstBitHigh, int64_t);
  HLK_TEST(FirstBitLow, int64_t);
  HLK_TEST(Saturate, HLSLHalf_t);
  HLK_TEST(Saturate, float);
  HLK_TEST(Saturate, double);

  // Unary

  HLK_TEST(Initialize, HLSLBool_t);
  HLK_TEST(Initialize, int16_t);
  HLK_TEST(Initialize, int32_t);
  HLK_TEST(Initialize, int64_t);
  HLK_TEST(Initialize, uint16_t);
  HLK_TEST(Initialize, uint32_t);
  HLK_TEST(Initialize, uint64_t);
  HLK_TEST(Initialize, HLSLHalf_t);
  HLK_TEST(Initialize, float);
  HLK_TEST(Initialize, double);

  HLK_TEST(ShuffleVector, HLSLBool_t);
  HLK_TEST(ShuffleVector, int16_t);
  HLK_TEST(ShuffleVector, int32_t);
  HLK_TEST(ShuffleVector, int64_t);
  HLK_TEST(ShuffleVector, uint16_t);
  HLK_TEST(ShuffleVector, uint32_t);
  HLK_TEST(ShuffleVector, uint64_t);
  HLK_TEST(ShuffleVector, HLSLHalf_t);
  HLK_TEST(ShuffleVector, float);
  HLK_TEST(ShuffleVector, double);

  // Explicit Cast

  HLK_TEST(CastToInt16, HLSLBool_t);
  HLK_TEST(CastToInt32, HLSLBool_t);
  HLK_TEST(CastToInt64, HLSLBool_t);
  HLK_TEST(CastToUint16, HLSLBool_t);
  HLK_TEST(CastToUint32, HLSLBool_t);
  HLK_TEST(CastToUint64, HLSLBool_t);
  HLK_TEST(CastToFloat16, HLSLBool_t);
  HLK_TEST(CastToFloat32, HLSLBool_t);
  HLK_TEST(CastToFloat64, HLSLBool_t);

  HLK_TEST(CastToBool, HLSLHalf_t);
  HLK_TEST(CastToInt16, HLSLHalf_t);
  HLK_TEST(CastToInt32, HLSLHalf_t);
  HLK_TEST(CastToInt64, HLSLHalf_t);
  HLK_TEST(CastToUint16_FromFP, HLSLHalf_t);
  HLK_TEST(CastToUint32_FromFP, HLSLHalf_t);
  HLK_TEST(CastToUint64_FromFP, HLSLHalf_t);
  HLK_TEST(CastToFloat32, HLSLHalf_t);
  HLK_TEST(CastToFloat64, HLSLHalf_t);

  HLK_TEST(CastToBool, float);
  HLK_TEST(CastToInt16, float);
  HLK_TEST(CastToInt32, float);
  HLK_TEST(CastToInt64, float);
  HLK_TEST(CastToUint16_FromFP, float);
  HLK_TEST(CastToUint32_FromFP, float);
  HLK_TEST(CastToUint64_FromFP, float);
  HLK_TEST(CastToFloat16, float);
  HLK_TEST(CastToFloat64, float);

  HLK_TEST(CastToBool, double);
  HLK_TEST(CastToInt16, double);
  HLK_TEST(CastToInt32, double);
  HLK_TEST(CastToInt64, double);
  HLK_TEST(CastToUint16_FromFP, double);
  HLK_TEST(CastToUint32_FromFP, double);
  HLK_TEST(CastToUint64_FromFP, double);
  HLK_TEST(CastToFloat16, double);
  HLK_TEST(CastToFloat32, double);

  HLK_TEST(CastToBool, uint16_t);
  HLK_TEST(CastToInt16, uint16_t);
  HLK_TEST(CastToInt32, uint16_t);
  HLK_TEST(CastToInt64, uint16_t);
  HLK_TEST(CastToUint32, uint16_t);
  HLK_TEST(CastToUint64, uint16_t);
  HLK_TEST(CastToFloat16, uint16_t);
  HLK_TEST(CastToFloat32, uint16_t);
  HLK_TEST(CastToFloat64, uint16_t);

  HLK_TEST(CastToBool, uint32_t);
  HLK_TEST(CastToInt16, uint32_t);
  HLK_TEST(CastToInt32, uint32_t);
  HLK_TEST(CastToInt64, uint32_t);
  HLK_TEST(CastToUint16, uint32_t);
  HLK_TEST(CastToUint64, uint32_t);
  HLK_TEST(CastToFloat16, uint32_t);
  HLK_TEST(CastToFloat32, uint32_t);
  HLK_TEST(CastToFloat64, uint32_t);

  HLK_TEST(CastToBool, uint64_t);
  HLK_TEST(CastToInt16, uint64_t);
  HLK_TEST(CastToInt32, uint64_t);
  HLK_TEST(CastToInt64, uint64_t);
  HLK_TEST(CastToUint16, uint64_t);
  HLK_TEST(CastToUint32, uint64_t);
  HLK_TEST(CastToFloat16, uint64_t);
  HLK_TEST(CastToFloat32, uint64_t);
  HLK_TEST(CastToFloat64, uint64_t);

  HLK_TEST(CastToBool, int16_t);
  HLK_TEST(CastToInt32, int16_t);
  HLK_TEST(CastToInt64, int16_t);
  HLK_TEST(CastToUint16, int16_t);
  HLK_TEST(CastToUint32, int16_t);
  HLK_TEST(CastToUint64, int16_t);
  HLK_TEST(CastToFloat16, int16_t);
  HLK_TEST(CastToFloat32, int16_t);
  HLK_TEST(CastToFloat64, int16_t);

  HLK_TEST(CastToBool, int32_t);
  HLK_TEST(CastToInt16, int32_t);
  HLK_TEST(CastToInt64, int32_t);
  HLK_TEST(CastToUint16, int32_t);
  HLK_TEST(CastToUint32, int32_t);
  HLK_TEST(CastToUint64, int32_t);
  HLK_TEST(CastToFloat16, int32_t);
  HLK_TEST(CastToFloat32, int32_t);
  HLK_TEST(CastToFloat64, int32_t);

  HLK_TEST(CastToBool, int64_t);
  HLK_TEST(CastToInt16, int64_t);
  HLK_TEST(CastToInt32, int64_t);
  HLK_TEST(CastToUint16, int64_t);
  HLK_TEST(CastToUint32, int64_t);
  HLK_TEST(CastToUint64, int64_t);
  HLK_TEST(CastToFloat16, int64_t);
  HLK_TEST(CastToFloat32, int64_t);
  HLK_TEST(CastToFloat64, int64_t);

  // Trigonometric

  HLK_TEST(Acos, HLSLHalf_t);
  HLK_TEST(Asin, HLSLHalf_t);
  HLK_TEST(Atan, HLSLHalf_t);
  HLK_TEST(Cos, HLSLHalf_t);
  HLK_TEST(Cosh, HLSLHalf_t);
  HLK_TEST(Sin, HLSLHalf_t);
  HLK_TEST(Sinh, HLSLHalf_t);
  HLK_TEST(Tan, HLSLHalf_t);
  HLK_TEST(Tanh, HLSLHalf_t);
  HLK_TEST(Acos, float);
  HLK_TEST(Asin, float);
  HLK_TEST(Atan, float);
  HLK_TEST(Cos, float);
  HLK_TEST(Cosh, float);
  HLK_TEST(Sin, float);
  HLK_TEST(Sinh, float);
  HLK_TEST(Tan, float);
  HLK_TEST(Tanh, float);

  // AsType

  HLK_TEST(AsFloat16, int16_t);
  HLK_TEST(AsInt16, int16_t);
  HLK_TEST(AsUint16, int16_t);
  HLK_TEST(AsFloat, int32_t);
  HLK_TEST(AsInt, int32_t);
  HLK_TEST(AsUint, int32_t);
  HLK_TEST(AsFloat16, uint16_t);
  HLK_TEST(AsInt16, uint16_t);
  HLK_TEST(AsUint16, uint16_t);
  HLK_TEST(AsFloat, uint32_t);
  HLK_TEST(AsInt, uint32_t);
  HLK_TEST(AsUint, uint32_t);
  HLK_TEST(AsDouble, uint32_t);
  HLK_TEST(AsFloat16, HLSLHalf_t);
  HLK_TEST(AsInt16, HLSLHalf_t);
  HLK_TEST(AsUint16, HLSLHalf_t);
  HLK_TEST(AsUint_SplitDouble, double);

  // Unary Math

  HLK_TEST(Abs, int16_t);
  HLK_TEST(Sign, int16_t);
  HLK_TEST(Abs, int32_t);
  HLK_TEST(Sign, int32_t);
  HLK_TEST(Abs, int64_t);
  HLK_TEST(Sign, int64_t);
  HLK_TEST(Abs, uint16_t);
  HLK_TEST(Sign, uint16_t);
  HLK_TEST(Abs, uint32_t);
  HLK_TEST(Sign, uint32_t);
  HLK_TEST(Abs, uint64_t);
  HLK_TEST(Sign, uint64_t);
  HLK_TEST(Abs, HLSLHalf_t);
  HLK_TEST(Ceil, HLSLHalf_t);
  HLK_TEST(Exp, HLSLHalf_t);
  HLK_TEST(Floor, HLSLHalf_t);
  HLK_TEST(Frac, HLSLHalf_t);
  HLK_TEST(Log, HLSLHalf_t);
  HLK_TEST(Rcp, HLSLHalf_t);
  HLK_TEST(Round, HLSLHalf_t);
  HLK_TEST(Rsqrt, HLSLHalf_t);
  HLK_TEST(Sign, HLSLHalf_t);
  HLK_TEST(Sqrt, HLSLHalf_t);
  HLK_TEST(Trunc, HLSLHalf_t);
  HLK_TEST(Exp2, HLSLHalf_t);
  HLK_TEST(Log10, HLSLHalf_t);
  HLK_TEST(Log2, HLSLHalf_t);
  HLK_TEST(Abs, float);
  HLK_TEST(Ceil, float);
  HLK_TEST(Exp, float);
  HLK_TEST(Floor, float);
  HLK_TEST(Frac, float);
  HLK_TEST(Log, float);
  HLK_TEST(Rcp, float);
  HLK_TEST(Round, float);
  HLK_TEST(Rsqrt, float);
  HLK_TEST(Sign, float);
  HLK_TEST(Sqrt, float);
  HLK_TEST(Trunc, float);
  HLK_TEST(Exp2, float);
  HLK_TEST(Log10, float);
  HLK_TEST(Log2, float);
  HLK_TEST(Frexp, float);
  HLK_TEST(Abs, double);
  HLK_TEST(Sign, double);

  // Binary Comparison

  HLK_TEST(LessThan, int16_t);
  HLK_TEST(LessEqual, int16_t);
  HLK_TEST(GreaterThan, int16_t);
  HLK_TEST(GreaterEqual, int16_t);
  HLK_TEST(Equal, int16_t);
  HLK_TEST(NotEqual, int16_t);
  HLK_TEST(LessThan, int32_t);
  HLK_TEST(LessEqual, int32_t);
  HLK_TEST(GreaterThan, int32_t);
  HLK_TEST(GreaterEqual, int32_t);
  HLK_TEST(Equal, int32_t);
  HLK_TEST(NotEqual, int32_t);
  HLK_TEST(LessThan, int64_t);
  HLK_TEST(LessEqual, int64_t);
  HLK_TEST(GreaterThan, int64_t);
  HLK_TEST(GreaterEqual, int64_t);
  HLK_TEST(Equal, int64_t);
  HLK_TEST(NotEqual, int64_t);
  HLK_TEST(LessThan, uint16_t);
  HLK_TEST(LessEqual, uint16_t);
  HLK_TEST(GreaterThan, uint16_t);
  HLK_TEST(GreaterEqual, uint16_t);
  HLK_TEST(Equal, uint16_t);
  HLK_TEST(NotEqual, uint16_t);
  HLK_TEST(LessThan, uint32_t);
  HLK_TEST(LessEqual, uint32_t);
  HLK_TEST(GreaterThan, uint32_t);
  HLK_TEST(GreaterEqual, uint32_t);
  HLK_TEST(Equal, uint32_t);
  HLK_TEST(NotEqual, uint32_t);
  HLK_TEST(LessThan, uint64_t);
  HLK_TEST(LessEqual, uint64_t);
  HLK_TEST(GreaterThan, uint64_t);
  HLK_TEST(GreaterEqual, uint64_t);
  HLK_TEST(Equal, uint64_t);
  HLK_TEST(NotEqual, uint64_t);
  HLK_TEST(LessThan, HLSLHalf_t);
  HLK_TEST(LessEqual, HLSLHalf_t);
  HLK_TEST(GreaterThan, HLSLHalf_t);
  HLK_TEST(GreaterEqual, HLSLHalf_t);
  HLK_TEST(Equal, HLSLHalf_t);
  HLK_TEST(NotEqual, HLSLHalf_t);
  HLK_TEST(LessThan, float);
  HLK_TEST(LessEqual, float);
  HLK_TEST(GreaterThan, float);
  HLK_TEST(GreaterEqual, float);
  HLK_TEST(Equal, float);
  HLK_TEST(NotEqual, float);
  HLK_TEST(LessThan, double);
  HLK_TEST(LessEqual, double);
  HLK_TEST(GreaterThan, double);
  HLK_TEST(GreaterEqual, double);
  HLK_TEST(Equal, double);
  HLK_TEST(NotEqual, double);

  // Binary Logical

  HLK_TEST(Logical_And, HLSLBool_t);
  HLK_TEST(Logical_Or, HLSLBool_t);

  // Ternary Logical
  HLK_TEST(Select, HLSLBool_t);
  HLK_TEST(Select, int16_t);
  HLK_TEST(Select, int32_t);
  HLK_TEST(Select, int64_t);
  HLK_TEST(Select, uint16_t);
  HLK_TEST(Select, uint32_t);
  HLK_TEST(Select, uint64_t);
  HLK_TEST(Select, HLSLHalf_t);
  HLK_TEST(Select, float);
  HLK_TEST(Select, double);

  // Reduction
  HLK_TEST(Any_Mixed, HLSLBool_t);
  HLK_TEST(Any_Zero, HLSLBool_t);
  HLK_TEST(Any_NoZero, HLSLBool_t);
  HLK_TEST(All_Mixed, HLSLBool_t);
  HLK_TEST(All_Zero, HLSLBool_t);
  HLK_TEST(All_NoZero, HLSLBool_t);

  HLK_TEST(Any_Mixed, int16_t);
  HLK_TEST(Any_Zero, int16_t);
  HLK_TEST(Any_NoZero, int16_t);
  HLK_TEST(All_Mixed, int16_t);
  HLK_TEST(All_Zero, int16_t);
  HLK_TEST(All_NoZero, int16_t);

  HLK_TEST(Any_Mixed, int32_t);
  HLK_TEST(Any_Zero, int32_t);
  HLK_TEST(Any_NoZero, int32_t);
  HLK_TEST(All_Mixed, int32_t);
  HLK_TEST(All_Zero, int32_t);
  HLK_TEST(All_NoZero, int32_t);

  HLK_TEST(Any_Mixed, int64_t);
  HLK_TEST(Any_Zero, int64_t);
  HLK_TEST(Any_NoZero, int64_t);
  HLK_TEST(All_Mixed, int64_t);
  HLK_TEST(All_Zero, int64_t);
  HLK_TEST(All_NoZero, int64_t);

  HLK_TEST(Dot, HLSLHalf_t);

  HLK_TEST(Dot, float);

private:
  bool Initialized = false;
  bool VerboseLogging = false;
  size_t OverrideInputSize = 0;
  CComPtr<ID3D12Device> D3DDevice;
};
