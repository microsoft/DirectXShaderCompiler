#ifndef LONGVECTORS_H
#define LONGVECTORS_H

#include <WexTestClass.h>

#include <optional>
#include <string>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include "LongVectorTestData.h"

namespace LongVector {

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

template <typename OpT, size_t Length>
const OpTypeMetaData<OpT> &
getOpType(const OpTypeMetaData<OpT> (&Values)[Length],
          const std::wstring &OpTypeString);

enum ValidationType {
  ValidationType_Epsilon,
  ValidationType_Ulp,
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

private:
  bool Initialized = false;
  bool VerboseLogging = false;
};

} // namespace LongVector

#endif // LONGVECTORS_H
