#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#define INLINE_TEST_METHOD_MARKUP
#include <WexTestClass.h>

#include "LinearAlgebraTestData.h"

#include "ShaderOpTest.h"
#include "dxc/Support/Global.h"

#include "HlslTestUtils.h"

#include "HlslExecTestUtils.h"

#include <algorithm>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace LinearAlgebra {

//
// Operation Types
//

enum class OpType : unsigned {
#define OP(SYMBOL, ARITY, DEFINE, SHADER_NAME, INPUT_SET_1, INPUT_SET_2) SYMBOL,
#include "LinearAlgebraOps.def"
  NumOpTypes
};

struct Operation {
  size_t Arity;
  const char *Define;
  const char *ShaderName;
  InputSet InputSets[2];
  OpType Type;
};

static constexpr Operation Operations[] = {
#define OP(SYMBOL, ARITY, DEFINE, SHADER_NAME, INPUT_SET_1, INPUT_SET_2)       \
  {ARITY,                                                                      \
   DEFINE,                                                                     \
   SHADER_NAME,                                                                \
   {InputSet::INPUT_SET_1, InputSet::INPUT_SET_2},                             \
   OpType::SYMBOL},
#include "LinearAlgebraOps.def"
};

constexpr const Operation &getOperation(OpType Op) {
  if (Op < OpType::NumOpTypes)
    return Operations[unsigned(Op)];
  std::abort();
}

//
// Data Types
//

struct DataType {
  const char *HLSLTypeString;
  const char *CompTypeString;
  bool Is16Bit;
  size_t HLSLSizeInBytes;
};

template <typename T> const DataType &getDataType() {
  static_assert(false && "Unknown data type");
}

#define DATA_TYPE(TYPE, HLSL_STRING, COMP_TYPE, HLSL_SIZE, IS_16BIT)           \
  template <> const DataType &getDataType<TYPE>() {                            \
    static DataType DT{HLSL_STRING, COMP_TYPE, IS_16BIT, HLSL_SIZE};           \
    return DT;                                                                 \
  }

DATA_TYPE(HLSLHalf_t, "float16_t", "ComponentType::F16", 2, true)
DATA_TYPE(float, "float", "ComponentType::F32", 4, false)
DATA_TYPE(double, "double", "ComponentType::F64", 8, false)
DATA_TYPE(int32_t, "int", "ComponentType::I32", 4, false)
DATA_TYPE(uint32_t, "uint", "ComponentType::U32", 4, false)

#undef DATA_TYPE

using HLSLTestDataTypes::DefaultValidation;
using HLSLTestDataTypes::doValuesMatch;
using HLSLTestDataTypes::HLSLHalf_t;
using HLSLTestDataTypes::isFloatingPointType;
using HLSLTestDataTypes::StrictValidation;
using HLSLTestDataTypes::ValidationConfig;
using HLSLTestDataTypes::ValidationType;

template <typename T>
bool doVectorsMatch(const std::vector<T> &Actual,
                    const std::vector<T> &Expected,
                    const ValidationConfig &Config, bool VerboseLogging) {
  DXASSERT(Actual.size() == Expected.size(),
           "Actual and Expected must be the same size");

  if (VerboseLogging)
    hlsl_test::LogCommentFmt(L"Verifying %zu elements", Actual.size());

  std::vector<size_t> MismatchedIndexes;
  for (size_t I = 0; I < Actual.size(); I++) {
    if (!doValuesMatch(Actual[I], Expected[I], Config.Tolerance, Config.Type))
      MismatchedIndexes.push_back(I);
  }

  if (MismatchedIndexes.empty())
    return true;

  for (size_t Index : MismatchedIndexes) {
    std::wstringstream Wss(L"");
    Wss << std::setprecision(15);
    Wss << L"Mismatch at Index: " << Index;
    Wss << L" Actual:" << Actual[Index];
    Wss << L" Expected:" << Expected[Index];
    hlsl_test::LogErrorFmt(Wss.str().c_str());
  }

  return false;
}

//
// Matrix dimensions for test iteration.
//

struct MatrixDims {
  size_t Rows;
  size_t Cols;
};

std::vector<MatrixDims> getMatrixSizesToTest() {
  return {{2, 2}, {4, 4}, {4, 8}, {8, 4}, {8, 8}};
}

//
// Build compiler options.
//

std::string getCompilerOptionsString(const Operation &Op,
                                     const DataType &ElemType, size_t Rows,
                                     size_t Cols, size_t KDim = 0) {
  std::stringstream Options;

  if (ElemType.Is16Bit)
    Options << " -enable-16bit-types";

  Options << " -D" << Op.Define;
  Options << " -DELEM_TYPE=" << ElemType.HLSLTypeString;
  Options << " -DOUT_TYPE=" << ElemType.HLSLTypeString;
  Options << " -DCOMP_TYPE=" << ElemType.CompTypeString;
  Options << " -DROWS=" << Rows;
  Options << " -DCOLS=" << Cols;

  if (KDim > 0)
    Options << " -DK_DIM=" << KDim;

  Options << " -DMATRIX_LAYOUT=0";

  return Options.str();
}

//
// Shader buffer helpers.
//

template <typename T>
void fillShaderBuffer(std::vector<BYTE> &ShaderBuffer,
                      const std::vector<T> &Data) {
  const size_t DataSize = sizeof(T) * Data.size();
  DXASSERT_NOMSG(ShaderBuffer.size() >= DataSize);

  if constexpr (std::is_same_v<T, HLSLHalf_t>) {
    auto *Ptr =
        reinterpret_cast<DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t I = 0; I < Data.size(); I++)
      Ptr[I] = Data[I].Val;
    return;
  }

  auto *Ptr = reinterpret_cast<T *>(ShaderBuffer.data());
  for (size_t I = 0; I < Data.size(); I++)
    Ptr[I] = Data[I];
}

template <typename T>
void readShaderBuffer(const MappedData &ShaderBuffer, std::vector<T> &OutData,
                      size_t NumElements) {
  if constexpr (std::is_same_v<T, HLSLHalf_t>) {
    auto *Ptr =
        static_cast<const DirectX::PackedVector::HALF *>(ShaderBuffer.data());
    for (size_t I = 0; I < NumElements; I++)
      OutData.push_back(HLSLHalf_t::FromHALF(Ptr[I]));
    return;
  }

  auto *Ptr = static_cast<const T *>(ShaderBuffer.data());
  for (size_t I = 0; I < NumElements; I++)
    OutData.push_back(Ptr[I]);
}

//
// Input building helpers. Following LongVector::buildTestInput pattern.
//

template <typename T> using InputSets = std::vector<std::vector<T>>;

template <typename T>
std::vector<T> buildTestInput(InputSet Set, size_t NumElements) {
  const std::vector<T> &RawData = getInputSet<T>(Set);

  std::vector<T> Result;
  Result.reserve(NumElements);
  for (size_t I = 0; I < NumElements; ++I)
    Result.push_back(RawData[I % RawData.size()]);

  return Result;
}

// Build an identity matrix of the given dimensions using the Identity InputSet
// for the diagonal value.
template <typename T>
std::vector<T> buildIdentityMatrix(size_t Rows, size_t Cols) {
  const T One = getInputSet<T>(InputSet::Identity)[0];
  const T Zero = One - One;
  std::vector<T> Result(Rows * Cols, Zero);
  size_t MinDim = Rows < Cols ? Rows : Cols;
  for (size_t I = 0; I < MinDim; ++I)
    Result[I * Cols + I] = One;
  return Result;
}

template <typename T>
InputSets<T> buildTestInputs(const Operation &Op, size_t Rows, size_t Cols,
                             size_t KDim) {
  InputSets<T> Inputs;
  const size_t NumElements = Rows * Cols;

  if (Op.Arity >= 1)
    Inputs.push_back(buildTestInput<T>(Op.InputSets[0], NumElements));

  if (Op.Arity >= 2) {
    // For binary ops the second input may be an identity matrix.
    if (Op.InputSets[1] == InputSet::Identity)
      Inputs.push_back(buildIdentityMatrix<T>(KDim, Cols));
    else
      Inputs.push_back(buildTestInput<T>(Op.InputSets[1], KDim * Cols));
  }

  return Inputs;
}

//
// Core GPU test runner. Returns the output buffer or nullopt if skipped.
//

template <typename T>
std::optional<std::vector<T>>
runLinAlgTest(ID3D12Device *D3DDevice, bool VerboseLogging, const Operation &Op,
              const InputSets<T> &Inputs, size_t Rows, size_t Cols, size_t KDim,
              size_t ExpectedOutputSize) {

  const DataType &ElemType = getDataType<T>();

  std::string CompilerOptions =
      getCompilerOptionsString(Op, ElemType, Rows, Cols, KDim);

  if (VerboseLogging)
    hlsl_test::LogCommentFmt(L"Compiler Options: %S", CompilerOptions.c_str());

  dxc::SpecificDllLoader DxilDllLoader;
  CComPtr<IStream> TestXML;
  readHlslDataIntoNewStream(L"ShaderOpArith.xml", &TestXML, DxilDllLoader);
  auto ShaderOpSet = std::make_shared<st::ShaderOpSet>();
  st::ParseShaderOpSetFromStream(TestXML, ShaderOpSet.get());

  std::shared_ptr<st::ShaderOpTestResult> TestResult =
      st::RunShaderOpTestAfterParse(
          D3DDevice, DxilDllLoader, Op.ShaderName,
          [&](LPCSTR Name, std::vector<BYTE> &ShaderData,
              st::ShaderOp *ShaderOp) {
            if (VerboseLogging)
              hlsl_test::LogCommentFmt(
                  L"LinAlg RunShaderOpTest CallBack. Resource Name: %S", Name);

            if (_stricmp(Name, "OutputMatrix") == 0) {
              ShaderOp->Shaders.at(0).Arguments = CompilerOptions.c_str();
              return;
            }

            for (size_t I = 0; I < 2; ++I) {
              std::string BufferName = "InputMatrix";
              BufferName += (char)('1' + I);
              if (_stricmp(Name, BufferName.c_str()) == 0) {
                if (I < Inputs.size() && !Inputs[I].empty())
                  fillShaderBuffer(ShaderData, Inputs[I]);
                return;
              }
            }

            LOG_ERROR_FMT_THROW(
                L"LinAlg RunShaderOpTest CallBack. Unexpected Resource: %S",
                Name);
          },
          std::move(ShaderOpSet));

  MappedData ShaderOutData;
  TestResult->Test->GetReadBackData("OutputMatrix", &ShaderOutData);

  std::vector<T> OutData;
  readShaderBuffer(ShaderOutData, OutData, ExpectedOutputSize);

  return OutData;
}

//
// runAndVerify - runs the GPU test and verifies results.
//

template <typename T>
void runAndVerify(ID3D12Device *D3DDevice, bool VerboseLogging,
                  const Operation &Op, const InputSets<T> &Inputs,
                  const std::vector<T> &Expected,
                  const ValidationConfig &Config, size_t Rows, size_t Cols,
                  size_t KDim) {

  auto Actual = runLinAlgTest<T>(D3DDevice, VerboseLogging, Op, Inputs, Rows,
                                 Cols, KDim, Expected.size());

  if (!Actual) {
    hlsl_test::LogCommentFmt(L"Test was skipped.");
    return;
  }

  VERIFY_IS_TRUE(doVectorsMatch(*Actual, Expected, Config, VerboseLogging));
}

//
// Op definitions. Each op carries a ValidationConfig.
// Specializations are expected to have a ValidationConfig member.
//

template <OpType OP, typename T> struct Op;

// ExpectedBuilder - specializations compute expected output from inputs.
template <OpType OP, typename T> struct ExpectedBuilder;

// FillMatrix: splat a scalar value across the entire matrix.
template <typename T> struct Op<OpType::FillMatrix, T> : StrictValidation {};

template <typename T> struct ExpectedBuilder<OpType::FillMatrix, T> {
  static std::vector<T> buildExpected(Op<OpType::FillMatrix, T> &,
                                      const InputSets<T> &, size_t Rows,
                                      size_t Cols, size_t) {
    const T FillVal = getInputSet<T>(InputSet::Fill)[0];
    return std::vector<T>(Rows * Cols, FillVal);
  }

  // FillMatrix input is special: just the scalar fill value.
  static InputSets<T> buildInputs(const Operation &, size_t, size_t, size_t) {
    return {{getInputSet<T>(InputSet::Fill)[0]}};
  }
};

// MatrixStore: load and store round-trip.
template <typename T>
struct Op<OpType::MatrixStore, T> : DefaultValidation<T> {};

template <typename T> struct ExpectedBuilder<OpType::MatrixStore, T> {
  static std::vector<T> buildExpected(Op<OpType::MatrixStore, T> &,
                                      const InputSets<T> &Inputs, size_t,
                                      size_t, size_t) {
    return Inputs[0];
  }
};

// MatrixAccumulate: accumulate into zero-initialized output.
template <typename T>
struct Op<OpType::MatrixAccumulate, T> : DefaultValidation<T> {};

template <typename T> struct ExpectedBuilder<OpType::MatrixAccumulate, T> {
  static std::vector<T> buildExpected(Op<OpType::MatrixAccumulate, T> &,
                                      const InputSets<T> &Inputs, size_t,
                                      size_t, size_t) {
    return Inputs[0];
  }
};

// MatrixMul: multiply input matrix by identity.
template <typename T> struct Op<OpType::MatrixMul, T> : DefaultValidation<T> {};

template <typename T> struct ExpectedBuilder<OpType::MatrixMul, T> {
  static std::vector<T> buildExpected(Op<OpType::MatrixMul, T> &,
                                      const InputSets<T> &Inputs, size_t,
                                      size_t, size_t) {
    // Multiplying by identity: result should equal Input1.
    return Inputs[0];
  }
};

//
// dispatchTest - orchestrates building inputs, computing expected results,
// and running the test across multiple matrix sizes.
// Follows the same pattern as LongVector::dispatchTest.
//

template <typename T, OpType OP>
void dispatchTest(ID3D12Device *D3DDevice, bool VerboseLogging) {

  const std::vector<MatrixDims> Sizes = getMatrixSizesToTest();
  constexpr const Operation &Operation = getOperation(OP);
  Op<OP, T> Op;

  for (const MatrixDims &Dims : Sizes) {
    const size_t Rows = Dims.Rows;
    const size_t Cols = Dims.Cols;
    const size_t KDim = (Operation.Arity >= 2) ? Cols : 0;

    // FillMatrix has special input handling (scalar, not a matrix).
    InputSets<T> Inputs;
    if constexpr (OP == OpType::FillMatrix)
      Inputs = ExpectedBuilder<OP, T>::buildInputs(Operation, Rows, Cols, KDim);
    else
      Inputs = buildTestInputs<T>(Operation, Rows, Cols, KDim);

    auto Expected =
        ExpectedBuilder<OP, T>::buildExpected(Op, Inputs, Rows, Cols, KDim);

    runAndVerify(D3DDevice, VerboseLogging, Operation, Inputs, Expected,
                 Op.ValidationConfig, Rows, Cols, KDim);
  }
}

} // namespace LinearAlgebra

using namespace LinearAlgebra;

//
// TAEF test entry point macro.
//
#define LINALG_TEST(Op, DataType)                                              \
  TEST_METHOD(Op##_##DataType) { runTest<DataType, OpType::Op>(); }

//
// Common test class for linear algebra tests.
// Follows the same pattern as LongVector::TestClassCommon.
//
class LinAlgTestClassCommon {
public:
  bool setupClass() {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    if (!Initialized) {
      Initialized = true;

      D3D12SDK = D3D12SDKSelector();

      WEX::TestExecution::RuntimeParameters::TryGetValue(L"VerboseLogging",
                                                         VerboseLogging);
      if (VerboseLogging)
        hlsl_test::LogCommentFmt(L"Verbose logging is enabled for this test.");
      else
        hlsl_test::LogCommentFmt(L"Verbose logging is disabled for this test.");

      bool FailIfRequirementsNotMet = false;
#ifdef _HLK_CONF
      FailIfRequirementsNotMet = true;
#endif
      WEX::TestExecution::RuntimeParameters::TryGetValue(
          L"FailIfRequirementsNotMet", FailIfRequirementsNotMet);

      const bool SkipUnsupported = !FailIfRequirementsNotMet;
      // Linear algebra requires at least SM 6.9 device support.
      if (!D3D12SDK->createDevice(&D3DDevice, D3D_SHADER_MODEL_6_9,
                                  SkipUnsupported)) {
        if (FailIfRequirementsNotMet)
          hlsl_test::LogErrorFmt(
              L"Device Creation failed, resulting in test failure, since "
              L"FailIfRequirementsNotMet is set.");

        return false;
      }
    }

    return true;
  }

  bool setupMethod() {
    if (D3DDevice && D3DDevice->GetDeviceRemovedReason() != S_OK) {
      hlsl_test::LogCommentFmt(L"Device was lost!");
      D3DDevice.Release();
    }

    if (!D3DDevice) {
      hlsl_test::LogCommentFmt(L"Creating device");

      const bool SkipUnsupported = false;
      VERIFY_IS_TRUE(D3D12SDK->createDevice(&D3DDevice, D3D_SHADER_MODEL_6_9,
                                            SkipUnsupported));
    }

    return true;
  }

  template <typename T, OpType OP> void runTest() {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

    dispatchTest<T, OP>(D3DDevice, VerboseLogging);
  }

protected:
  CComPtr<ID3D12Device> D3DDevice;

private:
  bool Initialized = false;
  std::optional<D3D12SDKSelector> D3D12SDK;
  bool VerboseLogging = false;
};

//
// TAEF Test Class
//
class DxilConf_SM610_LinearAlgebra : public LinAlgTestClassCommon {
public:
  BEGIN_TEST_CLASS(DxilConf_SM610_LinearAlgebra)
  TEST_CLASS_PROPERTY("Kits.TestName",
                      "D3D12 - Shader Model 6.10 - Linear Algebra Tests")
  TEST_CLASS_PROPERTY("Kits.TestId", "a1b2c3d4-e5f6-7890-abcd-ef1234567890")
  TEST_CLASS_PROPERTY("Kits.Description",
                      "Validates SM 6.10 linear algebra matrix operations")
  TEST_CLASS_PROPERTY(
      "Kits.Specification",
      "Device.Graphics.D3D12.DXILCore.ShaderModel610.CoreRequirement")
  TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(setupClass) { return LinAlgTestClassCommon::setupClass(); }
  TEST_METHOD_SETUP(setupMethod) {
    return LinAlgTestClassCommon::setupMethod();
  }

  // FillMatrix (Splat)
  LINALG_TEST(FillMatrix, float);
  LINALG_TEST(FillMatrix, HLSLHalf_t);
  LINALG_TEST(FillMatrix, int32_t);
  LINALG_TEST(FillMatrix, uint32_t);

  // MatrixStore (Load + Store round-trip)
  LINALG_TEST(MatrixStore, float);
  LINALG_TEST(MatrixStore, HLSLHalf_t);
  LINALG_TEST(MatrixStore, int32_t);
  LINALG_TEST(MatrixStore, uint32_t);

  // MatrixAccumulate (InterlockedAccumulate)
  LINALG_TEST(MatrixAccumulate, float);
  LINALG_TEST(MatrixAccumulate, HLSLHalf_t);

  // MatrixMul (Multiply)
  LINALG_TEST(MatrixMul, float);
  LINALG_TEST(MatrixMul, HLSLHalf_t);
};
