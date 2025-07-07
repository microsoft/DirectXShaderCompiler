#ifndef LONGVECTORS_H
#define LONGVECTORS_H

#include <array>
#include <ostream>
#include <random>
#include <sstream>
#include <string>

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
template <typename DataTypeT, typename LongVectorOpTypeT>
class TestConfig; // Forward declaration

class OpTest {
public:
  BEGIN_TEST_CLASS(OpTest)
  END_TEST_CLASS()

  TEST_CLASS_SETUP(classSetup);

  BEGIN_TEST_METHOD(binaryOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#BinaryOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(unaryOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#UnaryOpTable")
  END_TEST_METHOD()

  template <typename LongVectorOpTypeT>
  void dispatchTestByDataType(LongVectorOpTypeT OpType, std::wstring DataType,
                              TableParameterHandler &Handler);

  template <typename DataTypeT, typename LongVectorOpTypeT>
  void dispatchTestByVectorSize(LongVectorOpTypeT OpType,
                                TableParameterHandler &Handler);

  template <typename DataTypeT, typename LongVectorOpTypeT>
  void testBaseMethod(
      LongVector::TestConfig<DataTypeT, LongVectorOpTypeT> &TestConfig,
      size_t VectorSizeToTest);

private:
  dxc::DxcDllSupport DxcDllSupport;
  bool Initialized = false;
};

template <typename DataTypeT>
void fillShaderBufferFromLongVectorData(std::vector<BYTE> &ShaderBuffer,
                                        std::vector<DataTypeT> &TestData);

template <typename DataTypeT>
void fillLongVectorDataFromShaderBuffer(MappedData &ShaderBuffer,
                                        std::vector<DataTypeT> &TestData,
                                        size_t NumElements);

template <typename DataTypeT> constexpr bool isFloatingPointType() {
  return std::is_same_v<DataTypeT, float> || std::is_same_v<DataTypeT, double>;
}

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

template <typename DataTypeT>
std::vector<DataTypeT> getInputValueSetByKey(const std::wstring &Key,
                                             bool LogKey = true) {
  if (LogKey)
    WEX::Logging::Log::Comment(
        WEX::Common::String().Format(L"Using Value Set Key: %s", Key.c_str()));
  return std::vector<DataTypeT>(LongVectorTestData<DataTypeT>::Data.at(Key));
}

template <typename DataTypeT>
DataTypeT mod(const DataTypeT &A, const DataTypeT &B);

template <typename LongVectorOpTypeT> struct TestConfigTraits {
  TestConfigTraits(LongVectorOpTypeT OpType) : OpType(OpType) {}
  // LongVectorOpTypeT* Enum values. We don't use a UINT because
  // we want the type data.
  LongVectorOpTypeT OpType;
};

template <typename DataTypeT>
bool doValuesMatch(DataTypeT A, DataTypeT B, float Tolerance, ValidationType);
bool doValuesMatch(float A, float B, float Tolerance,
                   ValidationType ValidationType);
bool doValuesMatch(double A, double B, float Tolerance,
                   ValidationType ValidationType);

template <typename DataTypeT>
bool doVectorsMatch(const std::vector<DataTypeT> &ActualValues,
                    const std::vector<DataTypeT> &ExpectedValues,
                    float Tolerance, ValidationType ValidationType);
// Binary ops
template <typename DataTypeT, typename LongVectorOpTypeT>
std::vector<DataTypeT>
computeExpectedValues(const std::vector<DataTypeT> &InputVector1,
                      const std::vector<DataTypeT> &InputVector2,
                      const TestConfig<DataTypeT, LongVectorOpTypeT> &Config);

// Binary scalar ops
template <typename DataTypeT, typename LongVectorOpTypeT>
std::vector<DataTypeT>
computeExpectedValues(const std::vector<DataTypeT> &InputVector1,
                      const DataTypeT &ScalarInput,
                      const TestConfig<DataTypeT, LongVectorOpTypeT> &Config);

// Unary ops
template <typename DataTypeT, typename LongVectorOpTypeT>
std::vector<DataTypeT>
computeExpectedValues(const std::vector<DataTypeT> &InputVector1,
                      const TestConfig<DataTypeT, LongVectorOpTypeT> &Config);

template <typename DataTypeT>
void logLongVector(const std::vector<DataTypeT> &Values,
                   const std::wstring &Name);

// Used to pass into LongVectorOpTestBase
template <typename DataTypeT, typename LongVectorOpTypeT> class TestConfig {
public:
  TestConfig() = default;

  TestConfig(UnaryOpType OpType);
  TestConfig(BinaryOpType OpType);

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

  bool hasFunctionDefinition() const;
  std::string getOPERAND2String() const;

  // A helper to get the hlsl type as a string for a given C++ type.
  // Used in the long vector tests.
  std::string getHLSLTypeString() const;

  DataTypeT computeExpectedValue(const DataTypeT &A, const DataTypeT &B,
                                 BinaryOpType OpType) const;
  DataTypeT computeExpectedValue(const DataTypeT &A, const DataTypeT &B) const;
  DataTypeT computeExpectedValue(const DataTypeT &A, UnaryOpType OpType) const;
  DataTypeT computeExpectedValue(const DataTypeT &A) const;

  void setInputValueSet1(const std::wstring &InputValueSetName) {
    this->InputValueSetName1 = InputValueSetName;
  }

  void setInputValueSet2(const std::wstring &InputValueSetName) {
    this->InputValueSetName2 = InputValueSetName;
  }

  std::vector<DataTypeT> getInputValueSet1() const {
    return getInputValueSet(1);
  }

  std::vector<DataTypeT> getInputValueSet2() const {
    return getInputValueSet(2);
  }

  float getTolerance() const { return Tolerance; }
  LongVector::ValidationType getValidationType() const {
    return ValidationType;
  }

  std::string getCompilerOptionsString(size_t VectorSize) const;

private:
  std::vector<DataTypeT> getInputValueSet(size_t ValueSetIndex) const;

  // To be used for the value of -DOPERATOR
  std::string OperatorString;
  // To be used for the value of -DFUNC
  std::string IntrinsicString;
  LongVector::BasicOpType BasicOpType = LongVector::BasicOpType_EnumValueCount;
  float Tolerance = 0.0;
  LongVector::ValidationType ValidationType =
      LongVector::ValidationType::ValidationType_Epsilon;
  LongVector::TestConfigTraits<LongVectorOpTypeT> OpTypeTraits;
  std::wstring InputValueSetName1 = L"DefaultInputValueSet1";
  std::wstring InputValueSetName2 = L"DefaultInputValueSet2";
}; // class LongVector::TestConfig

}; // namespace LongVector

#include "LongVectors.tpp"

#endif // LONGVECTORS_H
