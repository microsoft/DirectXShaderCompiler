#include "LongVectors.h"
#include "HlslExecTestUtils.h"
#include <iomanip>

LongVector::BinaryOpType
LongVector::getBinaryOpType(const std::wstring &OpTypeString) {
  return getLongVectorOpType<LongVector::BinaryOpType>(
      binaryOpTypeStringToEnumMap, OpTypeString,
      std::size(binaryOpTypeStringToEnumMap));
}

LongVector::UnaryOpType
LongVector::getUnaryOpType(const std::wstring &OpTypeString) {
  return getLongVectorOpType<LongVector::UnaryOpType>(
      unaryOpTypeStringToEnumMap, OpTypeString,
      std::size(unaryOpTypeStringToEnumMap));
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

  auto OpType = LongVector::getBinaryOpType(OpTypeString);
  dispatchTestByDataType(OpType, DataType, Handler);
}

TEST_F(LongVector::OpTest, unaryOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const int TableSize = sizeof(UnaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(UnaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpType = LongVector::getUnaryOpType(OpTypeString);
  dispatchTestByDataType(OpType, DataType, Handler);
}

template <typename LongVectorOpTypeT>
void LongVector::OpTest::dispatchTestByDataType(
    LongVectorOpTypeT OpType, std::wstring DataType,
    TableParameterHandler &Handler) {
  using namespace WEX::Common;

  if (DataType == L"int16")
    dispatchTestByVectorSize<int16_t>(OpType, Handler);
  else if (DataType == L"int32")
    dispatchTestByVectorSize<int32_t>(OpType, Handler);
  else if (DataType == L"int64")
    dispatchTestByVectorSize<int64_t>(OpType, Handler);
  else if (DataType == L"uint16")
    dispatchTestByVectorSize<uint16_t>(OpType, Handler);
  else if (DataType == L"uint32")
    dispatchTestByVectorSize<uint32_t>(OpType, Handler);
  else if (DataType == L"uint64")
    dispatchTestByVectorSize<uint64_t>(OpType, Handler);
  else if (DataType == L"float32")
    dispatchTestByVectorSize<float>(OpType, Handler);
  else if (DataType == L"float64")
    dispatchTestByVectorSize<double>(OpType, Handler);
  else
    VERIFY_FAIL(
        String().Format(L"DataType: %s is not recognized.", DataType.c_str()));
}

template <typename DataTypeT, typename LongVectorOpTypeT>
void LongVector::OpTest::dispatchTestByVectorSize(
    LongVectorOpTypeT opType, TableParameterHandler &Handler) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  LongVector::TestConfig<DataTypeT, LongVectorOpTypeT> TestConfig(opType);

  // InputValueSetName1 is optional. So the string may be empty. An empty
  // string will result in the default value set for this DataType being used.
  std::wstring InputValueSet1(
      Handler.GetTableParamByName(L"InputValueSetName1")->m_str);
  if (!InputValueSet1.empty())
    TestConfig.setInputValueSet1(InputValueSet1);

  // InputValueSetName2 is optional. So the string may be empty. An empty
  // string will result in the default value set for this DataType being used.
  if (TestConfig.isBinaryOp()) {
    std::wstring InputValueSet2(
        Handler.GetTableParamByName(L"InputValueSetName2")->m_str);
    if (!InputValueSet2.empty())
      TestConfig.setInputValueSet2(InputValueSet2);
  }

  std::vector<size_t> InputVectorSizes = {3, 4, 5, 16, 17, 35, 100, 256, 1024};
  for (auto SizeToTest : InputVectorSizes) {
    testBaseMethod<DataTypeT, LongVectorOpTypeT>(TestConfig, SizeToTest);
  }
}

template <typename DataTypeT, typename LongVectorOpTypeT>
void LongVector::OpTest::testBaseMethod(
    LongVector::TestConfig<DataTypeT, LongVectorOpTypeT> &TestConfig,
    size_t VectorSizeToTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  hlsl_test::LogCommentFmt(L"Running LongVectorOpTestBase<%S, %zu>",
                           typeid(DataTypeT).name(), VectorSizeToTest);

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
  InputVector1.reserve(VectorSizeToTest);
  std::vector<DataTypeT> InputVector2; // May be unused, but must be defined.
  InputVector2.reserve(VectorSizeToTest);
  std::vector<DataTypeT> ScalarInput; // May be unused, but must be defined.
  const bool IsVectorBinaryOp =
      TestConfig.isBinaryOp() && !TestConfig.isScalarOp();

  std::vector<DataTypeT> InputVector1ValueSet = TestConfig.getInputValueSet1();
  std::vector<DataTypeT> InputVector2ValueSet =
      TestConfig.isBinaryOp() ? TestConfig.getInputValueSet2()
                              : std::vector<DataTypeT>();

  if (TestConfig.isScalarOp())
    // Scalar ops are always binary ops. So InputVector2ValueSet is initialized
    // with values above.
    ScalarInput.push_back(InputVector2ValueSet[0]);

  // Fill the input vectors with values from the value set. Repeat the values
  // when we reach the end of the value set.
  for (size_t Index = 0; Index < VectorSizeToTest; Index++) {
    InputVector1.push_back(
        InputVector1ValueSet[Index % InputVector1ValueSet.size()]);

    if (IsVectorBinaryOp)
      InputVector2.push_back(
          InputVector2ValueSet[Index % InputVector2ValueSet.size()]);
  }

  std::vector<DataTypeT> ExpectedVector;
  ExpectedVector.reserve(VectorSizeToTest);
  if (IsVectorBinaryOp)
    ExpectedVector =
        computeExpectedValues(InputVector1, InputVector2, TestConfig);
  else if (TestConfig.isScalarOp())
    ExpectedVector =
        computeExpectedValues(InputVector1, ScalarInput[0], TestConfig);
  else // Must be a unary op
    ExpectedVector = computeExpectedValues(InputVector1, TestConfig);

  if (LogInputs) {
    logLongVector<DataTypeT>(InputVector1, L"InputVector1");

    if (IsVectorBinaryOp)
      logLongVector<DataTypeT>(InputVector2, L"InputVector2");
    else if (TestConfig.isScalarOp())
      logLongVector<DataTypeT>(ScalarInput, L"ScalarInput");
  }

  // We have to construct the string outside of the lambda. Otherwise it's
  // cleaned up when the lambda finishes executing but before the shader runs.
  std::string CompilerOptionsString =
      TestConfig.getCompilerOptionsString(VectorSizeToTest);

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
          if (TestConfig.isScalarOp())
            fillShaderBufferFromLongVectorData<DataTypeT>(ShaderData,
                                                          ScalarInput);
          return;
        }

        // Process the callback for the InputVector1 resource.
        if (0 == _stricmp(Name, "InputVector1")) {
          fillShaderBufferFromLongVectorData<DataTypeT>(ShaderData,
                                                        InputVector1);
          return;
        }

        // Process the callback for the InputVector2 resource.
        if (0 == _stricmp(Name, "InputVector2")) {
          if (IsVectorBinaryOp)
            fillShaderBufferFromLongVectorData<DataTypeT>(ShaderData,
                                                          InputVector2);

          return;
        }

        LOG_ERROR_FMT_THROW(
            L"RunShaderOpTest CallBack. Unexpected Resource Name: %S", Name);
      });

  // Map the data from GPU to CPU memory so we can verify our expectations.
  MappedData ShaderOutData;
  TestResult->Test->GetReadBackData("OutputVector", &ShaderOutData);

  std::vector<DataTypeT> OutputVector;
  fillLongVectorDataFromShaderBuffer<DataTypeT>(ShaderOutData, OutputVector,
                                                VectorSizeToTest);

  VERIFY_SUCCEEDED(doVectorsMatch<DataTypeT>(OutputVector, ExpectedVector,
                                             TestConfig.getTolerance(),
                                             TestConfig.getValidationType()));
}
