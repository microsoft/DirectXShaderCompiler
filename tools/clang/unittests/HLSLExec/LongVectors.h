#pragma once

#include <array>
#include <limits>
#include <ostream>
#include <random>
#include <sstream>
#include <string>

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include <Verify.h>
#include "LongVectorTestData.h"

// A helper struct because C++ bools are 1 byte and HLSL bools are 4 bytes.
// Take int32_t as a constuctor argument and convert it to bool when needed.
// Comparisons cast to a bool because we only care if the bool representation is
// true or false.
struct HLSLBool_t {
  HLSLBool_t() : Val(0) {}
  HLSLBool_t(int32_t Val) : Val(Val) {}
  HLSLBool_t(bool Val) : Val(Val) {}
  HLSLBool_t(const HLSLBool_t &Other) : Val(Other.Val) {}

  bool operator==(const HLSLBool_t &Other) const {
    return static_cast<bool>(Val) == static_cast<bool>(Other.Val);
  }

  bool operator!=(const HLSLBool_t &Other) const {
    return static_cast<bool>(Val) != static_cast<bool>(Other.Val);
  }

  bool operator<(const HLSLBool_t &Other) const { return Val < Other.Val; }

  bool operator>(const HLSLBool_t &Other) const { return Val > Other.Val; }

  bool operator<=(const HLSLBool_t &Other) const { return Val <= Other.Val; }

  bool operator>=(const HLSLBool_t &Other) const { return Val >= Other.Val; }

  HLSLBool_t operator*(const HLSLBool_t &Other) const {
    return HLSLBool_t(Val * Other.Val);
  }

  HLSLBool_t operator+(const HLSLBool_t &Other) const {
    return HLSLBool_t(Val + Other.Val);
  }

  // So we can construct std::wstrings using std::wostream
  friend std::wostream &operator<<(std::wostream &Os, const HLSLBool_t &Obj) {
    Os << static_cast<bool>(Obj.Val);
    return Os;
  }

  // So we can construct std::strings using std::ostream
  friend std::ostream &operator<<(std::ostream &Os, const HLSLBool_t &Obj) {
    Os << static_cast<bool>(Obj.Val);
    return Os;
  }

  int32_t Val = 0;
};

//  No native float16 type in C++ until C++23 . So we use uint16_t to represent
//  it. Simple little wrapping struct to help handle the right behavior.
struct HLSLHalf_t {
  HLSLHalf_t() : Val(0) {}
  HLSLHalf_t(DirectX::PackedVector::HALF Val) : Val(Val) {}
  HLSLHalf_t(const HLSLHalf_t &Other) : Val(Other.Val) {}
  HLSLHalf_t(const float F) {
    Val = DirectX::PackedVector::XMConvertFloatToHalf(F);
  }
  HLSLHalf_t(const int I) {
    VERIFY_IS_TRUE(I == 0, L"HLSLHalf_t constructor with int override only meant for cases when initializing to 0.");
    const float F = static_cast<float>(I);
    Val = DirectX::PackedVector::XMConvertFloatToHalf(F);
  }

  bool operator==(const HLSLHalf_t &Other) const { return Val == Other.Val; }

  bool operator<(const HLSLHalf_t &Other) const {
    return DirectX::PackedVector::XMConvertHalfToFloat(Val) <
           DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
  }

  bool operator>(const HLSLHalf_t &Other) const {
    return DirectX::PackedVector::XMConvertHalfToFloat(Val) >
           DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
  }

  // Used by tolerance checks in the tests.
  bool operator>(float F) const {
    float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    return A > F;
  }

  bool operator<(float F) const {
    float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    return A < F;
  }

  bool operator<=(const HLSLHalf_t &Other) const {
    return DirectX::PackedVector::XMConvertHalfToFloat(Val) <=
           DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
  }

  bool operator>=(const HLSLHalf_t &Other) const {
    return DirectX::PackedVector::XMConvertHalfToFloat(Val) >=
           DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
  }

  bool operator!=(const HLSLHalf_t &Other) const { return Val != Other.Val; }

  HLSLHalf_t operator*(const HLSLHalf_t &Other) const {
    float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    return HLSLHalf_t(DirectX::PackedVector::XMConvertFloatToHalf(A * B));
  }

  HLSLHalf_t operator+(const HLSLHalf_t &Other) const {
    float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    return HLSLHalf_t(DirectX::PackedVector::XMConvertFloatToHalf(A + B));
  }

  HLSLHalf_t operator-(const HLSLHalf_t &Other) const {
    float A = DirectX::PackedVector::XMConvertHalfToFloat(Val);
    float B = DirectX::PackedVector::XMConvertHalfToFloat(Other.Val);
    return HLSLHalf_t(DirectX::PackedVector::XMConvertFloatToHalf(A - B));
  }

  // So we can construct std::wstrings using std::wostream
  friend std::wostream &operator<<(std::wostream &Os, const HLSLHalf_t &Obj) {
    Os << DirectX::PackedVector::XMConvertHalfToFloat(Obj.Val);
    return Os;
  }

  // So we can construct std::wstrings using std::wostream
  friend std::ostream &operator<<(std::ostream &Os, const HLSLHalf_t &Obj) {
    Os << DirectX::PackedVector::XMConvertHalfToFloat(Obj.Val);
    return Os;
  }

  // HALF is an alias to uint16_t
  DirectX::PackedVector::HALF Val = 0;
};

// Helper to fill the shader buffer based on type. Convenient to be used when
// copying HLSL*_t types so we can copy the underlying type directly instead of
// the struct.
template <typename T, std::size_t N>
void FillShaderBufferFromLongVectorData(std::vector<BYTE> &ShaderBuffer,
                                        std::array<T, N> &TestData) {

  // Note: DataSize for HLSLHalf_t and HLSLBool_t may be larger than the
  // underlying type in some cases. Thats fine. Resize just makes sure we have
  // enough space.
  const size_t DataSize = sizeof(T) * N;
  ShaderBuffer.resize(DataSize);

  if constexpr (std::is_same_v<T, HLSLHalf_t>) {
    DirectX::PackedVector::HALF *ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t i = 0; i < N; ++i) {
      ShaderBufferPtr[i] = TestData[i].Val;
    }
  } else if constexpr (std::is_same_v<T, HLSLBool_t>) {
    int32_t *ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t i = 0; i < N; ++i) {
      ShaderBufferPtr[i] = TestData[i].Val;
    }
  } else {
    T *ShaderBufferPtr = reinterpret_cast<T *>(ShaderBuffer.data());
    for (size_t i = 0; i < N; ++i) {
      ShaderBufferPtr[i] = TestData[i];
    }
  }
}

// Helper to fill the test data from the shader buffer based on type. Convenient
// to be used when copying HLSL*_t types so we can use the underlying type.
template <typename T, std::size_t N>
void FillLongVectorDataFromShaderBuffer(MappedData &ShaderBuffer,
                                        std::array<T, N> &TestData) {

  if constexpr (std::is_same_v<T, HLSLHalf_t>) {
    DirectX::PackedVector::HALF *ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t i = 0; i < N; ++i) {
      // HLSLHalf_t has a DirectX::PackedVector::HALF based constructor.
      TestData[i] = ShaderBufferPtr[i];
    }
  } else if constexpr (std::is_same_v<T, HLSLBool_t>) {
    int32_t *ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t i = 0; i < N; ++i) {
      // HLSLBool_t has a int32_t based constructor.
      TestData[i] = ShaderBufferPtr[i];
    }
  } else {
    T *ShaderBufferPtr = reinterpret_cast<T *>(ShaderBuffer.data());
    for (size_t i = 0; i < N; ++i) {
      TestData[i] = ShaderBufferPtr[i];
    }
  }
}

enum LongVectorBinaryOpType {
  LongVectorBinaryOpType_ScalarAdd,
  LongVectorBinaryOpType_ScalarMultiply,
  LongVectorBinaryOpType_Multiply,
  LongVectorBinaryOpType_Add,
  LongVectorBinaryOpType_Min,
  LongVectorBinaryOpType_Max,
  LongVectorBinaryOpType_ScalarMin,
  LongVectorBinaryOpType_ScalarMax,
  LongVectorBinaryOpType_EnumValueCount
};

struct LongVectorOpTypeStringToEnumValue {
  std::wstring OpTypeString;
  uint32_t OpTypeValue;
};

template <typename T>
T GetLongVectorOpType(const LongVectorOpTypeStringToEnumValue* Values, const std::wstring &OpTypeString, std::size_t Length) {
  for(size_t i = 0; i < Length; i++) {
    if (Values[i].OpTypeString == OpTypeString) {
      return static_cast<T>(Values[i].OpTypeValue);
    }
  }

  LogErrorFmtThrow(L"Invalid LongVectorOpType string: %s", OpTypeString.c_str());

  if(std::is_same_v<T, LongVectorBinaryOpType>)
    return static_cast<T>(LongVectorBinaryOpType_EnumValueCount);
  else if (std::is_same_v<T, LongVectorUnaryOpType>)
    return static_cast<T>(LongVectorUnaryOpType_EnumValueCount);
}

static const LongVectorOpTypeStringToEnumValue LongVectorBinaryOpTypeStringToEnumMap[] = {
    {L"LongVectorBinaryOpType_ScalarAdd", LongVectorBinaryOpType_ScalarAdd},
    {L"LongVectorBinaryOpType_ScalarMultiply",
     LongVectorBinaryOpType_ScalarMultiply},
    {L"LongVectorBinaryOpType_Multiply", LongVectorBinaryOpType_Multiply},
    {L"LongVectorBinaryOpType_Add", LongVectorBinaryOpType_Add},
    {L"LongVectorBinaryOpType_Min", LongVectorBinaryOpType_Min},
    {L"LongVectorBinaryOpType_Max", LongVectorBinaryOpType_Max},
    {L"LongVectorBinaryOpType_ScalarMin", LongVectorBinaryOpType_ScalarMin},
    {L"LongVectorBinaryOpType_ScalarMax", LongVectorBinaryOpType_ScalarMax},
};

static_assert(
    _countof(LongVectorBinaryOpTypeStringToEnumMap) ==
    LongVectorBinaryOpType_EnumValueCount,
    "LongVectorBinaryOpTypeStringToEnumMap size mismatch. Did you add a new enum value?");

LongVectorBinaryOpType GetLongVectorBinaryOpType(
    const std::wstring &OpTypeString) {
  return GetLongVectorOpType<LongVectorBinaryOpType>(
      LongVectorBinaryOpTypeStringToEnumMap, OpTypeString, std::size(LongVectorBinaryOpTypeStringToEnumMap));
}

enum LongVectorUnaryOpType {
  LongVectorUnaryOpType_Clamp,
  LongVectorUnaryOpType_Initialize,
  LongVectorUnaryOpType_EnumValueCount
};

static const LongVectorOpTypeStringToEnumValue LongVectorUnaryOpTypeStringToEnumMap[] = {
  {L"LongVectorUnaryOpType_Clamp", LongVectorUnaryOpType_Clamp},
  {L"LongVectorUnaryOpType_Initialize", LongVectorUnaryOpType_Initialize},
};

static_assert(
  _countof(LongVectorUnaryOpTypeStringToEnumMap) ==
  LongVectorUnaryOpType_EnumValueCount,
  "LongVectorUnaryOpTypeStringToEnumMap size mismatch. Did you add a new enum value?");

LongVectorUnaryOpType GetLongVectorUnaryOpType(const std::wstring &OpTypeString) {
  return GetLongVectorOpType<LongVectorUnaryOpType>(LongVectorUnaryOpTypeStringToEnumMap, OpTypeString, std::size(LongVectorUnaryOpTypeStringToEnumMap));
}

template <typename T>
std::vector<T> GetInputValueSetByKey(std::wstring Key) {

  if constexpr (std::is_same_v<T, HLSLBool_t>)
    return std::vector<T>(DefaultInputValueSet_bool.at(Key).begin(),
                        DefaultInputValueSet_bool.at(Key).end());
  else if constexpr (std::is_same_v<T, HLSLHalf_t>)
    return std::vector<T>(DefaultInputValueSet_float16.at(Key).begin(),
                        DefaultInputValueSet_float16.at(Key).end());
  else if constexpr (std::is_same_v<T, float>)
    return std::vector<T>(DefaultInputValueSet_float32.at(Key).begin(),
                        DefaultInputValueSet_float32.at(Key).end());
  else if constexpr (std::is_same_v<T, double>)
    return std::vector<T>(DefaultInputValueSet_float64.at(Key).begin(),
                        DefaultInputValueSet_float64.at(Key).end());
  else if constexpr (std::is_same_v<T, int16_t>)
    return std::vector<T>(DefaultInputValueSet_int16.at(Key).begin(),
                        DefaultInputValueSet_int16.at(Key).end());
  else if constexpr (std::is_same_v<T, int32_t>)
    return std::vector<T>(DefaultInputValueSet_int32.at(Key).begin(),
                        DefaultInputValueSet_int32.at(Key).end());
  else if constexpr (std::is_same_v<T, int64_t>)
    return std::vector<T>(DefaultInputValueSet_int64.at(Key).begin(),
                        DefaultInputValueSet_int64.at(Key).end());
  else if constexpr (std::is_same_v<T, uint16_t>)
    return std::vector<T>(DefaultInputValueSet_uint16.at(Key).begin(),
                        DefaultInputValueSet_uint16.at(Key).end());
  else if constexpr (std::is_same_v<T, uint32_t>)
    return std::vector<T>(DefaultInputValueSet_uint32.at(Key).begin(),
                        DefaultInputValueSet_uint32.at(Key).end());
  else if constexpr (std::is_same_v<T, uint64_t>)
    return std::vector<T>(DefaultInputValueSet_uint64.at(Key).begin(),
                        DefaultInputValueSet_uint64.at(Key).end());

  VERIFY_FAIL("GetInputValueSetByKey() Unsupported type.");
  return std::vector<T>();
}

// Used to pass into LongVectorOpTestBase
template <typename T> class LongVectorOpTestConfig {
public:
  LongVectorOpTestConfig() = default;

  LongVectorOpTestConfig(LongVectorUnaryOpType OpType) : UnaryOpType_(OpType) {
    IntrinsicString_ = "";

    if (IsFloatingPointType())
      Tolerance_ = 1;

    switch (OpType) {
    case LongVectorUnaryOpType_Clamp:
      OperatorString_ = ",";
      IntrinsicString_ = "TestClamp";
      break;
    case LongVectorUnaryOpType_Initialize:
      IntrinsicString_ = "TestInitialize";
      break;
    default:
      VERIFY_FAIL("Invalid LongVectorBinaryOpType");
    }
  }

  LongVectorOpTestConfig(LongVectorBinaryOpType OpType) : BinaryOpType_(OpType) {
    IntrinsicString_ = "";

    if (IsFloatingPointType())
      Tolerance_ = 1;

    switch (OpType) {
    case LongVectorBinaryOpType_ScalarAdd:
      OperatorString_ = "+";
      break;
    case LongVectorBinaryOpType_ScalarMultiply:
      OperatorString_ = "*";
      break;
    case LongVectorBinaryOpType_Multiply:
      OperatorString_ = "*";
      break;
    case LongVectorBinaryOpType_Add:
      OperatorString_ = "+";
      break;
    case LongVectorBinaryOpType_Min:
      OperatorString_ = ",";
      IntrinsicString_ = "min";
      break;
    case LongVectorBinaryOpType_Max:
      OperatorString_ = ",";
      IntrinsicString_ = "max";
      break;
    case LongVectorBinaryOpType_ScalarMin:
      OperatorString_ = ",";
      IntrinsicString_ = "min";
      break;
    case LongVectorBinaryOpType_ScalarMax:
      OperatorString_ = ",";
      IntrinsicString_ = "max";
      break;
    default:
      VERIFY_FAIL("Invalid LongVectorBinaryOpType");
    }
  }

  bool IsFloatingPointType() const {
    return std::is_same_v<T, float> || std::is_same_v<T, double> ||
           std::is_same_v<T, HLSLHalf_t>;
  }

  bool IsBinaryOp() const {
    return BinaryOpType_ != LongVectorBinaryOpType_EnumValueCount;
  }

  bool IsUnaryOp() const {
    return UnaryOpType_ != LongVectorUnaryOpType_EnumValueCount;
  }

  bool IsScalarOp() const {
    switch(BinaryOpType_) {
      case LongVectorBinaryOpType_ScalarAdd:
      case LongVectorBinaryOpType_ScalarMultiply:
      case LongVectorBinaryOpType_ScalarMin:
      case LongVectorBinaryOpType_ScalarMax:
        return true;
      default:
        return false;
    };
  }

  bool HasInputArguments() const {
    switch(UnaryOpType_) {
      case LongVectorUnaryOpType_Clamp:
        return true;
      default:
        return false;
    }
  }

  // A helper to get the hlsl type as a string for a given C++ type.
  // Used in the long vector tests.
  std::string GetHLSLTypeString() {
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

    std::string ErrStr("GetHLSLTypeString() Unsupported type: ");
    ErrStr.append(typeid(T).name());
    VERIFY_IS_TRUE(false, ErrStr.c_str());
    return "UnknownType";
  }

  T ComputeExpectedValue(const T &A, const T &B) const{
    if (IsBinaryOp()) {
      switch (BinaryOpType_) {
      case LongVectorBinaryOpType_ScalarAdd:
        return A + B;
      case LongVectorBinaryOpType_ScalarMultiply:
        return A * B;
      case LongVectorBinaryOpType_Multiply:
        return A * B;
      case LongVectorBinaryOpType_Add:
        return A + B;
      case LongVectorBinaryOpType_Min:
        return std::min(A, B);
      case LongVectorBinaryOpType_Max:
        return std::max(A, B);
      case LongVectorBinaryOpType_ScalarMin:
        return std::min(A, B);
      case LongVectorBinaryOpType_ScalarMax:
        return std::max(A, B);
      default:
        LogErrorFmtThrow(L"Unknown LongVectorBinaryOpType: %d", BinaryOpType_);
      }
    } else {
      LogErrorFmtThrow(L"ComputeExpectedValue(const T &A, const T &B) called for a unary op.: %d", UnaryOpType_);
    }

    return T();
  }

  T ComputeExpectedValue(const T &A) const{
    if (IsUnaryOp()) {
      switch (UnaryOpType_) {
      case LongVectorUnaryOpType_Clamp: {
        std::vector<T> ArgsArray = GetInputArgsArray();
        T Min = ArgsArray[0];
        T Max = ArgsArray[1];
        return std::clamp(A, Min, Max);
      }
      case LongVectorUnaryOpType_Initialize:
        return A;
      default:
        LogErrorFmtThrow(L"Unknown LongVectorUnaryOpType :%d", UnaryOpType_);
      }
    } else {
      LogErrorFmtThrow(L"ComputeExpectedValue(const T &A) called for a binary op: %d", BinaryOpType_);
    }

    return T();
  }

  void SetInputArgsArrayName(std::wstring InputArgsArrayName) {
    InputArgsArrayName_ = InputArgsArrayName;
  }

  void SetInputValueSet1(std::wstring InputValueSetName) {
    InputValueSetName1_ = InputValueSetName;
  }

  void SetInputValueSet2(std::wstring InputValueSetName) {
    InputValueSetName2_ = InputValueSetName;
  }

  std::vector<T> GetInputValueSet1() {
    return GetInputValueSet(1);
  }

  std::vector<T> GetInputValueSet2() {
    return GetInputValueSet(2);
  }

  std::vector<T> GetInputArgsArray() const{

    std::vector<T> InputArgs;

    std::wstring InputArgsArrayName = InputArgsArrayName_;

    if(UnaryOpType_ == LongVectorUnaryOpType_Clamp && InputArgsArrayName == L"") {
      InputArgsArrayName = L"DefaultClampArgs";
    }

    if (InputArgsArrayName.empty())
      VERIFY_FAIL("No args array name set.");

    if(std::is_same_v<T, HLSLBool_t> && UnaryOpType_ == LongVectorUnaryOpType_Clamp)
      VERIFY_FAIL("Clamp is not supported for bools.");
    else
      return GetInputValueSetByKey<T>(InputArgsArrayName);

    VERIFY_FAIL("Invalid type for args array.");
    return std::vector<T>();
  }

  LongVectorBinaryOpType GetBinaryOpType() const {
    return BinaryOpType_;
  }

  LongVectorUnaryOpType GetUnaryOpType() const {
    return UnaryOpType_;
  }

  float GetTolerance() const {
    return Tolerance_;
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
    CompilerOptions << OperatorString_;

    if (IsBinaryOp()) {
      CompilerOptions << " -DOPERAND2=";
      CompilerOptions << (IsScalarOp() ? "InputScalar" : "InputVector2");
  
      if (IsScalarOp())
        CompilerOptions << " -DIS_SCALAR_OP=1";
      else
        CompilerOptions << " -DIS_BINARY_VECTOR_OP=1";

      CompilerOptions << " -DFUNC=";
      CompilerOptions << IntrinsicString_;
    } else { // Unary Op
      CompilerOptions << " -DFUNC=";
      CompilerOptions << IntrinsicString_;
      CompilerOptions << " -DOPERAND2=";

      switch (GetUnaryOpType()) {
      case LongVectorUnaryOpType_Clamp:
        CompilerOptions << "ClampArgMinMax";
        CompilerOptions << " -DFUNC_CLAMP=1";
        break;
      case LongVectorUnaryOpType_Initialize:
        CompilerOptions << " -DFUNC_INITIALIZE=1";
        break;
      }
    }

    return CompilerOptions.str();
  }

private:
  std::vector<T> GetInputValueSet(size_t ValueSetIndex) {
    if(ValueSetIndex ==2 && !IsBinaryOp())
      VERIFY_FAIL("ValueSetindex==2 is only valid for binary ops.");

    std::wstring InputValueSetName = L"";
    if(ValueSetIndex == 1)
      InputValueSetName = InputValueSetName1_;
    else if (ValueSetIndex == 2)
      InputValueSetName = InputValueSetName2_;
    else
      VERIFY_FAIL("Invalid ValueSetIndex");

    return GetInputValueSetByKey<T>(InputValueSetName);
  }

  // To be used for the value of -DOPERATOR
  std::string OperatorString_;
  // To be used for the value of -DFUNC
  std::string IntrinsicString_;
  // Optional, can be used to override shader code.
  float Tolerance_ = 0.0;
  LongVectorBinaryOpType BinaryOpType_ = LongVectorBinaryOpType_EnumValueCount;
  LongVectorUnaryOpType UnaryOpType_ = LongVectorUnaryOpType_EnumValueCount;
  std::wstring InputValueSetName1_ = L"DefaultInputValueSet1";
  std::wstring InputValueSetName2_ = L"DefaultInputValueSet2";
  std::wstring InputArgsArrayName_ = L""; // No default args array
};

template <typename T> bool DoValuesMatch(T A, T B, float Tolerance) {
    if (Tolerance == 0.0f)
      return A == B;

    T Diff = A > B ? A - B : B - A;
    return Diff > Tolerance;
  }

inline bool DoValuesMatch(HLSLBool_t A, HLSLBool_t B, float) {
  return A == B; 
}

inline bool DoValuesMatch(HLSLHalf_t A, HLSLHalf_t B, float Tolerance) {
  return CompareHalfULP(A.Val, B.Val, Tolerance);
}

inline bool DoValuesMatch(float A, float B, float Tolerance) {
  const int IntTolerance = static_cast<int>(Tolerance);
  return CompareFloatULP(A, B, IntTolerance);
}

inline bool DoValuesMatch(double A, double B, float Tolerance) {
  const int64_t IntTolerance = static_cast<int64_t>(Tolerance);
  return CompareDoubleULP(A, B, IntTolerance);
}

template <typename T, std::size_t N>
bool DoArraysMatch(const std::array<T, N> &ActualValues,
                   const std::array<T, N> &ExpectedValues, float Tolerance) {
  // Stash mismatched indexes for easy failure logging later
  std::vector<size_t> MismatchedIndexes;
  for (size_t i = 0; i < N; ++i) {
    if (!DoValuesMatch(ActualValues[i], ExpectedValues[i], Tolerance))
      MismatchedIndexes.push_back(i);
  }

  if (MismatchedIndexes.empty())
    return true;

  if (!MismatchedIndexes.empty()) {
    for (size_t Index : MismatchedIndexes) {
      std::wstringstream Wss(L"");
      Wss << L"Mismatch at Index: " << Index;
      Wss << L" Actual Value:" << ActualValues[Index] << ",";
      Wss << L" Expected Value:" << ExpectedValues[Index];
      WEX::Logging::Log::Error(Wss.str().c_str());
    }
  }

  return false;
}

template <typename T, std::size_t N> 
std::array<T, N> ComputeExpectedValues(const std::array<T, N> &InputVector1, const std::array<T, N> &InputVector2, const LongVectorOpTestConfig<T> &Config) {

  VERIFY_IS_TRUE(Config.IsBinaryOp(), L"ComputeExpectedValues() called with a non-binary op config.");

  std::array<T, N> ExpectedValues = {};

  for(size_t i = 0; i < N; ++i) {
    ExpectedValues[i] = Config.ComputeExpectedValue(InputVector1[i], InputVector2[i]);
  }

  return ExpectedValues;
}

template <typename T, std::size_t N> 
std::array<T, N> ComputeExpectedValues(const std::array<T, N> &InputVector1, const T &ScalarInput, const LongVectorOpTestConfig<T> &Config) {

  VERIFY_IS_TRUE(Config.IsScalarOp(), L"ComputeExpectedValues() called with a non-binary non-scalar op config.");

  std::array<T, N> ExpectedValues = {};

  for(size_t i = 0; i < N; ++i) {
    ExpectedValues[i] = Config.ComputeExpectedValue(InputVector1[i], ScalarInput);
  }

  return ExpectedValues;
}

template <typename T, std::size_t N> 
std::array<T, N> ComputeExpectedValues(const std::array<T, N> &InputVector1, const LongVectorOpTestConfig<T> &Config) {

  VERIFY_IS_TRUE(Config.IsUnaryOp(), L"ComputeExpectedValues() called with a non-unary op config.");

  std::array<T, N> ExpectedValues = {};

  for(size_t i = 0; i < N; ++i) {
    ExpectedValues[i] = Config.ComputeExpectedValue(InputVector1[i]);
  }

  return ExpectedValues;
}

template <typename T, std::size_t N>
void LogLongVector(const std::array<T, N> &Values, std::wstring Name) {
  WEX::Logging::Log::Comment(WEX::Common::String().Format(
      L"LongVector Name: %s", Name.c_str()));
  
  const size_t LoggingWidth = 40;

  std::wstringstream Wss(L"LongVector Values: ");
  Wss << L"[";
  for (size_t i = 0; i < N; i++)
  {
    if(i % LoggingWidth == 0 && i != 0)
      Wss << L"\n ";
    Wss << Values[i];
    if(i != N - 1)
      Wss << L", ";
  }
  Wss << L" ]";

  WEX::Logging::Log::Comment(Wss.str().c_str());
}

template <typename T>
void LogScalar(const T &Value, std::wstring Name) {
  WEX::Logging::Log::Comment(WEX::Common::String().Format(
      L"Scalar Name: %s", Name.c_str()));

  std::wstringstream Wss(L"Scalar Value: ");
  Wss << Value;
  WEX::Logging::Log::Comment(Wss.str().c_str());
}