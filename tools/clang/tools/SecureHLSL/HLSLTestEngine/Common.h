#pragma once

#include <stdio.h>
#include <windows.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <vector>
#include <memory>
#include <stdint.h>
#include <DXC\Support\dxcapi.use.h>
#include <atlbase.h>
#include <atlconv.h>
#include <DXProgrammableCapture.h>
#include <Windows.Data.Json.h>
#include <Windows.Foundation.Collections.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::Data::Json;

#define IFT(x)  do { hr = (x); if (FAILED(hr)) { throw hr; } } while (false)
