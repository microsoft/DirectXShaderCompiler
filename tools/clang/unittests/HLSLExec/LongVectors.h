#ifndef LONGVECTORS_H
#define LONGVECTORS_H

#include <array>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <type_traits>
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

template <typename DataTypeT> class TestConfig; // Forward declaration

class OpTest {
public:
  BEGIN_TEST_CLASS(OpTest)
  END_TEST_CLASS()

  TEST_CLASS_SETUP(classSetup);

  BEGIN_TEST_METHOD(binaryOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#BinaryOpTable")
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

  template <typename LongVectorOpTypeT>
  void dispatchTestByDataType(LongVectorOpTypeT OpType, std::wstring DataType,
                              TableParameterHandler &Handler);

  template <typename DataTypeT, typename LongVectorOpTypeT>
  void dispatchTestByVectorLength(LongVectorOpTypeT OpType,
                                  TableParameterHandler &Handler);

  template <typename DataTypeT, typename LongVectorOpTypeT>
  void testBaseMethod(
      std::shared_ptr<LongVector::TestConfig<DataTypeT>> &TestConfig);

private:
  dxc::DxcDllSupport DxcDllSupport;
  bool Initialized = false;
};

// Used so we can dynamically resolve the type of data stored out the output
// LongVector for a test case. Leveraging another template paramter for an
// output data type on the TestConfig was too much.
using VariantVector =
    std::variant<std::vector<HLSLBool_t>, std::vector<HLSLHalf_t>,
                 std::vector<float>, std::vector<double>, std::vector<int16_t>,
                 std::vector<int32_t>, std::vector<int64_t>,
                 std::vector<uint16_t>, std::vector<uint32_t>,
                 std::vector<uint64_t>>;

// A helper struct to clear a VariantVector using std::visit.
// Example usage: std::visit(ClearVariantVector{}, MyVariantVector);
struct ClearVariantVector {
  template <typename T> void operator()(std::vector<T> &vec) const {
    vec.clear();
  }
};

template <typename DataTypeT>
void fillShaderBufferFromLongVectorData(std::vector<BYTE> &ShaderBuffer,
                                        std::vector<DataTypeT> &TestData);

template <typename DataTypeT>
void fillLongVectorDataFromShaderBuffer(MappedData &ShaderBuffer,
                                        std::vector<DataTypeT> &TestData,
                                        size_t NumElements);

template <typename DataTypeT> constexpr bool isFloatingPointType() {
  return std::is_same_v<DataTypeT, float> ||
         std::is_same_v<DataTypeT, double> ||
         std::is_same_v<DataTypeT, HLSLHalf_t>;
}

template <typename DataTypeT> std::string getHLSLTypeString();

struct LongVectorOpTypeStringToEnumValue {
  std::wstring OpTypeString;
  uint32_t OpTypeValue;
};

template <typename DataTypeT>
DataTypeT getLongVectorOpType(const LongVectorOpTypeStringToEnumValue *Values,
                              const std::wstring &OpTypeString,
                              std::size_t Length);

enum ValidationType {
  ValidationType_Epsilon,
  ValidationType_Ulp,
};

enum BasicOpType {
  BasicOpType_Binary,
  BasicOpType_Unary,
  BasicOpType_ScalarBinary,
  BasicOpType_EnumValueCount
};

enum BinaryOpType {
  BinaryOpType_ScalarAdd,
  BinaryOpType_ScalarMultiply,
  BinaryOpType_ScalarSubtract,
  BinaryOpType_ScalarDivide,
  BinaryOpType_ScalarModulus,
  BinaryOpType_Multiply,
  BinaryOpType_Add,
  BinaryOpType_Subtract,
  BinaryOpType_Divide,
  BinaryOpType_Modulus,
  BinaryOpType_Min,
  BinaryOpType_Max,
  BinaryOpType_ScalarMin,
  BinaryOpType_ScalarMax,
  BinaryOpType_EnumValueCount
};

static const LongVectorOpTypeStringToEnumValue binaryOpTypeStringToEnumMap[] = {
    {L"BinaryOpType_ScalarAdd", BinaryOpType_ScalarAdd},
    {L"BinaryOpType_ScalarMultiply", BinaryOpType_ScalarMultiply},
    {L"BinaryOpType_ScalarSubtract", BinaryOpType_ScalarSubtract},
    {L"BinaryOpType_ScalarDivide", BinaryOpType_ScalarDivide},
    {L"BinaryOpType_ScalarModulus", BinaryOpType_ScalarModulus},
    {L"BinaryOpType_Add", BinaryOpType_Add},
    {L"BinaryOpType_Multiply", BinaryOpType_Multiply},
    {L"BinaryOpType_Subtract", BinaryOpType_Subtract},
    {L"BinaryOpType_Divide", BinaryOpType_Divide},
    {L"BinaryOpType_Modulus", BinaryOpType_Modulus},
    {L"BinaryOpType_Min", BinaryOpType_Min},
    {L"BinaryOpType_Max", BinaryOpType_Max},
    {L"BinaryOpType_ScalarMin", BinaryOpType_ScalarMin},
    {L"BinaryOpType_ScalarMax", BinaryOpType_ScalarMax},
};

static_assert(_countof(binaryOpTypeStringToEnumMap) ==
                  BinaryOpType_EnumValueCount,
              "binaryOpTypeStringToEnumMap size mismatch. Did you "
              "add a new enum value?");

BinaryOpType getBinaryOpType(const std::wstring &OpTypeString);

enum UnaryOpType { UnaryOpType_Initialize, UnaryOpType_EnumValueCount };

static const LongVectorOpTypeStringToEnumValue unaryOpTypeStringToEnumMap[] = {
    {L"UnaryOpType_Initialize", UnaryOpType_Initialize},
};

static_assert(_countof(unaryOpTypeStringToEnumMap) ==
                  UnaryOpType_EnumValueCount,
              "unaryOpTypeStringToEnumMap size mismatch. Did you add "
              "a new enum value?");

UnaryOpType getUnaryOpType(const std::wstring &OpTypeString);

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

static const LongVectorOpTypeStringToEnumValue asTypeOpTypeStringToEnumMap[] = {
    {L"AsTypeOpType_AsFloat", AsTypeOpType_AsFloat},
    {L"AsTypeOpType_AsFloat16", AsTypeOpType_AsFloat16},
    {L"AsTypeOpType_AsInt", AsTypeOpType_AsInt},
    {L"AsTypeOpType_AsInt16", AsTypeOpType_AsInt16},
    {L"AsTypeOpType_AsUint", AsTypeOpType_AsUint},
    {L"AsTypeOpType_AsUint_SplitDouble", AsTypeOpType_AsUint_SplitDouble},
    {L"AsTypeOpType_AsUint16", AsTypeOpType_AsUint16},
    {L"AsTypeOpType_AsDouble", AsTypeOpType_AsDouble},
};

static_assert(_countof(asTypeOpTypeStringToEnumMap) ==
                  AsTypeOpType_EnumValueCount,
              "asTypeOpTypeStringToEnumMap size mismatch. Did you add "
              "a new enum value?");

AsTypeOpType getAsTypeOpType(const std::wstring &OpTypeString);

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

static const LongVectorOpTypeStringToEnumValue
    trigonometricOpTypeStringToEnumMap[] = {
        {L"TrigonometricOpType_Acos", TrigonometricOpType_Acos},
        {L"TrigonometricOpType_Asin", TrigonometricOpType_Asin},
        {L"TrigonometricOpType_Atan", TrigonometricOpType_Atan},
        {L"TrigonometricOpType_Cos", TrigonometricOpType_Cos},
        {L"TrigonometricOpType_Cosh", TrigonometricOpType_Cosh},
        {L"TrigonometricOpType_Sin", TrigonometricOpType_Sin},
        {L"TrigonometricOpType_Sinh", TrigonometricOpType_Sinh},
        {L"TrigonometricOpType_Tan", TrigonometricOpType_Tan},
        {L"TrigonometricOpType_Tanh", TrigonometricOpType_Tanh},
};

static_assert(_countof(trigonometricOpTypeStringToEnumMap) ==
                  TrigonometricOpType_EnumValueCount,
              "trigonometricOpTypeStringToEnumMap size mismatch. Did you add "
              "a new enum value?");

TrigonometricOpType getTrigonometricOpType(const std::wstring &OpTypeString);

template <typename DataTypeT>
std::vector<DataTypeT> getInputValueSetByKey(const std::wstring &Key,
                                             bool LogKey = true) {
  if (LogKey)
    WEX::Logging::Log::Comment(
        WEX::Common::String().Format(L"Using Value Set Key: %s", Key.c_str()));
  return std::vector<DataTypeT>(LongVectorTestData<DataTypeT>::Data.at(Key));
}

template <typename DataTypeT>
bool doValuesMatch(DataTypeT A, DataTypeT B, float Tolerance, ValidationType);
bool doValuesMatch(HLSLBool_t A, HLSLBool_t B, float, ValidationType);
bool doValuesMatch(HLSLHalf_t A, HLSLHalf_t B, float Tolerance,
                   ValidationType ValidationType);
bool doValuesMatch(float A, float B, float Tolerance,
                   ValidationType ValidationType);
bool doValuesMatch(double A, double B, float Tolerance,
                   ValidationType ValidationType);

template <typename DataTypeT>
bool doVectorsMatch(const std::vector<DataTypeT> &ActualValues,
                    const std::vector<DataTypeT> &ExpectedValues,
                    float Tolerance, ValidationType ValidationType);

template <typename DataTypeT>
void logLongVector(const std::vector<DataTypeT> &Values,
                   const std::wstring &Name);

// Helps handle the test configuration for LongVector operations.
// It was particularly useful helping manage logic of computing expected values
// and verifying the output. Especially helpful due to templating on the
// different data types and giving us a relatively clean way to leverage
// different logic paths for different HLSL instrinsics while keeping the main
// test code pretty generic.
template <typename DataTypeT> class TestConfig {
public:
  virtual ~TestConfig() = default;

  bool isBinaryOp() const {
    return BasicOpType == LongVector::BasicOpType_Binary ||
           BasicOpType == LongVector::BasicOpType_ScalarBinary;
  }

  bool isUnaryOp() const {
    return BasicOpType == LongVector::BasicOpType_Unary;
  }

  bool isScalarOp() const {
    return BasicOpType == LongVector::BasicOpType_ScalarBinary;
  }

  // Helpers to get the hlsl type as a string for a given C++ type.
  std::string getHLSLInputTypeString() const;
  virtual std::string getHLSLOutputTypeString() const;

  virtual void
  computeExpectedValues(const std::vector<DataTypeT> &InputVector1);
  virtual void
  computeExpectedValues(const std::vector<DataTypeT> &InputVector1,
                        const std::vector<DataTypeT> &InputVector2);
  void computeExpectedValues(const std::vector<DataTypeT> &InputVector1,
                             const DataTypeT &ScalarInput);

  void setInputValueSet1(const std::wstring &InputValueSetName) {
    InputValueSetName1 = InputValueSetName;
  }

  void setInputValueSet2(const std::wstring &InputValueSetName) {
    InputValueSetName2 = InputValueSetName;
  }

  void setLengthToTest(size_t LengthToTest) {
    // Make sure we clear the expected vector when setting a new length.
    // The TestConfig may be getting reused.
    std::visit(ClearVariantVector{}, ExpectedVector);

    this->LengthToTest = LengthToTest;
  }

  size_t getLengthToTest() const { return LengthToTest; }

  std::vector<DataTypeT> getInputValueSet1() const {
    return getInputValueSet(1);
  }

  std::vector<DataTypeT> getInputValueSet2() const {
    return getInputValueSet(2);
  }

  std::vector<DataTypeT> getInputArgsArray() const;

  float getTolerance() const { return Tolerance; }
  LongVector::ValidationType getValidationType() const {
    return ValidationType;
  }

  std::string getCompilerOptionsString() const;

  virtual bool
  verifyOutput(const std::shared_ptr<st::ShaderOpTestResult> &TestResult);

  void setOpTypeNameForLogging(const std::wstring &OpTypeNameForLogging) {
    OpTypeName = OpTypeNameForLogging;
  }

private:
  std::vector<DataTypeT> getInputValueSet(size_t ValueSetIndex) const;

  std::wstring InputValueSetName1 = L"DefaultInputValueSet1";
  std::wstring InputValueSetName2 = L"DefaultInputValueSet2";
  // No default args array
  std::wstring InputArgsArrayName = L"";

protected:
  // Prevent instances of TestConfig from being created directly. Want to force
  // a derived class to be used for creation.
  TestConfig() = default;

  // Templated version to be used when the output data type does not match the
  // input data type.
  template <typename OutputDataTypeT>
  bool verifyOutput(const std::shared_ptr<st::ShaderOpTestResult> &TestResult);

  // The appropriate computeExpectedValue should be implemented in derived
  // classes. Impelemented as virtual here to prevent requiring all derived
  // class from needing to implement. The OS builds disable RTTI, so using
  // dynamic casting to expose interfaces for these based on type isn't an
  // option. You're intended to use COM for that. But I'm not going to add all
  // of the COM overhead to this class just for that.
  virtual DataTypeT
  computeExpectedValue([[maybe_unused]] const DataTypeT &A,
                       [[maybe_unused]] const DataTypeT &B) const {
    LOG_ERROR_FMT_THROW(L"E_NOT_IMPL: computeExpectedValue for a Binary Op");
    return DataTypeT();
  }
  virtual DataTypeT
  computeExpectedValue([[maybe_unused]] const DataTypeT &A) const {
    LOG_ERROR_FMT_THROW(L"E_NOT_IMPL: computeExpectedValue for a Unary Op");
    return DataTypeT();
  }

  // To be used for the value of -DOPERATOR
  std::string OperatorString;
  // To be used for the value of -DFUNC
  std::string IntrinsicString;
  // Used to add special -D defines to the compiler options.
  std::string SpecialDefines = "";
  LongVector::BasicOpType BasicOpType = LongVector::BasicOpType_EnumValueCount;
  float Tolerance = 0.0;
  LongVector::ValidationType ValidationType =
      LongVector::ValidationType::ValidationType_Epsilon;
  // The input value sets are used to fill the shader buffer.
  // Default the TypedOutputVector to use DataTypeT, Ops that don't have a
  // matching output type will override this.
  LongVector::VariantVector ExpectedVector = std::vector<DataTypeT>{};
  size_t LengthToTest = 0;

  // Just used for logging purposes.
  std::wstring OpTypeName = L"UnknownOpType";
}; // class LongVector::TestConfig

template <typename DataTypeT>
class TestConfigAsType : public LongVector::TestConfig<DataTypeT> {
public:
  TestConfigAsType(LongVector::AsTypeOpType OpType);

  void
  computeExpectedValues(const std::vector<DataTypeT> &InputVector1) override;
  void
  computeExpectedValues(const std::vector<DataTypeT> &InputVector1,
                        const std::vector<DataTypeT> &InputVector2) override;
  std::string getHLSLOutputTypeString() const override;
  bool verifyOutput(
      const std::shared_ptr<st::ShaderOpTestResult> &TestResult) override;

private:
  template <typename DataTypeInT>
  HLSLHalf_t asFloat16([[maybe_unused]] const DataTypeInT &A) const {
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsFloat16 DataTypeInT: %s",
                        typeid(DataTypeInT).name());
    return HLSLHalf_t();
  }

  HLSLHalf_t asFloat16(const HLSLHalf_t &A) const { return HLSLHalf_t(A.Val); }

  HLSLHalf_t asFloat16(const int16_t &A) const {
    return HLSLHalf_t(LongVector::bit_cast<DirectX::PackedVector::HALF>(A));
  }

  HLSLHalf_t asFloat16(const uint16_t &A) const {
    return HLSLHalf_t(LongVector::bit_cast<DirectX::PackedVector::HALF>(A));
  }

  template <typename DataTypeInT> float asFloat(const DataTypeInT &) const {
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsFloat DataTypeInT: %S",
                        typeid(DataTypeInT).name());
    return float();
  }

  float asFloat(const float &A) const { return float(A); }
  float asFloat(const int32_t &A) const {
    return LongVector::bit_cast<float>(A);
  }
  float asFloat(const uint32_t &A) const {
    return LongVector::bit_cast<float>(A);
  }

  template <typename DataTypeInT>
  int32_t asInt([[maybe_unused]] const DataTypeInT &A) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsInt DataTypeInT: %S",
                        typeid(DataTypeInT).name());
    return int32_t();
  }

  int32_t asInt(const float &A) const {
    return LongVector::bit_cast<int32_t>(A);
  }
  int32_t asInt(const int32_t &A) const { return A; }
  int32_t asInt(const uint32_t &A) const {
    return LongVector::bit_cast<int32_t>(A);
  }

  template <typename DataTypeInT>
  int16_t asInt16([[maybe_unused]] const DataTypeInT &A) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsInt16 DataTypeInT: %S",
                        typeid(DataTypeInT).name());
    return int16_t();
  }

  int16_t asInt16(const HLSLHalf_t &A) const {
    return LongVector::bit_cast<int16_t>(A.Val);
  }
  int16_t asInt16(const int16_t &A) const { return A; }
  int16_t asInt16(const uint16_t &A) const {
    return LongVector::bit_cast<int16_t>(A);
  }

  template <typename DataTypeInT>
  uint16_t asUint16([[maybe_unused]] const DataTypeInT &A) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsUint16 DataTypeInT: %S",
                        typeid(DataTypeInT).name());
    return uint16_t();
  }

  uint16_t asUint16(const HLSLHalf_t &A) const {
    return LongVector::bit_cast<uint16_t>(A.Val);
  }
  uint16_t asUint16(const uint16_t &A) const { return A; }
  uint16_t asUint16(const int16_t &A) const {
    return LongVector::bit_cast<uint16_t>(A);
  }

  template <typename DataTypeInT>
  unsigned int asUint([[maybe_unused]] const DataTypeInT &A) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: Invalid AsUint DataTypeInT: %S",
                        typeid(DataTypeInT).name());
    return unsigned int();
  }

  unsigned int asUint(const unsigned int &A) const { return A; }
  unsigned int asUint(const float &A) const {
    return LongVector::bit_cast<unsigned int>(A);
  }
  unsigned int asUint(const int &A) const {
    return LongVector::bit_cast<unsigned int>(A);
  }

  template <typename DataTypeInT>
  void splitDouble([[maybe_unused]] const DataTypeInT &A,
                   [[maybe_unused]] uint32_t &LowBits,
                   [[maybe_unused]] uint32_t &HighBits) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: splitDouble only accepts a double "
                        L"as input. Have DataTypeInT: %s",
                        typeid(DataTypeInT).name());
  }

  void splitDouble(const double &A, uint32_t &LowBits,
                   uint32_t &HighBits) const {
    uint64_t Bits = 0;
    std::memcpy(&Bits, &A, sizeof(Bits));
    LowBits = static_cast<uint32_t>(Bits & 0xFFFFFFFF);
    HighBits = static_cast<uint32_t>(Bits >> 32);
  }

  template <typename DataTypeInT>
  double asDouble([[maybe_unused]] const DataTypeInT &LowBits,
                  [[maybe_unused]] const DataTypeInT &HighBits) const {
    // This path is unexpected outside of an issue when brining up new tests. So
    // throwing an exception is appropriate.
    LOG_ERROR_FMT_THROW(L"Programmer Error: asDouble only accepts two uint32_t "
                        L"inputs. Have DataTypeInT : %S",
                        typeid(DataTypeInT).name());
    return 0.0;
  }

  double asDouble(const uint32_t &LowBits, const uint32_t &HighBits) const {
    uint64_t Bits = (static_cast<uint64_t>(HighBits) << 32) | LowBits;
    double Result;
    std::memcpy(&Result, &Bits, sizeof(Result));
    return Result;
  }

  AsTypeOpType OpType = LongVector::AsTypeOpType_EnumValueCount;
};

template <typename DataTypeT>
class TestConfigTrigonometric : public LongVector::TestConfig<DataTypeT> {
public:
  TestConfigTrigonometric(LongVector::TrigonometricOpType OpType);
  DataTypeT computeExpectedValue(const DataTypeT &A) const override;

private:
  LongVector::TrigonometricOpType OpType =
      LongVector::TrigonometricOpType_EnumValueCount;
};

template <typename DataTypeT>
class TestConfigUnary : public LongVector::TestConfig<DataTypeT> {
public:
  TestConfigUnary(LongVector::UnaryOpType OpType);
  DataTypeT computeExpectedValue(const DataTypeT &A) const override;

private:
  LongVector::UnaryOpType OpType = LongVector::UnaryOpType_EnumValueCount;
};

template <typename DataTypeT>
class TestConfigBinary : public LongVector::TestConfig<DataTypeT> {
public:
  TestConfigBinary(LongVector::BinaryOpType OpType);
  DataTypeT computeExpectedValue(const DataTypeT &A,
                                 const DataTypeT &B) const override;

private:
  LongVector::BinaryOpType OpType = LongVector::BinaryOpType_EnumValueCount;

  // Helpers so we do the right thing for float types. HLSLHalf_t is handled in
  // an operator overload.
  template <typename DataTypeT>
  DataTypeT mod(const DataTypeT &A, const DataTypeT &B) const {
    return A % B;
  }

  template <> float mod(const float &A, const float &B) const {
    return std::fmod(A, B);
  }

  template <> double mod(const double &A, const double &B) const {
    return std::fmod(A, B);
  }
};

template <typename DataTypeT>
std::shared_ptr<LongVector::TestConfig<DataTypeT>>
makeTestConfig(UnaryOpType OpType);

template <typename DataTypeT>
std::shared_ptr<LongVector::TestConfig<DataTypeT>>
makeTestConfig(BinaryOpType OpType);

template <typename DataTypeT>
std::shared_ptr<LongVector::TestConfig<DataTypeT>>
makeTestConfig(TrigonometricOpType OpType);

template <typename DataTypeT>
std::shared_ptr<LongVector::TestConfig<DataTypeT>>
makeTestConfig(AsTypeOpType OpType);

}; // namespace LongVector

#endif // LONGVECTORS_H
