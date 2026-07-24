#ifndef HLSLEXECTESTUTILS_H
#define HLSLEXECTESTUTILS_H

#include <atlcomcli.h>
#include <d3d12.h>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>

#include "ShaderOpTest.h"
#include "dxc/Support/dxcapi.use.h"

// D3D_SHADER_MODEL_6_10 is not yet in the released Windows SDK. Define locally
// so the test can query 6.10 driver support. This should be removed once
// widely supported.
#if defined(D3D12_PREVIEW_SDK_VERSION) && D3D12_PREVIEW_SDK_VERSION < 720
static const D3D_SHADER_MODEL D3D_SHADER_MODEL_6_10 = (D3D_SHADER_MODEL)0x6a;
#endif

// Local highest shader model known to DXC. Update this when adding support
// for new shader models. Unlike D3D_HIGHEST_SHADER_MODEL from the SDK,
// this stays in sync with DXC's own capabilities.
#define DXC_HIGHEST_SHADER_MODEL D3D_SHADER_MODEL_6_10

bool useDxbc();

/// Manages D3D12 (Agility) SDK selection
///
/// Based on TAEF runtime parameters, this picks an appropriate D3D12 SDK.
///
/// TAEF parameters:
///
///  D3D12SDKPath: relative or absolute path to the D3D12 Agility SDK bin
///  directory. Absolute path is only supported on OS's that support
///  ID3D12DeviceFactory.
///
///  D3D12SDKVersion: requested SDK version
///
///    0: auto-detect (quietly fallback to inbox version)
///
///    1: auto-detect (fail if unable to use the auto-detected version)
///
///   >1: use specified version
class D3D12SDKSelector {
  CComPtr<ID3D12DeviceFactory> DeviceFactory;

public:
  D3D12SDKSelector();
  ~D3D12SDKSelector();

  bool createDevice(ID3D12Device **D3DDevice,
                    D3D_SHADER_MODEL TestModel = D3D_SHADER_MODEL_6_0,
                    bool SkipUnsupported = true);
};

void readHlslDataIntoNewStream(LPCWSTR RelativePath, IStream **Stream,
                               dxc::SpecificDllLoader &Support);

bool doesDeviceSupportInt64(ID3D12Device *pDevice);
bool doesDeviceSupportDouble(ID3D12Device *pDevice);
bool doesDeviceSupportWaveOps(ID3D12Device *pDevice);
bool doesDeviceSupportBarycentrics(ID3D12Device *pDevice);
bool doesDeviceSupportNative16bitOps(ID3D12Device *pDevice);
bool doesDeviceSupportMeshShaders(ID3D12Device *pDevice);
bool doesDeviceSupportRayTracing(ID3D12Device *pDevice);
bool doesDeviceSupportMeshAmpDerivatives(ID3D12Device *pDevice);
bool doesDeviceSupportTyped64Atomics(ID3D12Device *pDevice);
bool doesDeviceSupportHeap64Atomics(ID3D12Device *pDevice);
bool doesDeviceSupportShared64Atomics(ID3D12Device *pDevice);
bool doesDeviceSupportAdvancedTexOps(ID3D12Device *pDevice);
bool doesDeviceSupportWritableMSAA(ID3D12Device *pDevice);
bool doesDeviceSupportEnhancedBarriers(ID3D12Device *pDevice);
bool doesDeviceSupportRelaxedFormatCasting(ID3D12Device *pDevice);
bool isFallbackPathEnabled();

namespace linalg_test {

enum class Tier : UINT {
  NotSupported = 0,
  Tier1_0 = 0x10,
};

enum class DataType : UINT {
  None = 0,
  SInt16 = 2,
  UInt16 = 3,
  SInt32 = 4,
  UInt32 = 5,
  Float16 = 7,
  Float32 = 8,
  SInt8 = 18,
  UInt8 = 19,
  Float8E4M3FN = 20,
  Float8E5M2 = 21,
};

enum class MatrixLayout : UINT {
  RowMajor = 0,
  ColumnMajor = 1,
  MulOptimal = 2,
  OuterProductOptimal = 3,
};

enum class OperationType : UINT {
  MatrixConstruction = 0,
  WaveMatrixMultiply = 1,
  ThreadGroupMatrixMultiply = 2,
  ThreadVectorMatrixMultiply = 3,
  ThreadOuterProduct = 4,
  AtomicAccumulateStore = 5,
};

enum class MultiplicationFlags : UINT {
  None = 0,
  Supported = 1,
  EmulatedInputs = 2,
  EmulatedOutputs = 4,
  Transpose = 8,
};

enum class MatrixRole {
  A,
  B,
  Accumulator,
};

enum class ExecutionScope : UINT {
  Thread = 1,
  Wave = 2,
  ThreadGroup = 4,
};

using ScopeFlags = UINT;

enum class AtomicDestination {
  RWByteAddressBuffer,
  GroupShared,
};

enum class CapabilityRequirement {
  Mandatory,
  CapabilityGated,
};

enum class Applicability {
  Execute,
  NotApplicable,
  Fail,
};

struct TierSupport {
  Tier LinearAlgebraTier = Tier::NotSupported;

  bool supported() const { return LinearAlgebraTier != Tier::NotSupported; }
};

struct MatrixConstructionQuery {
  DataType ComponentType;
  UINT WaveSize;
};

struct MatrixConstructionSupport {
  UINT MinM = 0;
  UINT MinK = 0;
  UINT MinN = 0;

  bool valid() const;
  bool supported() const;
  bool supports(MatrixRole Role, UINT Rows, UINT Columns) const;
};

struct MatrixMultiplyShape {
  UINT M;
  UINT K;
  UINT N;
};

struct WaveMatrixMultiplyQuery {
  UINT WaveSize;
  DataType MatrixAComponentType;
  DataType MatrixBComponentType;
  DataType AccumulatorComponentType;
};

struct WaveMatrixMultiplySupport {
  MultiplicationFlags SupportFlags = MultiplicationFlags::None;
  std::vector<MatrixMultiplyShape> Shapes;

  bool valid() const;
  bool supported() const;
  bool supportsShape(UINT M, UINT K, UINT N) const;
};

struct ThreadGroupMatrixMultiplyQuery {
  WaveMatrixMultiplyQuery WaveInputs;
  MatrixMultiplyShape Shape;
};

struct ThreadGroupMatrixMultiplySupport {
  MultiplicationFlags SupportFlags = MultiplicationFlags::None;
  UINT MinThreadGroupSize = 0;
  UINT MaxThreadGroupSize = 0;
  UINT PreferredThreadGroupSize = 0;

  bool valid() const;
  bool supported() const;
  bool supportsThreadGroupSize(UINT ThreadGroupSize) const;
};

struct ThreadVectorMatrixMultiplyQuery {
  DataType VectorInputType;
  DataType MatrixInputType;
  DataType BiasInputType;
  DataType VectorResultType;
};

struct ThreadVectorMatrixMultiplySupport {
  MultiplicationFlags SupportFlags = MultiplicationFlags::None;

  bool valid() const;
  bool supported() const;
};

struct ThreadOuterProductQuery {
  DataType InputComponentType;
  DataType ResultComponentType;
};

struct ThreadOuterProductSupport {
  bool Supported = false;

  bool supported() const { return Supported; }
};

struct AtomicAccumulateStoreQuery {
  DataType ComponentType;
};

struct AtomicAccumulateStoreSupport {
  bool RWByteAddressBufferSupported = false;
  bool GroupSharedSupported = false;

  bool supports(AtomicDestination Destination) const;
};

bool hasFlag(MultiplicationFlags Value, MultiplicationFlags Flag);
ScopeFlags legalScopes(OperationType Operation);
bool isLegalScope(OperationType Operation, ExecutionScope Scope);
Applicability classifyApplicability(HRESULT QueryResult, bool Supported,
                                    CapabilityRequirement Requirement);

HRESULT queryTierSupport(ID3D12Device *Device, TierSupport &Support);
HRESULT queryMatrixConstruction(ID3D12Device *Device,
                                const MatrixConstructionQuery &Query,
                                MatrixConstructionSupport &Support);
HRESULT queryWaveMatrixMultiply(ID3D12Device *Device,
                                const WaveMatrixMultiplyQuery &Query,
                                WaveMatrixMultiplySupport &Support);
HRESULT
queryThreadGroupMatrixMultiply(ID3D12Device *Device,
                               const ThreadGroupMatrixMultiplyQuery &Query,
                               ThreadGroupMatrixMultiplySupport &Support);
HRESULT
queryThreadVectorMatrixMultiply(ID3D12Device *Device,
                                const ThreadVectorMatrixMultiplyQuery &Query,
                                ThreadVectorMatrixMultiplySupport &Support);
HRESULT queryThreadOuterProduct(ID3D12Device *Device,
                                const ThreadOuterProductQuery &Query,
                                ThreadOuterProductSupport &Support);
HRESULT queryAtomicAccumulateStore(ID3D12Device *Device,
                                   const AtomicAccumulateStoreQuery &Query,
                                   AtomicAccumulateStoreSupport &Support);

} // namespace linalg_test

UINT getMaxGroupSharedMemoryCS(ID3D12Device *Device);
UINT getMaxGroupSharedMemoryAS(ID3D12Device *Device);
UINT getMaxGroupSharedMemoryMS(ID3D12Device *Device);

/// Create a ShaderOp for a compute shader dispatch.
std::unique_ptr<st::ShaderOp>
createComputeOp(const char *Source, const char *Target, const char *RootSig,
                const char *Args = nullptr, UINT DispatchX = 1,
                UINT DispatchY = 1, UINT DispatchZ = 1);

/// Add a UAV buffer resource to a ShaderOp.
void addUAVBuffer(st::ShaderOp *Op, const char *Name, UINT64 Width,
                  bool ReadBack, const char *Init = "zero");

/// Add a SRV buffer resource to a ShaderOp.
void addSRVBuffer(st::ShaderOp *Op, const char *Name, UINT64 Width,
                  const char *Init = "zero");

enum class RawBufferViewKind {
  SRV,
  UAV,
};

struct RawBufferView {
  const char *DescriptorName;
  const char *ResourceName;
  RawBufferViewKind Kind;
  UINT64 FirstByte;
  UINT64 NumBytes;
};

/// Add a shader-visible descriptor table containing raw buffer views.
void addRawBufferDescriptorTable(st::ShaderOp *Op, UINT RootIndex,
                                 const char *HeapName,
                                 std::initializer_list<RawBufferView> Views);

/// Bind a resource to a root view parameter by index.
void addRootView(st::ShaderOp *Op, UINT Index, const char *ResName);

/// Run a programmatically-built ShaderOp and return the result.
std::shared_ptr<st::ShaderOpTestResult> runShaderOp(
    ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
    std::unique_ptr<st::ShaderOp> Op,
    st::ShaderOpTest::TInitCallbackFn InitCallback = nullptr,
    st::ShaderOpTest::TCommandCallbackFn PostDispatchCallback = nullptr);

/// Compiles an HLSL shader using the DXC API to verify it is well-formed.
/// Fails the test on compile error.
void compileShader(dxc::SpecificDllLoader &DxcSupport, const char *Source,
                   const char *Target, const std::string &Args,
                   bool VerboseLogging = false);

// Host-side linear-algebra matrix-conversion helpers. The implementation uses
// SDK-neutral ABI mirrors for the preview D3D12 interfaces so these remain
// available when the tests are built against released Windows SDK headers.
/// Query the number of bytes required to store an NumRows x NumColumns matrix
/// of the given datatype in the specified device layout.
UINT getLinAlgMatrixByteSize(ID3D12Device *Device, UINT NumRows,
                             UINT NumColumns, linalg_test::DataType DataType,
                             linalg_test::MatrixLayout Layout, UINT Stride);

/// Record a GPU matrix layout conversion onto \p List using
/// ID3D12GraphicsCommandListPreview::ConvertLinearAlgebraMatrix. Both
/// \p SrcBuffer (in \p SrcLayout) and \p DestBuffer (receiving \p DestLayout)
/// must be passed in the D3D12_RESOURCE_STATE_UNORDERED_ACCESS state; the
/// conversion requires the source in NON_PIXEL_SHADER_RESOURCE, so this helper
/// transitions it and leaves the destination in UNORDERED_ACCESS. The caller is
/// responsible for ensuring that writes to \p SrcBuffer have completed before
/// this conversion reads it.
void recordLinAlgMatrixConversion(
    ID3D12GraphicsCommandList *List, ID3D12Resource *SrcBuffer, UINT SrcSize,
    ID3D12Resource *DestBuffer, UINT DestSize, UINT NumRows, UINT NumColumns,
    linalg_test::DataType DataType, linalg_test::MatrixLayout SrcLayout,
    UINT SrcStride, linalg_test::MatrixLayout DestLayout, UINT DestStride);

#endif // HLSLEXECTESTUTILS_H
