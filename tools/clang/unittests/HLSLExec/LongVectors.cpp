#include "LongVectors.h"
#include "HlslExecTestUtils.h"
#include <iomanip>

template <typename LongVectorOpTypeT>
const LongVector::OpTypeMetaData<LongVectorOpTypeT> &
LongVector::getLongVectorOpType(const OpTypeMetaData<LongVectorOpTypeT> *Values,
                                const std::wstring &OpTypeString,
                                std::size_t Length) {
  for (size_t i = 0; i < Length; i++) {
    if (Values[i].OpTypeString == OpTypeString)
      return Values[i];
  }

  LOG_ERROR_FMT_THROW(L"Invalid LongVectorOpType string: %s",
                      OpTypeString.c_str());

  throw std::runtime_error("Invalid LongVectorOpType string");
}

const LongVector::OpTypeMetaData<LongVector::BinaryOpType> &
LongVector::getBinaryOpType(const std::wstring &OpTypeString) {
  return getLongVectorOpType<LongVector::BinaryOpType>(
      binaryOpTypeStringToOpMetaData, OpTypeString,
      std::size(binaryOpTypeStringToOpMetaData));
}

const LongVector::OpTypeMetaData<LongVector::UnaryOpType> &
LongVector::getUnaryOpType(const std::wstring &OpTypeString) {
  return getLongVectorOpType<LongVector::UnaryOpType>(
      unaryOpTypeStringToOpMetaData, OpTypeString,
      std::size(unaryOpTypeStringToOpMetaData));
}

const LongVector::OpTypeMetaData<LongVector::AsTypeOpType> &
LongVector::getAsTypeOpType(const std::wstring &OpTypeString) {
  return getLongVectorOpType<LongVector::AsTypeOpType>(
      asTypeOpTypeStringToOpMetaData, OpTypeString,
      std::size(asTypeOpTypeStringToOpMetaData));
}

const LongVector::OpTypeMetaData<LongVector::TrigonometricOpType> &
LongVector::getTrigonometricOpType(const std::wstring &OpTypeString) {
  return getLongVectorOpType<LongVector::TrigonometricOpType>(
      trigonometricOpTypeStringToOpMetaData, OpTypeString,
      std::size(trigonometricOpTypeStringToOpMetaData));
}

// Helper to fill the test data from the shader buffer based on type. Convenient
// to be used when copying HLSL*_t types so we can use the underlying type.
template <typename DataTypeT>
void LongVector::fillLongVectorDataFromShaderBuffer(
    MappedData &ShaderBuffer, std::vector<DataTypeT> &TestData,
    size_t NumElements) {

  if constexpr (std::is_same_v<DataTypeT, HLSLHalf_t>) {
    DirectX::PackedVector::HALF *ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i)
      // HLSLHalf_t has a DirectX::PackedVector::HALF based constructor.
      TestData.push_back(ShaderBufferPtr[i]);
    return;
  }

  if constexpr (std::is_same_v<DataTypeT, HLSLBool_t>) {
    int32_t *ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i)
      // HLSLBool_t has a int32_t based constructor.
      TestData.push_back(ShaderBufferPtr[i]);
    return;
  }

  DataTypeT *ShaderBufferPtr =
      reinterpret_cast<DataTypeT *>(ShaderBuffer.data());
  for (size_t i = 0; i < NumElements; ++i)
    TestData.push_back(ShaderBufferPtr[i]);
  return;
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

  DXASSERT(
      ActualValues.size() == ExpectedValues.size(),
      "Programmer error: Actual and Expected vectors must be the same size.");

  // Stash mismatched indexes for easy failure logging later
  std::vector<size_t> MismatchedIndexes;
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
void fillExpectedVector(LongVector::VariantVector &ExpectedVector, size_t Count,
                        ComputeFnT ComputeFn) {
  auto *TypedExpectedValues =
      std::get_if<std::vector<DataTypeT>>(&ExpectedVector);

  VERIFY_IS_NOT_NULL(TypedExpectedValues,
                     L"Expected vector is not of the correct type.");

  for (size_t Index = 0; Index < Count; ++Index)
    TypedExpectedValues->push_back(ComputeFn(Index));
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

template <typename DataTypeT> std::string LongVector::getHLSLTypeString() {
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

// These are helper arrays to be used with the TableParameterHandler that parses
// the LongVectorOpTable.xml file for us.
static TableParameter BinaryOpParameters[] = {
    {L"DataType", TableParameter::STRING, true},
    {L"OpTypeEnum", TableParameter::STRING, true},
    {L"InputValueSetName1", TableParameter::STRING, false},
    {L"InputValueSetName2", TableParameter::STRING, false},
};

static TableParameter UnaryOpParameters[] = {
    {L"DataType", TableParameter::STRING, true},
    {L"OpTypeEnum", TableParameter::STRING, true},
    {L"InputValueSetName1", TableParameter::STRING, false},
};

static TableParameter AsTypeOpParameters[] = {
    // DataTypeOut is determined at runtime based on the OpType.
    // For example...AsUint has an output type of uint32_t.
    {L"DataTypeIn", TableParameter::STRING, true},
    {L"OpTypeEnum", TableParameter::STRING, true},
    {L"InputValueSetName1", TableParameter::STRING, false},
    {L"InputValueSetName2", TableParameter::STRING, false},
};

bool LongVector::OpTest::classSetup() {
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
  }

  return true;
}

TEST_F(LongVector::OpTest, binaryOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  using namespace WEX::Common;

  const int TableSize = sizeof(BinaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(BinaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpTypeMD = LongVector::getBinaryOpType(OpTypeString);
  dispatchTestByDataType(OpTypeMD, DataType, Handler);
}

TEST_F(LongVector::OpTest, trigonometricOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const int TableSize = sizeof(UnaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(UnaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpTypeMD = LongVector::getTrigonometricOpType(OpTypeString);
  dispatchTestByDataType(OpTypeMD, DataType, Handler);
}

TEST_F(LongVector::OpTest, unaryOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const int TableSize = sizeof(UnaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(UnaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpTypeMD = LongVector::getUnaryOpType(OpTypeString);
  dispatchTestByDataType(OpTypeMD, DataType, Handler);
}

TEST_F(LongVector::OpTest, asTypeOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const int TableSize = sizeof(AsTypeOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(AsTypeOpParameters, TableSize);

  std::wstring DataTypeIn(Handler.GetTableParamByName(L"DataTypeIn")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpTypeMD = LongVector::getAsTypeOpType(OpTypeString);
  dispatchTestByDataType(OpTypeMD, DataTypeIn, Handler);
}

template <typename LongVectorOpTypeT>
void LongVector::OpTest::dispatchTestByDataType(
    const LongVector::OpTypeMetaData<LongVectorOpTypeT> &OpTypeMd,
    std::wstring DataType, TableParameterHandler &Handler) {
  using namespace WEX::Common;

  if (DataType == L"bool")
    dispatchTestByVectorLength<HLSLBool_t>(OpTypeMd, Handler);
  else if (DataType == L"int16")
    dispatchTestByVectorLength<int16_t>(OpTypeMd, Handler);
  else if (DataType == L"int32")
    dispatchTestByVectorLength<int32_t>(OpTypeMd, Handler);
  else if (DataType == L"int64")
    dispatchTestByVectorLength<int64_t>(OpTypeMd, Handler);
  else if (DataType == L"uint16")
    dispatchTestByVectorLength<uint16_t>(OpTypeMd, Handler);
  else if (DataType == L"uint32")
    dispatchTestByVectorLength<uint32_t>(OpTypeMd, Handler);
  else if (DataType == L"uint64")
    dispatchTestByVectorLength<uint64_t>(OpTypeMd, Handler);
  else if (DataType == L"float16")
    dispatchTestByVectorLength<HLSLHalf_t>(OpTypeMd, Handler);
  else if (DataType == L"float32")
    dispatchTestByVectorLength<float>(OpTypeMd, Handler);
  else if (DataType == L"float64")
    dispatchTestByVectorLength<double>(OpTypeMd, Handler);
  else
    VERIFY_FAIL(
        String().Format(L"DataType: %ls is not recognized.", DataType.c_str()));
}

template <>
void LongVector::OpTest::dispatchTestByDataType(
    const LongVector::OpTypeMetaData<LongVector::TrigonometricOpType> &OpTypeMd,
    std::wstring DataType, TableParameterHandler &Handler) {
  using namespace WEX::Common;

  if (DataType == L"float16")
    dispatchTestByVectorLength<HLSLHalf_t>(OpTypeMd, Handler);
  else if (DataType == L"float32")
    dispatchTestByVectorLength<float>(OpTypeMd, Handler);
  else if (DataType == L"float64")
    dispatchTestByVectorLength<double>(OpTypeMd, Handler);
  else
    LOG_ERROR_FMT_THROW(
        L"Trigonometric ops are only supported for floating point types. "
        L"DataType: %ls is not recognized.",
        DataType.c_str());
}

template <typename DataTypeT, typename LongVectorOpTypeT>
void LongVector::OpTest::dispatchTestByVectorLength(
    const LongVector::OpTypeMetaData<LongVectorOpTypeT> &OpTypeMd,
    TableParameterHandler &Handler) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  auto TestConfig = LongVector::makeTestConfig<DataTypeT>(OpTypeMd);

  // InputValueSetName1 is optional. So the string may be empty. An empty
  // string will result in the default value set for this DataType being used.
  std::wstring InputValueSet1(
      Handler.GetTableParamByName(L"InputValueSetName1")->m_str);
  if (!InputValueSet1.empty())
    TestConfig->setInputValueSet1(InputValueSet1);

  // InputValueSetName2 is optional. So the string may be empty. An empty
  // string will result in the default value set for this DataType being used.
  if (TestConfig->isBinaryOp()) {
    std::wstring InputValueSet2(
        Handler.GetTableParamByName(L"InputValueSetName2")->m_str);
    if (!InputValueSet2.empty())
      TestConfig->setInputValueSet2(InputValueSet2);
  }

  // Manual override to test a specific vector size. Convenient for debugging
  // issues.
  size_t InputSizeToTestOverride = 0;
  WEX::TestExecution::RuntimeParameters::TryGetValue(L"LongVectorInputSize",
                                                     InputSizeToTestOverride);

  std::vector<size_t> InputVectorSizes;
  if (InputSizeToTestOverride)
    InputVectorSizes.push_back(InputSizeToTestOverride);
  else
    InputVectorSizes = {3, 4, 5, 16, 17, 35, 100, 256, 1024};

  for (auto SizeToTest : InputVectorSizes) {
    // We could create a new config for each test case with the new length, but
    // that feels wasteful. Instead, we just update the length to test.
    TestConfig->setLengthToTest(SizeToTest);
    testBaseMethod<DataTypeT>(TestConfig);
  }
}

template <typename DataTypeT>
void LongVector::OpTest::testBaseMethod(
    std::unique_ptr<LongVector::TestConfig<DataTypeT>> &TestConfig) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const size_t VectorLengthToTest = TestConfig->getLengthToTest();

  hlsl_test::LogCommentFmt(L"Running LongVectorOpTestBase<%S, %zu>",
                           typeid(DataTypeT).name(), VectorLengthToTest);

  bool LogInputs = false;
  WEX::TestExecution::RuntimeParameters::TryGetValue(L"LongVectorLogInputs",
                                                     LogInputs);

  CComPtr<ID3D12Device> D3DDevice;
  if (!createDevice(&D3DDevice, ExecTestUtils::D3D_SHADER_MODEL_6_9, false)) {
#ifdef _HLK_CONF
    LOG_ERROR_FMT_THROW(
        L"Device does not support SM 6.9. Can't run these tests.");
#else
    WEX::Logging::Log::Comment(
        "Device does not support SM 6.9. Can't run these tests.");
    WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    return;
#endif
  }

  std::vector<DataTypeT> InputVector1;
  InputVector1.reserve(VectorLengthToTest);
  std::vector<DataTypeT> InputVector2; // May be unused, but must be defined.
  InputVector2.reserve(VectorLengthToTest);
  std::vector<DataTypeT> ScalarInput; // May be unused, but must be defined.
  const bool IsVectorBinaryOp =
      TestConfig->isBinaryOp() && !TestConfig->isScalarOp();

  std::vector<DataTypeT> InputVector1ValueSet = TestConfig->getInputValueSet1();
  std::vector<DataTypeT> InputVector2ValueSet =
      TestConfig->isBinaryOp() ? TestConfig->getInputValueSet2()
                               : std::vector<DataTypeT>();

  if (TestConfig->isScalarOp())
    // Scalar ops are always binary ops. So InputVector2ValueSet is initialized
    // with values above.
    ScalarInput.push_back(InputVector2ValueSet[0]);

  // Fill the input vectors with values from the value set. Repeat the values
  // when we reach the end of the value set.
  for (size_t Index = 0; Index < VectorLengthToTest; Index++) {
    InputVector1.push_back(
        InputVector1ValueSet[Index % InputVector1ValueSet.size()]);

    if (IsVectorBinaryOp)
      InputVector2.push_back(
          InputVector2ValueSet[Index % InputVector2ValueSet.size()]);
  }

  if (IsVectorBinaryOp)
    TestConfig->computeExpectedValues(InputVector1, InputVector2);
  else if (TestConfig->isScalarOp())
    TestConfig->computeExpectedValues(InputVector1, ScalarInput[0]);
  else // Must be a unary op
    TestConfig->computeExpectedValues(InputVector1);

  if (LogInputs) {
    logLongVector(InputVector1, L"InputVector1");

    if (IsVectorBinaryOp)
      logLongVector(InputVector2, L"InputVector2");
    else if (TestConfig->isScalarOp())
      logLongVector(ScalarInput, L"ScalarInput");
  }

  // We have to construct the string outside of the lambda. Otherwise it's
  // cleaned up when the lambda finishes executing but before the shader runs.
  std::string CompilerOptionsString = TestConfig->getCompilerOptionsString();

  // The name of the shader we want to use in ShaderOpArith.xml. Could also add
  // logic to set this name in ShaderOpArithTable.xml so we can use different
  // shaders for different tests.
  LPCSTR ShaderName = "LongVectorOp";
  // ShaderOpArith.xml defines the input/output resources and the shader source.
  CComPtr<IStream> TestXML;
  readHlslDataIntoNewStream(L"ShaderOpArith.xml", &TestXML, DxcDllSupport);

  // RunShaderOpTest is a helper function that handles resource creation
  // and setup. It also handles the shader compilation and execution. It takes a
  // callback that is called when the shader is compiled, but before it is
  // executed.
  std::shared_ptr<st::ShaderOpTestResult> TestResult = st::RunShaderOpTest(
      D3DDevice, DxcDllSupport, TestXML, ShaderName,
      [&](LPCSTR Name, std::vector<BYTE> &ShaderData, st::ShaderOp *ShaderOp) {
        hlsl_test::LogCommentFmt(L"RunShaderOpTest CallBack. Resource Name: %S",
                                 Name);

        // This callback is called once for each resource defined for
        // "LongVectorOp" in ShaderOpArith.xml. All callbacks are fired for each
        // resource. We determine whether they are applicable to the test case
        // when they run.

        // Process the callback for the OutputVector resource.
        if (0 == _stricmp(Name, "OutputVector")) {
          // We only need to set the compiler options string once. So this is a
          // convenient place to do it.
          ShaderOp->Shaders.at(0).Arguments = CompilerOptionsString.c_str();

          return;
        }

        // Process the callback for the InputFuncArgs resource.
        if (0 == _stricmp(Name, "InputFuncArgs")) {
          if (TestConfig->isScalarOp())
            fillShaderBufferFromLongVectorData(ShaderData, ScalarInput);
          return;
        }

        // Process the callback for the InputVector1 resource.
        if (0 == _stricmp(Name, "InputVector1")) {
          fillShaderBufferFromLongVectorData(ShaderData, InputVector1);
          return;
        }

        // Process the callback for the InputVector2 resource.
        if (0 == _stricmp(Name, "InputVector2")) {
          if (IsVectorBinaryOp)
            fillShaderBufferFromLongVectorData(ShaderData, InputVector2);
          return;
        }

        LOG_ERROR_FMT_THROW(
            L"RunShaderOpTest CallBack. Unexpected Resource Name: %S", Name);
      });

  // The TestConfig object handles the logic for extracting the shader output
  // based on the op type.
  VERIFY_SUCCEEDED(TestConfig->verifyOutput(TestResult));
}

// Helper to fill the shader buffer based on type. Convenient to be used when
// copying HLSL*_t types so we can copy the underlying type directly instead of
// the struct.
template <typename DataTypeT>
void LongVector::fillShaderBufferFromLongVectorData(
    std::vector<BYTE> &ShaderBuffer, std::vector<DataTypeT> &TestData) {

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
    return;
  }

  if constexpr (std::is_same_v<DataTypeT, HLSLBool_t>) {
    int32_t *ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t i = 0; i < NumElements; ++i)
      ShaderBufferPtr[i] = TestData[i].Val;
    return;
  }

  DataTypeT *ShaderBufferPtr =
      reinterpret_cast<DataTypeT *>(ShaderBuffer.data());
  for (size_t i = 0; i < NumElements; ++i)
    ShaderBufferPtr[i] = TestData[i];
  return;
}

template <typename DataTypeT>
std::string LongVector::TestConfig<DataTypeT>::getHLSLInputTypeString() const {
  return LongVector::getHLSLTypeString<DataTypeT>();
}

template <typename DataTypeT>
std::string LongVector::TestConfig<DataTypeT>::getHLSLOutputTypeString() const {

  // Normal case, output matches input type ( DataTypeT )
  if (auto *Vec = std::get_if<std::vector<DataTypeT>>(&ExpectedVector))
    return LongVector::getHLSLTypeString<DataTypeT>();

  // Non normal cases should be handled in a derived TestConfig class. i.e
  // TestConfigAsType::getHLSLOutputTypeString()
  LOG_ERROR_FMT_THROW(
      L"getHLSLOutputTypeString() called with an unsupported op type: %ls",
      OpTypeName.c_str());
  return std::string("UnknownType");
}

// Returns the compiler options string to be used for the shader compilation.
// Reference ShaderOpArith.xml and the 'LongVectorOp' shader source to see how
// the defines are used in the shader code.
template <typename DataTypeT>
std::string
LongVector::TestConfig<DataTypeT>::getCompilerOptionsString() const {

  std::stringstream CompilerOptions("");
  std::string HLSLInputType = getHLSLInputTypeString();

  CompilerOptions << "-DTYPE=";
  CompilerOptions << HLSLInputType;
  CompilerOptions << " -DNUM=";
  CompilerOptions << LengthToTest;
  const bool Is16BitType =
      (HLSLInputType == "int16_t" || HLSLInputType == "uint16_t" ||
       HLSLInputType == "half");
  CompilerOptions << (Is16BitType ? " -enable-16bit-types" : "");
  CompilerOptions << " -DOPERATOR=";
  CompilerOptions << (Operator ? *Operator : " ");

  if (isBinaryOp()) {
    CompilerOptions << " -DOPERAND2=";
    CompilerOptions << (isScalarOp() ? "InputScalar" : "InputVector2");

    if (isScalarOp())
      CompilerOptions << " -DIS_SCALAR_OP=1";
    else
      CompilerOptions << " -DIS_BINARY_VECTOR_OP=1";

    CompilerOptions << " -DFUNC=";
    CompilerOptions << (Intrinsic ? *Intrinsic : "");
  } else { // Unary Op
    CompilerOptions << " -DFUNC=";
    CompilerOptions << (Intrinsic ? *Intrinsic : "");
    // Not used for unary ops, but needs to be a " " for compilation of the
    // shader after macro expansion.
    CompilerOptions << " -DOPERAND2= ";
  }

  // For most of the ops this string is empty.
  CompilerOptions << (SpecialDefines ? *SpecialDefines : " ");

  std::string HLSLOutputType = getHLSLOutputTypeString();
  CompilerOptions << " -DOUT_TYPE=";
  CompilerOptions << HLSLOutputType;

  return CompilerOptions.str();
}

template <typename DataTypeT>
std::vector<DataTypeT> LongVector::TestConfig<DataTypeT>::getInputValueSet(
    size_t ValueSetIndex) const {

  // Calling with ValueSetIndex == 2 is only valid for binary ops.
  DXASSERT_NOMSG(!(ValueSetIndex == 2 && !isBinaryOp()));

  std::wstring InputValueSetName = L"";
  if (ValueSetIndex == 1)
    InputValueSetName = InputValueSetName1;
  else if (ValueSetIndex == 2)
    InputValueSetName = InputValueSetName2;
  else
    VERIFY_FAIL("Invalid ValueSetIndex");

  return getInputValueSetByKey<DataTypeT>(InputValueSetName);
}

// Public version of verifyOutput. Handles logic to dispatch to the correct
// templated verifyOutput based on the expected output type.
template <typename DataTypeT>
bool LongVector::TestConfig<DataTypeT>::verifyOutput(
    const std::shared_ptr<st::ShaderOpTestResult> &TestResult) {
  // First try the most common case where the output datatype matches the input
  // datatype (DataTypeT). std::get_if will return a null pointer if the variant
  // isn't holding a std::vector<DataTypeT>
  if (auto TypedExpectedValues =
          std::get_if<std::vector<DataTypeT>>(&ExpectedVector)) {
    return verifyOutput<DataTypeT>(TestResult);
  }

  // If we get here, its likely a programmer error. DataTypeT is the DataType
  // passed in to the TestConfig when its created. The only time the
  // ExpectedVector has a diffferent type is for a handful of ops, such as
  // casting, where the output type is different from the input type. But proper
  // dispatching to verifyOutput with the correct data type is intended to be
  // handled by derived classes. Hence, we throw an error here. See callers of
  // the private version of verifyOutput for examples of proper usage.
  LOG_ERROR_FMT_THROW(
      L"verifyOutput() called with an unsupported expected vector type: %ls.",
      typeid(ExpectedVector).name());
  return false;
}

// Private version of verifyOutput. Expected to be called internally when we've
// resolved what the expected output type is.
template <typename DataTypeT>
template <typename OutputDataTypeT>
bool LongVector::TestConfig<DataTypeT>::verifyOutput(
    const std::shared_ptr<st::ShaderOpTestResult> &TestResult) {

  if (auto TypedExpectedValues =
          std::get_if<std::vector<OutputDataTypeT>>(&ExpectedVector)) {
    MappedData ShaderOutData;
    TestResult->Test->GetReadBackData("OutputVector", &ShaderOutData);

    // For most of the ops, the output vector size is the same as the input
    // vector size. But some, such as the AsUint_SplitDouble op, have an
    // output vector size that is double the input vector size.
    const size_t OutputVectorSize = (*TypedExpectedValues).size();

    std::vector<OutputDataTypeT> ActualValues;
    fillLongVectorDataFromShaderBuffer(ShaderOutData, ActualValues,
                                       OutputVectorSize);

    return doVectorsMatch(ActualValues, *TypedExpectedValues, Tolerance,
                          ValidationType);
  }

  // This is the private TestConfig::VerifyOutput. If this is hitting its most
  // likely a new test case for a new op type that is misconfigured.
  LOG_ERROR_FMT_THROW(L"PRIVATE verifyOutput() called with an unsupported "
                      L"expected vector type: %ls.",
                      typeid(ExpectedVector).name());
  return false;
}

// Generic computeExpectedValues for Unary ops. Derived classes override
// computeExpectedValue.
template <typename DataTypeT>
void LongVector::TestConfig<DataTypeT>::computeExpectedValues(
    const std::vector<DataTypeT> &InputVector1) {
  fillExpectedVector<DataTypeT>(
      ExpectedVector, InputVector1.size(),
      [&](size_t Index) { return computeExpectedValue(InputVector1[Index]); });
}

// Generic computeExpectedValues for Binary ops. Derived classes override
// computeExpectedValue.
template <typename DataTypeT>
void LongVector::TestConfig<DataTypeT>::computeExpectedValues(
    const std::vector<DataTypeT> &InputVector1,
    const std::vector<DataTypeT> &InputVector2) {
  fillExpectedVector<DataTypeT>(
      ExpectedVector, InputVector1.size(), [&](size_t Index) {
        return computeExpectedValue(InputVector1[Index], InputVector2[Index]);
      });
}

// Generic computeExpectedValues for Scalar Binary ops. Derived classes override
// computeExpectedValue.
template <typename DataTypeT>
void LongVector::TestConfig<DataTypeT>::computeExpectedValues(
    const std::vector<DataTypeT> &InputVector1, const DataTypeT &ScalarInput) {
  fillExpectedVector<DataTypeT>(
      ExpectedVector, InputVector1.size(), [&](size_t Index) {
        return computeExpectedValue(InputVector1[Index], ScalarInput);
      });
}

template <typename DataTypeT>
LongVector::TestConfigAsType<DataTypeT>::TestConfigAsType(
    const LongVector::OpTypeMetaData<LongVector::AsTypeOpType> &OpTypeMd)
    : LongVector::TestConfig<DataTypeT>(OpTypeMd), OpType(OpTypeMd.OpType) {

  BasicOpType = LongVector::BasicOpType_Unary;

  switch (OpType) {
  case LongVector::AsTypeOpType_AsFloat16:
    ExpectedVector = std::vector<HLSLHalf_t>{};
    break;
  case LongVector::AsTypeOpType_AsFloat:
    ExpectedVector = std::vector<float>{};
    break;
  case LongVector::AsTypeOpType_AsInt:
    ExpectedVector = std::vector<int32_t>{};
    break;
  case LongVector::AsTypeOpType_AsInt16:
    ExpectedVector = std::vector<int16_t>{};
    break;
  case LongVector::AsTypeOpType_AsUint:
    ExpectedVector = std::vector<uint32_t>{};
    break;
  case LongVector::AsTypeOpType_AsUint_SplitDouble:
    SpecialDefines = " -DFUNC_ASUINT_SPLITDOUBLE=1";
    ExpectedVector = std::vector<uint32_t>{};
    break;
  case LongVector::AsTypeOpType_AsUint16:
    ExpectedVector = std::vector<uint16_t>{};
    break;
  case LongVector::AsTypeOpType_AsDouble:
    ExpectedVector = std::vector<double>{};
    BasicOpType = LongVector::BasicOpType_Binary;
    break;
  default:
    VERIFY_FAIL("Invalid AsTypeOpType");
  }
}

template <typename DataTypeT>
void LongVector::TestConfigAsType<DataTypeT>::computeExpectedValues(
    const std::vector<DataTypeT> &InputVector1) {

  switch (OpType) {
  case AsTypeOpType_AsFloat16:
    fillExpectedVector<HLSLHalf_t>(
        ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asFloat16(InputVector1[Index]); });
    break;
  case AsTypeOpType_AsFloat:
    fillExpectedVector<float>(
        ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asFloat(InputVector1[Index]); });
    break;
  case AsTypeOpType_AsInt:
    fillExpectedVector<int32_t>(
        ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asInt(InputVector1[Index]); });
    break;
  case AsTypeOpType_AsInt16:
    fillExpectedVector<int16_t>(
        ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asInt16(InputVector1[Index]); });
    break;
  case AsTypeOpType_AsUint:
    fillExpectedVector<uint32_t>(
        ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asUint(InputVector1[Index]); });
    break;
  case AsTypeOpType_AsUint_SplitDouble: {
    // SplitDouble is a special case. We fill the first half of the expected
    // vector with the expected low bits of each input double and the second
    // half with the high bits of each input double. Doing things this way
    // helps keep the rest of the generic logic in the LongVector test code
    // simple.
    auto *TypedExpectedValues =
        std::get_if<std::vector<uint32_t>>(&ExpectedVector);
    VERIFY_IS_NOT_NULL(TypedExpectedValues,
                       L"Expected vector is not of the correct type.");
    TypedExpectedValues->resize(InputVector1.size() * 2);
    uint32_t LowBits, HighBits;
    const size_t InputSize = InputVector1.size();
    for (size_t Index = 0; Index < InputSize; ++Index) {
      splitDouble(InputVector1[Index], LowBits, HighBits);
      (*TypedExpectedValues)[Index] = LowBits;
      (*TypedExpectedValues)[Index + InputSize] = HighBits;
    }
    break;
  }
  case AsTypeOpType_AsUint16:
    fillExpectedVector<uint16_t>(
        ExpectedVector, InputVector1.size(),
        [&](size_t Index) { return asUint16(InputVector1[Index]); });
    break;
  default:
    LOG_ERROR_FMT_THROW(L"Unsupported AsType op: %ls", OpTypeName.c_str());
  }
}

template <typename DataTypeT>
void LongVector::TestConfigAsType<DataTypeT>::computeExpectedValues(
    const std::vector<DataTypeT> &InputVector1,
    const std::vector<DataTypeT> &InputVector2) {

  // AsTypeOpType_AsDouble is the only binary op type for AsType. The rest are
  // Unary ops.
  DXASSERT_NOMSG(OpType == LongVector::AsTypeOpType_AsDouble);

  fillExpectedVector<double>(
      ExpectedVector, InputVector1.size(), [&](size_t Index) {
        return asDouble(InputVector1[Index], InputVector2[Index]);
      });
}

template <typename DataTypeT>
std::string
LongVector::TestConfigAsType<DataTypeT>::getHLSLOutputTypeString() const {

  switch (OpType) {
  case LongVector::AsTypeOpType_AsFloat16:
    return LongVector::getHLSLTypeString<HLSLHalf_t>();
  case LongVector::AsTypeOpType_AsFloat:
    return LongVector::getHLSLTypeString<float>();
  case LongVector::AsTypeOpType_AsInt:
    return LongVector::getHLSLTypeString<int32_t>();
  case LongVector::AsTypeOpType_AsInt16:
    return LongVector::getHLSLTypeString<int16_t>();
  case LongVector::AsTypeOpType_AsUint:
    return LongVector::getHLSLTypeString<uint32_t>();
  case LongVector::AsTypeOpType_AsUint_SplitDouble:
    return LongVector::getHLSLTypeString<uint32_t>();
  case LongVector::AsTypeOpType_AsUint16:
    return LongVector::getHLSLTypeString<uint16_t>();
  case LongVector::AsTypeOpType_AsDouble:
    return LongVector::getHLSLTypeString<double>();
  default:
    LOG_ERROR_FMT_THROW(
        L"getHLSLOutputTypeString() called with an unsupported op type: %ls",
        OpTypeName.c_str());
    return std::string("UnknownType");
  }
}

// Override verifyOutput for AsTypeOpType as the output type for these ops
// doesn't match the input type of the config. Calls a private templated version
// of verifyOutput with the correct data type based on the op.
template <typename DataTypeT>
bool LongVector::TestConfigAsType<DataTypeT>::verifyOutput(
    const std::shared_ptr<st::ShaderOpTestResult> &TestResult) {

  switch (OpType) {
  case LongVector::AsTypeOpType_AsFloat:
    return TestConfig::verifyOutput<float>(TestResult);
  case LongVector::AsTypeOpType_AsFloat16:
    return TestConfig::verifyOutput<HLSLHalf_t>(TestResult);
  case LongVector::AsTypeOpType_AsInt:
    return TestConfig::verifyOutput<int32_t>(TestResult);
  case LongVector::AsTypeOpType_AsInt16:
    return TestConfig::verifyOutput<int16_t>(TestResult);
  case LongVector::AsTypeOpType_AsUint:
    return TestConfig::verifyOutput<uint32_t>(TestResult);
  case LongVector::AsTypeOpType_AsUint_SplitDouble:
    return TestConfig::verifyOutput<uint32_t>(TestResult);
  case LongVector::AsTypeOpType_AsUint16:
    return TestConfig::verifyOutput<uint16_t>(TestResult);
  case LongVector::AsTypeOpType_AsDouble:
    return TestConfig::verifyOutput<double>(TestResult);
  default:
    LOG_ERROR_FMT_THROW(
        L"verifyOutput() called with an unsupported AsTypeOpType: %ls",
        OpTypeName.c_str());
    return false;
  }
}

template <typename DataTypeT>
LongVector::TestConfigTrigonometric<DataTypeT>::TestConfigTrigonometric(
    const LongVector::OpTypeMetaData<LongVector::TrigonometricOpType> &OpTypeMd)
    : LongVector::TestConfig<DataTypeT>(OpTypeMd), OpType(OpTypeMd.OpType) {

  static_assert(
      isFloatingPointType<DataTypeT>(),
      "Trigonometric ops are only supported for floating point types.");

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
}

// computeExpectedValue Trigonometric
template <typename DataTypeT>
DataTypeT LongVector::TestConfigTrigonometric<DataTypeT>::computeExpectedValue(
    const DataTypeT &A) const {

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
    LOG_ERROR_FMT_THROW(L"Unknown TrigonometricOpType: %ls",
                        OpTypeName.c_str());
    return DataTypeT();
  }
}

template <typename DataTypeT>
LongVector::TestConfigUnary<DataTypeT>::TestConfigUnary(
    const LongVector::OpTypeMetaData<LongVector::UnaryOpType> &OpTypeMd)
    : LongVector::TestConfig<DataTypeT>(OpTypeMd), OpType(OpTypeMd.OpType) {

  BasicOpType = LongVector::BasicOpType_Unary;

  switch (OpType) {
  case LongVector::UnaryOpType_Initialize:
    SpecialDefines = " -DFUNC_INITIALIZE=1";
    break;
  default:
    VERIFY_FAIL("Invalid UnaryOpType");
  }
}

template <typename DataTypeT>
DataTypeT LongVector::TestConfigUnary<DataTypeT>::computeExpectedValue(
    const DataTypeT &A) const {
  if (OpType != LongVector::UnaryOpType_Initialize) {
    LOG_ERROR_FMT_THROW(L"computeExpectedValue(const DataTypeT &A, "
                        L"LongVector::UnaryOpType OpType) called on an "
                        L"unrecognized unary op: %ls",
                        OpTypeName.c_str());
  }

  return DataTypeT(A);
}

template <typename DataTypeT>
LongVector::TestConfigBinary<DataTypeT>::TestConfigBinary(
    const LongVector::OpTypeMetaData<LongVector::BinaryOpType> &OpTypeMd)
    : LongVector::TestConfig<DataTypeT>(OpTypeMd), OpType(OpTypeMd.OpType) {

  if (isFloatingPointType<DataTypeT>()) {
    Tolerance = 1;
    ValidationType = LongVector::ValidationType_Ulp;
  }

  switch (OpType) {
  case LongVector::BinaryOpType_ScalarAdd:
  case LongVector::BinaryOpType_ScalarMultiply:
  case LongVector::BinaryOpType_ScalarSubtract:
  case LongVector::BinaryOpType_ScalarDivide:
  case LongVector::BinaryOpType_ScalarModulus:
  case LongVector::BinaryOpType_ScalarMin:
  case LongVector::BinaryOpType_ScalarMax:
    BasicOpType = LongVector::BasicOpType_ScalarBinary;
    break;
  case LongVector::BinaryOpType_Multiply:
  case LongVector::BinaryOpType_Add:
  case LongVector::BinaryOpType_Subtract:
  case LongVector::BinaryOpType_Divide:
  case LongVector::BinaryOpType_Modulus:
  case LongVector::BinaryOpType_Min:
  case LongVector::BinaryOpType_Max:
    BasicOpType = LongVector::BasicOpType_Binary;
    break;
  default:
    VERIFY_FAIL("Invalid BinaryOpType");
  }
}

template <typename DataTypeT>
DataTypeT LongVector::TestConfigBinary<DataTypeT>::computeExpectedValue(
    const DataTypeT &A, const DataTypeT &B) const {
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
    LOG_ERROR_FMT_THROW(L"Unknown BinaryOpType: %ls", OpTypeName.c_str());
    return DataTypeT();
  }
}