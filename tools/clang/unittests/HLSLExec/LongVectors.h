#ifndef LONGVECTORS_H
#define LONGVECTORS_H

#include <array>
#include <optional>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include <Verify.h>

#include "LongVectorTestData.h"
#include "ShaderOpTest.h"
#include "TableParameterHandler.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Test/HlslTestUtils.h"

namespace LongVector {

// Used to compute the hash of a std::wstring at compile time. Gives us a way to
// create switch statements with a std::wstring.
// Note: Because this is evaluated at compile time the compiler detects hash
// collisions via an duplicate case statement error.
inline constexpr auto Hash_djb2a(const std::wstring_view String) {
  unsigned long Hash{1337};
  for (wchar_t c : String) {
    Hash = ((Hash << 5) + Hash) ^ static_cast<std::size_t>(c);
  }
  return Hash;
}

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

// Used so we can dynamically resolve the type of data stored out the output
// LongVector for a test case.
using VariantVector =
    std::variant<std::vector<HLSLBool_t>, std::vector<HLSLHalf_t>,
                 std::vector<float>, std::vector<double>, std::vector<int16_t>,
                 std::vector<int32_t>, std::vector<int64_t>,
                 std::vector<uint16_t>, std::vector<uint32_t>,
                 std::vector<uint64_t>>;

template <typename T>
void fillShaderBufferFromLongVectorData(std::vector<BYTE> &ShaderBuffer,
                                        const std::vector<T> &TestData);

template <typename T>
void fillLongVectorDataFromShaderBuffer(const MappedData &ShaderBuffer,
                                        std::vector<T> &TestData,
                                        size_t NumElements);

template <typename T> constexpr bool isFloatingPointType() {
  return std::is_same_v<T, float> || std::is_same_v<T, double> ||
         std::is_same_v<T, HLSLHalf_t>;
}

template <typename T> constexpr bool is16BitType() {
  return std::is_same_v<T, int16_t> || std::is_same_v<T, uint16_t> ||
         std::is_same_v<T, HLSLHalf_t>;
}

template <typename T> std::string getHLSLTypeString();

enum SCALAR_INPUT_FLAGS : uint16_t {
  // SCALAR_INPUT_FLAGS_OPERAND_1_IS_SCALAR is intentionally omitted. Input 1 is
  // always a vector.
  SCALAR_INPUT_FLAGS_NONE = 0x0,
  SCALAR_INPUT_FLAGS_OPERAND_2_IS_SCALAR = 0x2,
  SCALAR_INPUT_FLAGS_OPERAND_3_IS_SCALAR = 0x4,
};

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
  uint16_t ScalarInputFlags = static_cast<uint16_t>(SCALAR_INPUT_FLAGS_NONE);
};

template <typename OpT, size_t Length>
const OpTypeMetaData<OpT> &
getOpType(const OpTypeMetaData<OpT> (&Values)[Length],
          const std::wstring &OpTypeString);

enum ValidationType {
  ValidationType_Epsilon,
  ValidationType_Ulp,
};

enum BasicOpType {
  BasicOpType_Unary,
  BasicOpType_Binary,
  BasicOpType_Ternary,
  BasicOpType_EnumValueCount
};

enum UnaryOpType { UnaryOpType_Initialize, UnaryOpType_EnumValueCount };

static const OpTypeMetaData<UnaryOpType> unaryOpTypeStringToOpMetaData[] = {
    {L"UnaryOpType_Initialize", UnaryOpType_Initialize, "TestInitialize"},
};

static_assert(_countof(unaryOpTypeStringToOpMetaData) ==
                  UnaryOpType_EnumValueCount,
              "unaryOpTypeStringToOpMetaData size mismatch. Did you add "
              "a new enum value?");

const OpTypeMetaData<UnaryOpType> &
getUnaryOpType(const std::wstring &OpTypeString) {
  return getOpType<UnaryOpType>(unaryOpTypeStringToOpMetaData, OpTypeString);
}

enum AsTypeOpType {
  AsTypeOpType_AsFloat,
  AsTypeOpType_AsFloat16,
  AsTypeOpType_AsInt,
  AsTypeOpType_AsInt16,
  AsTypeOpType_AsUint,
  AsTypeOpType_AsUint_SplitDouble,
  AsTypeOpType_AsUint16,
  AsTypeOpType_AsDouble,
  AsTypeOpType_EnumValueCount
};

static const OpTypeMetaData<AsTypeOpType> asTypeOpTypeStringToOpMetaData[] = {
    {L"AsTypeOpType_AsFloat", AsTypeOpType_AsFloat, "asfloat"},
    {L"AsTypeOpType_AsFloat16", AsTypeOpType_AsFloat16, "asfloat16"},
    {L"AsTypeOpType_AsInt", AsTypeOpType_AsInt, "asint"},
    {L"AsTypeOpType_AsInt16", AsTypeOpType_AsInt16, "asint16"},
    {L"AsTypeOpType_AsUint", AsTypeOpType_AsUint, "asuint"},
    {L"AsTypeOpType_AsUint_SplitDouble", AsTypeOpType_AsUint_SplitDouble,
     "TestAsUintSplitDouble"},
    {L"AsTypeOpType_AsUint16", AsTypeOpType_AsUint16, "asuint16"},
    {L"AsTypeOpType_AsDouble", AsTypeOpType_AsDouble, "asdouble", ","},
};

static_assert(_countof(asTypeOpTypeStringToOpMetaData) ==
                  AsTypeOpType_EnumValueCount,
              "asTypeOpTypeStringToOpMetaData size mismatch. Did you add "
              "a new enum value?");

const OpTypeMetaData<AsTypeOpType> &
getAsTypeOpType(const std::wstring &OpTypeString) {
  return getOpType<AsTypeOpType>(asTypeOpTypeStringToOpMetaData, OpTypeString);
}

enum TrigonometricOpType {
  TrigonometricOpType_Acos,
  TrigonometricOpType_Asin,
  TrigonometricOpType_Atan,
  TrigonometricOpType_Cos,
  TrigonometricOpType_Cosh,
  TrigonometricOpType_Sin,
  TrigonometricOpType_Sinh,
  TrigonometricOpType_Tan,
  TrigonometricOpType_Tanh,
  TrigonometricOpType_EnumValueCount
};

static const OpTypeMetaData<TrigonometricOpType>
    trigonometricOpTypeStringToOpMetaData[] = {
        {L"TrigonometricOpType_Acos", TrigonometricOpType_Acos, "acos"},
        {L"TrigonometricOpType_Asin", TrigonometricOpType_Asin, "asin"},
        {L"TrigonometricOpType_Atan", TrigonometricOpType_Atan, "atan"},
        {L"TrigonometricOpType_Cos", TrigonometricOpType_Cos, "cos"},
        {L"TrigonometricOpType_Cosh", TrigonometricOpType_Cosh, "cosh"},
        {L"TrigonometricOpType_Sin", TrigonometricOpType_Sin, "sin"},
        {L"TrigonometricOpType_Sinh", TrigonometricOpType_Sinh, "sinh"},
        {L"TrigonometricOpType_Tan", TrigonometricOpType_Tan, "tan"},
        {L"TrigonometricOpType_Tanh", TrigonometricOpType_Tanh, "tanh"},
};

static_assert(
    _countof(trigonometricOpTypeStringToOpMetaData) ==
        TrigonometricOpType_EnumValueCount,
    "trigonometricOpTypeStringToOpMetaData size mismatch. Did you add "
    "a new enum value?");

const OpTypeMetaData<TrigonometricOpType> &
getTrigonometricOpType(const std::wstring &OpTypeString) {
  return getOpType<TrigonometricOpType>(trigonometricOpTypeStringToOpMetaData,
                                        OpTypeString);
}

enum UnaryMathOpType {
  UnaryMathOpType_Abs,
  UnaryMathOpType_Sign,
  UnaryMathOpType_Ceil,
  UnaryMathOpType_Floor,
  UnaryMathOpType_Trunc,
  UnaryMathOpType_Round,
  UnaryMathOpType_Frac,
  UnaryMathOpType_Sqrt,
  UnaryMathOpType_Rsqrt,
  UnaryMathOpType_Exp,
  UnaryMathOpType_Exp2,
  UnaryMathOpType_Log,
  UnaryMathOpType_Log2,
  UnaryMathOpType_Log10,
  UnaryMathOpType_Rcp,
  UnaryMathOpType_Frexp,
  UnaryMathOpType_EnumValueCount
};

static const OpTypeMetaData<UnaryMathOpType>
    unaryMathOpTypeStringToOpMetaData[] = {
        {L"UnaryMathOpType_Abs", UnaryMathOpType_Abs, "abs"},
        {L"UnaryMathOpType_Sign", UnaryMathOpType_Sign, "sign"},
        {L"UnaryMathOpType_Ceil", UnaryMathOpType_Ceil, "ceil"},
        {L"UnaryMathOpType_Floor", UnaryMathOpType_Floor, "floor"},
        {L"UnaryMathOpType_Trunc", UnaryMathOpType_Trunc, "trunc"},
        {L"UnaryMathOpType_Round", UnaryMathOpType_Round, "round"},
        {L"UnaryMathOpType_Frac", UnaryMathOpType_Frac, "frac"},
        {L"UnaryMathOpType_Sqrt", UnaryMathOpType_Sqrt, "sqrt"},
        {L"UnaryMathOpType_Rsqrt", UnaryMathOpType_Rsqrt, "rsqrt"},
        {L"UnaryMathOpType_Exp", UnaryMathOpType_Exp, "exp"},
        {L"UnaryMathOpType_Exp2", UnaryMathOpType_Exp2, "exp2"},
        {L"UnaryMathOpType_Log", UnaryMathOpType_Log, "log"},
        {L"UnaryMathOpType_Log2", UnaryMathOpType_Log2, "log2"},
        {L"UnaryMathOpType_Log10", UnaryMathOpType_Log10, "log10"},
        {L"UnaryMathOpType_Rcp", UnaryMathOpType_Rcp, "rcp"},
        {L"UnaryMathOpType_Frexp", UnaryMathOpType_Frexp, "TestFrexp"},
};

static_assert(_countof(unaryMathOpTypeStringToOpMetaData) ==
                  UnaryMathOpType_EnumValueCount,
              "unaryMathOpTypeStringToOpMetaData size mismatch. Did you add "
              "a new enum value?");

const OpTypeMetaData<UnaryMathOpType> &
getUnaryMathOpType(const std::wstring &OpTypeString) {
  return getOpType<UnaryMathOpType>(unaryMathOpTypeStringToOpMetaData,
                                    OpTypeString);
}

enum BinaryMathOpType {
  BinaryMathOpType_Multiply,
  BinaryMathOpType_Add,
  BinaryMathOpType_Subtract,
  BinaryMathOpType_Divide,
  BinaryMathOpType_Modulus,
  BinaryMathOpType_Min,
  BinaryMathOpType_Max,
  BinaryMathOpType_Ldexp,
  BinaryMathOpType_EnumValueCount
};

static const OpTypeMetaData<BinaryMathOpType>
    binaryMathOpTypeStringToOpMetaData[] = {
        {L"BinaryMathOpType_Add", BinaryMathOpType_Add, std::nullopt, "+"},
        {L"BinaryMathOpType_Multiply", BinaryMathOpType_Multiply, std::nullopt,
         "*"},
        {L"BinaryMathOpType_Subtract", BinaryMathOpType_Subtract, std::nullopt,
         "-"},
        {L"BinaryMathOpType_Divide", BinaryMathOpType_Divide, std::nullopt,
         "/"},
        {L"BinaryMathOpType_Modulus", BinaryMathOpType_Modulus, std::nullopt,
         "%"},
        {L"BinaryMathOpType_Min", BinaryMathOpType_Min, "min", ","},
        {L"BinaryMathOpType_Max", BinaryMathOpType_Max, "max", ","},
        {L"BinaryMathOpType_Ldexp", BinaryMathOpType_Ldexp, "ldexp", ","},
};

static_assert(_countof(binaryMathOpTypeStringToOpMetaData) ==
                  BinaryMathOpType_EnumValueCount,
              "binaryMathOpTypeStringToOpMetaData size mismatch. Did you "
              "add a new enum value?");

const OpTypeMetaData<BinaryMathOpType> &
getBinaryMathOpType(const std::wstring &OpTypeString) {
  return getOpType<BinaryMathOpType>(binaryMathOpTypeStringToOpMetaData,
                                     OpTypeString);
}

enum TernaryMathOpType {
  TernaryMathOpType_Fma,
  TernaryMathOpType_Mad,
  TernaryMathOpType_SmoothStep,
  TernaryMathOpType_EnumValueCount
};

static const OpTypeMetaData<TernaryMathOpType>
    ternaryMathOpTypeStringToOpMetaData[] = {
        {L"TernaryMathOpType_Fma", TernaryMathOpType_Fma, "fma"},
        {L"TernaryMathOpType_Mad", TernaryMathOpType_Mad, "mad"},
        {L"TernaryMathOpType_SmoothStep", TernaryMathOpType_SmoothStep,
         "smoothstep"},
};

static_assert(_countof(ternaryMathOpTypeStringToOpMetaData) ==
                  TernaryMathOpType_EnumValueCount,
              "ternaryMathOpTypeStringToOpMetaData size mismatch. Did you "
              "add a new enum value?");

const OpTypeMetaData<TernaryMathOpType> &
getTernaryMathOpType(const std::wstring &OpTypeString) {
  return getOpType<TernaryMathOpType>(ternaryMathOpTypeStringToOpMetaData,
                                      OpTypeString);
}

template <typename T>
std::vector<T> getInputValueSetByKey(const std::wstring &Key,
                                     bool LogKey = true) {
  if (LogKey)
    WEX::Logging::Log::Comment(
        WEX::Common::String().Format(L"Using Value Set Key: %s", Key.c_str()));
  return std::vector<T>(TestData<T>::Data.at(Key));
}

// The TAEF test class.
template <typename T> class TestConfig; // Forward declaration.
class OpTest {
public:
  BEGIN_TEST_CLASS(OpTest)
  END_TEST_CLASS()

  TEST_CLASS_SETUP(classSetup);

  BEGIN_TEST_METHOD(unaryMathOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#UnaryMathOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(binaryMathOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#BinaryMathOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(ternaryMathOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#TernaryMathOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(trigonometricOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#TrigonometricOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(unaryOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#UnaryOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(asTypeOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#AsTypeOpTable")
  END_TEST_METHOD()

  template <typename OpT>
  void dispatchTestByDataType(const OpTypeMetaData<OpT> &OpTypeMD,
                              std::wstring DataType,
                              TableParameterHandler &Handler);

  void dispatchTrigonometricOpTestByDataType(
      const OpTypeMetaData<TrigonometricOpType> &OpTypeMD,
      std::wstring DataType, TableParameterHandler &Handler);

  void dispatchUnaryMathOpTestByDataType(
      const OpTypeMetaData<UnaryMathOpType> &OpTypeMD, std::wstring DataType,
      TableParameterHandler &Handler);

  template <typename T, typename OpT>
  void dispatchTestByVectorLength(const OpTypeMetaData<OpT> &OpTypeMD,
                                  TableParameterHandler &Handler);

  template <typename T>
  void testBaseMethod(std::unique_ptr<TestConfig<T>> &TestConfig);

private:
  dxc::SpecificDllLoader DxilDllLoader;
  bool Initialized = false;
  bool VerboseLogging = false;
};

template <typename T>
bool doValuesMatch(T A, T B, float Tolerance, ValidationType);
bool doValuesMatch(HLSLBool_t A, HLSLBool_t B, float, ValidationType);
bool doValuesMatch(HLSLHalf_t A, HLSLHalf_t B, float Tolerance,
                   ValidationType ValidationType);
bool doValuesMatch(float A, float B, float Tolerance,
                   ValidationType ValidationType);
bool doValuesMatch(double A, double B, float Tolerance,
                   ValidationType ValidationType);

template <typename T>
bool doVectorsMatch(const std::vector<T> &ActualValues,
                    const std::vector<T> &ExpectedValues, float Tolerance,
                    ValidationType ValidationType, bool VerboseLogging = false);

template <typename T>
void logLongVector(const std::vector<T> &Values, const std::wstring &Name);

// The TestInputs struct is used to help simplify calls to
// TestConfig::computeExpectedValues and to help us re-infer information about
// the number and type of inputs for the intrinsic being tested.
// InputVector1 represnts the first argument to the intrinsic being tested.
// InputVector1 is always present as all intrinsics being tested take at least
// one input.
// The other std::optional InputVectorN members represent the argument N for the
// intrinsic. They are stored as std::optional as they may not be present. It
// depends on the intrinsic being tested.
// The length of the InputVector is used to infer if the argument is a scalar
// or vector.
template <typename T> struct TestInputs {
  std::vector<T> InputVector1;
  std::optional<std::vector<T>> InputVector2 = std::nullopt;
  std::optional<std::vector<T>> InputVector3 = std::nullopt;
};

// Base interface for an ExpectedValueComputer. Derived classes implement
// based on their operation type. This class and its derived classes are
// responsible for computing the expected values for a given set of inputs.
// This class pattern allows us to abstract common logic for filling the
// expected vector based on operation type.
template <typename T> class ExpectedValueComputerBase {
public:
  virtual ~ExpectedValueComputerBase() = default;
  virtual VariantVector computeExpectedValues(const TestInputs<T> &Inputs) = 0;
};

// Default T2 to T1 as most intrinsics have a return type that matches the input
// type. For intrinsics that don't match that pattern the caller can specify
// the output type explicitly via an argument for T2.
template <typename T1, typename T2 = T1>
class UnaryOpExpectedValueComputer : public ExpectedValueComputerBase<T1> {
public:
  using ComputeFuncPtr = std::function<T2(const T1 &)>;
  UnaryOpExpectedValueComputer(ComputeFuncPtr func) : ComputeFunc(func) {}
  VariantVector computeExpectedValues(const TestInputs<T1> &Inputs) override {
    VariantVector ExpectedVector = generateExpectedVector<T2>(
        Inputs.InputVector1.size(),
        [&](size_t Index) { return ComputeFunc(Inputs.InputVector1[Index]); });
    return ExpectedVector;
  }

private:
  const ComputeFuncPtr ComputeFunc;
};

// Default T2 to T1 as most intrinsics have a return type that matches the input
// type. For intrinsics that don't match that pattern the caller can specify
// the output type explicitly via an argument for T2.
template <typename T1, typename T2 = T1>
class BinaryOpExpectedValueComputer : public ExpectedValueComputerBase<T1> {
public:
  using ComputeFuncPtr = std::function<T2(const T1 &, const T1 &)>;

  BinaryOpExpectedValueComputer(ComputeFuncPtr func) : ComputeFunc(func) {}

  VariantVector computeExpectedValues(const TestInputs<T1> &Inputs) override {

    const auto &Input1 = Inputs.InputVector1;

    DXASSERT_NOMSG(Inputs.InputVector2.has_value());
    const auto &Input2 = Inputs.InputVector2.value();

    VariantVector ExpectedVector =
        generateExpectedVector<T2>(Input1.size(), [&](size_t Index) {
          const T1 &B = (Input2.size() == 1 ? Input2[0] : Input2[Index]);

          return ComputeFunc(Input1[Index], B);
        });

    return ExpectedVector;
  }

private:
  const ComputeFuncPtr ComputeFunc;
};

// Default T2 to T1 as most intrinsics have a return type that matches the input
// type. For intrinsics that don't match that pattern the caller can specify
// the output type explicitly via an argument for T2.
template <typename T1, typename T2 = T1>
class TernaryOpExpectedValueComputer : public ExpectedValueComputerBase<T1> {

public:
  using ComputeFuncPtr = std::function<T2(const T1 &, const T1 &, const T1 &)>;

  TernaryOpExpectedValueComputer(ComputeFuncPtr func) : ComputeFunc(func) {}

  VariantVector computeExpectedValues(const TestInputs<T1> &Inputs) override {

    const auto &Input1 = Inputs.InputVector1;

    DXASSERT_NOMSG(Inputs.InputVector2.has_value());
    const auto &Input2 = Inputs.InputVector2.value();

    DXASSERT_NOMSG(Inputs.InputVector3.has_value());
    const auto &Input3 = Inputs.InputVector3.value();

    VariantVector ExpectedVector =
        generateExpectedVector<T2>(Input1.size(), [&](size_t Index) {
          const T1 &B = (Input2.size() == 1 ? Input2[0] : Input2[Index]);
          const T1 &C = (Input3.size() == 1 ? Input3[0] : Input3[Index]);

          return ComputeFunc(Input1[Index], B, C);
        });

    return ExpectedVector;
  }

private:
  const ComputeFuncPtr ComputeFunc;
};

// Helps handle the test configuration for LongVector operations.
// It is particularly useful helping manage logic of computing expected values
// and verifying the output. Especially helpful due to templating on the
// different data types and giving us a relatively clean way to leverage
// different logic paths for different HLSL instrinsics while keeping the main
// test code pretty generic.
// For each *OpType enum defined a *TestConfig specialization should be
// implemented to handle the specific logic for that operation. Generally that
// includes some basic setup like setting the BasicOpType as well as logic to
// compute expected values. See ExpectedValueComputer* use and definitions for
// more details on computing expected values.
template <typename T> class TestConfig {
public:
  virtual ~TestConfig() = default;

  void fillInputs(TestInputs<T> &Inputs) const;

  // Derived classes don't need to implement this function if they configure and
  // set a ExpectedValueComputer member.
  virtual void computeExpectedValues(const TestInputs<T> &Inputs);

  void setInputValueSetKey(const std::wstring &InputValueSetName,
                           size_t Index) {
    VERIFY_IS_TRUE(Index < (InputValueSetKeys.size()),
                   L"Index out of bounds for InputValueSetKeys");
    InputValueSetKeys[Index] = InputValueSetName;
  }

  void setLengthToTest(size_t LengthToTest) {
    this->LengthToTest = LengthToTest;
  }
  void setVerboseLogging(bool VerboseLogging) {
    this->VerboseLogging = VerboseLogging;
  }

  std::string getCompilerOptionsString() const;

  bool verifyOutput(const std::shared_ptr<st::ShaderOpTestResult> &TestResult);

  size_t getNumOperands() const;
  std::string getBasicOpTypeHexString() const;

private:
  std::vector<T> getInputValueSet(size_t ValueSetIndex) const;

  // Helpers to get the hlsl type as a string for a given C++ type.
  std::string getHLSLInputTypeString() const { return getHLSLTypeString<T>(); }
  std::string getHLSLOutputTypeString() const;

  template <typename OutT>
  bool verifyOutput(const std::shared_ptr<st::ShaderOpTestResult> &TestResult,
                    const std::vector<OutT> &ExpectedVector);

  // The input value sets are used to fill the shader buffer.
  std::array<std::wstring, 3> InputValueSetKeys = {L"DefaultInputValueSet1",
                                                   L"DefaultInputValueSet2",
                                                   L"DefaultInputValueSet3"};

protected:
  // Prevent instances of TestConfig from being created directly. Want to force
  // a derived class to be used for creation.
  template <typename OpT>
  TestConfig(const OpTypeMetaData<OpT> &OpTypeMd)
      : OpTypeName(OpTypeMd.OpTypeString), Intrinsic(OpTypeMd.Intrinsic),
        Operator(OpTypeMd.Operator),
        ScalarInputFlags(OpTypeMd.ScalarInputFlags) {}

  // Helper to initialize a unary value computer.
  template <typename OutT>
  void InitUnaryOpValueComputer(std::function<OutT(const T &)> ComputeFunc) {
    DXASSERT_NOMSG(BasicOpType == BasicOpType_Unary);
    DXASSERT_NOMSG(ExpectedValueComputer == nullptr);
    ExpectedValueComputer =
        std::make_unique<UnaryOpExpectedValueComputer<T, OutT>>(ComputeFunc);
  }

  // Helper to initialize a binary value computer.
  template <typename OutT>
  void InitBinaryOpValueComputer(
      std::function<OutT(const T &, const T &)> ComputeFunc) {
    DXASSERT_NOMSG(BasicOpType == BasicOpType_Binary);
    DXASSERT_NOMSG(ExpectedValueComputer == nullptr);
    ExpectedValueComputer =
        std::make_unique<BinaryOpExpectedValueComputer<T, OutT>>(ComputeFunc);
  }

  // Helper to initialize a ternary value computer.
  template <typename OutT>
  void InitTernaryOpValueComputer(
      std::function<OutT(const T &, const T &, const T &)> ComputeFunc) {
    DXASSERT_NOMSG(BasicOpType == BasicOpType_Ternary);
    DXASSERT_NOMSG(ExpectedValueComputer == nullptr);
    ExpectedValueComputer =
        std::make_unique<TernaryOpExpectedValueComputer<T, OutT>>(ComputeFunc);
  }

  std::unique_ptr<ExpectedValueComputerBase<T>> ExpectedValueComputer = nullptr;

  // To be used for the value of -DOPERATOR
  std::optional<std::string> Operator;
  // To be used for the value of -DFUNC
  std::optional<std::string> Intrinsic;
  // Used to add special -D defines to the compiler options.
  std::optional<std::string> SpecialDefines = std::nullopt;
  BasicOpType BasicOpType = BasicOpType_EnumValueCount;
  float Tolerance = 0.0;
  ValidationType ValidationType = ValidationType::ValidationType_Epsilon;
  // Default the TypedOutputVector to use T, Ops that don't have a
  // matching output type will override this.
  VariantVector ExpectedVector = std::vector<T>{};
  size_t LengthToTest = 0;

  // Just used for logging purposes.
  std::wstring OpTypeName = L"UnknownOpType";
  bool VerboseLogging = false;

  const uint16_t ScalarInputFlags;
}; // class TestConfig

// Specialized TestConfig for AsType operations. Implements logic for computing
// expected values. Also includes overrides of individual functions for
// computing expected values to provide runtime errors if an unsupported data
// type is used for a given intrinsic. See the individual overrides of functions
// for more details.
template <typename T> class AsTypeOpTestConfig : public TestConfig<T> {
public:
  AsTypeOpTestConfig(const OpTypeMetaData<AsTypeOpType> &OpTypeMd);

  // Override the base class method so we can handle split double as it has two
  // out parameters.
  void computeExpectedValues(const TestInputs<T> &Inputs) override;

private:
  void computeExpectedValues_SplitDouble(const std::vector<T> &InputVector);

  template <typename T>
  HLSLHalf_t asFloat16([[maybe_unused]] const T &A) const {
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsFloat16 T: %s",
                        typeid(T).name());
    return HLSLHalf_t();
  }

  HLSLHalf_t asFloat16(const HLSLHalf_t &A) const { return HLSLHalf_t(A.Val); }

  HLSLHalf_t asFloat16(const int16_t &A) const {
    return HLSLHalf_t(bit_cast<DirectX::PackedVector::HALF>(A));
  }

  HLSLHalf_t asFloat16(const uint16_t &A) const {
    return HLSLHalf_t(bit_cast<DirectX::PackedVector::HALF>(A));
  }

  template <typename T> float asFloat(const T &) const {
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsFloat T: %S",
                        typeid(T).name());
    return 0.0f;
  }

  float asFloat(const float &A) const { return float(A); }
  float asFloat(const int32_t &A) const { return bit_cast<float>(A); }
  float asFloat(const uint32_t &A) const { return bit_cast<float>(A); }

  template <typename T> int32_t asInt([[maybe_unused]] const T &A) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsInt T: %S",
                        typeid(T).name());
    return 0;
  }

  int32_t asInt(const float &A) const { return bit_cast<int32_t>(A); }
  int32_t asInt(const int32_t &A) const { return A; }
  int32_t asInt(const uint32_t &A) const { return bit_cast<int32_t>(A); }

  template <typename T> int16_t asInt16([[maybe_unused]] const T &A) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsInt16 T: %S",
                        typeid(T).name());
    return 0;
  }

  int16_t asInt16(const HLSLHalf_t &A) const {
    return bit_cast<int16_t>(A.Val);
  }
  int16_t asInt16(const int16_t &A) const { return A; }
  int16_t asInt16(const uint16_t &A) const { return bit_cast<int16_t>(A); }

  template <typename T> uint16_t asUint16([[maybe_unused]] const T &A) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsUint16 T: %S",
                        typeid(T).name());
    return 0;
  }

  uint16_t asUint16(const HLSLHalf_t &A) const {
    return bit_cast<uint16_t>(A.Val);
  }
  uint16_t asUint16(const uint16_t &A) const { return A; }
  uint16_t asUint16(const int16_t &A) const { return bit_cast<uint16_t>(A); }

  template <typename T> unsigned int asUint([[maybe_unused]] const T &A) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsUint T: %S",
                        typeid(T).name());
    return 0;
  }

  unsigned int asUint(const unsigned int &A) const { return A; }
  unsigned int asUint(const float &A) const {
    return bit_cast<unsigned int>(A);
  }
  unsigned int asUint(const int &A) const { return bit_cast<unsigned int>(A); }

  template <typename T>
  void splitDouble([[maybe_unused]] const T &A,
                   [[maybe_unused]] uint32_t &LowBits,
                   [[maybe_unused]] uint32_t &HighBits) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: splitDouble only accepts a double "
                        L"as input. Have DataTypeInT: %s",
                        typeid(T).name());
  }

  void splitDouble(const double &A, uint32_t &LowBits,
                   uint32_t &HighBits) const {
    uint64_t Bits = 0;
    std::memcpy(&Bits, &A, sizeof(Bits));
    LowBits = static_cast<uint32_t>(Bits & 0xFFFFFFFF);
    HighBits = static_cast<uint32_t>(Bits >> 32);
  }

  template <typename T>
  double asDouble([[maybe_unused]] const T &LowBits,
                  [[maybe_unused]] const T &HighBits) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: asDouble only accepts two uint32_t "
                        L"inputs. Have T : %S",
                        typeid(T).name());
    return 0.0;
  }

  double asDouble(const uint32_t &LowBits, const uint32_t &HighBits) const {
    uint64_t Bits = (static_cast<uint64_t>(HighBits) << 32) | LowBits;
    double Result;
    std::memcpy(&Result, &Bits, sizeof(Result));
    return Result;
  }

  AsTypeOpType OpType = AsTypeOpType_EnumValueCount;
};

template <typename T> class TrigonometricOpTestConfig : public TestConfig<T> {
public:
  TrigonometricOpTestConfig(
      const OpTypeMetaData<TrigonometricOpType> &OpTypeMd);

  T computeExpectedValue(const T &A) const;

private:
  TrigonometricOpType OpType = TrigonometricOpType_EnumValueCount;
};

template <typename T> class UnaryOpTestConfig : public TestConfig<T> {
public:
  UnaryOpTestConfig(const OpTypeMetaData<UnaryOpType> &OpTypeMd);

  T computeExpectedValue(const T &A) const;

private:
  UnaryOpType OpType = UnaryOpType_EnumValueCount;
};

template <typename T> class UnaryMathOpTestConfig : public TestConfig<T> {
public:
  UnaryMathOpTestConfig(const OpTypeMetaData<UnaryMathOpType> &OpTypeMd);

  // Override the base class method so we can handle frexp as it has
  // an out parameter.
  void computeExpectedValues(const TestInputs<T> &Inputs) override;
  T computeExpectedValue(const T &A) const;

private:
  void computeExpectedValues_Frexp(const std::vector<T> &InputVector);

  UnaryMathOpType OpType = UnaryMathOpType_EnumValueCount;

  // The majority of HLSL intrinsics return a DataType matching the
  // input DataType. However, Sign always returns an int32_t.
  template <typename T> int32_t sign(const T &A) const {
    // Return 1 for positive, -1 for negative, 0 for zero.
    // Wrap comparison operands in DataTypeInT constructor to make sure
    // we are comparing the same type.
    return A > T(0) ? 1 : A < T(0) ? -1 : 0;
  }

  template <typename T> T abs(const T &A) const {
    if constexpr (std::is_unsigned<T>::value)
      return T(A);
    else
      // Cast to T for the int16_t case because std::abs implicitly
      // converts to and returns an int. Without the cast back to an int the
      // compiler will complain that the implicit conversion back to int16_t has
      // lost precision.
      return static_cast<T>((std::abs)(A));
  }

  template <typename T = DataTypeT>
  typename std::enable_if<!(std::is_same<T, float>::value), float>::type
  frexp([[maybe_unused]] const T &A, [[maybe_unused]] float *Exponent) const {
    LOG_ERROR_FMT_THROW(L"Programmer Error: frexp only accepts floats. "
                        L"Have DataTypeT: %s",
                        typeid(T).name());
    return 0.0f;
  }

  template <typename T = DataTypeT>
  typename std::enable_if<(std::is_same<T, float>::value), T>::type
  frexp(const T &A, T *Exponent) const {
    int IntExp = 0;

    // std::frexp returns a signed mantissa. But the HLSL implmentation returns
    // an unsigned mantissa.
    T mantissa = std::abs(std::frexp(A, &IntExp));

    // std::frexp returns the exponent as an int, but HLSL stores it as a float.
    // However, the HLSL exponents fractional component is always 0. So it can
    // conversion between float and int is safe.
    *Exponent = static_cast<T>(IntExp);
    return mantissa;
  }
};

template <typename T> class BinaryMathOpTestConfig : public TestConfig<T> {
public:
  BinaryMathOpTestConfig(const OpTypeMetaData<BinaryMathOpType> &OpTypeMd);

  T computeExpectedValue(const T &A, const T &B) const;

private:
  BinaryMathOpType OpType = BinaryMathOpType_EnumValueCount;

  // Helpers so we do the right thing for float types. HLSLHalf_t is handled in
  // an operator overload.
  template <typename T = DataTypeT> T mod(const T &A, const T &B) const {
    return A % B;
  }

  template <> float mod(const float &A, const float &B) const {
    return std::fmod(A, B);
  }

  template <> double mod(const double &A, const double &B) const {
    return std::fmod(A, B);
  }

  template <typename T = T>
  typename std::enable_if<!(std::is_same<T, float>::value ||
                            std::is_same<T, HLSLHalf_t>::value),
                          T>::type
  ldexp([[maybe_unused]] const T &A, [[maybe_unused]] const T &Exponent) const {
    LOG_ERROR_FMT_THROW(L"Programmer Error: ldexp only accepts floatlikes. "
                        L"Have T: %s",
                        typeid(T).name());
    return T();
  }

  template <typename T = T>
  typename std::enable_if<(std::is_same<T, float>::value ||
                           std::is_same<T, HLSLHalf_t>::value),
                          T>::type
  ldexp(const T &A, const T &Exponent) const {
    return A * T(std::pow(2.0f, Exponent));
  }
};

template <typename T> class TernaryMathOpTestConfig : public TestConfig<T> {
public:
  TernaryMathOpTestConfig(const OpTypeMetaData<TernaryMathOpType> &OpTypeMd);

  T computeExpectedValue(const T &A, const T &B, const T &C) const {
    switch (OpType) {
    case TernaryMathOpType_Fma:
      return fma(A, B, C);
    case TernaryMathOpType_Mad:
      return mad(A, B, C);
    case TernaryMathOpType_SmoothStep:
      return smoothStep(A, B, C);
    default:
      LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid TernaryMathOpType: %d",
                          OpType);
      return T();
    }
  }

private:
  TernaryMathOpType OpType = TernaryMathOpType_EnumValueCount;

  template <typename T>
  T fma([[maybe_unused]] const T &A, [[maybe_unused]] const T &B,
        const T &C) const {
    LOG_ERROR_FMT_THROW(L"Programmer Error: fma only accepts doubles. Have "
                        L"T: %s",
                        typeid(T).name());
    return T();
  }

  // fma only accepts doubles
  template <>
  double fma(const double &A, const double &B, const double &C) const {
    return A * B + C;
  }

  // Mad is only enabled for numeric types. Capture that by having an fallback
  // that errors out if bool is used.
  template <typename T>
  typename std::enable_if<std::is_same<T, HLSLBool_t>::value, T>::type
  mad([[maybe_unused]] const T &A, [[maybe_unused]] const T &B,
      const T &C) const {
    LOG_ERROR_FMT_THROW(L"Programmer Error: mad does not support HLSLBool_t");
    return T();
  }

  template <typename T>
  typename std::enable_if<!std::is_same<T, HLSLBool_t>::value, T>::type
  mad(const T &A, const T &B, const T &C) const {
    return A * B + C;
  }

  // Smoothstep Fallback: only enabled when T is NOT a floatlike
  template <typename T>
  typename std::enable_if<!(std::is_same<T, float>::value ||
                            std::is_same<T, HLSLHalf_t>::value ||
                            std::is_same<T, double>::value),
                          T>::type
  smoothStep([[maybe_unused]] const T &Min, [[maybe_unused]] const T &Max,
             [[maybe_unused]] const T &X) const {
    LOG_ERROR_FMT_THROW(L"Programmer Error: smoothStep only accepts "
                        L"floatlikes. Have T: %s",
                        typeid(T).name());
    return T();
  }

  // Smoothstep is only enabled for floatlikes
  template <typename T = T>
  typename std::enable_if<std::is_same<T, float>::value ||
                              std::is_same<T, HLSLHalf_t>::value ||
                              std::is_same<T, double>::value,
                          T>::type
  smoothStep(const T &Min, const T &Max, const T &X) const {
    DXASSERT_NOMSG(Min < Max);

    if (X <= Min)
      return T(0);
    if (X >= Max)
      return T(1);

    T NormalizedX = (X - Min) / (Max - Min);
    NormalizedX = std::clamp(NormalizedX, T(0), T(1));
    return NormalizedX * NormalizedX * (T(3) - T(2) * NormalizedX);
  }
};

template <typename T>
std::unique_ptr<TestConfig<T>>
makeTestConfig(const OpTypeMetaData<UnaryOpType> &OpTypeMetaData) {
  return std::make_unique<UnaryOpTestConfig<T>>(OpTypeMetaData);
}

template <typename T>
std::unique_ptr<TestConfig<T>>
makeTestConfig(const OpTypeMetaData<TrigonometricOpType> &OpTypeMetaData) {
  return std::make_unique<TrigonometricOpTestConfig<T>>(OpTypeMetaData);
}

template <typename T>
std::unique_ptr<TestConfig<T>>
makeTestConfig(const OpTypeMetaData<AsTypeOpType> &OpTypeMetaData) {
  return std::make_unique<AsTypeOpTestConfig<T>>(OpTypeMetaData);
}

template <typename T>
std::unique_ptr<TestConfig<T>>
makeTestConfig(const OpTypeMetaData<UnaryMathOpType> &OpTypeMetaData) {
  return std::make_unique<UnaryMathOpTestConfig<T>>(OpTypeMetaData);
}

template <typename T>
std::unique_ptr<TestConfig<T>>
makeTestConfig(const OpTypeMetaData<BinaryMathOpType> &OpTypeMetaData) {
  return std::make_unique<BinaryMathOpTestConfig<T>>(OpTypeMetaData);
}

template <typename T>
std::unique_ptr<TestConfig<T>>
makeTestConfig(const OpTypeMetaData<TernaryMathOpType> &OpTypeMetaData) {
  return std::make_unique<TernaryMathOpTestConfig<T>>(OpTypeMetaData);
}
}; // namespace LongVector

#endif // LONGVECTORS_H
