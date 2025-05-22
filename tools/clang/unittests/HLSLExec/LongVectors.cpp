#include "LongVectors.h"
#include "TableParameterHandler.h"
#include <iomanip>

#include "HlslExecTestUtils.h"

// Move to HLSLTestUtils??? GetPathToHlslDataFile is in there. Would need to
// inlcude dxcapi.use.h in the test util header.

using namespace LongVector;

class LongVectorTest : public WEX::TestClass<LongVectorTest> {
public:
  BEGIN_TEST_CLASS(LongVectorTest)
  TEST_CLASS_PROPERTY(L"Owner", L"alsepkow")
  TEST_CLASS_PROPERTY(L"TestClassification", L"Feature")
  TEST_CLASS_PROPERTY(L"BinaryUnderTest", L"mfsvr.dll")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(ClassSetup)

  BEGIN_TEST_METHOD(BinaryOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#BinaryOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(TrigonometricOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#TrigonometricOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(UnaryOpTest)
  TEST_METHOD_PROPERTY(L"DataSource",
                       L"Table:LongVectorOpTable.xml#UnaryOpTable")
  END_TEST_METHOD()

  template <typename LongVectorOpType>
  void LongVectorOpTestDispatchByDataType(LongVectorOpType OpType,
                                          std::wstring DataType,
                                          TableParameterHandler &Handler);

  template <typename DataType, typename LongVectorOpType>
  void LongVectorOpTestDispatchByVectorSize(LongVectorOpType OpType,
                                            TableParameterHandler &Handler);

  template <typename DataType, typename LongVectorOpType>
  void LongVectorOpTestBase(
      LongVectorOpTestConfig<DataType, LongVectorOpType> &TestConfig,
      size_t VectorSizeToTest);

  dxc::DxcDllSupport m_support;
  bool m_Initialized = false;
};

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

bool LongVectorTest::ClassSetup() {
  // Run this only once.
  if (!m_Initialized) {
    m_Initialized = true;

    HMODULE hRuntime = LoadLibraryW(L"d3d12.dll");
    if (hRuntime == NULL)
      return false;
    // Do not: FreeLibrary(hRuntime);
    // If we actually free the library, it defeats the purpose of
    // EnableAgilitySDK and EnableExperimentalMode.

    HRESULT hr;
    hr = EnableAgilitySDK(hRuntime);
    if (FAILED(hr)) {
      hlsl_test::LogCommentFmt(L"Unable to enable Agility SDK - 0x%08x.", hr);
    } else if (hr == S_FALSE) {
      hlsl_test::LogCommentFmt(L"Agility SDK not enabled.");
    } else {
      hlsl_test::LogCommentFmt(L"Agility SDK enabled.");
    }

    hr = EnableExperimentalMode(hRuntime);
    if (FAILED(hr)) {
      hlsl_test::LogCommentFmt(
          L"Unable to enable shader experimental mode - 0x%08x.", hr);
    } else if (hr == S_FALSE) {
      hlsl_test::LogCommentFmt(L"Experimental mode not enabled.");
    } else {
      hlsl_test::LogCommentFmt(L"Experimental mode enabled.");
    }

    hr = EnableDebugLayer();
    if (FAILED(hr)) {
      hlsl_test::LogCommentFmt(L"Unable to enable debug layer - 0x%08x.", hr);
    } else if (hr == S_FALSE) {
      hlsl_test::LogCommentFmt(L"Debug layer not enabled.");
    } else {
      hlsl_test::LogCommentFmt(L"Debug layer enabled.");
    }
  }

  return true;
}

TEST_F(LongVectorTest, BinaryOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  using namespace WEX::Common;

  const int TableSize = sizeof(BinaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(BinaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpType = LongVector::GetBinaryOpType(OpTypeString);
  LongVectorOpTestDispatchByDataType(OpType, DataType, Handler);
}

TEST_F(LongVectorTest, TrigonometricOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const int TableSize = sizeof(UnaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(UnaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpType = LongVector::GetTrigonometricOpType(OpTypeString);
  LongVectorOpTestDispatchByDataType(OpType, DataType, Handler);
}

TEST_F(LongVectorTest, UnaryOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  const int TableSize = sizeof(UnaryOpParameters) / sizeof(TableParameter);
  TableParameterHandler Handler(UnaryOpParameters, TableSize);

  std::wstring DataType(Handler.GetTableParamByName(L"DataType")->m_str);
  std::wstring OpTypeString(Handler.GetTableParamByName(L"OpTypeEnum")->m_str);

  auto OpType = LongVector::GetUnaryOpType(OpTypeString);
  LongVectorOpTestDispatchByDataType(OpType, DataType, Handler);
}

template <typename LongVectorOpType>
void LongVectorTest::LongVectorOpTestDispatchByDataType(
    LongVectorOpType OpType, std::wstring DataType,
    TableParameterHandler &Handler) {
  using namespace WEX::Common;

  if (DataType == L"bool")
    LongVectorOpTestDispatchByVectorSize<HLSLBool_t>(OpType, Handler);
  else if (DataType == L"int16")
    LongVectorOpTestDispatchByVectorSize<int16_t>(OpType, Handler);
  else if (DataType == L"int32")
    LongVectorOpTestDispatchByVectorSize<int32_t>(OpType, Handler);
  else if (DataType == L"int64")
    LongVectorOpTestDispatchByVectorSize<int64_t>(OpType, Handler);
  else if (DataType == L"uint16")
    LongVectorOpTestDispatchByVectorSize<uint16_t>(OpType, Handler);
  else if (DataType == L"uint32")
    LongVectorOpTestDispatchByVectorSize<uint32_t>(OpType, Handler);
  else if (DataType == L"uint64")
    LongVectorOpTestDispatchByVectorSize<uint64_t>(OpType, Handler);
  else if (DataType == L"float16")
    LongVectorOpTestDispatchByVectorSize<HLSLHalf_t>(OpType, Handler);
  else if (DataType == L"float32")
    LongVectorOpTestDispatchByVectorSize<float>(OpType, Handler);
  else if (DataType == L"float64")
    LongVectorOpTestDispatchByVectorSize<double>(OpType, Handler);
  else
    VERIFY_FAIL(
        String().Format(L"DataType: %s is not recognized.", DataType.c_str()));
}

template <typename DataType, typename LongVectorOpType>
void LongVectorTest::LongVectorOpTestDispatchByVectorSize(
    LongVectorOpType opType, TableParameterHandler &Handler) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  LongVectorOpTestConfig<DataType, LongVectorOpType> TestConfig(opType);

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
    LongVectorOpTestBase<DataType, LongVectorOpType>(TestConfig, SizeToTest);
  }
}

template <typename DataType, typename LongVectorOpType>
void LongVectorTest::LongVectorOpTestBase(
    LongVectorOpTestConfig<DataType, LongVectorOpType> &TestConfig,
    size_t VectorSizeToTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  hlsl_test::LogCommentFmt(L"Running LongVectorOpTestBase<%S, %zu>",
                           typeid(DataType).name(), VectorSizeToTest);

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

  std::vector<DataType> InputVector1;
  InputVector1.reserve(VectorSizeToTest);
  std::vector<DataType> InputVector2; // May be unused, but must be defined.
  InputVector2.reserve(VectorSizeToTest);
  std::vector<DataType> ScalarInput; // May be unused, but must be defined.
  std::vector<DataType> InputArgsArray;
  const bool IsVectorBinaryOp =
      TestConfig.IsBinaryOp() && !TestConfig.IsScalarOp();

  std::vector<DataType> InputVector1ValueSet = TestConfig.GetInputValueSet1();
  std::vector<DataType> InputVector2ValueSet =
      TestConfig.IsBinaryOp() ? TestConfig.GetInputValueSet2()
                              : std::vector<DataType>();

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

  std::vector<DataType> ExpectedVector;
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
    LogLongVector<DataType>(InputVector1, L"InputVector1");

    if (IsVectorBinaryOp)
      LogLongVector<DataType>(InputVector2, L"InputVector2");
    else if (TestConfig.IsScalarOp())
      LogLongVector<DataType>(ScalarInput, L"ScalarInput");

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
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &TestXML, m_support);

  // RunShaderOpTest is a helper function that handles resource creation
  // and setup. It also handles the shader compilation and execution. It takes a
  // callback that is called when the shader is compiled, but before it is
  // executed.
  std::shared_ptr<st::ShaderOpTestResult> TestResult = st::RunShaderOpTest(
      D3DDevice, m_support, TestXML, ShaderName,
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
            FillShaderBufferFromLongVectorData<DataType>(ShaderData,
                                                         ScalarInput);
          } else if (TestConfig.HasInputArguments()) {
            FillShaderBufferFromLongVectorData<DataType>(ShaderData,
                                                         InputArgsArray);
          }

          return;
        }

        // Process the callback for the InputVector1 resource.
        if (0 == _stricmp(Name, "InputVector1")) {
          FillShaderBufferFromLongVectorData<DataType>(ShaderData,
                                                       InputVector1);
          return;
        }

        // Process the callback for the InputVector2 resource.
        if (0 == _stricmp(Name, "InputVector2")) {
          if (IsVectorBinaryOp) {
            FillShaderBufferFromLongVectorData<DataType>(ShaderData,
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

  std::vector<DataType> OutputVector;
  FillLongVectorDataFromShaderBuffer<DataType>(ShaderOutData, OutputVector,
                                               VectorSizeToTest);

  VERIFY_SUCCEEDED(DoVectorsMatch<DataType>(OutputVector, ExpectedVector,
                                            TestConfig.GetTolerance(),
                                            TestConfig.GetValidationType()));
}

///////////////////////////////////// Helpers
// Helper to fill the shader buffer based on type. Convenient to be used when
// copying HLSL*_t types so we can copy the underlying type directly instead of
// the struct.

BinaryOpType LongVector::GetBinaryOpType(const std::wstring &OpTypeString) {
  return GetLongVectorOpType<BinaryOpType>(
      BinaryOpTypeStringToEnumMap, OpTypeString,
      std::size(BinaryOpTypeStringToEnumMap));
}

UnaryOpType LongVector::GetUnaryOpType(const std::wstring &OpTypeString) {
  return GetLongVectorOpType<UnaryOpType>(
      UnaryOpTypeStringToEnumMap, OpTypeString,
      std::size(UnaryOpTypeStringToEnumMap));
}

TrigonometricOpType
LongVector::GetTrigonometricOpType(const std::wstring &OpTypeString) {
  return GetLongVectorOpType<TrigonometricOpType>(
      TrigonometricOpTypeStringToEnumMap, OpTypeString,
      std::size(TrigonometricOpTypeStringToEnumMap));
}