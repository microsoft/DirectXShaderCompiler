#ifndef HLSLEXECTESTUTILS_H
#define HLSLEXECTESTUTILS_H

#include <atlcomcli.h>
#include <d3d12.h>
#include <memory>
#include <optional>
#include <string>
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

// TODO(#8661): Remove me when GroupSharedLimit is available in a release SDK.
#if defined(D3D12_PREVIEW_SDK_VERSION)
UINT getMaxGroupSharedMemoryCS(ID3D12Device *Device);
UINT getMaxGroupSharedMemoryAS(ID3D12Device *Device);
UINT getMaxGroupSharedMemoryMS(ID3D12Device *Device);
#endif // defined(D3D12_PREVIEW_SDK_VERSION)

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

// Host-side linear-algebra matrix-conversion helpers. These need the D3D12
// linear-algebra API (the D3D12_LINEAR_ALGEBRA_* types and the conversion
// methods on ID3D12DevicePreview / ID3D12GraphicsCommandListPreview), which is
// gated behind the preview SDK's DIRECT3D_LINEAR_ALGEBRA feature macro. When
// absent, these helpers and the tests using them are compiled out (they Skip at
// runtime).
#if defined(DIRECT3D_LINEAR_ALGEBRA)
/// Query the number of bytes required to store an NumRows x NumColumns matrix
/// of the given datatype in the specified device layout.
UINT getLinAlgMatrixByteSize(ID3D12Device *Device, UINT NumRows,
                             UINT NumColumns,
                             D3D12_LINEAR_ALGEBRA_DATATYPE DataType,
                             D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT Layout,
                             UINT Stride);

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
    D3D12_LINEAR_ALGEBRA_DATATYPE DataType,
    D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT SrcLayout, UINT SrcStride,
    D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT DestLayout, UINT DestStride);
#endif // defined(DIRECT3D_LINEAR_ALGEBRA)

#endif // HLSLEXECTESTUTILS_H
