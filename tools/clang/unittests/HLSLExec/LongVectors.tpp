template <typename DataTypeT>
DataTypeT LongVector::GetLongVectorOpType(const LongVectorOpTypeStringToEnumValue *Values,
                             const std::wstring &OpTypeString,
                             std::size_t Length) {
  for (size_t i = 0; i < Length; i++) {
    if (Values[i].OpTypeString == OpTypeString) {
      return static_cast<DataTypeT>(Values[i].OpTypeValue);
    }
  }

  LOG_ERROR_FMT_THROW(L"Invalid LongVectorOpType string: %s",
                      OpTypeString.c_str());

  return static_cast<DataTypeT>(UINT_MAX);
}

// Helper to fill the shader buffer based on type. Convenient to be used when
// copying HLSL*_t types so we can copy the underlying type directly instead of
// the struct.
template <typename DataTypeT>
void LongVector::FillShaderBufferFromLongVectorData(std::vector<BYTE> &ShaderBuffer,
                                        std::vector<DataTypeT> &TestData) {

  // Note: DataSize for HLSLHalf_t and HLSLBool_t may be larger than the
  // underlying type in some cases. Thats fine. Resize just makes sure we have
  // enough space.
  const size_t NumElements = TestData.size();
  const size_t DataSize = sizeof(DataTypeT) * NumElements;
  ShaderBuffer.resize(DataSize);

  if constexpr (std::is_same_v<DataTypeT, HLSLHalf_t>) {
    DirectX::PackedVector::HALF *ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      ShaderBufferPtr[i] = TestData[i].Val;
    }
  } else if constexpr (std::is_same_v<DataTypeT, HLSLBool_t>) {
    int32_t *ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      ShaderBufferPtr[i] = TestData[i].Val;
    }
  } else {
    DataTypeT *ShaderBufferPtr =
        reinterpret_cast<DataTypeT *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      ShaderBufferPtr[i] = TestData[i];
    }
  }
}

// Helpers so we do the right thing for float types. HLSLHalf_t is handled in an
// operator overload.
template <typename DataTypeT>
DataTypeT LongVector::Mod(const DataTypeT &A, const DataTypeT &B) {
  return A % B;
}

template <> float LongVector::Mod(const float &A, const float &B) {
  return std::fmod(A, B);
}

template <> double LongVector::Mod(const double &A, const double &B) {
  return std::fmod(A, B);
}

// Helper to fill the test data from the shader buffer based on type. Convenient
// to be used when copying HLSL*_t types so we can use the underlying type.
template <typename DataTypeT>
void LongVector::FillLongVectorDataFromShaderBuffer(MappedData &ShaderBuffer,
                                        std::vector<DataTypeT> &TestData,
                                        size_t NumElements) {
  if constexpr (std::is_same_v<DataTypeT, HLSLHalf_t>) {
    DirectX::PackedVector::HALF *ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      // HLSLHalf_t has a DirectX::PackedVector::HALF based constructor.
      TestData.push_back(ShaderBufferPtr[i]);
    }
  } else if constexpr (std::is_same_v<DataTypeT, HLSLBool_t>) {
    int32_t *ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      // HLSLBool_t has a int32_t based constructor.
      TestData.push_back(ShaderBufferPtr[i]);
    }
  } else {
    DataTypeT *ShaderBufferPtr =
        reinterpret_cast<DataTypeT *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i) {
      TestData.push_back(ShaderBufferPtr[i]);
    }
  }
}

template <typename DataTypeT>
bool LongVector::DoValuesMatch(DataTypeT A, DataTypeT B, float Tolerance,
                   LongVector::ValidationType) {
  if (Tolerance == 0.0f)
    return A == B;

  DataTypeT Diff = A > B ? A - B : B - A;
  return Diff > Tolerance;
}

bool LongVector::DoValuesMatch(HLSLBool_t A, HLSLBool_t B, float,
                          LongVector::ValidationType) {
  return A == B;
}

bool LongVector::DoValuesMatch(HLSLHalf_t A, HLSLHalf_t B, float Tolerance,
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

bool LongVector::DoValuesMatch(float A, float B, float Tolerance,
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

bool LongVector::DoValuesMatch(double A, double B, float Tolerance,
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
bool LongVector::DoVectorsMatch(const std::vector<DataTypeT> &ActualValues,
                    const std::vector<DataTypeT> &ExpectedValues,
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

template <typename DataTypeT, typename LongVectorOpTypeT>
std::vector<DataTypeT> LongVector::ComputeExpectedValues(
    const std::vector<DataTypeT> &InputVector1,
    const std::vector<DataTypeT> &InputVector2,
    const LongVector::TestConfig<DataTypeT, LongVectorOpTypeT> &Config) {

  VERIFY_IS_TRUE(
      Config.IsBinaryOp(),
      L"ComputeExpectedValues() called with a non-binary op config.");

  std::vector<DataTypeT> ExpectedValues = {};

  for (size_t i = 0; i < InputVector1.size(); ++i) {
    ExpectedValues.push_back(
        Config.ComputeExpectedValue(InputVector1[i], InputVector2[i]));
  }

  return ExpectedValues;
}

template <typename DataTypeT, typename LongVectorOpTypeT>
std::vector<DataTypeT> LongVector::ComputeExpectedValues(
    const std::vector<DataTypeT> &InputVector1, const DataTypeT &ScalarInput,
    const LongVector::TestConfig<DataTypeT, LongVectorOpTypeT> &Config) {

  VERIFY_IS_TRUE(Config.IsScalarOp(), L"ComputeExpectedValues() called with a "
                                      L"non-binary non-scalar op config.");

  std::vector<DataTypeT> ExpectedValues;

  for (size_t i = 0; i < InputVector1.size(); ++i) {
    ExpectedValues.push_back(
        Config.ComputeExpectedValue(InputVector1[i], ScalarInput));
  }

  return ExpectedValues;
}

template <typename DataTypeT, typename LongVectorOpTypeT>
std::vector<DataTypeT> LongVector::ComputeExpectedValues(
    const std::vector<DataTypeT> &InputVector1,
    const LongVector::TestConfig<DataTypeT, LongVectorOpTypeT> &Config) {

  VERIFY_IS_TRUE(Config.IsUnaryOp(),
                 L"ComputeExpectedValues() called with a non-unary op config.");

  std::vector<DataTypeT> ExpectedValues;

  for (size_t i = 0; i < InputVector1.size(); ++i) {
    ExpectedValues.push_back(Config.ComputeExpectedValue(InputVector1[i]));
  }

  return ExpectedValues;
}

template <typename DataTypeT>
void LongVector::LogLongVector(const std::vector<DataTypeT> &Values,
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

template <typename DataTypeT>
void LongVector::LogScalar(const DataTypeT &Value, const std::wstring &Name) {
  WEX::Logging::Log::Comment(
      WEX::Common::String().Format(L"Scalar Name: %s", Name.c_str()));

  std::wstringstream Wss(L"");
  Wss << L"Scalar Value: ";
  Wss << Value;
  WEX::Logging::Log::Comment(Wss.str().c_str());
}

template <typename DataTypeT, typename LongVectorOpTypeT>
LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::TestConfig(LongVector::UnaryOpType OpType)
    : OpTypeTraits(OpType) {
  IntrinsicString = "";
  BasicOpType = LongVector::BasicOpType_Unary;

  if (IsFloatingPointType<DataTypeT>())
    Tolerance = 1;

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

template <typename DataTypeT, typename LongVectorOpTypeT>
LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::TestConfig(LongVector::BinaryOpType OpType)
   : OpTypeTraits(OpType) {
  IntrinsicString = "";
  BasicOpType = LongVector::BasicOpType_Binary;

  if (IsFloatingPointType<DataTypeT>())
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

template <typename DataTypeT, typename LongVectorOpTypeT>
bool LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::HasFunctionDefinition() const {
  if constexpr (std::is_same_v<LongVectorOpTypeT, LongVector::UnaryOpType>) {
    if (OpTypeTraits.OpType == LongVector::UnaryOpType_Clamp)
      return true;
    else if (OpTypeTraits.OpType == LongVector::UnaryOpType_Initialize)
      return true;
    else
      return false;
  } else
    return false;
}

template <typename DataTypeT, typename LongVectorOpTypeT>
std::string LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::GetOPERAND2String() const {
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

template <typename DataTypeT, typename LongVectorOpTypeT>
std::string LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::GetHLSLTypeString() const {
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

  std::string ErrStr("GetHLSLTypeString() Unsupported type: ");
  ErrStr.append(typeid(DataTypeT).name());
  VERIFY_IS_TRUE(false, ErrStr.c_str());
  return "UnknownType";
}

template <typename DataTypeT, typename LongVectorOpTypeT>
DataTypeT LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::ComputeExpectedValue(const DataTypeT &A, const DataTypeT &B,
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
    return DataTypeT();
  }
}

template <typename DataTypeT, typename LongVectorOpTypeT>
DataTypeT LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::ComputeExpectedValue(const DataTypeT &A, const DataTypeT &B) const {
  if(!IsBinaryOp())
    LOG_ERROR_FMT_THROW(
        L"ComputeExpectedValue(const DataTypeT &A, const DataTypeT &B) called "
        L"on a unary op: %d",
        OpTypeTraits.OpType);

  return ComputeExpectedValue(A, B, static_cast<LongVector::BinaryOpType>(OpTypeTraits.OpType));
}

template <typename DataTypeT, typename LongVectorOpTypeT>
DataTypeT LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::ComputeExpectedValue(const DataTypeT &A,
                              LongVector::TrigonometricOpType OpType) const {
  // The trig functions are only valid on floating point types. The constexpr in
  // this case is a relatively easy and clean way to prevent the compiler from
  // erroring out trying to resolve these for the non floating point types. We
  // won't use them in the first place.
  if constexpr (IsFloatingPointType<DataTypeT>()) {
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

  LOG_ERROR_FMT_THROW(L"ComputeExpectedValue(const DataTypeT &A, "
                      L"LongVectorOpTypeT OpType) called on a "
                      L"non-float type: %d",
                      OpType);

  return DataTypeT();
}

template <typename DataTypeT, typename LongVectorOpTypeT>
DataTypeT LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::ComputeExpectedValue(const DataTypeT &A,
                              LongVector::UnaryOpType OpType) const {
  switch (OpType) {
  case LongVector::UnaryOpType_Clamp: {
    std::vector<DataTypeT> ArgsArray = GetInputArgsArray();
    DataTypeT Min = ArgsArray[0];
    DataTypeT Max = ArgsArray[1];
    return std::clamp(A, Min, Max);
  }
  case LongVector::UnaryOpType_Initialize:
    return A;
  default:
    LOG_ERROR_FMT_THROW(L"Unknown UnaryOpType :%d", OpTypeTraits.OpType);
    return DataTypeT();
  }
}

template <typename DataTypeT, typename LongVectorOpTypeT>
DataTypeT LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::ComputeExpectedValue(const DataTypeT &A) const {

  if constexpr (std::is_same_v<LongVectorOpTypeT, LongVector::TrigonometricOpType>) {
    const auto OpType = static_cast<LongVector::TrigonometricOpType>(OpTypeTraits.OpType);
    // HLSLHalf_t is a struct. We need to call the constructor to get the
    // expected value.
    return ComputeExpectedValue(A, OpType);
  }

  if constexpr (std::is_same_v<LongVectorOpTypeT, LongVector::UnaryOpType>) {
    const auto OpType = static_cast<LongVector::UnaryOpType>(OpTypeTraits.OpType);
    // HLSLHalf_t is a struct. We need to call the constructor to get the
    // expected value.
    return ComputeExpectedValue(A, OpType);
  }

  LOG_ERROR_FMT_THROW(
      L"ComputeExpectedValue(const DataType&A) called on an unrecognized binary op: %d",
      OpTypeTraits.OpType);

  return DataTypeT();
}

template <typename DataTypeT, typename LongVectorOpTypeT>
std::vector<DataTypeT>  LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::GetInputArgsArray() const {

  std::vector<DataTypeT> InputArgs;

  std::wstring LocalInputArgsArrayName = InputArgsArrayName;

  if (IsClampOp() && LocalInputArgsArrayName == L"") {
    LocalInputArgsArrayName = L"DefaultClampArgs";
  }

  if (LocalInputArgsArrayName.empty())
    VERIFY_FAIL("No args array name set.");

  if (std::is_same_v<DataTypeT, HLSLBool_t> && IsClampOp())
    VERIFY_FAIL("Clamp is not supported for bools.");
  else
    return GetInputValueSetByKey<DataTypeT>(LocalInputArgsArrayName, false);

  VERIFY_FAIL("Invalid type for args array.");
  return std::vector<DataTypeT>();
}

template <typename DataTypeT, typename LongVectorOpTypeT>
std::string LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::GetCompilerOptionsString(size_t VectorSize) const {
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

template <typename DataTypeT, typename LongVectorOpTypeT>
std::vector<DataTypeT> LongVector::TestConfig<DataTypeT, LongVectorOpTypeT>::GetInputValueSet(size_t ValueSetIndex) const {
  if (ValueSetIndex == 2 && !IsBinaryOp())
    VERIFY_FAIL("ValueSetindex==2 is only valid for binary ops.");

  std::wstring InputValueSetName = L"";
  if (ValueSetIndex == 1)
    InputValueSetName = InputValueSetName1;
  else if (ValueSetIndex == 2)
    InputValueSetName = InputValueSetName2;
  else
    VERIFY_FAIL("Invalid ValueSetIndex");

  return GetInputValueSetByKey<DataTypeT>(InputValueSetName);
}
