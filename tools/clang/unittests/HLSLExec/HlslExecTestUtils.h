#ifndef HLSLEXECTESTUTILS_H
#define HLSLEXECTESTUTILS_H

#include <d3d12.h>
#include <windows.h>

#include "dxc/Support/dxcapi.use.h"

namespace ExecTestUtils {
// This is defined in d3d.h for Windows 10 Anniversary Edition SDK, but we
// only require the Windows 10 SDK.
typedef enum D3D_SHADER_MODEL {
  D3D_SHADER_MODEL_5_1 = 0x51,
  D3D_SHADER_MODEL_6_0 = 0x60,
  D3D_SHADER_MODEL_6_1 = 0x61,
  D3D_SHADER_MODEL_6_2 = 0x62,
  D3D_SHADER_MODEL_6_3 = 0x63,
  D3D_SHADER_MODEL_6_4 = 0x64,
  D3D_SHADER_MODEL_6_5 = 0x65,
  D3D_SHADER_MODEL_6_6 = 0x66,
  D3D_SHADER_MODEL_6_7 = 0x67,
  D3D_SHADER_MODEL_6_8 = 0x68,
  D3D_SHADER_MODEL_6_9 = 0x69,
  D3D_HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_9
} D3D_SHADER_MODEL;
} // namespace ExecTestUtils

bool useDxbc();
HRESULT enableDebugLayer();
HRESULT enableExperimentalMode(HMODULE Runtime);
HRESULT enableAgilitySDK(HMODULE Runtime);
bool createDevice(ID3D12Device **D3DDevice,
                  ExecTestUtils::D3D_SHADER_MODEL TestModel =
                      ExecTestUtils::D3D_SHADER_MODEL_6_0,
                  bool SkipUnsupported = true);

void readHlslDataIntoNewStream(LPCWSTR RelativePath, IStream **Stream,
                               dxc::SpecificDllLoader &Support);

#endif // HLSLEXECTESTUTILS_H
