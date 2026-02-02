#ifndef HLSLEXECTESTUTILS_H
#define HLSLEXECTESTUTILS_H

#include <atlcomcli.h>
#include <d3d12.h>
#include <optional>
#include <windows.h>

#include "dxc/Support/dxcapi.use.h"

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

#endif // HLSLEXECTESTUTILS_H
