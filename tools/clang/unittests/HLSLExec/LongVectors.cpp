#include "LongVectors.h"
#include "HlslExecTestUtils.h"
#include <iomanip>

namespace LongVector {

template <typename OpT, size_t Length>
const OpTypeMetaData<OpT> &
getOpType(const OpTypeMetaData<OpT> (&Values)[Length],
          const std::wstring &OpTypeString) {
  for (size_t I = 0; I < Length; I++) {
    if (Values[I].OpTypeString == OpTypeString)
      return Values[I];
  }

  LOG_ERROR_FMT_THROW(L"Invalid OpType string: %ls", OpTypeString.c_str());

  // We need to return something to satisfy the compiler. We can't annotate
  // LOG_ERROR_FMT_THROW with [[noreturn]] because the TAEF VERIFY_* macros that
  // it uses are re-mapped on Unix to not throw exceptions, so they naturally
  // return. If we hit this point it is a programmer error when implementing a
  // test. Specifically, an entry for this OpTypeString is missing in the
  // static OpTypeStringToOpMetaData array. Or something has been
  // corrupted. Test execution is invalid at this point. Usin std::abort() keeps
  // the compiler happy about no return path. And LOG_ERROR_FMT_THROW will still
  // provide a useful error message via gtest logging on Unix systems.
  std::abort();
}

// Helper to fill the test data from the shader buffer based on type. Convenient
// to be used when copying HLSL*_t types so we can use the underlying type.
template <typename T>
void fillLongVectorDataFromShaderBuffer(const MappedData &ShaderBuffer,
                                        std::vector<T> &TestData,
                                        size_t NumElements) {

  if constexpr (std::is_same_v<T, HLSLHalf_t>) {
    auto ShaderBufferPtr =
        static_cast<const DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      // HLSLHalf_t has a DirectX::PackedVector::HALF based constructor.
      TestData.push_back(ShaderBufferPtr[I]);
    return;
  }

  if constexpr (std::is_same_v<T, HLSLBool_t>) {
    auto ShaderBufferPtr = static_cast<const int32_t *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      // HLSLBool_t has a int32_t based constructor.
      TestData.push_back(ShaderBufferPtr[I]);
    return;
  }

  auto ShaderBufferPtr = static_cast<const T *>(ShaderBuffer.data());
  for (size_t I = 0; I < NumElements; I++)
    TestData.push_back(ShaderBufferPtr[I]);
  return;
}

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
  case ValidationType_Epsilon:
    return CompareHalfEpsilon(A.Val, B.Val, Tolerance);
  case ValidationType_Ulp:
    return CompareHalfULP(A.Val, B.Val, Tolerance);
  default:
    WEX::Logging::Log::Error(
        L"Invalid ValidationType. Expecting Epsilon or ULP.");
    return false;
  }
}

bool doValuesMatch(float A, float B, float Tolerance,
                   ValidationType ValidationType) {
  switch (ValidationType) {
  case ValidationType_Epsilon:
    return CompareFloatEpsilon(A, B, Tolerance);
  case ValidationType_Ulp: {
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

bool doValuesMatch(double A, double B, float Tolerance,
                   ValidationType ValidationType) {
  switch (ValidationType) {
  case ValidationType_Epsilon:
    return CompareDoubleEpsilon(A, B, Tolerance);
  case ValidationType_Ulp: {
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
      WEX::Logging::Log::Error(Wss.str().c_str());
    }
  }

  return false;
}

// A helper to create and fill the expected vector with computed values.
// Also helps us factor out the generic fill loop via a passed in ComputeFn.
template <typename T, typename ComputeFnT>
VariantVector generateExpectedVector(size_t Count, ComputeFnT ComputeFn) {

  VariantVector ExpectedVector = std::vector<T>{};
  auto *TypedExpectedValues = std::get_if<std::vector<T>>(&ExpectedVector);

  // A TestConfig may be reused for a different vector length. So this is a
  // good time to make sure we clear the expected vector.
  TypedExpectedValues->clear();

  for (size_t Index = 0; Index < Count; ++Index)
    TypedExpectedValues->push_back(ComputeFn(Index));

  return ExpectedVector;
}

template <typename T>
void logLongVector(const std::vector<T> &Values, const std::wstring &Name) {
  WEX::Logging::Log::Comment(
      WEX::Common::String().Format(L"LongVector Name: %s", Name.c_str()));

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

  WEX::Logging::Log::Comment(Wss.str().c_str());
}

template <typename T> std::string getHLSLTypeString() {
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

  LOG_ERROR_FMT_THROW(L"Unsupported type: %S", typeid(T).name());
  return "UnknownType";
}

// These are helper arrays to be used with the TableParameterHandler that parses
// the LongVectorOpTable.xml file for us.
static TableParameter UnaryOpParameters[] = {
    {L"DataType", TableParameter::STRING, true},
    {L"OpTypeEnum", TableParameter::STRING, true},
    {L"InputValueSetName1", TableParameter::STRING, false},
};

static TableParameter BinaryOpParameters[] = {
    {L"DataType", TableParameter::STRING, true},
    {L"OpTypeEnum", TableParameter::STRING, true},
    {L"InputValueSetName1", TableParameter::STRING, false},
    {L"InputValueSetName2", TableParameter::STRING, false},
    {L"ScalarInputFlags", TableParameter::STRING, false},
};

static TableParameter TernaryOpParameters[] = {
    {L"DataType", TableParameter::STRING, true},
    {L"OpTypeEnum", TableParameter::STRING, true},
    {L"InputValueSetName1", TableParameter::STRING, false},
    {L"InputValueSetName2", TableParameter::STRING, false},
    {L"InputValueSetName3", TableParameter::STRING, false},
    {L"ScalarInputFlags", TableParameter::STRING, false},
};

static TableParameter AsTypeOpParameters[] = {
    // DataTypeOut is determined at runtime based on the OpType.
    // For example...AsUint has an output type of uint32_t.
    {L"DataTypeIn", TableParameter::STRING, true},
    {L"OpTypeEnum", TableParameter::STRING, true},
    {L"InputValueSetName1", TableParameter::STRING, false},
    {L"InputValueSetName2", TableParameter::STRING, false},
    {L"ScalarInputFlags", TableParameter::STRING, false},
};

bool OpTest::classSetup() {
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

    WEX::TestExecution::RuntimeParameters::TryGetValue(L"VerboseLogging",
                                                       VerboseLogging);
    if (VerboseLogging)
      WEX::Logging::Log::Comment(L"Verbose logging is enabled for this test.");
    else
      WEX::Logging::Log::Comment(L"Verbose logging is disabled for this test.");
  }

  return true;
}

TEST_F(OpTest, trigonometricOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const size_t TableSize = sizeof(UnaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(UnaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpTypeMD = getTrigonometricOpType(OpTypeString);
  dispatchTrigonometricOpTestByDataType(OpTypeMD, DataType, Handler);
}

TEST_F(OpTest, unaryOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const size_t TableSize = sizeof(UnaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(UnaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpTypeMD = getUnaryOpType(OpTypeString);
  dispatchTestByDataType(OpTypeMD, DataType, Handler);
}

TEST_F(OpTest, asTypeOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const size_t TableSize = sizeof(AsTypeOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(AsTypeOpParameters, TableSize);

  std::wstring DataTypeIn(Handler.GetTableParamByName(L"DataTypeIn")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpTypeMD = getAsTypeOpType(OpTypeString);
  dispatchTestByDataType(OpTypeMD, DataTypeIn, Handler);
}

TEST_F(OpTest, unaryMathOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const int TableSize = sizeof(UnaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(UnaryOpParameters, TableSize);

  std::wstring DataTypeIn(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpTypeMD = getUnaryMathOpType(OpTypeString);
  dispatchUnaryMathOpTestByDataType(OpTypeMD, DataTypeIn, Handler);
}

TEST_F(OpTest, binaryMathOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  using namespace WEX::Common;

  const size_t TableSize = sizeof(BinaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(BinaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpTypeMD = getBinaryMathOpType(OpTypeString);

  std::wstring ScalarInputFlags(
      Handler.GetTableParamByName(L"ScalarInputFlags")->m_str);
  if (!ScalarInputFlags.empty())
    VERIFY_IS_TRUE(
        IsHexString(ScalarInputFlags.c_str(), &OpTypeMD.ScalarInputFlags),
        L"ScalarInputFlags must be a hex string if provided.");

  dispatchTestByDataType(OpTypeMD, DataType, Handler);
}

TEST_F(OpTest, ternaryMathOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const size_t TableSize = sizeof(TernaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(TernaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpTypeMD = getTernaryMathOpType(OpTypeString);

  std::wstring ScalarInputFlags(
      Handler.GetTableParamByName(L"ScalarInputFlags")->m_str);
  if (!ScalarInputFlags.empty())
    VERIFY_IS_TRUE(
        IsHexString(ScalarInputFlags.c_str(), &OpTypeMD.ScalarInputFlags),
        L"ScalarInputFlags must be a hex string if provided.");

  dispatchTestByDataType(OpTypeMD, DataType, Handler);
}

// Generic dispatch that dispatchs all DataTypes recognized in these tests
template <typename OpT>
void OpTest::dispatchTestByDataType(const OpTypeMetaData<OpT> &OpTypeMd,
                                    std::wstring DataType,
                                    TableParameterHandler &Handler) {
  switch (Hash_djb2a(DataType)) {
  case Hash_djb2a(L"bool"):
    dispatchTestByVectorLength<HLSLBool_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"int16"):
    dispatchTestByVectorLength<int16_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"int32"):
    dispatchTestByVectorLength<int32_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"int64"):
    dispatchTestByVectorLength<int64_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint16"):
    dispatchTestByVectorLength<uint16_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint32"):
    dispatchTestByVectorLength<uint32_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint64"):
    dispatchTestByVectorLength<uint64_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float16"):
    dispatchTestByVectorLength<HLSLHalf_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float32"):
    dispatchTestByVectorLength<float>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float64"):
    dispatchTestByVectorLength<double>(OpTypeMd, Handler);
    return;
  default:
    LOG_ERROR_FMT_THROW(L"Unrecognized DataType: %ls for OpType: %ls.",
                        DataType.c_str(), OpTypeMd.OpTypeString.c_str());
  }
}

// Unary math ops don't support HLSLBool_t. If we included a dispatcher for
// them by allowing the generic dispatchTestByDataType then we would get
// compile errors for a bunch of the templated std lib functions we call to
// compute unary math ops. This is easier and cleaner than guarding against in
// at that point.
void OpTest::dispatchUnaryMathOpTestByDataType(
    const OpTypeMetaData<UnaryMathOpType> &OpTypeMd, std::wstring DataType,
    TableParameterHandler &Handler) {

  switch (Hash_djb2a(DataType)) {
  case Hash_djb2a(L"int16"):
    dispatchTestByVectorLength<int16_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"int32"):
    dispatchTestByVectorLength<int32_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"int64"):
    dispatchTestByVectorLength<int64_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint16"):
    dispatchTestByVectorLength<uint16_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint32"):
    dispatchTestByVectorLength<uint32_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"uint64"):
    dispatchTestByVectorLength<uint64_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float16"):
    dispatchTestByVectorLength<HLSLHalf_t>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float32"):
    dispatchTestByVectorLength<float>(OpTypeMd, Handler);
    return;
  case Hash_djb2a(L"float64"):
    dispatchTestByVectorLength<double>(OpTypeMd, Handler);
    return;
  default:
    LOG_ERROR_FMT_THROW(L"Invalid UnaryMathOpType DataType: %ls.",
                        DataType.c_str());
  }
}

// Specialized dispatch for Trigonometric op tests (tan, sin, etc)
// Trig ops only support fp16, fp32, and fp64. So we don't want to
// to generate code paths for any other types. Emit a runtime error via
// LOG_ERROR_FMT_THROW if someone accidentally trys to add support for
// a different DataType.
void OpTest::dispatchTrigonometricOpTestByDataType(
    const OpTypeMetaData<TrigonometricOpType> &OpTypeMd, std::wstring DataType,
    TableParameterHandler &Handler) {

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

template <typename T, typename OpT>
void OpTest::dispatchTestByVectorLength(const OpTypeMetaData<OpT> &OpTypeMd,
                                        TableParameterHandler &Handler) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  auto TestConfig = makeTestConfig<T>(OpTypeMd);
  TestConfig->setVerboseLogging(VerboseLogging);
  auto OperandCount = TestConfig->getNumOperands();

  std::wstring Name = L"InputValueSetName";
  for (size_t I = 0; I < OperandCount; I++) {
    auto NameI = Name + std::to_wstring(I + 1);
    std::wstring InputValueSetName(
        Handler.GetTableParamByName(NameI.c_str())->m_str);
    if (!InputValueSetName.empty())
      TestConfig->setInputValueSetKey(InputValueSetName, I);
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
    testBaseMethod<T>(TestConfig);
  }
}

template <typename T>
void OpTest::testBaseMethod(std::unique_ptr<TestConfig<T>> &TestConfig) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

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

  TestInputs<T> Inputs = TestInputs<T>();
  TestConfig->fillInputs(Inputs);

  TestConfig->computeExpectedValues(Inputs);

  if (VerboseLogging) {
    logLongVector(Inputs.InputVector1, L"InputVector1");
    if (Inputs.InputVector2.has_value())
      logLongVector(Inputs.InputVector2.value(), L"InputVector2");
    if (Inputs.InputVector3.has_value())
      logLongVector(Inputs.InputVector3.value(), L"InputVector3");
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
  readHlslDataIntoNewStream(L"ShaderOpArith.xml", &TestXML, DxilDllLoader);

  // RunShaderOpTest is a helper function that handles resource creation
  // and setup. It also handles the shader compilation and execution. It takes a
  // callback that is called when the shader is compiled, but before it is
  // executed.
  std::shared_ptr<st::ShaderOpTestResult> TestResult = st::RunShaderOpTest(
      D3DDevice, DxilDllLoader, TestXML, ShaderName,
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

        // Process the callback for the InputVector1 resource.
        if (0 == _stricmp(Name, "InputVector1")) {
          fillShaderBufferFromLongVectorData(ShaderData, Inputs.InputVector1);
          return;
        }

        // Process the callback for the InputVector2 resource.
        if (0 == _stricmp(Name, "InputVector2")) {
          if (Inputs.InputVector2.has_value())
            fillShaderBufferFromLongVectorData(ShaderData,
                                               Inputs.InputVector2.value());
          return;
        }

        // Process the callback for the InputVector3 resource.
        if (0 == _stricmp(Name, "InputVector3")) {
          if (Inputs.InputVector3.has_value())
            fillShaderBufferFromLongVectorData(ShaderData,
                                               Inputs.InputVector3.value());
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
    auto ShaderBufferPtr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      ShaderBufferPtr[I] = TestData[I].Val;
    return;
  }

  if constexpr (std::is_same_v<T, HLSLBool_t>) {
    auto ShaderBufferPtr = reinterpret_cast<int32_t *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      ShaderBufferPtr[I] = TestData[I].Val;
    return;
  }

  auto ShaderBufferPtr = reinterpret_cast<T *>(ShaderBuffer.data());
  for (size_t I = 0; I < NumElements; I++)
    ShaderBufferPtr[I] = TestData[I];
  return;
}

// Returns the compiler options string to be used for the shader compilation.
// Reference ShaderOpArith.xml and the 'LongVectorOp' shader source to see how
// the defines are used in the shader code.
template <typename T>
std::string TestConfig<T>::getCompilerOptionsString() const {

  std::stringstream CompilerOptions("");

  if (is16BitType<T>())
    CompilerOptions << " -enable-16bit-types";

  CompilerOptions << " -DTYPE=" << getHLSLInputTypeString();
  CompilerOptions << " -DNUM=" << LengthToTest;

  CompilerOptions << " -DOPERATOR=";
  if (Operator)
    CompilerOptions << *Operator;

  CompilerOptions << " -DFUNC=";
  if (Intrinsic)
    CompilerOptions << *Intrinsic;

  // For most of the ops this string is std::nullopt.
  if (SpecialDefines)
    CompilerOptions << " " << *SpecialDefines;

  CompilerOptions << " -DOUT_TYPE=" << getHLSLOutputTypeString();

  CompilerOptions << " -DBASIC_OP_TYPE=" << getBasicOpTypeHexString();

  CompilerOptions << " -DOPERAND_IS_SCALAR_FLAGS=";
  CompilerOptions << "0x" << std::hex << ScalarInputFlags;

  return CompilerOptions.str();
}

template <typename T>
std::string TestConfig<T>::getBasicOpTypeHexString() const {

  if (BasicOpType == BasicOpType_Unary)
    return "0x1";
  if (BasicOpType == BasicOpType_Binary)
    return "0x2";
  if (BasicOpType == BasicOpType_Ternary)
    return "0x3";

  LOG_ERROR_FMT_THROW(L"Invalid BasicOpType: %d",
                      static_cast<int>(BasicOpType));
  return "0x0";
}

template <typename T> size_t TestConfig<T>::getNumOperands() const {
  if (BasicOpType == BasicOpType_Unary)
    return 1;

  if (BasicOpType == BasicOpType_Binary)
    return 2;

  if (BasicOpType == BasicOpType_Ternary)
    return 3;

  LOG_ERROR_FMT_THROW(L"Invalid BasicOpType: %d",
                      static_cast<int>(BasicOpType));
  return 0;
}

template <typename T>
std::vector<T> TestConfig<T>::getInputValueSet(size_t ValueSetIndex) const {
  if (BasicOpType == BasicOpType_Unary && ValueSetIndex == 0)
    return getInputValueSetByKey<T>(InputValueSetKeys[ValueSetIndex]);

  if (BasicOpType == BasicOpType_Binary && ValueSetIndex <= 1)
    return getInputValueSetByKey<T>(InputValueSetKeys[ValueSetIndex]);

  if (BasicOpType == BasicOpType_Ternary && ValueSetIndex <= 2)
    return getInputValueSetByKey<T>(InputValueSetKeys[ValueSetIndex]);

  LOG_ERROR_FMT_THROW(L"Invalid ValueSetIndex: %d for OpType: %ls",
                      ValueSetIndex, OpTypeName.c_str());
  return std::vector<T>();
}

template <typename T>
std::string TestConfig<T>::getHLSLOutputTypeString() const {
  // std::visit allows us to dispatch a call to getHLSLTypeString<T>() with the
  // the current underlying element type of ExpectedVector.
  return std::visit(
      [](const auto &Vec) {
        using ElementType = typename std::decay_t<decltype(Vec)>::value_type;
        return getHLSLTypeString<ElementType>();
      },
      ExpectedVector);
}

template <typename T>
bool TestConfig<T>::verifyOutput(
    const std::shared_ptr<st::ShaderOpTestResult> &TestResult) {

  // std::visit allows us to dispatch a call to the private version of
  // verifyOutput using a std::vector<T> that matches the type currently held in
  // ExpectedVector. This works because ExpectedVector is a std::variant of
  // vector types, and the lambda receives the active type at runtime. It's
  // important that the TestConfig instance has correctly assigned the expected
  // output type to ExpectedVector. By default, this is std::vector<T>,
  // but ops like AsTypeOpType must override it. For example,
  // AsTypeOpType_AsFloat16 sets ExpectedVector to std::vector<HLSLHalf_t>.
  return std::visit(
      [this, &TestResult](const auto &Vec) {
        using ElementType = typename std::decay_t<decltype(Vec)>::value_type;
        return this->verifyOutput<ElementType>(TestResult, Vec);
      },
      ExpectedVector);
}

// Private version of verifyOutput. Called by the public version of verifyOutput
// which resolves OutT based on the ExpectedVector type. Most intrinsics will
// have an OutT that matches the input type being tested (which is T). But some,
// such as the 'AsType' ops, i.e 'AsUint' have an OutT that doesn't match T.
template <typename T>    // Primary template from TestConfig
template <typename OutT> // Secondary template for verifyOutput
bool TestConfig<T>::verifyOutput(
    const std::shared_ptr<st::ShaderOpTestResult> &TestResult,
    const std::vector<OutT> &ExpectedVector) {

  WEX::Logging::Log::Comment(WEX::Common::String().Format(
      L"verifyOutput with OpType: %ls ExpectedVector<%S>", OpTypeName.c_str(),
      typeid(OutT).name()));

  DXASSERT(!ExpectedVector.empty(),
           "Programmer Error: ExpectedVector is empty.");

  MappedData ShaderOutData;
  TestResult->Test->GetReadBackData("OutputVector", &ShaderOutData);

  const size_t OutputVectorSize = ExpectedVector.size();

  std::vector<OutT> ActualValues;
  fillLongVectorDataFromShaderBuffer(ShaderOutData, ActualValues,
                                     OutputVectorSize);

  return doVectorsMatch(ActualValues, ExpectedVector, Tolerance, ValidationType,
                        VerboseLogging);
}

// Generic fillInput. Fill the inputs based on the OpType and the
// ScalarInputFlags.
template <typename T>
void TestConfig<T>::fillInputs(TestInputs<T> &Inputs) const {

  auto fillVecFromValueSet = [this](std::vector<T> &Vec, size_t ValueSetIndex,
                                    size_t Count) {
    std::vector<T> ValueSet = getInputValueSet(ValueSetIndex);
    for (size_t Index = 0; Index < Count; Index++)
      Vec.push_back(ValueSet[Index % ValueSet.size()]);
  };

  size_t ValueSetIndex = 0;

  fillVecFromValueSet(Inputs.InputVector1, ValueSetIndex++, LengthToTest);

  if (BasicOpType == BasicOpType_Unary)
    return;

  DXASSERT_NOMSG(BasicOpType == BasicOpType_Binary ||
                 BasicOpType == BasicOpType_Ternary);

  const size_t Input2Length =
      (ScalarInputFlags & SCALAR_INPUT_FLAGS_OPERAND_2_IS_SCALAR)
          ? 1
          : LengthToTest;
  Inputs.InputVector2 = std::vector<T>();
  fillVecFromValueSet(*Inputs.InputVector2, ValueSetIndex++, Input2Length);

  if (BasicOpType == BasicOpType_Binary)
    return;

  DXASSERT_NOMSG(BasicOpType == BasicOpType_Ternary);

  const size_t Input3Length =
      (ScalarInputFlags & SCALAR_INPUT_FLAGS_OPERAND_3_IS_SCALAR)
          ? 1
          : LengthToTest;
  Inputs.InputVector3 = std::vector<T>();
  fillVecFromValueSet(*Inputs.InputVector3, ValueSetIndex++, Input3Length);
}

template <typename T>
AsTypeOpTestConfig<T>::AsTypeOpTestConfig(
    const OpTypeMetaData<AsTypeOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  BasicOpType = BasicOpType_Unary;

  switch (OpType) {
  case AsTypeOpType_AsFloat16: {
    auto ComputeFunc = [this](const T &Val) { return asFloat16(Val); };
    InitUnaryOpValueComputer<HLSLHalf_t>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsFloat: {
    auto ComputeFunc = [this](const T &Val) { return asFloat(Val); };
    InitUnaryOpValueComputer<float>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsInt: {
    auto ComputeFunc = [this](const T &Val) { return asInt(Val); };
    InitUnaryOpValueComputer<int32_t>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsInt16: {
    auto ComputeFunc = [this](const T &Val) { return asInt16(Val); };
    InitUnaryOpValueComputer<int16_t>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsUint: {
    auto ComputeFunc = [this](const T &Val) { return asUint(Val); };
    InitUnaryOpValueComputer<uint32_t>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsUint_SplitDouble: {
    SpecialDefines = " -DFUNC_ASUINT_SPLITDOUBLE=1";
    break;
  }
  case AsTypeOpType_AsUint16: {
    auto ComputeFunc = [this](const T &Val) { return asUint16(Val); };
    InitUnaryOpValueComputer<uint16_t>(ComputeFunc);
    break;
  }
  case AsTypeOpType_AsDouble: {
    BasicOpType = BasicOpType_Binary;
    auto ComputeFunc = [this](const T &A, const T &B) {
      return asDouble(A, B);
    };
    InitBinaryOpValueComputer<double>(ComputeFunc);
    break;
  }
  default:
    LOG_ERROR_FMT_THROW(L"Unsupported AsTypeOpType: %ls", OpTypeName.c_str());
  }
}

template <typename T>
void TestConfig<T>::computeExpectedValues(const TestInputs<T> &Inputs) {

  // Either a ExpectedValueComputer member should be set or the deriving class
  // should have overridden computeExpectedValues.
  if (!ExpectedValueComputer)
    LOG_ERROR_FMT_THROW(
        L"Programmer Error: ExpectedValueComputer is not set for OpType: %ls.",
        OpTypeName.c_str());

  ExpectedVector = ExpectedValueComputer->computeExpectedValues(Inputs);
}

template <typename T>
void AsTypeOpTestConfig<T>::computeExpectedValues(const TestInputs<T> &Inputs) {

  if (BasicOpType != BasicOpType_Unary && BasicOpType != BasicOpType_Binary)
    LOG_ERROR_FMT_THROW(L"Programmer Error: computeExpectedValue called with "
                        L"unexpected BasicOpType: %d",
                        static_cast<int>(BasicOpType));

  if (ExpectedValueComputer)
    ExpectedVector = ExpectedValueComputer->computeExpectedValues(Inputs);
  else
    // Only SplitDouble has special handling. All other ops will have an
    // ExpectedValueComputer set.
    computeExpectedValues_SplitDouble(Inputs.InputVector1);
}

template <typename T>
void AsTypeOpTestConfig<T>::computeExpectedValues_SplitDouble(
    const std::vector<T> &InputVector) {

  DXASSERT_NOMSG(OpType == AsTypeOpType_AsUint_SplitDouble);

  // SplitDouble is a special case. We fill the first half of the expected
  // vector with the expected low bits of each input double and the second
  // half with the high bits of each input double. Doing things this way
  // helps keep the rest of the generic logic in the LongVector test code
  // simple.
  ExpectedVector = std::vector<uint32_t>{};
  auto *TypedExpectedValues =
      std::get_if<std::vector<uint32_t>>(&ExpectedVector);
  TypedExpectedValues->resize(InputVector.size() * 2);

  uint32_t LowBits, HighBits;
  const size_t InputSize = InputVector.size();

  for (size_t Index = 0; Index < InputSize; ++Index) {
    splitDouble(InputVector[Index], LowBits, HighBits);
    (*TypedExpectedValues)[Index] = LowBits;
    (*TypedExpectedValues)[Index + InputSize] = HighBits;
  }
}

template <typename T>
TrigonometricOpTestConfig<T>::TrigonometricOpTestConfig(
    const OpTypeMetaData<TrigonometricOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  static_assert(
      isFloatingPointType<T>(),
      "Trigonometric ops are only supported for floating point types.");

  BasicOpType = BasicOpType_Unary;

  // All trigonometric ops are floating point types.
  // These trig functions are defined to have a max absolute error of 0.0008
  // as per the D3D functional specs. An example with this spec for sin and
  // cos is available here:
  // https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#22.10.20
  ValidationType = ValidationType_Epsilon;
  if (std::is_same_v<T, HLSLHalf_t>)
    Tolerance = 0.0010f;
  else if (std::is_same_v<T, float>)
    Tolerance = 0.0008f;

  auto ComputeFunc = [this](const T &A) {
    return this->computeExpectedValue(A);
  };
  InitUnaryOpValueComputer<T>(ComputeFunc);
}

// computeExpectedValue Trigonometric
template <typename T>
T TrigonometricOpTestConfig<T>::computeExpectedValue(const T &A) const {

  switch (OpType) {
  case TrigonometricOpType_Acos:
    return std::acos(A);
  case TrigonometricOpType_Asin:
    return std::asin(A);
  case TrigonometricOpType_Atan:
    return std::atan(A);
  case TrigonometricOpType_Cos:
    return std::cos(A);
  case TrigonometricOpType_Cosh:
    return std::cosh(A);
  case TrigonometricOpType_Sin:
    return std::sin(A);
  case TrigonometricOpType_Sinh:
    return std::sinh(A);
  case TrigonometricOpType_Tan:
    return std::tan(A);
  case TrigonometricOpType_Tanh:
    return std::tanh(A);
  default:
    LOG_ERROR_FMT_THROW(L"Unknown TrigonometricOpType: %ls",
                        OpTypeName.c_str());
    return T();
  }
}

template <typename T>
UnaryOpTestConfig<T>::UnaryOpTestConfig(
    const OpTypeMetaData<UnaryOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  BasicOpType = BasicOpType_Unary;

  switch (OpType) {
  case UnaryOpType_Initialize:
    SpecialDefines = " -DFUNC_INITIALIZE=1";
    break;
  default:
    LOG_ERROR_FMT_THROW(L"Unsupported UnaryOpType: %ls", OpTypeName.c_str());
  }

  auto ComputeFunc = [this](const T &A) {
    return this->computeExpectedValue(A);
  };
  InitUnaryOpValueComputer<T>(ComputeFunc);
}

template <typename T>
T UnaryOpTestConfig<T>::computeExpectedValue(const T &A) const {
  if (OpType != UnaryOpType_Initialize) {
    LOG_ERROR_FMT_THROW(L"computeExpectedValue(const T &A, "
                        L"UnaryOpType OpType) called on an "
                        L"unrecognized unary op: %ls",
                        OpTypeName.c_str());
    return T();
  }

  return T(A);
}

template <typename T>
UnaryMathOpTestConfig<T>::UnaryMathOpTestConfig(
    const OpTypeMetaData<UnaryMathOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  BasicOpType = BasicOpType_Unary;

  if (isFloatingPointType<T>()) {
    Tolerance = 1;
    ValidationType = ValidationType_Ulp;
  }

  switch (OpType) {
  case (UnaryMathOpType_Sign): {
    // Sign has overridden special logic.
    auto ComputeFunc = [this](const T &A) { return this->sign(A); };
    InitUnaryOpValueComputer<int32_t>(ComputeFunc);
    break;
  }
  case (UnaryMathOpType_Frexp):
    // Don't initialize a ValueComputer, Frexp has special logic for handling
    // its output
    SpecialDefines = " -DFUNC_FREXP=1";
    break;
  default: {
    auto ComputeFunc = [this](const T &A) {
      return this->computeExpectedValue(A);
    };
    InitUnaryOpValueComputer<T>(ComputeFunc);
  }
  }
}

template <typename T>
void UnaryMathOpTestConfig<T>::computeExpectedValues(
    const TestInputs<T> &Inputs) {

  // Base case
  if (ExpectedValueComputer) {
    ExpectedVector = ExpectedValueComputer->computeExpectedValues(Inputs);
    return;
  }

  computeExpectedValues_Frexp(Inputs.InputVector1);
}

// Frexp has a return value as well as an output paramater. So we handle it
// with special logic. Frexp is only supported for fp32 values.
template <typename T>
void UnaryMathOpTestConfig<T>::computeExpectedValues_Frexp(
    const std::vector<T> &InputVector) {

  DXASSERT_NOMSG(OpType == UnaryMathOpType_Frexp);

  ExpectedVector = std::vector<float>{};
  auto *TypedExpectedValues = std::get_if<std::vector<float>>(&ExpectedVector);

  // Expected values size is doubled. In the first half we store the Mantissas
  // and in the second half we store the Exponents. This way we can leverage the
  // existing logic which verify expected values in a single vector. We just
  // need to make sure that we organize the output in the same way in the shader
  // and when we read it back.
  const size_t InputSize = InputVector.size();
  TypedExpectedValues->resize(InputSize * 2);
  float Exp = 0;
  float Man = 0;

  for (size_t Index = 0; Index < InputSize; ++Index) {
    Man = frexp(InputVector[Index], &Exp);
    (*TypedExpectedValues)[Index] = Man;
    (*TypedExpectedValues)[Index + InputSize] = Exp;
  }
}

template <typename T>
T UnaryMathOpTestConfig<T>::computeExpectedValue(const T &A) const {

  if constexpr (std::is_integral<T>::value) {
    // Abs and Sign are the only UnaryMathOps thats support integral types.
    // Sign always returns int32 values, so its handled elsewhere.
    DXASSERT_NOMSG(OpType == UnaryMathOpType_Abs);
    return abs(A);
  }

  if constexpr (!isFloatingPointType<T>()) {
    LOG_ERROR_FMT_THROW(L"Programmer error: UnaryMathOpType OpType: %ls only "
                        L"supports floating point types",
                        OpTypeName.c_str());
    return T();
  }

  // Most of the std math functions here are only defined for floating point
  // types. If we don't use a mechanism to ensure that we're only using floating
  // point types then the compiler will complain about implicit conversions.
  if constexpr (isFloatingPointType<T>()) {
    // A bunch of the std match functions here are  wrapped in () to avoid
    // collisions with the macro defitions for various functions in windows.h
    switch (OpType) {
    case UnaryMathOpType_Abs:
      return abs(A);
    case UnaryMathOpType_Ceil:
      return (std::ceil)(A);
    case UnaryMathOpType_Floor:
      // float only
      return (std::floor)(A);
    case UnaryMathOpType_Trunc:
      // float only
      return (std::trunc)(A);
    case UnaryMathOpType_Round:
      // float only
      return (std::round)(A);
    case UnaryMathOpType_Frac:
      // std::frac is not a standard C++ function, but we can implement it as
      return A - T((std::floor)(A));
    case UnaryMathOpType_Sqrt:
      return (std::sqrt)(A);
    case UnaryMathOpType_Rsqrt:
      // std::rsqrt is not a standard C++ function, but we can implement it as
      return T(1.0) / T((std::sqrt)(A));
    case UnaryMathOpType_Exp:
      return (std::exp)(A);
    case UnaryMathOpType_Exp2:
      return (std::exp2)(A);
    case UnaryMathOpType_Log:
      return (std::log)(A);
    case UnaryMathOpType_Log2:
      return (std::log2)(A);
    case UnaryMathOpType_Log10:
      return (std::log10)(A);
    case UnaryMathOpType_Rcp:
      // std::.rcp is not a standard C++ function, but we can implement it as
      return T(1.0) / A;
    default:
      LOG_ERROR_FMT_THROW(L"computeExpectedValue(const T &A)"
                          L"called on an unrecognized unary math op: %ls",
                          OpTypeName.c_str());
      return T();
    }
  }
}

template <typename T>
BinaryMathOpTestConfig<T>::BinaryMathOpTestConfig(
    const OpTypeMetaData<BinaryMathOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  if (isFloatingPointType<T>()) {
    Tolerance = 1;
    ValidationType = ValidationType_Ulp;
  }

  BasicOpType = BasicOpType_Binary;

  auto ComputeFunc = [this](const T &A, const T &B) {
    return this->computeExpectedValue(A, B);
  };
  InitBinaryOpValueComputer<T>(ComputeFunc);
}

template <typename T>
T BinaryMathOpTestConfig<T>::computeExpectedValue(const T &A,
                                                  const T &B) const {

  switch (OpType) {
  case BinaryMathOpType_Multiply:
    return A * B;
  case BinaryMathOpType_Add:
    return A + B;
  case BinaryMathOpType_Subtract:
    return A - B;
  case BinaryMathOpType_Divide:
    return A / B;
  case BinaryMathOpType_Modulus:
    return mod(A, B);
  case BinaryMathOpType_Min:
    // std::max and std::min are wrapped in () to avoid collisions with the //
    // macro defintions for min and max in windows.h
    return (std::min)(A, B);
  case BinaryMathOpType_Max:
    return (std::max)(A, B);
  case BinaryMathOpType_Ldexp:
    return ldexp(A, B);
  default:
    LOG_ERROR_FMT_THROW(L"Unknown BinaryMathOpType: %ls", OpTypeName.c_str());
    return T();
  }
}

template <typename T>
TernaryMathOpTestConfig<T>::TernaryMathOpTestConfig(
    const OpTypeMetaData<TernaryMathOpType> &OpTypeMd)
    : TestConfig<T>(OpTypeMd), OpType(OpTypeMd.OpType) {

  if (isFloatingPointType<T>()) {
    Tolerance = 1;
    ValidationType = ValidationType_Ulp;
  }

  BasicOpType = BasicOpType_Ternary;

  switch (OpType) {
  case TernaryMathOpType_Fma:
  case TernaryMathOpType_Mad:
  case TernaryMathOpType_SmoothStep:
    break;
  default:
    LOG_ERROR_FMT_THROW(L"Invalid TernaryMathOpType: %ls", OpTypeName.c_str());
  }

  auto ComputeFunc = [this](const T &A, const T &B, const T &C) {
    return this->computeExpectedValue(A, B, C);
  };
  InitTernaryOpValueComputer<T>(ComputeFunc);
}

}; // namespace LongVector
