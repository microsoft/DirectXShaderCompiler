#ifndef HLSLEXECTESTUTILS_H
#define HLSLEXECTESTUTILS_H

#include "dxc/Support/dxcapi.use.h"
#include "dxc/Test/HlslTestUtils.h"
#include <Verify.h>
#include <d3d12.h>
#include <dxgi1_4.h>

static const D3D_SHADER_MODEL HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_9;

static bool UseDebugIfaces() { return true; }

static bool UseDxbc() {
#ifdef _HLK_CONF
  return false;
#else
  return hlsl_test::GetTestParamBool(L"DXBC");
#endif
}

static bool UseWarpByDefault() {
#ifdef _HLK_CONF
  return false;
#else
  return true;
#endif
}

// A more recent Windows SDK than currently required is needed for these.
typedef HRESULT(WINAPI *D3D12EnableExperimentalFeaturesFn)(
    UINT NumFeatures, __in_ecount(NumFeatures) const IID *pIIDs,
    __in_ecount_opt(NumFeatures) void *pConfigurationStructs,
    __in_ecount_opt(NumFeatures) UINT *pConfigurationStructSizes);

static const GUID D3D12ExperimentalShaderModelsID =
    {/* 76f5573e-f13a-40f5-b297-81ce9e18933f */
     0x76f5573e,
     0xf13a,
     0x40f5,
     {0xb2, 0x97, 0x81, 0xce, 0x9e, 0x18, 0x93, 0x3f}};

// Used to create D3D12SDKConfiguration to enable AgilitySDK programmatically.
typedef HRESULT(WINAPI *D3D12GetInterfaceFn)(REFCLSID rclsid, REFIID riid,
                                             void **ppvDebug);

#ifndef __ID3D12SDKConfiguration_INTERFACE_DEFINED__

// Copied from AgilitySDK D3D12.h to programmatically enable when in developer
// mode.
#define __ID3D12SDKConfiguration_INTERFACE_DEFINED__

EXTERN_C const GUID DECLSPEC_SELECTANY IID_ID3D12SDKConfiguration = {
    0xe9eb5314,
    0x33aa,
    0x42b2,
    {0xa7, 0x18, 0xd7, 0x7f, 0x58, 0xb1, 0xf1, 0xc7}};
EXTERN_C const GUID DECLSPEC_SELECTANY CLSID_D3D12SDKConfiguration = {
    0x7cda6aca,
    0xa03e,
    0x49c8,
    {0x94, 0x58, 0x03, 0x34, 0xd2, 0x0e, 0x07, 0xce}};

MIDL_INTERFACE("e9eb5314-33aa-42b2-a718-d77f58b1f1c7")
ID3D12SDKConfiguration : public IUnknown {
public:
  virtual HRESULT STDMETHODCALLTYPE SetSDKVersion(UINT SDKVersion,
                                                  LPCSTR SDKPath) = 0;
};
#endif /* __ID3D12SDKConfiguration_INTERFACE_DEFINED__ */

static std::wstring GetModuleName() {
  wchar_t moduleName[MAX_PATH + 1] = {0};
  DWORD length = GetModuleFileNameW(NULL, moduleName, MAX_PATH);
  if (length == 0 || length == MAX_PATH) {
    return std::wstring(); // Error condition
  }
  return std::wstring(moduleName, length);
}

static std::wstring ComputeSDKFullPath(std::wstring SDKPath) {
  std::wstring modulePath = GetModuleName();
  size_t pos = modulePath.rfind('\\');
  if (pos == std::wstring::npos)
    return SDKPath;
  if (SDKPath.substr(0, 2) != L".\\")
    return SDKPath;
  return modulePath.substr(0, pos) + SDKPath.substr(1);
}

static UINT GetD3D12SDKVersion(std::wstring SDKPath) {
  // Try to automatically get the D3D12SDKVersion from the DLL
  UINT SDKVersion = 0;
  std::wstring D3DCorePath = ComputeSDKFullPath(SDKPath);
  D3DCorePath.append(L"D3D12Core.dll");
  HMODULE hCore = LoadLibraryW(D3DCorePath.c_str());
  if (hCore) {
    if (UINT *pSDKVersion = (UINT *)GetProcAddress(hCore, "D3D12SDKVersion"))
      SDKVersion = *pSDKVersion;
    FreeModule(hCore);
  }
  return SDKVersion;
}

static bool CreateDevice(ID3D12Device **ppDevice,
                         D3D_SHADER_MODEL testModel = D3D_SHADER_MODEL_6_0,
                         bool skipUnsupported = true) {
  if (testModel > HIGHEST_SHADER_MODEL) {
    UINT minor = (UINT)testModel & 0x0f;
    hlsl_test::LogCommentFmt(L"Installed SDK does not support "
                             L"shader model 6.%1u",
                             minor);

    if (skipUnsupported) {
      WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
    }

    return false;
  }
  CComPtr<IDXGIFactory4> factory;
  CComPtr<ID3D12Device> pDevice;

  *ppDevice = nullptr;

  VERIFY_SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
  if (hlsl_test::GetTestParamUseWARP(UseWarpByDefault())) {
    CComPtr<IDXGIAdapter> warpAdapter;
    VERIFY_SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
    HRESULT createHR = D3D12CreateDevice(warpAdapter, D3D_FEATURE_LEVEL_11_0,
                                         IID_PPV_ARGS(&pDevice));
    if (FAILED(createHR)) {
      hlsl_test::LogCommentFmt(
          L"The available version of WARP does not support d3d12.");

      if (skipUnsupported) {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
      }

      return false;
    }

    if (GetModuleHandleW(L"d3d10warp.dll") != NULL) {
      WCHAR szFullModuleFilePath[MAX_PATH] = L"";
      GetModuleFileNameW(GetModuleHandleW(L"d3d10warp.dll"),
                         szFullModuleFilePath, sizeof(szFullModuleFilePath));
      WEX::Logging::Log::Comment(WEX::Common::String().Format(
          L"WARP driver loaded from: %S", szFullModuleFilePath));
    }

  } else {
    CComPtr<IDXGIAdapter1> hardwareAdapter;
    WEX::Common::String AdapterValue;
    HRESULT hr = WEX::TestExecution::RuntimeParameters::TryGetValue(
        L"Adapter", AdapterValue);
    if (SUCCEEDED(hr)) {
      st::GetHardwareAdapter(factory, AdapterValue, &hardwareAdapter);
    } else {
      WEX::Logging::Log::Comment(
          L"Using default hardware adapter with D3D12 support.");
    }

    VERIFY_SUCCEEDED(D3D12CreateDevice(hardwareAdapter, D3D_FEATURE_LEVEL_11_0,
                                       IID_PPV_ARGS(&pDevice)));
  }
  // retrieve adapter information
  LUID adapterID = pDevice->GetAdapterLuid();
  CComPtr<IDXGIAdapter> adapter;
  factory->EnumAdapterByLuid(adapterID, IID_PPV_ARGS(&adapter));
  DXGI_ADAPTER_DESC AdapterDesc;
  VERIFY_SUCCEEDED(adapter->GetDesc(&AdapterDesc));
  hlsl_test::LogCommentFmt(L"Using Adapter:%s", AdapterDesc.Description);

  if (pDevice == nullptr)
    return false;

  if (!UseDxbc()) {
    // Check for DXIL support.
    typedef struct D3D12_FEATURE_DATA_SHADER_MODEL {
      D3D_SHADER_MODEL HighestShaderModel;
    } D3D12_FEATURE_DATA_SHADER_MODEL;
    const UINT D3D12_FEATURE_SHADER_MODEL = 7;
    D3D12_FEATURE_DATA_SHADER_MODEL SMData;
    SMData.HighestShaderModel = testModel;
    if (FAILED(pDevice->CheckFeatureSupport(
            (D3D12_FEATURE)D3D12_FEATURE_SHADER_MODEL, &SMData,
            sizeof(SMData))) ||
        SMData.HighestShaderModel < testModel) {
      UINT minor = (UINT)testModel & 0x0f;
      hlsl_test::LogCommentFmt(L"The selected device does not support "
                               L"shader model 6.%1u",
                               minor);

      if (skipUnsupported) {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
      }

      return false;
    }
  }

  if (UseDebugIfaces()) {
    CComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(pDevice->QueryInterface(&pInfoQueue))) {
      pInfoQueue->SetMuteDebugOutput(FALSE);
    }
  }

  *ppDevice = pDevice.Detach();
  return true;
}

inline void ReadHlslDataIntoNewStream(LPCWSTR relativePath, IStream **ppStream,
                                      dxc::DxcDllSupport &support) {
  VERIFY_SUCCEEDED(support.Initialize());
  CComPtr<IDxcLibrary> pLibrary;
  CComPtr<IDxcBlobEncoding> pBlob;
  CComPtr<IStream> pStream;
  std::wstring path = hlsl_test::GetPathToHlslDataFile(
      relativePath, HLSLDATAFILEPARAM, DEFAULT_EXEC_TEST_DIR);
  VERIFY_SUCCEEDED(support.CreateInstance(CLSID_DxcLibrary, &pLibrary));
  VERIFY_SUCCEEDED(pLibrary->CreateBlobFromFile(path.c_str(), nullptr, &pBlob));
  VERIFY_SUCCEEDED(pLibrary->CreateStreamFromBlobReadOnly(pBlob, &pStream));
  *ppStream = pStream.Detach();
}

static HRESULT EnableAgilitySDK(HMODULE hRuntime, UINT SDKVersion,
                                LPCWSTR SDKPath) {
  D3D12GetInterfaceFn pD3D12GetInterface =
      (D3D12GetInterfaceFn)GetProcAddress(hRuntime, "D3D12GetInterface");
  CComPtr<ID3D12SDKConfiguration> pD3D12SDKConfiguration;
  IFR(pD3D12GetInterface(CLSID_D3D12SDKConfiguration,
                         IID_PPV_ARGS(&pD3D12SDKConfiguration)));
  IFR(pD3D12SDKConfiguration->SetSDKVersion(SDKVersion, CW2A(SDKPath)));

  // Currently, it appears that the SetSDKVersion will succeed even when
  // D3D12Core is not found, or its version doesn't match.  When that's the
  // case, will cause a failure in the very next thing that actually requires
  // D3D12Core.dll to be loaded instead.  So, we attempt to clear experimental
  // features next, which is a valid use case and a no-op at this point.  This
  // requires D3D12Core to be loaded.  If this fails, we know the AgilitySDK
  // setting actually failed.
  D3D12EnableExperimentalFeaturesFn pD3D12EnableExperimentalFeatures =
      (D3D12EnableExperimentalFeaturesFn)GetProcAddress(
          hRuntime, "D3D12EnableExperimentalFeatures");
  if (pD3D12EnableExperimentalFeatures == nullptr) {
    // If this failed, D3D12 must be too old for AgilitySDK.  But if that's
    // the case, creating D3D12SDKConfiguration should have failed.  So while
    // this case shouldn't be hit, fail if it is.
    return HRESULT_FROM_WIN32(GetLastError());
  }
  return pD3D12EnableExperimentalFeatures(0, nullptr, nullptr, nullptr);
}

static HRESULT EnableExperimentalShaderModels(HMODULE hRuntime) {
  D3D12EnableExperimentalFeaturesFn pD3D12EnableExperimentalFeatures =
      (D3D12EnableExperimentalFeaturesFn)GetProcAddress(
          hRuntime, "D3D12EnableExperimentalFeatures");
  if (pD3D12EnableExperimentalFeatures == nullptr) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  return pD3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModelsID,
                                          nullptr, nullptr);
}

static HRESULT EnableExperimentalShaderModels() {
  HMODULE hRuntime = LoadLibraryW(L"d3d12.dll");
  if (hRuntime == NULL)
    return E_FAIL;
  return EnableExperimentalShaderModels(hRuntime);
}

static HRESULT EnableAgilitySDK(HMODULE hRuntime) {
  // D3D12SDKVersion > 1 will use provided version, otherwise, auto-detect.
  // D3D12SDKVersion == 1 means fail if we can't auto-detect.
  UINT SDKVersion = 0;
  WEX::TestExecution::RuntimeParameters::TryGetValue(L"D3D12SDKVersion",
                                                     SDKVersion);

  // SDKPath must be relative path from .exe, which means relative to
  // TE.exe location, and must start with ".\\", such as with the
  // default: ".\\D3D12\\"
  WEX::Common::String SDKPath;
  if (SUCCEEDED(WEX::TestExecution::RuntimeParameters::TryGetValue(
          L"D3D12SDKPath", SDKPath))) {
    // Make sure path ends in backslash
    if (!SDKPath.IsEmpty() && SDKPath.Right(1) != "\\") {
      SDKPath.Append("\\");
    }
  }
  if (SDKPath.IsEmpty()) {
    SDKPath = L".\\D3D12\\";
  }

  bool mustFind = SDKVersion > 0;
  if (SDKVersion <= 1) {
    // lookup version from D3D12Core.dll
    SDKVersion = GetD3D12SDKVersion((LPCWSTR)SDKPath);
    if (mustFind && SDKVersion == 0) {
      hlsl_test::LogErrorFmt(L"Agility SDK not found in relative path: %s",
                             (LPCWSTR)SDKPath);
      return E_FAIL;
    }
  }

  // Not found, not asked for.
  if (SDKVersion == 0)
    return S_FALSE;

  HRESULT hr = EnableAgilitySDK(hRuntime, SDKVersion, (LPCWSTR)SDKPath);
  if (FAILED(hr)) {
    // If SDKVersion provided, fail if not successful.
    // 1 means we should find it, and fill in the version automatically.
    if (mustFind) {
      hlsl_test::LogErrorFmt(
          L"Failed to set Agility SDK version %d at path: %s", SDKVersion,
          (LPCWSTR)SDKPath);
      return hr;
    }
    return S_FALSE;
  }
  if (hr == S_OK) {
    hlsl_test::LogCommentFmt(L"Agility SDK version set to: %d", SDKVersion);
  }
  return hr;
}

static HRESULT EnableExperimentalMode(HMODULE hRuntime) {
#ifdef _FORCE_EXPERIMENTAL_SHADERS
  bool bExperimentalShaderModels = true;
#else
  bool bExperimentalShaderModels =
      hlsl_test::GetTestParamBool(L"ExperimentalShaders");
#endif // _FORCE_EXPERIMENTAL_SHADERS

  HRESULT hr = S_FALSE;
  if (bExperimentalShaderModels) {
    hr = EnableExperimentalShaderModels(hRuntime);
    if (SUCCEEDED(hr)) {
      WEX::Logging::Log::Comment(L"Experimental shader models enabled.");
    }
  }

  return hr;
}

static HRESULT EnableDebugLayer() {
  // The debug layer does net yet validate DXIL programs that require
  // rewriting, but basic logging should work properly.
  HRESULT hr = S_FALSE;
  if (UseDebugIfaces()) {
    CComPtr<ID3D12Debug> debugController;
    hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
    if (SUCCEEDED(hr)) {
      debugController->EnableDebugLayer();
      hr = S_OK;
    }
  }
  return hr;
}

#endif // HLSLEXECTESTUTILS_H
