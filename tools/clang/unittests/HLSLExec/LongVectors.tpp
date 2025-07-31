template <typename DataTypeT>
DataTypeT LongVector::getLongVectorOpType(const LongVectorOpTypeStringToEnumValue *Values,
                             const std::wstring &OpTypeString,
                             std::size_t Length) {
  for (size_t i = 0; i < Length; i++) {
    if (Values[i].OpTypeString == OpTypeString)
      return static_cast<DataTypeT>(Values[i].OpTypeValue);
  }

  LOG_ERROR_FMT_THROW(L"Invalid LongVectorOpType string: %s",
                      OpTypeString.c_str());

  return static_cast<DataTypeT>(UINT_MAX);
}

// Helper to fill the shader buffer based on type. Convenient to be used when
// copying HLSL*_t types so we can copy the underlying type directly instead of
// the struct.
template <typename DataTypeT>
void LongVector::fillShaderBufferFromLongVectorData(std::vector<BYTE> &ShaderBuffer, std::vector<DataTypeT> &TestData) {

  // Note: DataSize for HLSLHalf_t and HLSLBool_t may be larger than the
  // underlying type in some cases. Thats fine. Resize just makes sure we have
  // enough space.
  const size_t NumElements = TestData.size();
  const size_t DataSize = sizeof(DataTypeT) * NumElements;
  ShaderBuffer.resize(DataSize);

  if constexpr (std::is_same_v<DataTypeT, HLSLHalf_t>) {
    DirectX::PackedVector::HALF *ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i)
      ShaderBufferPtr[i] = TestData[i].Val;
  } else if constexpr (std::is_same_v<DataTypeT, HLSLBool_t>) {
    int32_t *ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i)
      ShaderBufferPtr[i] = TestData[i].Val;
  } else {
    DataTypeT *ShaderBufferPtr =
        reinterpret_cast<DataTypeT *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i)
      ShaderBufferPtr[i] = TestData[i];
  }
}

// Helpers so we do the right thing for float types. HLSLHalf_t is handled in an
// operator overload.
template <typename DataTypeT>
DataTypeT LongVector::mod(const DataTypeT &A, const DataTypeT &B) {
  return A % B;
}

template <> float LongVector::mod(const float &A, const float &B) {
  return std::fmod(A, B);
}

template <> double LongVector::mod(const double &A, const double &B) {
  return std::fmod(A, B);
}

// Helper to fill the test data from the shader buffer based on type. Convenient
// to be used when copying HLSL*_t types so we can use the underlying type.
template <typename DataTypeT>
void LongVector::fillLongVectorDataFromShaderBuffer(MappedData &ShaderBuffer,
                                        std::vector<DataTypeT> &TestData,
                                        size_t NumElements) {
  if constexpr (std::is_same_v<DataTypeT, HLSLHalf_t>) {
    DirectX::PackedVector::HALF *ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i)
      // HLSLHalf_t has a DirectX::PackedVector::HALF based constructor.
      TestData.push_back(ShaderBufferPtr[i]);
  } else if constexpr (std::is_same_v<DataTypeT, HLSLBool_t>) {
    int32_t *ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i)
      // HLSLBool_t has a int32_t based constructor.
      TestData.push_back(ShaderBufferPtr[i]);
  } else {
    DataTypeT *ShaderBufferPtr =
        reinterpret_cast<DataTypeT *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i)
      TestData.push_back(ShaderBufferPtr[i]);
  }
}

template <typename DataTypeT>
bool LongVector::doValuesMatch(DataTypeT A, DataTypeT B, float Tolerance,
                   LongVector::ValidationType) {
  if (Tolerance == 0.0f)
    return A == B;

  DataTypeT Diff = A > B ? A - B : B - A;
  return Diff <= Tolerance;
}

bool LongVector::doValuesMatch(HLSLBool_t A, HLSLBool_t B, float,
                          LongVector::ValidationType) {
  return A == B;
}

bool LongVector::doValuesMatch(HLSLHalf_t A, HLSLHalf_t B, float Tolerance,
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

bool LongVector::doValuesMatch(float A, float B, float Tolerance,
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

bool LongVector::doValuesMatch(double A, double B, float Tolerance,
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


template <typename DataTypeT>
bool LongVector::doVectorsMatch(const std::vector<DataTypeT> &ActualValues,
                   const std::vector<DataTypeT> &ExpectedValues,
                   float Tolerance,
                   LongVector::ValidationType ValidationType) {
  // Stash mismatched indexes for easy failure logging later
  std::vector<size_t> MismatchedIndexes;
  VERIFY_IS_TRUE(ActualValues.size() == ExpectedValues.size(),
                 L"doVectorsMatch() called with mismatched vector sizes.");
  for (size_t i = 0; i < ActualValues.size(); ++i) {
    if (!doValuesMatch(ActualValues[i], ExpectedValues[i], Tolerance,
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

// A helper to fill the expected vector with computed values. Lets us factor out
// the re-used std::get_if code and the for loop.
template <typename DataTypeT, typename ComputeFnT>
void fillExpectedVector(LongVector::VariantVector& ExpectedVector, size_t Count,
ComputeFnT ComputeFn) {
  auto* TypedExpectedValues = std::get_if<std::vector<DataTypeT>>(&ExpectedVector);

  VERIFY_IS_NOT_NULL(TypedExpectedValues, L"Expected vector is not of the correct type.");

  for (size_t Index = 0; Index < Count; ++Index)
    TypedExpectedValues->push_back(ComputeFn(Index));
}

template <typename DataTypeT, typename LongVectorOpTypeT>
void LongVector::computeExpectedValues(
  const std::vector<DataTypeT> &InputVector1,
  const std::vector<DataTypeT> &InputVector2,
  LongVector::TestConfig<DataTypeT, LongVectorOpTypeT> &Config) {

  VERIFY_IS_TRUE(
      Config.isBinaryOp(),
      L"computeExpectedValues() called with a non-binary op config.");

  if (Config.isAsTypeOp())
    Config.computeExpectedValuesForAsTypeOp(InputVector1, InputVector2);
  else {
    fillExpectedVector<DataTypeT>(Config.getExpectedVector(), InputVector1.size(),
      [&](size_t Index) {
        return Config.computeExpectedValue(InputVector1[Index],
                                           InputVector2[Index]);
      });
  }
}

template <typename DataTypeT, typename LongVectorOpTypeT>
void LongVector::computeExpectedValues(
    const std::vector<DataTypeT> &InputVector1, const DataTypeT &ScalarInput,
    LongVector::TestConfig<DataTypeT, LongVectorOpTypeT> &Config) {

  VERIFY_IS_TRUE(Config.isScalarOp(), L"computeExpectedValues() called with a "
                                      L"non-binary non-scalar op config.");

  fillExpectedVector<DataTypeT>(Config.getExpectedVector(), InputVector1.size(),
    [&](size_t Index) {
      return Config.computeExpectedValue(InputVector1[Index], ScalarInput);
    });
}

template <typename DataTypeT, typename LongVectorOpTypeT>
void LongVector::computeExpectedValues(
    const std::vector<DataTypeT> &InputVector1,
    LongVector::TestConfig<DataTypeT, LongVectorOpTypeT> &Config) {

  VERIFY_IS_TRUE(Config.isUnaryOp(),
                 L"computeExpectedValues() called with a non-unary op config.");

  if (Config.isAsTypeOp())
    Config.computeExpectedValuesForAsTypeOp(InputVector1);
  else {
    fillExpectedVector<DataTypeT>(Config.getExpectedVector(), InputVector1.size(),
      [&](size_t Index) {
        return Config.computeExpectedValue(InputVector1[Index]);
      });
  }
}

template <typename DataTypeT>
void LongVector::logLongVector(const std::vector<DataTypeT> &Values,
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

template <typename DataTypeT, typename LongVectorOpTypeT>
LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::TestConfig(LongVector::UnaryOpType OpType)
    : OpTypeTraits(OpType) {
  IntrinsicString = "";
  BasicOpType = LongVector::BasicOpType_Unary;

  if (isFloatingPointType<DataTypeT>())
    Tolerance = 1;

  switch (OpType) {
  case LongVector::UnaryOpType_Initialize:
    IntrinsicString = "TestInitialize";
    SpecialDefines = " -DFUNC_INITIALIZE=1";
    break;
  case LongVector::UnaryOpType_AsFloat:
    IntrinsicString = "asfloat";
    ExpectedVector = std::vector<float>{};
    break;
  case LongVector::UnaryOpType_AsFloat16:
    IntrinsicString = "asfloat16";
    ExpectedVector = std::vector<HLSLHalf_t>{};
    break;
  case LongVector::UnaryOpType_AsInt:
    IntrinsicString = "asint";
    ExpectedVector = std::vector<int32_t>{};
    break;
  case LongVector::UnaryOpType_AsInt16:
    IntrinsicString = "asint16";
    ExpectedVector = std::vector<int16_t>{};
    break;
  case LongVector::UnaryOpType_AsUint:
    IntrinsicString = "asuint";
    ExpectedVector = std::vector<uint32_t>{};
    break;
  case LongVector::UnaryOpType_AsUint_SplitDouble:
    IntrinsicString = "TestAsUintSplitDouble";
    SpecialDefines = "  -DFUNC_ASUINT_SPLITDOUBLE=1";
    ExpectedVector = std::vector<uint32_t>{};
    break;
  case LongVector::UnaryOpType_AsUint16:
    IntrinsicString = "asuint16";
    ExpectedVector = std::vector<uint16_t>{};
    break;
  default:
    VERIFY_FAIL("Invalid UnaryOpType");
  }
}

template <typename DataTypeT, typename LongVectorOpTypeT>
LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::TestConfig(LongVector::BinaryOpType OpType)
   : OpTypeTraits(OpType) {
  IntrinsicString = "";
  BasicOpType = LongVector::BasicOpType_Binary;

  if (isFloatingPointType<DataTypeT>())
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
  case LongVector::BinaryOpType_AsDouble:
    OperatorString = ",";
    IntrinsicString = "asdouble";
    ExpectedVector = std::vector<double>{};
    break;
  default:
    VERIFY_FAIL("Invalid BinaryOpType");
  }
}

template <typename DataTypeT, typename LongVectorOpTypeT>
LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::TestConfig(LongVector::TrigonometricOpType OpType)
    : OpTypeTraits(OpType) {
  IntrinsicString = "";
  BasicOpType = LongVector::BasicOpType_Unary;

  // All trigonometric ops are floating point types.
  // These trig functions are defined to have a max absolute error of 0.0008
  // as per the D3D functional specs. An example with this spec for sin and
  // cos is available here:
  // https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#22.10.20
  ValidationType = LongVector::ValidationType_Epsilon;
  if (std::is_same_v<DataTypeT, HLSLHalf_t>)
    Tolerance = 0.0010f;
  else if (std::is_same_v<DataTypeT, float>)
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

template <typename DataTypeT>
std::string LongVector::getHLSLTypeString() {
  if (std::is_same_v<DataTypeT, HLSLBool_t>)
    return "bool";
  if (std::is_same_v<DataTypeT, HLSLHalf_t>)
    return "half";
  if (std::is_same_v<DataTypeT, float>)
    return "float";
  if (std::is_same_v<DataTypeT, double>)
    return "double";
  if (std::is_same_v<DataTypeT, int16_t>)
    return "int16_t";
  if (std::is_same_v<DataTypeT, int32_t>)
    return "int";
  if (std::is_same_v<DataTypeT, int64_t>)
    return "int64_t";
  if (std::is_same_v<DataTypeT, uint16_t>)
    return "uint16_t";
  if (std::is_same_v<DataTypeT, uint32_t>)
    return "uint32_t";
  if (std::is_same_v<DataTypeT, uint64_t>)
    return "uint64_t";

  std::string ErrStr("getHLSLTypeString() Unsupported type: ");
  ErrStr.append(typeid(DataTypeT).name());
  VERIFY_IS_TRUE(false, ErrStr.c_str());
  return "UnknownType";
}

template <typename DataTypeT, typename LongVectorOpTypeT>
std::string LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::getHLSLInputTypeString() const {
  return LongVector::getHLSLTypeString<DataTypeT>();
}

template <typename DataTypeT, typename LongVectorOpTypeT>
std::string LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::getHLSLOutputTypeString() const {

  // Normal case, output matches input type ( DataTypeT )
  if (auto* Vec = std::get_if<std::vector<DataTypeT>>(&ExpectedVector))
    return LongVector::getHLSLTypeString<DataTypeT>();

  if(!isAsTypeOp())  {
    LOG_ERROR_FMT_THROW(L"getHLSLOutputTypeString() called with an unsupported op type: %d", OpTypeTraits.OpType);
    return std::string("UnknownType");
  }

  if (isBinaryOp())
    // There is only one binary AsType op, which is BinaryOpType_AsDouble.
    return LongVector::getHLSLTypeString<double>();

  // We're an AsType op and we're not a Binary op, so we must be a Unary op.
  auto OpType = static_cast<LongVector::UnaryOpType>(OpTypeTraits.OpType);
  switch (OpType) {
    case LongVector::UnaryOpType_AsFloat16:
      return LongVector::getHLSLTypeString<HLSLHalf_t>();
    case LongVector::UnaryOpType_AsFloat:
      return LongVector::getHLSLTypeString<float>();
    case LongVector::UnaryOpType_AsInt:
      return LongVector::getHLSLTypeString<int32_t>();
    case LongVector::UnaryOpType_AsInt16:
      return LongVector::getHLSLTypeString<int16_t>();
    case LongVector::UnaryOpType_AsUint:
      return LongVector::getHLSLTypeString<uint32_t>();
    case LongVector::UnaryOpType_AsUint_SplitDouble:
      return LongVector::getHLSLTypeString<uint32_t>();
    case LongVector::UnaryOpType_AsUint16:
      return LongVector::getHLSLTypeString<uint16_t>();
    default:
      LOG_ERROR_FMT_THROW(L"getHLSLOutputTypeString() called with an unsupported op type: %d", OpType);
      return std::string("UnknownType");
  }
}

//computeExpectedValuesForAsTypeOp for Unary ops
template <typename DataTypeT, typename LongVectorOpTypeT>
void LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::computeExpectedValuesForAsTypeOp(
    const std::vector<DataTypeT>& InputVector1) {

  VERIFY_IS_TRUE(isAsTypeOp(),
                 L"computeExpectedValuesForAsTypeOp() called with a non-AsType op config.");

  VERIFY_IS_TRUE(isUnaryOp(), L"computeExpectedValuesForAsTypeOp() called with a non-unary op config.");

  const auto OpType = static_cast<LongVector::UnaryOpType>(OpTypeTraits.OpType);

  switch (OpType) {
    case UnaryOpType_AsFloat16:
      fillExpectedVector<HLSLHalf_t>(ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asFloat16(InputVector1[Index]); });
      break;
    case UnaryOpType_AsFloat:
      fillExpectedVector<float>(ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asFloat(InputVector1[Index]); });
      break;
    case UnaryOpType_AsInt:
      fillExpectedVector<int32_t>(ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asInt(InputVector1[Index]); });
      break;
    case UnaryOpType_AsInt16:
      fillExpectedVector<int16_t>(ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asInt16(InputVector1[Index]); });
      break;
    case UnaryOpType_AsUint:
      fillExpectedVector<uint32_t>(ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asUint(InputVector1[Index]); });
      break;
    case UnaryOpType_AsUint_SplitDouble:
    {
      // SplitDouble is a special case. We fill the first half of the expected
      // vector with the expected low bits of each input double and the second
      // half with the high bits of each input double. Doing things this way
      // helps keep the rest of the generic logic in the LongVector test code
      // simple.
      auto* TypedExpectedValues = std::get_if<std::vector<uint32_t>>(&ExpectedVector);
      VERIFY_IS_NOT_NULL(TypedExpectedValues, L"Expected vector is not of the correct type.");
      TypedExpectedValues->resize(InputVector1.size() * 2);
      uint32_t LowBits, HighBits;
      const size_t InputSize = InputVector1.size();
      for(size_t Index = 0; Index < InputSize; ++Index) {
        splitDouble(InputVector1[Index], LowBits, HighBits);
        (*TypedExpectedValues)[Index] = LowBits;
        (*TypedExpectedValues)[Index + InputSize] = HighBits;
      }
      break;
    }
    case UnaryOpType_AsUint16:
      fillExpectedVector<uint16_t>(ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asUint16(InputVector1[Index]); });
      break;
    default:
      LOG_ERROR_FMT_THROW(L"Unsupported AsType op: %d", OpTypeTraits.OpType);
  }
}

//computeExpectedValuesForAsTypeOp for Binary ops
template <typename DataTypeT, typename LongVectorOpTypeT>
void LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::computeExpectedValuesForAsTypeOp(
    const std::vector<DataTypeT>& InputVector1, const std::vector<DataTypeT>& InputVector2) {

  VERIFY_IS_TRUE(isAsTypeOp(),
                 L"computeExpectedValuesForAsTypeOp() called with a non-AsType op config.");

  VERIFY_IS_TRUE(isBinaryOp(), L"computeExpectedValuesForAsTypeOp() called with a non-binary op config.");

  const auto OpType = static_cast<LongVector::BinaryOpType>(OpTypeTraits.OpType);

  if (OpType != LongVector::BinaryOpType_AsDouble)
    LOG_ERROR_FMT_THROW(L"computeExpectedValuesForAsTypeOp() called with an unsupported op type: %d", OpType);

  fillExpectedVector<double>(ExpectedVector, InputVector1.size(),
  [&](size_t Index) { return asDouble(InputVector1[Index], InputVector2[Index]); });
}

template <typename DataTypeT, typename LongVectorOpTypeT>
bool LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::isAsTypeOp(LongVector::UnaryOpType OpType) const {
  return ( OpType == LongVector::UnaryOpType_AsFloat ||
      OpType == LongVector::UnaryOpType_AsFloat16 ||
      OpType == LongVector::UnaryOpType_AsInt ||
      OpType == LongVector::UnaryOpType_AsInt16 ||
      OpType == LongVector::UnaryOpType_AsUint ||
      OpType == LongVector::UnaryOpType_AsUint_SplitDouble ||
      OpType == LongVector::UnaryOpType_AsUint16);
}

template <typename DataTypeT, typename LongVectorOpTypeT>
bool LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::isAsTypeOp(LongVector::BinaryOpType OpType) const {
  return  OpType == LongVector::BinaryOpType_AsDouble;
}

//computeExpectedValue Binary
template <typename DataTypeT, typename LongVectorOpTypeT>
DataTypeT LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::computeExpectedValue(const DataTypeT &A, const DataTypeT &B,
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
    return mod(A, B);
  case LongVector::BinaryOpType_Multiply:
    return A * B;
  case LongVector::BinaryOpType_Add:
    return A + B;
  case LongVector::BinaryOpType_Subtract:
    return A - B;
  case LongVector::BinaryOpType_Divide:
    return A / B;
  case LongVector::BinaryOpType_Modulus:
    return mod(A, B);
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
    return DataTypeT();
  }
}

// Generic computeExpectedValue Binary. Dispatches to the correct
// computeExpectedValue based on the OpTypeTraits.OpType.
template <typename DataTypeT, typename LongVectorOpTypeT>
DataTypeT LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::computeExpectedValue(const DataTypeT &A, const DataTypeT &B) const {
  if(!isBinaryOp())
    LOG_ERROR_FMT_THROW(
      L"computeExpectedValue(const DataTypeT &A, const DataTypeT &B) called "
      L"on a unary op: %d",
      OpTypeTraits.OpType);

  return computeExpectedValue(A, B, static_cast<LongVector::BinaryOpType>(OpTypeTraits.OpType));
}

// computeExpectedValue Trigonometric
template <typename DataTypeT, typename LongVectorOpTypeT>
DataTypeT LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::computeExpectedValue(const DataTypeT &A,
                              LongVector::TrigonometricOpType OpType) const {
  // The trig functions are only valid on floating point types. The constexpr in
  // this case is a relatively easy and clean way to prevent the compiler from
  // erroring out trying to resolve these for the non floating point types. We
  // won't use them in the first place.
  if constexpr (isFloatingPointType<DataTypeT>()) {
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
      return DataTypeT();
    }
  }

  LOG_ERROR_FMT_THROW(L"computeExpectedValue(const DataTypeT &A, "
                      L"LongVectorOpTypeT OpType) called on a "
                      L"non-float type: %d",
                      OpType);

  return DataTypeT();
}

// Generic computeExpectedValue Unary
template <typename DataTypeT, typename LongVectorOpTypeT>
DataTypeT LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::computeExpectedValue(const DataTypeT &A,
                              LongVector::UnaryOpType OpType) const {

  // Initialize is currently the only unary op that calls this basic computeExpectedValue
  if (OpType != LongVector::UnaryOpType_Initialize) {
    LOG_ERROR_FMT_THROW(L"Unknown UnaryOpType :%d", OpTypeTraits.OpType);
    return DataTypeT();
  }

  return A;
}

// Generic computeExpectedValue Unary. Dispatches to the correct
// computeExpectedValue based LongVectorOpTypeT.
template <typename DataTypeT, typename LongVectorOpTypeT>
DataTypeT LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::computeExpectedValue(const DataTypeT &A) const {

  if constexpr (std::is_same_v<LongVectorOpTypeT, LongVector::TrigonometricOpType>) {
    const auto OpType = static_cast<LongVector::TrigonometricOpType>(OpTypeTraits.OpType);
    // HLSLHalf_t is a struct. We need to call the constructor to get the
    // expected value.
    return computeExpectedValue(A, OpType);
  }

  if constexpr (std::is_same_v<LongVectorOpTypeT, LongVector::UnaryOpType>) {
    const auto OpType = static_cast<LongVector::UnaryOpType>(OpTypeTraits.OpType);
    // HLSLHalf_t is a struct. We need to call the constructor to get the
    // expected value.
    return computeExpectedValue(A, OpType);
  }

  LOG_ERROR_FMT_THROW(
      L"computeExpectedValue(const DataType&A) called on an unrecognized binary op: %d",
      OpTypeTraits.OpType);

  return DataTypeT();
}

template <typename DataTypeT, typename LongVectorOpTypeT>
std::string LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::getCompilerOptionsString() const {
  
  std::stringstream CompilerOptions("");
  std::string HLSLInputType = getHLSLInputTypeString();

  CompilerOptions << "-DTYPE=";
  CompilerOptions << HLSLInputType;
  CompilerOptions << " -DNUM=";
  CompilerOptions << LengthToTest;
  const bool Is16BitType =
      (HLSLInputType == "int16_t" || HLSLInputType == "uint16_t" || HLSLInputType == "half");
  CompilerOptions << (Is16BitType ? " -enable-16bit-types" : "");
  CompilerOptions << " -DOPERATOR=";
  CompilerOptions << OperatorString;

  if (isBinaryOp()) {
    CompilerOptions << " -DOPERAND2=";
    CompilerOptions << (isScalarOp() ? "InputScalar" : "InputVector2");

    if (isScalarOp())
      CompilerOptions << " -DIS_SCALAR_OP=1";
    else
      CompilerOptions << " -DIS_BINARY_VECTOR_OP=1";

    CompilerOptions << " -DFUNC=";
    CompilerOptions << IntrinsicString;
  } else { // Unary Op
    CompilerOptions << " -DFUNC=";
    CompilerOptions << IntrinsicString;
    // Not used for unary ops, but needs to be a " " for compilation of the
    // shader after macro expansion.
    CompilerOptions << " -DOPERAND2= ";
  }

  // For most of the ops this string is empty.
  CompilerOptions << SpecialDefines;

  std::string HLSLOutputType = getHLSLOutputTypeString();
  CompilerOptions << " -DOUT_TYPE=";
  CompilerOptions << HLSLOutputType;

  return CompilerOptions.str();
}

template <typename DataTypeT, typename LongVectorOpTypeT>
std::vector<DataTypeT> LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::getInputValueSet(size_t ValueSetIndex) const {
  if (ValueSetIndex == 2 && !isBinaryOp())
    VERIFY_FAIL("ValueSetindex==2 is only valid for binary ops.");

  std::wstring InputValueSetName = L"";
  if (ValueSetIndex == 1)
    InputValueSetName = InputValueSetName1;
  else if (ValueSetIndex == 2)
    InputValueSetName = InputValueSetName2;
  else
    VERIFY_FAIL("Invalid ValueSetIndex");

  return getInputValueSetByKey<DataTypeT>(InputValueSetName);
}

template <typename DataTypeT, typename LongVectorOpTypeT>
bool LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::resolveOutputTypeAndVerifyOutput(const std::shared_ptr<st::ShaderOpTestResult> &TestResult) {
  if(!isAsTypeOp()) {
    // Currently only AsType ops are supported to have an output type that
    // doesn't match the input type. This may change in the future when more
    // tests cases are added. If it does then this check will need to be updated
    // at that point.
    LOG_ERROR_FMT_THROW(L"resolveOutputTypeAndVerifyOutput() called with a non-unary op: %d", OpTypeTraits.OpType);
    return false;
  }

  // There is only one binary AsType op, which is BinaryOpType_AsDouble.
  if (isBinaryOp())
    return verifyOutput<double>(TestResult);

  switch (static_cast<LongVector::UnaryOpType>(OpTypeTraits.OpType)) {
  case LongVector::UnaryOpType_AsFloat:
      return verifyOutput<float>(TestResult);
  case LongVector::UnaryOpType_AsFloat16:
      return verifyOutput<HLSLHalf_t>(TestResult);
  case LongVector::UnaryOpType_AsInt:
      return verifyOutput<int32_t>(TestResult);
  case LongVector::UnaryOpType_AsInt16:
      return verifyOutput<int16_t>(TestResult);
  case LongVector::UnaryOpType_AsUint:
      return verifyOutput<uint32_t>(TestResult);
  case LongVector::UnaryOpType_AsUint_SplitDouble:
      return verifyOutput<uint32_t>(TestResult);
  case LongVector::UnaryOpType_AsUint16:
      return verifyOutput<uint16_t>(TestResult);
  default:
    LOG_ERROR_FMT_THROW(L"verifyOutput() called with an unsupported UnaryOpType: %d", OpTypeTraits.OpType);
    return false;
  }
}

// Public version of verifyOutput. Handles logic to dispatch to the correct
// templated verifyOutput based on the expected output type.
template <typename DataTypeT, typename LongVectorOpTypeT>
bool LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::verifyOutput(const std::shared_ptr<st::ShaderOpTestResult> &TestResult) {
  // First try the most common case where the output datatype matches the input
  // datatype (DataTypeT). std::get_if will return a null pointer if the variant
  // isn't holding a std::vector<DataTypeT>
  if (auto TypedExpectedValues = std::get_if<std::vector<DataTypeT>>(&ExpectedVector)) {
    return verifyOutput<DataTypeT>(TestResult);
  }

  // This implies that the input type (DataTypeT) is different from the 
  // output type. We will need to resolve the output type based on
  // LongVectorOpTypeT
  return resolveOutputTypeAndVerifyOutput(TestResult);
}

// Private version of verifyOutput. Expected to be called internally when we've
// resolved what the expected output type is.
template <typename DataTypeT, typename LongVectorOpTypeT>
template <typename OutputDataTypeT>
bool LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::verifyOutput(const std::shared_ptr<st::ShaderOpTestResult> &TestResult) {

  if (auto TypedExpectedValues = std::get_if<std::vector<OutputDataTypeT>>(&ExpectedVector)) {
    MappedData ShaderOutData;
    TestResult->Test->GetReadBackData("OutputVector", &ShaderOutData);

    // For most of the ops, the output vector size is the same as the input
    // vector size. But some, such as the AsUint_SplitDouble op, have an
    // output vector size that is double the input vector size.
    const size_t OutputVectorSize = (*TypedExpectedValues).size();

    std::vector<OutputDataTypeT> ActualValues;
    fillLongVectorDataFromShaderBuffer(ShaderOutData, ActualValues, OutputVectorSize);

    return doVectorsMatch(ActualValues, *TypedExpectedValues, Tolerance, ValidationType);
  }

  // This is the private TestConfig::VerifyOutput, expected to only be used when
  // DataTypeT and OutputDataTypeT are the same type. 
  LOG_ERROR_FMT_THROW(L"Programmer Error? Expected vector type does not match the expected type: %s",
                      typeid(OutputDataTypeT).name());
  return false;
}
