#include "LongVectors.h"
#include <iomanip>

#include "HlslExecTestUtils.h" // TODO: Can remove?

LongVector::BinaryOpType
LongVector::GetBinaryOpType(const std::wstring &OpTypeString) {
  return GetLongVectorOpType<LongVector::BinaryOpType>(
      BinaryOpTypeStringToEnumMap, OpTypeString,
      std::size(BinaryOpTypeStringToEnumMap));
}

LongVector::UnaryOpType
LongVector::GetUnaryOpType(const std::wstring &OpTypeString) {
  return GetLongVectorOpType<LongVector::UnaryOpType>(
      UnaryOpTypeStringToEnumMap, OpTypeString,
      std::size(UnaryOpTypeStringToEnumMap));
}

LongVector::TrigonometricOpType
LongVector::GetTrigonometricOpType(const std::wstring &OpTypeString) {
  return GetLongVectorOpType<LongVector::TrigonometricOpType>(
      TrigonometricOpTypeStringToEnumMap, OpTypeString,
      std::size(TrigonometricOpTypeStringToEnumMap));
}

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
    {L"InputArgsName", TableParameter::STRING, false},
};

bool LongVector::OpTest::ClassSetup() {
  // Run this only once.
  if (!m_Initialized) {
    m_Initialized = true;

    HMODULE Runtime = LoadLibraryW(L"d3d12.dll");
    if (Runtime == NULL)
      return false;
    // Do not: FreeLibrary(hRuntime);
    // If we actually free the library, it defeats the purpose of
    // EnableAgilitySDK and EnableExperimentalMode.

    HRESULT HR;
    HR = EnableAgilitySDK(Runtime);
    if (FAILED(HR)) {
      hlsl_test::LogCommentFmt(L"Unable to enable Agility SDK - 0x%08x.", HR);
    } else if (HR == S_FALSE) {
      hlsl_test::LogCommentFmt(L"Agility SDK not enabled.");
    } else {
      hlsl_test::LogCommentFmt(L"Agility SDK enabled.");
    }

    HR = EnableExperimentalMode(Runtime);
    if (FAILED(HR)) {
      hlsl_test::LogCommentFmt(
          L"Unable to enable shader experimental mode - 0x%08x.", HR);
    } else if (HR == S_FALSE) {
      hlsl_test::LogCommentFmt(L"Experimental mode not enabled.");
    } else {
      hlsl_test::LogCommentFmt(L"Experimental mode enabled.");
    }

    HR = EnableDebugLayer();
    if (FAILED(HR)) {
      hlsl_test::LogCommentFmt(L"Unable to enable debug layer - 0x%08x.", HR);
    } else if (HR == S_FALSE) {
      hlsl_test::LogCommentFmt(L"Debug layer not enabled.");
    } else {
      hlsl_test::LogCommentFmt(L"Debug layer enabled.");
    }
  }

  return true;
}

TEST_F(LongVector::OpTest, BinaryOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  using namespace WEX::Common;

  const int TableSize = sizeof(BinaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(BinaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpType = LongVector::GetBinaryOpType(OpTypeString);
  DispatchTestByDataType(OpType, DataType, Handler);
}

TEST_F(LongVector::OpTest, TrigonometricOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const int TableSize = sizeof(UnaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(UnaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpType = LongVector::GetTrigonometricOpType(OpTypeString);
  DispatchTestByDataType(OpType, DataType, Handler);
}

TEST_F(LongVector::OpTest, UnaryOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const int TableSize = sizeof(UnaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(UnaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpType = LongVector::GetUnaryOpType(OpTypeString);
  DispatchTestByDataType(OpType, DataType, Handler);
}

template <typename LongVectorOpTypeT>
void LongVector::OpTest::DispatchTestByDataType(
    LongVectorOpTypeT OpType, std::wstring DataType,
    TableParameterHandler &Handler) {
  using namespace WEX::Common;

  if (DataType == L"bool")
    DispatchTestByVectorSize<HLSLBool_t>(OpType, Handler);
  else if (DataType == L"int16")
    DispatchTestByVectorSize<int16_t>(OpType, Handler);
  else if (DataType == L"int32")
    DispatchTestByVectorSize<int32_t>(OpType, Handler);
  else if (DataType == L"int64")
    DispatchTestByVectorSize<int64_t>(OpType, Handler);
  else if (DataType == L"uint16")
    DispatchTestByVectorSize<uint16_t>(OpType, Handler);
  else if (DataType == L"uint32")
    DispatchTestByVectorSize<uint32_t>(OpType, Handler);
  else if (DataType == L"uint64")
    DispatchTestByVectorSize<uint64_t>(OpType, Handler);
  else if (DataType == L"float16")
    DispatchTestByVectorSize<HLSLHalf_t>(OpType, Handler);
  else if (DataType == L"float32")
    DispatchTestByVectorSize<float>(OpType, Handler);
  else if (DataType == L"float64")
    DispatchTestByVectorSize<double>(OpType, Handler);
  else
    VERIFY_FAIL(
        String().Format(L"DataType: %s is not recognized.", DataType.c_str()));
}

template <typename DataTypeT, typename LongVectorOpTypeT>
void LongVector::OpTest::DispatchTestByVectorSize(
    LongVectorOpTypeT opType, TableParameterHandler &Handler) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  LongVector::TestConfig<DataTypeT, LongVectorOpTypeT> TestConfig(opType);

  // InputValueSetName1 is optional. So the string may be empty. An empty
  // string will result in the default value set for this DataType being used.
  std::wstring InputValueSet1(
      Handler.GetTableParamByName(L"InputValueSetName1")->m_str);
  if (!InputValueSet1.empty()) {
    TestConfig.SetInputValueSet1(InputValueSet1);
  }

  // InputValueSetName2 is optional. So the string may be empty. An empty
  // string will result in the default value set for this DataType being used.
  if (TestConfig.IsBinaryOp()) {
    std::wstring InputValueSet2(
        Handler.GetTableParamByName(L"InputValueSetName2")->m_str);
    if (!InputValueSet2.empty()) {
      TestConfig.SetInputValueSet2(InputValueSet2);
    }
  }

  if (TestConfig.HasInputArguments()) {
    std::wstring InputArgsName(
        Handler.GetTableParamByName(L"InputArgsName")->m_str);
    if (!InputArgsName.empty()) {
      TestConfig.SetInputArgsArrayName(InputArgsName);
    }
  }

  std::vector<size_t> InputVectorSizes = {3, 4, 5, 16, 17, 35, 100, 256, 1024};
  for (auto SizeToTest : InputVectorSizes) {
    TestBaseMethod<DataTypeT, LongVectorOpTypeT>(TestConfig, SizeToTest);
  }
}

template <typename DataTypeT, typename LongVectorOpTypeT>
void LongVector::OpTest::TestBaseMethod(
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
  if (!CreateDevice(&D3DDevice, D3D_SHADER_MODEL_6_9, false)) {
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
  std::vector<DataTypeT> InputArgsArray;
  const bool IsVectorBinaryOp =
      TestConfig.IsBinaryOp() && !TestConfig.IsScalarOp();

  std::vector<DataTypeT> InputVector1ValueSet = TestConfig.GetInputValueSet1();
  std::vector<DataTypeT> InputVector2ValueSet =
      TestConfig.IsBinaryOp() ? TestConfig.GetInputValueSet2()
                              : std::vector<DataTypeT>();

  if (TestConfig.IsScalarOp())
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

  if (TestConfig.HasInputArguments()) {
    InputArgsArray = TestConfig.GetInputArgsArray();
  }

  std::vector<DataTypeT> ExpectedVector;
  ExpectedVector.reserve(VectorSizeToTest);
  if (IsVectorBinaryOp)
    ExpectedVector =
        ComputeExpectedValues(InputVector1, InputVector2, TestConfig);
  else if (TestConfig.IsScalarOp())
    ExpectedVector =
        ComputeExpectedValues(InputVector1, ScalarInput[0], TestConfig);
  else // Must be a unary op
    ExpectedVector = ComputeExpectedValues(InputVector1, TestConfig);

  if (LogInputs) {
    LogLongVector<DataTypeT>(InputVector1, L"InputVector1");

    if (IsVectorBinaryOp)
      LogLongVector<DataTypeT>(InputVector2, L"InputVector2");
    else if (TestConfig.IsScalarOp())
      LogLongVector<DataTypeT>(ScalarInput, L"ScalarInput");

    if (TestConfig.HasInputArguments()) {
      for (size_t Index = 0; Index < InputArgsArray.size(); Index++) {
        std::wstring InputArgName =
            L"InputArg[" + std::to_wstring(Index) + L"]";
        LogScalar(InputArgsArray[Index], InputArgName);
      }
    }
  }

  // We have to construct the string outside of the lambda. Otherwise it's
  // cleaned up when the lambda finishes executing but before the shader runs.
  std::string CompilerOptionsString =
      TestConfig.GetCompilerOptionsString(VectorSizeToTest);

  // The name of the shader we want to use in ShaderOpArith.xml. Could also add
  // logic to set this name in ShaderOpArithTable.xml so we can use different
  // shaders for different tests.
  LPCSTR ShaderName = "LongVectorOp";
  // ShaderOpArith.xml defines the input/output resources and the shader source.
  CComPtr<IStream> TestXML;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &TestXML, m_Support);

  // RunShaderOpTest is a helper function that handles resource creation
  // and setup. It also handles the shader compilation and execution. It takes a
  // callback that is called when the shader is compiled, but before it is
  // executed.
  std::shared_ptr<st::ShaderOpTestResult> TestResult = st::RunShaderOpTest(
      D3DDevice, m_Support, TestXML, ShaderName,
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
          if (TestConfig.IsScalarOp()) {
            FillShaderBufferFromLongVectorData<DataTypeT>(ShaderData,
                                                          ScalarInput);
          } else if (TestConfig.HasInputArguments()) {
            FillShaderBufferFromLongVectorData<DataTypeT>(ShaderData,
                                                          InputArgsArray);
          }

          return;
        }

        // Process the callback for the InputVector1 resource.
        if (0 == _stricmp(Name, "InputVector1")) {
          FillShaderBufferFromLongVectorData<DataTypeT>(ShaderData,
                                                        InputVector1);
          return;
        }

        // Process the callback for the InputVector2 resource.
        if (0 == _stricmp(Name, "InputVector2")) {
          if (IsVectorBinaryOp) {
            FillShaderBufferFromLongVectorData<DataTypeT>(ShaderData,
                                                          InputVector2);
          }
          return;
        }

        LOG_ERROR_FMT_THROW(
            L"RunShaderOpTest CallBack. Unexpected Resource Name: %S", Name);
      });

  // Map the data from GPU to CPU memory so we can verify our expectations.
  MappedData ShaderOutData;
  TestResult->Test->GetReadBackData("OutputVector", &ShaderOutData);

  std::vector<DataTypeT> OutputVector;
  FillLongVectorDataFromShaderBuffer<DataTypeT>(ShaderOutData, OutputVector,
                                                VectorSizeToTest);

  VERIFY_SUCCEEDED(DoVectorsMatch<DataTypeT>(OutputVector, ExpectedVector,
                                             TestConfig.GetTolerance(),
                                             TestConfig.GetValidationType()));
}
