#include "HlslExecTestUtils.h"

#include "ShaderOpTest.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"

#include "HlslTestUtils.h"

#include <Verify.h>
#include <atlcomcli.h>
#include <d3d12.h>
#include <dxgi1_4.h>

static bool useDebugIfaces() { return true; }

bool useDxbc() {
#ifdef _HLK_CONF
  return false;
#else
  return hlsl_test::GetTestParamBool(L"DXBC");
#endif
}

static bool useWarpByDefault() {
#ifdef _HLK_CONF
  return false;
#else
  return true;
#endif
}

static std::wstring getModuleName() {
  wchar_t ModuleName[MAX_PATH + 1] = {0};
  const DWORD Length = GetModuleFileNameW(NULL, ModuleName, MAX_PATH);

  if (Length == 0 || Length == MAX_PATH)
    return std::wstring(); // Error condition

  return std::wstring(ModuleName, Length);
}

static std::wstring computeSDKFullPath(std::wstring SDKPath) {
  std::wstring ModulePath = getModuleName();
  const size_t Pos = ModulePath.rfind('\\');

  if (Pos == std::wstring::npos)
    return SDKPath;

  if (SDKPath.substr(0, 2) != L".\\")
    return SDKPath;

  return ModulePath.substr(0, Pos) + SDKPath.substr(1);
}

static UINT getD3D12SDKVersion(std::wstring SDKPath) {
  // Try to automatically get the D3D12SDKVersion from the DLL
  UINT SDKVersion = 0;
  std::wstring D3DCorePath = computeSDKFullPath(SDKPath);
  D3DCorePath.append(L"D3D12Core.dll");
  HMODULE D3DCore = LoadLibraryW(D3DCorePath.c_str());
  if (D3DCore) {
    if (UINT *SDKVersionOut =
            (UINT *)GetProcAddress(D3DCore, "D3D12SDKVersion"))
      SDKVersion = *SDKVersionOut;
    FreeModule(D3DCore);
  }
  return SDKVersion;
}

bool createDevice(ID3D12Device **D3DDevice, D3D_SHADER_MODEL TestModel,
                  bool SkipUnsupported) {
  if (TestModel > D3D_HIGHEST_SHADER_MODEL) {
    const UINT Minor = (UINT)TestModel & 0x0f;
    hlsl_test::LogCommentFmt(L"Installed SDK does not support "
                             L"shader model 6.%1u",
                             Minor);

    if (SkipUnsupported)
      WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);

    return false;
  }
  CComPtr<IDXGIFactory4> DXGIFactory;
  CComPtr<ID3D12Device> D3DDeviceCom;

  *D3DDevice = nullptr;

  VERIFY_SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&DXGIFactory)));
  if (hlsl_test::GetTestParamUseWARP(useWarpByDefault())) {
    // The WARP_DLL runtime parameter can be used to specify a specific DLL to
    // load.  To force this to be used, we make sure that this DLL is loaded
    // before attempting to create the device.

    struct WarpDll {
      HMODULE Module = NULL;

      ~WarpDll() { Close(); }

      void Close() {
        if (Module) {
          FreeLibrary(Module);
          Module = NULL;
        }
      }
    };

    WarpDll ExplicitlyLoadedWarpDll;
    WEX::Common::String WarpDllPath;
    if (SUCCEEDED(WEX::TestExecution::RuntimeParameters::TryGetValue(
            L"WARP_DLL", WarpDllPath))) {
      WEX::Logging::Log::Comment(WEX::Common::String().Format(
          L"WARP_DLL requested: %ls", (const wchar_t *)WarpDllPath));
      ExplicitlyLoadedWarpDll.Module = LoadLibraryExW(WarpDllPath, NULL, 0);
      VERIFY_WIN32_BOOL_SUCCEEDED(!!ExplicitlyLoadedWarpDll.Module);
    }

    // Create the WARP device
    CComPtr<IDXGIAdapter> WarpAdapter;
    VERIFY_SUCCEEDED(DXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&WarpAdapter)));
    HRESULT CreateHR = D3D12CreateDevice(WarpAdapter, D3D_FEATURE_LEVEL_11_0,
                                         IID_PPV_ARGS(&D3DDeviceCom));
    if (FAILED(CreateHR)) {
      hlsl_test::LogCommentFmt(
          L"The available version of WARP does not support d3d12.");

      if (SkipUnsupported)
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);

      return false;
    }

    // Now that the WARP device is created we can release our reference to the
    // warp dll.
    ExplicitlyLoadedWarpDll.Close();

    // Log the actual version of WARP that's loaded so we can be sure that
    // we're using the version we think.
    if (GetModuleHandleW(L"d3d10warp.dll") != NULL) {
      WCHAR FullModuleFilePath[MAX_PATH] = L"";
      GetModuleFileNameW(GetModuleHandleW(L"d3d10warp.dll"), FullModuleFilePath,
                         sizeof(FullModuleFilePath));
      WEX::Logging::Log::Comment(WEX::Common::String().Format(
          L"WARP driver loaded from: %ls", FullModuleFilePath));
    }

  } else {
    CComPtr<IDXGIAdapter1> HardwareAdapter;
    WEX::Common::String AdapterValue;
    HRESULT HR = WEX::TestExecution::RuntimeParameters::TryGetValue(
        L"Adapter", AdapterValue);
    if (SUCCEEDED(HR))
      st::GetHardwareAdapter(DXGIFactory, AdapterValue, &HardwareAdapter);
    else
      WEX::Logging::Log::Comment(
          L"Using default hardware adapter with D3D12 support.");

    VERIFY_SUCCEEDED(D3D12CreateDevice(HardwareAdapter, D3D_FEATURE_LEVEL_11_0,
                                       IID_PPV_ARGS(&D3DDeviceCom)));
  }
  // retrieve adapter information
  const LUID AdapterID = D3DDeviceCom->GetAdapterLuid();
  CComPtr<IDXGIAdapter> DXGIAdapter;
  DXGIFactory->EnumAdapterByLuid(AdapterID, IID_PPV_ARGS(&DXGIAdapter));
  DXGI_ADAPTER_DESC AdapterDesc;
  VERIFY_SUCCEEDED(DXGIAdapter->GetDesc(&AdapterDesc));
  hlsl_test::LogCommentFmt(L"Using Adapter:%s", AdapterDesc.Description);

  if (D3DDeviceCom == nullptr)
    return false;

  if (!useDxbc()) {
    D3D12_FEATURE_DATA_SHADER_MODEL SMData;
    SMData.HighestShaderModel = TestModel;
    if (FAILED(D3DDeviceCom->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,
                                                 &SMData, sizeof(SMData))) ||
        SMData.HighestShaderModel < TestModel) {
      const UINT Minor = (UINT)TestModel & 0x0f;
      hlsl_test::LogCommentFmt(L"The selected device does not support "
                               L"shader model 6.%1u",
                               Minor);

      if (SkipUnsupported)
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);

      return false;
    }
  }

  if (useDebugIfaces()) {
    CComPtr<ID3D12InfoQueue> InfoQueue;
    if (SUCCEEDED(D3DDeviceCom->QueryInterface(&InfoQueue)))
      InfoQueue->SetMuteDebugOutput(FALSE);
  }

  *D3DDevice = D3DDeviceCom.Detach();
  return true;
}

void readHlslDataIntoNewStream(LPCWSTR RelativePath, IStream **Stream,
                               dxc::SpecificDllLoader &Support) {
  VERIFY_SUCCEEDED(
      Support.InitializeForDll(dxc::kDxCompilerLib, "DxcCreateInstance"));
  CComPtr<IDxcLibrary> Library;
  CComPtr<IDxcBlobEncoding> Blob;
  CComPtr<IStream> StreamCom;
  std::wstring Path = hlsl_test::GetPathToHlslDataFile(
      RelativePath, HLSLDATAFILEPARAM, DEFAULT_EXEC_TEST_DIR);
  VERIFY_SUCCEEDED(Support.CreateInstance(CLSID_DxcLibrary, &Library));
  VERIFY_SUCCEEDED(Library->CreateBlobFromFile(Path.c_str(), nullptr, &Blob));
  VERIFY_SUCCEEDED(Library->CreateStreamFromBlobReadOnly(Blob, &StreamCom));
  *Stream = StreamCom.Detach();
}

static HRESULT enableAgilitySDK(HMODULE Runtime, UINT SDKVersion,
                                LPCWSTR SDKPath) {
  auto GetInterfaceFunc = reinterpret_cast<decltype(&D3D12GetInterface)>(
      GetProcAddress(Runtime, "D3D12GetInterface"));
  CComPtr<ID3D12SDKConfiguration> D3D12SDKConfiguration;
  IFR(GetInterfaceFunc(CLSID_D3D12SDKConfiguration,
                       IID_PPV_ARGS(&D3D12SDKConfiguration)));
  IFR(D3D12SDKConfiguration->SetSDKVersion(SDKVersion, CW2A(SDKPath)));

  // Currently, it appears that the SetSDKVersion will succeed even when
  // D3D12Core is not found, or its version doesn't match.  When that's the
  // case, will cause a failure in the very next thing that actually requires
  // D3D12Core.dll to be loaded instead.  So, we attempt to clear experimental
  // features next, which is a valid use case and a no-op at this point.  This
  // requires D3D12Core to be loaded.  If this fails, we know the AgilitySDK
  // setting actually failed.
  auto ExperimentalFeaturesFunc =
      reinterpret_cast<decltype(&D3D12EnableExperimentalFeatures)>(
          GetProcAddress(Runtime, "D3D12EnableExperimentalFeatures"));
  if (ExperimentalFeaturesFunc == nullptr)
    // If this failed, D3D12 must be too old for AgilitySDK.  But if that's
    // the case, creating D3D12SDKConfiguration should have failed.  So while
    // this case shouldn't be hit, fail if it is.
    return HRESULT_FROM_WIN32(GetLastError());

  return ExperimentalFeaturesFunc(0, nullptr, nullptr, nullptr);
}

static HRESULT
enableExperimentalShaderModels(HMODULE hRuntime,
                               UUID AdditionalFeatures[] = nullptr,
                               size_t NumAdditionalFeatures = 0) {
  auto ExperimentalFeaturesFunc =
      reinterpret_cast<decltype(&D3D12EnableExperimentalFeatures)>(
          GetProcAddress(hRuntime, "D3D12EnableExperimentalFeatures"));
  if (ExperimentalFeaturesFunc == nullptr)
    return HRESULT_FROM_WIN32(GetLastError());

  std::vector<UUID> Features;

  Features.push_back(D3D12ExperimentalShaderModels);

  if (AdditionalFeatures != nullptr && NumAdditionalFeatures > 0)
    Features.insert(Features.end(), AdditionalFeatures,
                    AdditionalFeatures + NumAdditionalFeatures);

  return ExperimentalFeaturesFunc((UINT)Features.size(), Features.data(),
                                  nullptr, nullptr);
}

static HRESULT
enableExperimentalShaderModels(UUID AdditionalFeatures[] = nullptr,
                               size_t NumAdditionalFeatures = 0) {
  HMODULE Runtime = LoadLibraryW(L"d3d12.dll");
  if (Runtime == NULL)
    return E_FAIL;
  return enableExperimentalShaderModels(Runtime, AdditionalFeatures,
                                        NumAdditionalFeatures);
}

HRESULT enableAgilitySDK(HMODULE Runtime) {
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
    if (!SDKPath.IsEmpty() && SDKPath.Right(1) != "\\")
      SDKPath.Append("\\");
  }

  if (SDKPath.IsEmpty())
    SDKPath = L".\\D3D12\\";

  const bool MustFind = SDKVersion > 0;
  if (SDKVersion <= 1) {
    // lookup version from D3D12Core.dll
    SDKVersion = getD3D12SDKVersion((LPCWSTR)SDKPath);
    if (MustFind && SDKVersion == 0) {
      hlsl_test::LogErrorFmt(L"Agility SDK not found in relative path: %s",
                             (LPCWSTR)SDKPath);
      return E_FAIL;
    }
  }

  // Not found, not asked for.
  if (SDKVersion == 0)
    return S_FALSE;

  HRESULT HR = enableAgilitySDK(Runtime, SDKVersion, (LPCWSTR)SDKPath);
  if (FAILED(HR)) {
    // If SDKVersion provided, fail if not successful.
    // 1 means we should find it, and fill in the version automatically.
    if (MustFind) {
      hlsl_test::LogErrorFmt(
          L"Failed to set Agility SDK version %d at path: %s", SDKVersion,
          (LPCWSTR)SDKPath);
      return HR;
    }
    return S_FALSE;
  }
  if (HR == S_OK)
    hlsl_test::LogCommentFmt(L"Agility SDK version set to: %d", SDKVersion);

  return HR;
}

HRESULT enableExperimentalMode(HMODULE Runtime) {
#ifdef _FORCE_EXPERIMENTAL_SHADERS
  bool ExperimentalShaderModels = true;
#else
  bool ExperimentalShaderModels =
      hlsl_test::GetTestParamBool(L"ExperimentalShaders");
#endif // _FORCE_EXPERIMENTAL_SHADERS

  HRESULT HR = S_FALSE;
  if (ExperimentalShaderModels) {
    HR = enableExperimentalShaderModels(Runtime);
    if (SUCCEEDED(HR))
      WEX::Logging::Log::Comment(L"Experimental shader models enabled.");
  }

  return HR;
}

HRESULT enableDebugLayer() {
  // The debug layer does net yet validate DXIL programs that require
  // rewriting, but basic logging should work properly.
  HRESULT HR = S_FALSE;
  if (useDebugIfaces()) {
    CComPtr<ID3D12Debug> DebugController;
    HR = D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController));
    if (SUCCEEDED(HR)) {
      DebugController->EnableDebugLayer();
      HR = S_OK;
    }
  }
  return HR;
}
