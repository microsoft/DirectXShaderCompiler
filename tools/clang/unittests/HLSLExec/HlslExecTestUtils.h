#ifndef HLSLEXECTESTUTILS_H
#define HLSLEXECTESTUTILS_H

#include <d3d12.h>
#include <windows.h>

#include "dxc/Support/dxcapi.use.h"

bool useDxbc();
HRESULT enableDebugLayer();
HRESULT enableExperimentalMode(HMODULE Runtime);
HRESULT enableAgilitySDK(HMODULE Runtime);
bool createDevice(ID3D12Device **D3DDevice,
                  D3D_SHADER_MODEL TestModel = D3D_SHADER_MODEL_6_0,
                  bool SkipUnsupported = true);

void readHlslDataIntoNewStream(LPCWSTR RelativePath, IStream **Stream,
                               dxc::SpecificDllLoader &Support);

#endif // HLSLEXECTESTUTILS_H
