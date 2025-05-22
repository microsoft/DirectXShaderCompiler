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
#include "dxc/Support/WinIncludes.h"
#include "dxc/Test/HlslTestUtils.h"

// Helper to fill the shader buffer based on type. Convenient to be used when
// copying HLSL*_t types so we can copy the underlying type directly instead of
// the struct.
template <typename DataType>
void FillShaderBufferFromLongVectorData(std::vector<BYTE> &ShaderBuffer,
                                        std::vector<DataType> &TestData) {

  // Note: DataSize for HLSLHalf_t and HLSLBool_t may be larger than the
  // underlying type in some cases. Thats fine. Resize just makes sure we have
  // enough space.
  const size_t NumElements = TestData.size();
  const size_t DataSize = sizeof(DataType) * NumElements;
  ShaderBuffer.resize(DataSize);

  if constexpr (std::is_same_v<DataType, HLSLHalf_t>) {
    DirectX::PackedVector::HALF *ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      ShaderBufferPtr[i] = TestData[i].Val;
    }
  } else if constexpr (std::is_same_v<DataType, HLSLBool_t>) {
    int32_t *ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      ShaderBufferPtr[i] = TestData[i].Val;
    }
  } else {
    DataType *ShaderBufferPtr =
        reinterpret_cast<DataType *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      ShaderBufferPtr[i] = TestData[i];
    }
  }
}

// Helper to fill the test data from the shader buffer based on type. Convenient
// to be used when copying HLSL*_t types so we can use the underlying type.
template <typename DataType>
void FillLongVectorDataFromShaderBuffer(MappedData &ShaderBuffer,
                                        std::vector<DataType> &TestData,
                                        size_t NumElements) {
  if constexpr (std::is_same_v<DataType, HLSLHalf_t>) {
    DirectX::PackedVector::HALF *ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      // HLSLHalf_t has a DirectX::PackedVector::HALF based constructor.
      TestData.push_back(ShaderBufferPtr[i]);
    }
  } else if constexpr (std::is_same_v<DataType, HLSLBool_t>) {
    int32_t *ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      // HLSLBool_t has a int32_t based constructor.
      TestData.push_back(ShaderBufferPtr[i]);
    }
  } else {
    DataType *ShaderBufferPtr =
        reinterpret_cast<DataType *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      TestData.push_back(ShaderBufferPtr[i]);
    }
  }
}

template <typename DataType> constexpr bool IsFloatingPointType() {
  return std::is_same_v<DataType, float> || std::is_same_v<DataType, double> ||
         std::is_same_v<DataType, HLSLHalf_t>;
}

struct LongVectorOpTypeStringToEnumValue {
  std::wstring OpTypeString;
  uint32_t OpTypeValue;
};

template <typename DataType>
DataType GetLongVectorOpType(const LongVectorOpTypeStringToEnumValue *Values,
                             const std::wstring &OpTypeString,
                             std::size_t Length) {
  for (size_t i = 0; i < Length; i++) {
    if (Values[i].OpTypeString == OpTypeString) {
      return static_cast<DataType>(Values[i].OpTypeValue);
    }
  }

  LOG_ERROR_FMT_THROW(L"Invalid LongVectorOpType string: %s",
                      OpTypeString.c_str());

  return static_cast<DataType>(UINT_MAX);
}

namespace LongVector {

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
  // Below this line to be moved to HLSLOpType (+, -, * etc)
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
  // Below this line to be moved to HLSLMathOpType (min, max etc)
  BinaryOpType_Min,
  BinaryOpType_Max,
  BinaryOpType_ScalarMin,
  BinaryOpType_ScalarMax,
  BinaryOpType_EnumValueCount
};

static const LongVectorOpTypeStringToEnumValue BinaryOpTypeStringToEnumMap[] = {
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

static_assert(_countof(BinaryOpTypeStringToEnumMap) ==
                  BinaryOpType_EnumValueCount,
              "BinaryOpTypeStringToEnumMap size mismatch. Did you "
              "add a new enum value?");

BinaryOpType GetBinaryOpType(const std::wstring &OpTypeString);

enum UnaryOpType {
  UnaryOpType_Clamp,
  UnaryOpType_Initialize,
  UnaryOpType_EnumValueCount
};

static const LongVectorOpTypeStringToEnumValue UnaryOpTypeStringToEnumMap[] = {
    {L"UnaryOpType_Clamp", UnaryOpType_Clamp},
    {L"UnaryOpType_Initialize", UnaryOpType_Initialize},
};

static_assert(_countof(UnaryOpTypeStringToEnumMap) ==
                  UnaryOpType_EnumValueCount,
              "UnaryOpTypeStringToEnumMap size mismatch. Did you add "
              "a new enum value?");

UnaryOpType GetUnaryOpType(const std::wstring &OpTypeString);

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
    TrigonometricOpTypeStringToEnumMap[] = {
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

static_assert(_countof(TrigonometricOpTypeStringToEnumMap) ==
                  TrigonometricOpType_EnumValueCount,
              "TrigonometricOpTypeStringToEnumMap size mismatch. Did you add "
              "a new enum value?");

TrigonometricOpType GetTrigonometricOpType(const std::wstring &OpTypeString);

}; // namespace LongVector

template <typename DataType>
std::vector<DataType> GetInputValueSetByKey(const std::wstring &Key,
                                            bool LogKey = true) {
  if (LogKey)
    WEX::Logging::Log::Comment(
        WEX::Common::String().Format(L"Using Value Set Key: %s", Key.c_str()));
  return std::vector<DataType>(LongVectorTestData<DataType>::Data.at(Key));
}

// Helpers so we do the right thing for float types. HLSLHalf_t is handled in an
// operator overload.
template <typename DataType>
inline DataType Mod(const DataType &A, const DataType &B) {
  return A % B;
}

template <> inline float Mod(const float &A, const float &B) {
  return std::fmod(A, B);
}

template <> inline double Mod(const double &A, const double &B) {
  return std::fmod(A, B);
}

template <typename LongVectorOpType> struct LongVectorOpTestConfigTraits {
  LongVectorOpTestConfigTraits(LongVectorOpType OpType) : OpType(OpType) {}
  // LongVectorOpType* Enum values. We don't use a UINT because
  // we want the type data.
  LongVectorOpType OpType;
};

// Used to pass into LongVectorOpTestBase
template <typename DataType, typename LongVectorOpType>
class LongVectorOpTestConfig {
public:
  LongVectorOpTestConfig() = default;

  LongVectorOpTestConfig(LongVector::UnaryOpType OpType)
      : OpTypeTraits(OpType) {
    IntrinsicString = "";

    if (IsFloatingPointType<DataType>())
      Tolerance = 1;

    BasicOpType = LongVector::BasicOpType_Unary;

    switch (OpType) {
    case LongVector::UnaryOpType_Clamp:
      OperatorString = ",";
      IntrinsicString = "TestClamp";
      break;
    case LongVector::UnaryOpType_Initialize:
      IntrinsicString = "TestInitialize";
      break;
    default:
      VERIFY_FAIL("Invalid UnaryOpType");
    }
  }

  LongVectorOpTestConfig(LongVector::BinaryOpType OpType)
      : OpTypeTraits(OpType) {
    IntrinsicString = "";
    BasicOpType = LongVector::BasicOpType_Binary;

    if (IsFloatingPointType<DataType>())
      Tolerance = 1;
    ValidationType = LongVector::ValidationType_Ulp;

    switch (OpType) {
    case LongVector::BinaryOpType_ScalarAdd:
      BasicOpType = LongVector::BasicOpType_ScalarBinary;
      OperatorString = "+";
      break;
    case LongVector::BinaryOpType_ScalarMultiply:
      BasicOpType = LongVector::BasicOpType_ScalarBinary;
      OperatorString = "*";
      break;
    case LongVector::BinaryOpType_ScalarSubtract:
      BasicOpType = LongVector::BasicOpType_ScalarBinary;
      OperatorString = "-";
      break;
    case LongVector::BinaryOpType_ScalarDivide:
      BasicOpType = LongVector::BasicOpType_ScalarBinary;
      OperatorString = "/";
      break;
    case LongVector::BinaryOpType_ScalarModulus:
      BasicOpType = LongVector::BasicOpType_ScalarBinary;
      OperatorString = "%";
      break;
    case LongVector::BinaryOpType_Multiply:
      OperatorString = "*";
      break;
    case LongVector::BinaryOpType_Add:
      OperatorString = "+";
      break;
    case LongVector::BinaryOpType_Subtract:
      OperatorString = "-";
      break;
    case LongVector::BinaryOpType_Divide:
      OperatorString = "/";
      break;
    case LongVector::BinaryOpType_Modulus:
      OperatorString = "%";
      break;
    case LongVector::BinaryOpType_Min:
      OperatorString = ",";
      IntrinsicString = "min";
      break;
    case LongVector::BinaryOpType_Max:
      OperatorString = ",";
      IntrinsicString = "max";
      break;
    case LongVector::BinaryOpType_ScalarMin:
      BasicOpType = LongVector::BasicOpType_ScalarBinary;
      OperatorString = ",";
      IntrinsicString = "min";
      break;
    case LongVector::BinaryOpType_ScalarMax:
      BasicOpType = LongVector::BasicOpType_ScalarBinary;
      OperatorString = ",";
      IntrinsicString = "max";
      break;
    default:
      VERIFY_FAIL("Invalid BinaryOpType");
    }
  }

  LongVectorOpTestConfig(LongVector::TrigonometricOpType OpType)
      : OpTypeTraits(OpType) {
    IntrinsicString = "";
    BasicOpType = LongVector::BasicOpType_Unary;

    // All trigonometric ops are floating point types.
    // These trig functions are defined to have a max absolute error of 0.0008
    // as per the D3D functional specs. An example with this spec for sin and
    // cos is available here:
    // https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#22.10.20
    ValidationType = LongVector::ValidationType_Epsilon;
    if (std::is_same_v<DataType, HLSLHalf_t>)
      Tolerance = 0.0010f;
    else if (std::is_same_v<DataType, float>)
      Tolerance = 0.0008f;
    else
      VERIFY_FAIL(
          "Invalid type for trigonometric op. Expecting half or float.");

    switch (OpType) {
    case LongVector::TrigonometricOpType_Acos:
      IntrinsicString = "acos";
      break;
    case LongVector::TrigonometricOpType_Asin:
      IntrinsicString = "asin";
      break;
    case LongVector::TrigonometricOpType_Atan:
      IntrinsicString = "atan";
      break;
    case LongVector::TrigonometricOpType_Cos:
      IntrinsicString = "cos";
      break;
    case LongVector::TrigonometricOpType_Cosh:
      IntrinsicString = "cosh";
      break;
    case LongVector::TrigonometricOpType_Sin:
      IntrinsicString = "sin";
      break;
    case LongVector::TrigonometricOpType_Sinh:
      IntrinsicString = "sinh";
      break;
    case LongVector::TrigonometricOpType_Tan:
      IntrinsicString = "tan";
      break;
    case LongVector::TrigonometricOpType_Tanh:
      IntrinsicString = "tanh";
      break;
    default:
      VERIFY_FAIL("Invalid TrigonometricOpType");
    }
  }

  bool IsBinaryOp() const {
    return BasicOpType == LongVector::BasicOpType_Binary ||
           BasicOpType == LongVector::BasicOpType_ScalarBinary;
  }

  bool IsUnaryOp() const {
    return BasicOpType == LongVector::BasicOpType_Unary;
  }

  bool IsScalarOp() const {
    return BasicOpType == LongVector::BasicOpType_ScalarBinary;
  }

  bool HasInputArguments() const {
    // TODO: Right now only clamp has input args. Will need to update this
    // later.
    if constexpr (std::is_same_v<LongVectorOpType, LongVector::UnaryOpType>)
      return IsClampOp();
    else
      return false;
  }

  bool HasFunctionDefinition() const {
    if constexpr (std::is_same_v<LongVectorOpType, LongVector::UnaryOpType>) {
      if (OpTypeTraits.OpType == LongVector::UnaryOpType_Clamp)
        return true;
      else if (OpTypeTraits.OpType == LongVector::UnaryOpType_Initialize)
        return true;
      else
        return false;
    } else
      return false;
  }

  std::string GetOPERAND2String() const {
    if (HasFunctionDefinition()) {
      switch (static_cast<LongVector::UnaryOpType>(OpTypeTraits.OpType)) {
      case LongVector::UnaryOpType_Clamp:
        return std::string("ClampArgMinMax -DFUNC_CLAMP=1");
      case LongVector::UnaryOpType_Initialize:
        return std::string(" -DFUNC_INITIALIZE=1");
      default:
        VERIFY_FAIL("Invalid UnaryOpType");
      }
    }
    return std::string("");
  }

  // A helper to get the hlsl type as a string for a given C++ type.
  // Used in the long vector tests.
  std::string GetHLSLTypeString() {
    if (std::is_same_v<DataType, HLSLBool_t>)
      return "bool";
    if (std::is_same_v<DataType, HLSLHalf_t>)
      return "half";
    if (std::is_same_v<DataType, float>)
      return "float";
    if (std::is_same_v<DataType, double>)
      return "double";
    if (std::is_same_v<DataType, int16_t>)
      return "int16_t";
    if (std::is_same_v<DataType, int32_t>)
      return "int";
    if (std::is_same_v<DataType, int64_t>)
      return "int64_t";
    if (std::is_same_v<DataType, uint16_t>)
      return "uint16_t";
    if (std::is_same_v<DataType, uint32_t>)
      return "uint32_t";
    if (std::is_same_v<DataType, uint64_t>)
      return "uint64_t";

    std::string ErrStr("GetHLSLTypeString() Unsupported type: ");
    ErrStr.append(typeid(DataType).name());
    VERIFY_IS_TRUE(false, ErrStr.c_str());
    return "UnknownType";
  }

  template <typename DataType, typename LongVectorOpType>
  DataType ComputeExpectedValue(const DataType &A, const DataType &B,
                                LongVectorOpType OpType) const {
    // I couldn't find a clean way to do this with templates. So I added this
    // work around for now. This gets things to compile. No caller should ever
    // hit this. But if they do, throw an exception.
    // Intend to clean this up before PR completion.
    LOG_ERROR_FMT_THROW(L"ComputeExpectedValue(const DataType &A, const "
                        L"DataType &B, LongVectorOpType OpType) "
                        L"called on a non-binary op: %d",
                        OpType);
    return DataType(A + B);
  }

  template <>
  DataType ComputeExpectedValue(const DataType &A, const DataType &B,
                                LongVector::BinaryOpType OpType) const {
    switch (OpType) {
    case LongVector::BinaryOpType_ScalarAdd:
      return A + B;
    case LongVector::BinaryOpType_ScalarMultiply:
      return A * B;
    case LongVector::BinaryOpType_ScalarSubtract:
      return A - B;
    case LongVector::BinaryOpType_ScalarDivide:
      return A / B;
    case LongVector::BinaryOpType_ScalarModulus:
      return Mod(A, B);
    case LongVector::BinaryOpType_Multiply:
      return A * B;
    case LongVector::BinaryOpType_Add:
      return A + B;
    case LongVector::BinaryOpType_Subtract:
      return A - B;
    case LongVector::BinaryOpType_Divide:
      return A / B;
    case LongVector::BinaryOpType_Modulus:
      return Mod(A, B);
    case LongVector::BinaryOpType_Min:
      // std::max and std::min are wrapped in () to avoid collisions with the //
      // macro defintions for min and max in windows.h
      return (std::min)(A, B);
    case LongVector::BinaryOpType_Max:
      return (std::max)(A, B);
    case LongVector::BinaryOpType_ScalarMin:
      return (std::min)(A, B);
    case LongVector::BinaryOpType_ScalarMax:
      return (std::max)(A, B);
    default:
      LOG_ERROR_FMT_THROW(L"Unknown BinaryOpType: %d", OpTypeTraits.OpType);
      return DataType();
    }
  }

  DataType ComputeExpectedValue(const DataType &A, const DataType &B) const {
    return ComputeExpectedValue<DataType>(A, B, OpTypeTraits.OpType);
  }

  template <typename DataType, typename LongVectorOpType>
  DataType ComputeExpectedValue(const DataType &A,
                                LongVectorOpType OpType) const {
    LOG_ERROR_FMT_THROW(L"ComputeExpectedValue(const DataType &A, "
                        L"LongVectorOpType OpType) called on a "
                        L"non-unary op: %d",
                        OpType);
    return A;
  }

  template <>
  DataType ComputeExpectedValue(const DataType &A,
                                LongVector::TrigonometricOpType OpType) const {
    // TODO: Is there a better way to handle this? IsFloatingPointType is a
    // constexpr. This prevents this function from hitting a compile error for
    // the non-float types - even though we should never call it for them.
    if constexpr (IsFloatingPointType<DataType>()) {
      switch (OpType) {
      case LongVector::TrigonometricOpType_Acos:
        return std::acos(A);
      case LongVector::TrigonometricOpType_Asin:
        return std::asin(A);
      case LongVector::TrigonometricOpType_Atan:
        return std::atan(A);
      case LongVector::TrigonometricOpType_Cos:
        return std::cos(A);
      case LongVector::TrigonometricOpType_Cosh:
        return std::cosh(A);
      case LongVector::TrigonometricOpType_Sin:
        return std::sin(A);
      case LongVector::TrigonometricOpType_Sinh:
        return std::sinh(A);
      case LongVector::TrigonometricOpType_Tan:
        return std::tan(A);
      case LongVector::TrigonometricOpType_Tanh:
        return std::tanh(A);
      default:
        LOG_ERROR_FMT_THROW(L"Unknown TrigonometricOpType: %d",
                            OpTypeTraits.OpType);
        return DataType();
      }
    }

    LOG_ERROR_FMT_THROW(L"ComputeExpectedValue(const DataType &A, "
                        L"LongVectorOpType OpType) called on a "
                        L"non-float type: %d",
                        OpType);
    return DataType();
  }

  template <>
  DataType ComputeExpectedValue(const DataType &A,
                                LongVector::UnaryOpType OpType) const {
    switch (OpType) {
    case LongVector::UnaryOpType_Clamp: {
      std::vector<DataType> ArgsArray = GetInputArgsArray();
      DataType Min = ArgsArray[0];
      DataType Max = ArgsArray[1];
      return std::clamp(A, Min, Max);
    }
    case LongVector::UnaryOpType_Initialize:
      return A;
    default:
      LOG_ERROR_FMT_THROW(L"Unknown UnaryOpType :%d", OpTypeTraits.OpType);
      return DataType();
    }
  }

  DataType ComputeExpectedValue(const DataType &A) const {
    if (IsUnaryOp())
      return ComputeExpectedValue<DataType>(A, OpTypeTraits.OpType);
    else
      // We need to explicitly handle this case to keep the compiler happy. But
      // this path is not valid.
      LOG_ERROR_FMT_THROW(
          L"ComputeExpectedValue(const DataType&A) called on a binary op: %d",
          OpTypeTraits.OpType);
    return DataType();
  }

  void SetInputArgsArrayName(const std::wstring &InputArgsArrayName) {
    this->InputArgsArrayName = InputArgsArrayName;
  }

  void SetInputValueSet1(const std::wstring &InputValueSetName) {
    InputValueSetName1 = InputValueSetName;
  }

  void SetInputValueSet2(const std::wstring &InputValueSetName) {
    InputValueSetName2 = InputValueSetName;
  }

  bool IsClampOp() const {
    if constexpr (std::is_same_v<LongVectorOpType, LongVector::UnaryOpType>)
      return OpTypeTraits.OpType == LongVector::UnaryOpType_Clamp;
    else
      return false;
  }

  std::vector<DataType> GetInputValueSet1() { return GetInputValueSet(1); }

  std::vector<DataType> GetInputValueSet2() { return GetInputValueSet(2); }

  std::vector<DataType> GetInputArgsArray() const {

    std::vector<DataType> InputArgs;

    std::wstring LocalInputArgsArrayName = InputArgsArrayName;

    if (IsClampOp() && LocalInputArgsArrayName == L"") {
      LocalInputArgsArrayName = L"DefaultClampArgs";
    }

    if (LocalInputArgsArrayName.empty())
      VERIFY_FAIL("No args array name set.");

    if (std::is_same_v<DataType, HLSLBool_t> && IsClampOp())
      VERIFY_FAIL("Clamp is not supported for bools.");
    else
      return GetInputValueSetByKey<DataType>(LocalInputArgsArrayName, false);

    VERIFY_FAIL("Invalid type for args array.");
    return std::vector<DataType>();
  }

  float GetTolerance() const { return Tolerance; }
  LongVector::ValidationType GetValidationType() const {
    return ValidationType;
  }

  std::string GetCompilerOptionsString(size_t VectorSize) {
    std::stringstream CompilerOptions("");
    std::string HLSLType = GetHLSLTypeString();
    CompilerOptions << "-DTYPE=";
    CompilerOptions << HLSLType;
    CompilerOptions << " -DNUM=";
    CompilerOptions << VectorSize;
    const bool Is16BitType =
        (HLSLType == "int16_t" || HLSLType == "uint16_t" || HLSLType == "half");
    CompilerOptions << (Is16BitType ? " -enable-16bit-types" : "");
    CompilerOptions << " -DOPERATOR=";
    CompilerOptions << OperatorString;

    if (IsBinaryOp()) {
      CompilerOptions << " -DOPERAND2=";
      CompilerOptions << (IsScalarOp() ? "InputScalar" : "InputVector2");

      if (IsScalarOp())
        CompilerOptions << " -DIS_SCALAR_OP=1";
      else
        CompilerOptions << " -DIS_BINARY_VECTOR_OP=1";

      CompilerOptions << " -DFUNC=";
      CompilerOptions << IntrinsicString;
    } else { // Unary Op
      CompilerOptions << " -DFUNC=";
      CompilerOptions << IntrinsicString;
      CompilerOptions << " -DOPERAND2=";
      CompilerOptions << GetOPERAND2String();
    }

    return CompilerOptions.str();
  }

private:
  std::vector<DataType> GetInputValueSet(size_t ValueSetIndex) {
    if (ValueSetIndex == 2 && !IsBinaryOp())
      VERIFY_FAIL("ValueSetindex==2 is only valid for binary ops.");

    std::wstring InputValueSetName = L"";
    if (ValueSetIndex == 1)
      InputValueSetName = InputValueSetName1;
    else if (ValueSetIndex == 2)
      InputValueSetName = InputValueSetName2;
    else
      VERIFY_FAIL("Invalid ValueSetIndex");

    return GetInputValueSetByKey<DataType>(InputValueSetName);
  }

  // To be used for the value of -DOPERATOR
  std::string OperatorString;
  // To be used for the value of -DFUNC
  std::string IntrinsicString;
  LongVector::BasicOpType BasicOpType = LongVector::BasicOpType_EnumValueCount;
  float Tolerance = 0.0;
  LongVector::ValidationType ValidationType =
      LongVector::ValidationType::ValidationType_Epsilon;
  LongVectorOpTestConfigTraits<LongVectorOpType> OpTypeTraits;
  std::wstring InputValueSetName1 = L"DefaultInputValueSet1";
  std::wstring InputValueSetName2 = L"DefaultInputValueSet2";
  std::wstring InputArgsArrayName = L""; // No default args array
};

template <typename DataType>
bool DoValuesMatch(DataType A, DataType B, float Tolerance,
                   LongVector::ValidationType) {
  if (Tolerance == 0.0f)
    return A == B;

  DataType Diff = A > B ? A - B : B - A;
  return Diff > Tolerance;
}

inline bool DoValuesMatch(HLSLBool_t A, HLSLBool_t B, float,
                          LongVector::ValidationType) {
  return A == B;
}

inline bool DoValuesMatch(HLSLHalf_t A, HLSLHalf_t B, float Tolerance,
                          LongVector::ValidationType ValidationType) {
  switch (ValidationType) {
  case LongVector::ValidationType_Epsilon:
    return CompareHalfEpsilon(A.Val, B.Val, Tolerance);
  case LongVector::ValidationType_Ulp:
    return CompareHalfULP(A.Val, B.Val, Tolerance);
  default:
    WEX::Logging::Log::Error(
        L"Invalid ValidationType. Expecting Epsilon or ULP.");
    return false;
  }
}

inline bool DoValuesMatch(float A, float B, float Tolerance,
                          LongVector::ValidationType ValidationType) {
  switch (ValidationType) {
  case LongVector::ValidationType_Epsilon:
    return CompareFloatEpsilon(A, B, Tolerance);
  case LongVector::ValidationType_Ulp: {
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

inline bool DoValuesMatch(double A, double B, float Tolerance,
                          LongVector::ValidationType ValidationType) {
  switch (ValidationType) {
  case LongVector::ValidationType_Epsilon:
    return CompareDoubleEpsilon(A, B, Tolerance);
  case LongVector::ValidationType_Ulp: {
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

template <typename DataType>
bool DoVectorsMatch(const std::vector<DataType> &ActualValues,
                    const std::vector<DataType> &ExpectedValues,
                    float Tolerance,
                    LongVector::ValidationType ValidationType) {
  // Stash mismatched indexes for easy failure logging later
  std::vector<size_t> MismatchedIndexes;
  VERIFY_IS_TRUE(ActualValues.size() == ExpectedValues.size(),
                 L"DoVectorsMatch() called with mismatched vector sizes.");
  for (size_t i = 0; i < ActualValues.size(); ++i) {
    if (!DoValuesMatch(ActualValues[i], ExpectedValues[i], Tolerance,
                       ValidationType))
      MismatchedIndexes.push_back(i);
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

template <typename DataType, typename LongVectorOpType>
std::vector<DataType> ComputeExpectedValues(
    const std::vector<DataType> &InputVector1,
    const std::vector<DataType> &InputVector2,
    const LongVectorOpTestConfig<DataType, LongVectorOpType> &Config) {

  VERIFY_IS_TRUE(
      Config.IsBinaryOp(),
      L"ComputeExpectedValues() called with a non-binary op config.");

  std::vector<DataType> ExpectedValues = {};

  for (size_t i = 0; i < InputVector1.size(); ++i) {
    ExpectedValues.push_back(
        Config.ComputeExpectedValue(InputVector1[i], InputVector2[i]));
  }

  return ExpectedValues;
}

template <typename DataType, typename LongVectorOpType>
std::vector<DataType> ComputeExpectedValues(
    const std::vector<DataType> &InputVector1, const DataType &ScalarInput,
    const LongVectorOpTestConfig<DataType, LongVectorOpType> &Config) {

  VERIFY_IS_TRUE(Config.IsScalarOp(), L"ComputeExpectedValues() called with a "
                                      L"non-binary non-scalar op config.");

  std::vector<DataType> ExpectedValues;

  for (size_t i = 0; i < InputVector1.size(); ++i) {
    ExpectedValues.push_back(
        Config.ComputeExpectedValue(InputVector1[i], ScalarInput));
  }

  return ExpectedValues;
}

template <typename DataType, typename LongVectorOpType>
std::vector<DataType> ComputeExpectedValues(
    const std::vector<DataType> &InputVector1,
    const LongVectorOpTestConfig<DataType, LongVectorOpType> &Config) {

  VERIFY_IS_TRUE(Config.IsUnaryOp(),
                 L"ComputeExpectedValues() called with a non-unary op config.");

  std::vector<DataType> ExpectedValues;

  for (size_t i = 0; i < InputVector1.size(); ++i) {
    ExpectedValues.push_back(Config.ComputeExpectedValue(InputVector1[i]));
  }

  return ExpectedValues;
}

template <typename DataType>
void LogLongVector(const std::vector<DataType> &Values,
                   const std::wstring &Name) {
  WEX::Logging::Log::Comment(
      WEX::Common::String().Format(L"LongVector Name: %s", Name.c_str()));

  const size_t LoggingWidth = 40;

  std::wstringstream Wss(L"");
  Wss << L"LongVector Values: ";
  Wss << L"[";
  const size_t NumElements = Values.size();
  for (size_t i = 0; i < NumElements; i++) {
    if (i % LoggingWidth == 0 && i != 0)
      Wss << L"\n ";
    Wss << Values[i];
    if (i != NumElements - 1)
      Wss << L", ";
  }
  Wss << L" ]";

  WEX::Logging::Log::Comment(Wss.str().c_str());
}

template <typename DataType>
void LogScalar(const DataType &Value, const std::wstring &Name) {
  WEX::Logging::Log::Comment(
      WEX::Common::String().Format(L"Scalar Name: %s", Name.c_str()));

  std::wstringstream Wss(L"");
  Wss << L"Scalar Value: ";
  Wss << Value;
  WEX::Logging::Log::Comment(Wss.str().c_str());
}

#endif // LONGVECTORS_H