#include "HlslExecTestUtils.h"

#include "ShaderOpTest.h"
#include "dxc/Support/dxcapi.use.h"

#include "HlslTestUtils.h"

#include <Verify.h>
#include <atlcomcli.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <filesystem>
#include <optional>

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

static std::wstring computeSDKFullPath(const std::wstring &SDKPath) {
  if (std::filesystem::path(SDKPath).is_absolute())
    return SDKPath;

  std::wstring ModulePath = getModuleName();
  const size_t Pos = ModulePath.rfind('\\');

  if (Pos == std::wstring::npos)
    return SDKPath;

  if (SDKPath.substr(0, 2) != L".\\")
    return SDKPath;

  return ModulePath.substr(0, Pos) + SDKPath.substr(1);
}

static UINT getD3D12SDKVersion(std::wstring SDKPath) {
  using namespace hlsl_test;

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
    LogCommentFmt(L"%s - D3D12SDKVersion is %d", D3DCorePath.c_str(),
                  SDKVersion);
  } else {
    LogCommentFmt(L"%s - unable to load", D3DCorePath.c_str());
  }
  return SDKVersion;
}

static bool createDevice(
    ID3D12Device **D3DDevice, ExecTestUtils::D3D_SHADER_MODEL TestModel,
    bool SkipUnsupported,
    std::function<HRESULT(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **)>
        CreateDevice

) {
  if (TestModel > ExecTestUtils::D3D_HIGHEST_SHADER_MODEL) {
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
      HMODULE Module = NULL; // NOLINT

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
    HRESULT CreateHR = CreateDevice(WarpAdapter, D3D_FEATURE_LEVEL_11_0,
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

    VERIFY_SUCCEEDED(CreateDevice(HardwareAdapter, D3D_FEATURE_LEVEL_11_0,
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
    // Check for DXIL support.
    typedef struct D3D12_FEATURE_DATA_SHADER_MODEL {
      ExecTestUtils::D3D_SHADER_MODEL HighestShaderModel;
    } D3D12_FEATURE_DATA_SHADER_MODEL;
    const UINT D3D12_FEATURE_SHADER_MODEL = 7;
    D3D12_FEATURE_DATA_SHADER_MODEL SMData;
    SMData.HighestShaderModel = TestModel;
    if (FAILED(D3DDeviceCom->CheckFeatureSupport(
            (D3D12_FEATURE)D3D12_FEATURE_SHADER_MODEL, &SMData,
            sizeof(SMData))) ||
        SMData.HighestShaderModel < TestModel) {
      const UINT Minor = (UINT)TestModel & 0x0f;
      hlsl_test::LogCommentFmt(L"The selected device does not support "
                               L"shader model 6.%1u (highest is 6.%1u)",
                               Minor, SMData.HighestShaderModel & 0x0f);

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

static bool enableDebugLayer() {
  using namespace hlsl_test;

  CComPtr<ID3D12Debug> DebugController;
  HRESULT HR;
  if (FAILED(HR = D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController)))) {
    LogErrorFmt(L"Failed to get ID3D12Debug: 0x%08x", HR);
    return false;
  }

  DebugController->EnableDebugLayer();
  return true;
}

struct AgilitySDKConfiguration {
  WEX::Common::String SDKPath;
  UINT SDKVersion = 0;
  bool MustFind = false;
};

static std::optional<AgilitySDKConfiguration> getAgilitySDKConfiguration() {
  using hlsl_test::LogErrorFmt;
  using WEX::TestExecution::RuntimeParameters;

  AgilitySDKConfiguration C;

  // For global configuration, D3D12SDKPath must be relative path from .exe,
  // which means relative to TE.exe location, and must start with ".\\", such as
  // with the default: ".\\D3D12\\".
  //
  // For ID3D12DeviceFactory-style configuration, D3D12SDKPath can be an
  // absolute path.
  if (SUCCEEDED(RuntimeParameters::TryGetValue(L"D3D12SDKPath", C.SDKPath))) {
    // Make sure path ends in backslash
    if (!C.SDKPath.IsEmpty() && C.SDKPath.Right(1) != "\\")
      C.SDKPath.Append("\\");
  }

  if (C.SDKPath.IsEmpty())
    C.SDKPath = L".\\D3D12\\";

  // D3D12SDKVersion > 1 will use provided version, otherwise, auto-detect.
  // D3D12SDKVersion == 1 means fail if we can't auto-detect.
  RuntimeParameters::TryGetValue(L"D3D12SDKVersion", C.SDKVersion);

  C.MustFind = C.SDKVersion >= 1;

  if (C.SDKVersion <= 1) {
    // Use the version supported by the SDK in the path.
    C.SDKVersion = getD3D12SDKVersion(std::wstring(C.SDKPath));
    if (C.SDKVersion == 0) {
      if (C.MustFind) {
        LogErrorFmt(L"Agility SDK not found in relative path: %s",
                    static_cast<const wchar_t *>(C.SDKPath));
        return std::nullopt;
      }

      // No AgilitySDK found, caller indicated that they just want to use the
      // inbox D3D12 in this case.
      return AgilitySDKConfiguration{};
    }
  }

  return C;
}

static bool
enableGlobalAgilitySDK(const std::optional<AgilitySDKConfiguration> &C) {
  using namespace hlsl_test;

  if (!C)
    return false;

  if (C->SDKVersion == 0)
    return false;

  CComPtr<ID3D12SDKConfiguration> SDKConfig;
  HRESULT HR;
  if (FAILED(HR = D3D12GetInterface(CLSID_D3D12SDKConfiguration,
                                    IID_PPV_ARGS(&SDKConfig)))) {
    LogErrorFmt(L"Failed to get ID3D12SDKConfiguration instance: 0x%08x", HR);
    return !C->MustFind;
  }

  if (FAILED(HR = SDKConfig->SetSDKVersion(C->SDKVersion, CW2A(C->SDKPath)))) {
    LogErrorFmt(L"SetSDKVersion(%d, %s) failed: 0x%08x", C->SDKVersion,
                static_cast<const wchar_t *>(C->SDKPath), HR);
    return !C->MustFind;
  }

  // Currently, it appears that the SetSDKVersion will succeed even when
  // D3D12Core is not found, or its version doesn't match.  When that's the
  // case, will cause a failure in the very next thing that actually requires
  // D3D12Core.dll to be loaded instead.  So, we attempt to clear experimental
  // features next, which is a valid use case and a no-op at this point.  This
  // requires D3D12Core to be loaded.  If this fails, we know the AgilitySDK
  // setting actually failed.
  if (FAILED(
          HR = D3D12EnableExperimentalFeatures(0, nullptr, nullptr, nullptr))) {
    LogWarningFmt(L"D3D12EnableExperimentalFeatures(0...) failed: 0x%08x", HR);
    return !C->MustFind;
  }

  return true;
}

static bool isExperimentalShadersEnabled() {
  return hlsl_test::GetTestParamBool(L"ExperimentalShaders");
}

static bool enableGlobalExperimentalMode() {
  using namespace hlsl_test;

  if (!isExperimentalShadersEnabled())
    return false;

  HRESULT HR;
  if (FAILED(HR = D3D12EnableExperimentalFeatures(
                 1, &D3D12ExperimentalShaderModels, nullptr, nullptr))) {
    LogWarningFmt(L"D3D12EnableExperimentalFeatures("
                  L"D3D12ExperimentalShaderModels) failed: 0x%08x",
                  HR);
    return false;
  }

  return true;
}

static void
setGlobalConfiguration(const std::optional<AgilitySDKConfiguration> &C) {
  using namespace hlsl_test;

  if (enableGlobalAgilitySDK(C))
    LogCommentFmt(L"Agility SDK enabled.");
  else
    LogCommentFmt(L"Agility SDK not enabled.");

  if (enableGlobalExperimentalMode())
    LogCommentFmt(L"Experimental mode enabled.");
  else
    LogCommentFmt(L"Experimental mode not enabled.");
}

static bool enableExperimentalMode(ID3D12DeviceFactory *DeviceFactory) {
  using namespace hlsl_test;

  if (!isExperimentalShadersEnabled())
    return false;

  HRESULT HR;
  if (FAILED(HR = DeviceFactory->EnableExperimentalFeatures(
                 1, &D3D12ExperimentalShaderModels, nullptr, nullptr))) {
    LogWarningFmt(L"EnableExperimentalFeature(D3D12ExperimentalShaderModels) "
                  L"failed: 0x%08x",
                  HR);
    return false;
  }

  return true;
}

static CComPtr<ID3D12DeviceFactory>
createDeviceFactorySDK(const AgilitySDKConfiguration &C) {
  using namespace hlsl_test;

  HRESULT HR;

  CComPtr<ID3D12SDKConfiguration1> SDKConfig;
  if (FAILED(HR = D3D12GetInterface(CLSID_D3D12SDKConfiguration,
                                    IID_PPV_ARGS(&SDKConfig)))) {
    LogCommentFmt(L"Failed to get ID3D12SDKConfiguration1 interface: 0x%08x",
                  HR);
    return nullptr;
  }

  CComPtr<ID3D12DeviceFactory> DeviceFactory;
  if (FAILED(
          HR = SDKConfig->CreateDeviceFactory(C.SDKVersion, CW2A(C.SDKPath),
                                              IID_PPV_ARGS(&DeviceFactory)))) {
    LogCommentFmt(L"CreateDeviceFactory(%d, '%s', ...) failed: 0x%08x",
                  C.SDKVersion, static_cast<const wchar_t *>(C.SDKPath), HR);
    return nullptr;
  }

  LogCommentFmt(L"Using DeviceFactory for SDKVersion %d, SDKPath %s",
                C.SDKVersion, static_cast<const wchar_t *>(C.SDKPath));

  if (enableExperimentalMode(DeviceFactory))
    LogCommentFmt(L"Experimental mode enabled.");
  else
    LogCommentFmt(L"Experimental mode not enabled.");

  return DeviceFactory;
}

std::optional<D3D12SDK> D3D12SDK::create() {
  using namespace hlsl_test;

  if (enableDebugLayer())
    LogCommentFmt(L"Debug layer enabled");
  else
    LogCommentFmt(L"Debug layer not enabled");

  std::optional<AgilitySDKConfiguration> C = getAgilitySDKConfiguration();

  if (C && C->SDKVersion > 0) {
    CComPtr<ID3D12DeviceFactory> DeviceFactory = createDeviceFactorySDK(*C);
    if (DeviceFactory)
      return D3D12SDK(DeviceFactory);
  }

  setGlobalConfiguration(C);
  return D3D12SDK(nullptr);
}

D3D12SDK::D3D12SDK(CComPtr<ID3D12DeviceFactory> DeviceFactory)
    : DeviceFactory(std::move(DeviceFactory)) {}

D3D12SDK::~D3D12SDK() {
  using namespace hlsl_test;

  if (DeviceFactory) {
    DeviceFactory.Release();

    HRESULT HR;
    CComPtr<ID3D12SDKConfiguration1> SDKConfig;
    if (FAILED(HR = D3D12GetInterface(CLSID_D3D12SDKConfiguration,
                                      IID_PPV_ARGS(&SDKConfig)))) {
      LogCommentFmt(L"Failed to get ID3D12SDKConfiguration1 interface: 0x%08x",
                    HR);
      return;
    }

    // Workaround internal bug #55347376 by not calling FreeUnusedSDKs
    // SDKConfig->FreeUnusedSDKs();
  }
}

bool D3D12SDK::createDevice(ID3D12Device **D3DDevice,
                            ExecTestUtils::D3D_SHADER_MODEL TestModel,
                            bool SkipUnsupported) {

  if (DeviceFactory) {
    hlsl_test::LogCommentFmt(L"Creating device using DeviceFactory");
    return ::createDevice(
        D3DDevice, TestModel, SkipUnsupported,
        [&](IUnknown *A, D3D_FEATURE_LEVEL FL, REFIID R, void **P) {
          hlsl_test::LogCommentFmt(L"Calling DeviceFactory->CreateDevice");
          HRESULT Hr = DeviceFactory->CreateDevice(A, FL, R, P);
          hlsl_test::LogCommentFmt(L" Result: 0x%x", Hr);
          return Hr;
        });
  }

  return ::createDevice(D3DDevice, TestModel, SkipUnsupported,
                        D3D12CreateDevice);
}