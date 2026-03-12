#ifndef HLSLEXECTESTUTILS_H
#define HLSLEXECTESTUTILS_H

#include <atlcomcli.h>
#include <d3d12.h>
#include <optional>
#include <windows.h>

#include "dxc/Support/dxcapi.use.h"

// D3D_SHADER_MODEL_6_10 is not yet in the released Windows SDK.
// Define locally so the tests can target SM 6.10.
// Once the public SDK ships with this, a compile break (redefinition error)
// will signal that this local definition should be removed.
static const D3D_SHADER_MODEL D3D_SHADER_MODEL_6_10 = (D3D_SHADER_MODEL)0x6a;

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

UINT getMaxGroupSharedMemoryCS(ID3D12Device *Device);
UINT getMaxGroupSharedMemoryAS(ID3D12Device *Device);
UINT getMaxGroupSharedMemoryMS(ID3D12Device *Device);

#endif // HLSLEXECTESTUTILS_H
