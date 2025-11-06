#include "HlslExecTestUtils.h"

#include "ShaderOpTest.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Test/HlslTestUtils.h"
#include <Verify.h>
#include <atlcomcli.h>
#include <d3d12.h>
#include <dxgi1_4.h>
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

// A more recent Windows SDK than currently required is needed for these.
typedef HRESULT(WINAPI *D3D12EnableExperimentalFeaturesFn)(
    UINT NumFeatures, __in_ecount(NumFeatures) const IID *IIDs,
    __in_ecount_opt(NumFeatures) void *ConfigurationStructs,
    __in_ecount_opt(NumFeatures) UINT *ConfigurationStructSizes);

static const GUID D3D12ExperimentalShaderModelsID =
    {/* 76f5573e-f13a-40f5-b297-81ce9e18933f */
     0x76f5573e,
     0xf13a,
     0x40f5,
     {0xb2, 0x97, 0x81, 0xce, 0x9e, 0x18, 0x93, 0x3f}};

// Used to create D3D12SDKConfiguration to enable AgilitySDK programmatically.
typedef HRESULT(WINAPI *D3D12GetInterfaceFn)(REFCLSID Rclsid, REFIID Riid,
                                             void **Debug);

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

bool createDevice(ID3D12Device **D3DDevice,
                  ExecTestUtils::D3D_SHADER_MODEL TestModel,
                  bool SkipUnsupported) {
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
  D3D12GetInterfaceFn GetInterfaceFunc =
      (D3D12GetInterfaceFn)GetProcAddress(Runtime, "D3D12GetInterface");
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
  D3D12EnableExperimentalFeaturesFn ExperimentalFeaturesFunc =
      (D3D12EnableExperimentalFeaturesFn)GetProcAddress(
          Runtime, "D3D12EnableExperimentalFeatures");
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
  D3D12EnableExperimentalFeaturesFn ExperimentalFeaturesFunc =
      (D3D12EnableExperimentalFeaturesFn)GetProcAddress(
          hRuntime, "D3D12EnableExperimentalFeatures");
  if (ExperimentalFeaturesFunc == nullptr)
    return HRESULT_FROM_WIN32(GetLastError());

  std::vector<UUID> Features;

  Features.push_back(D3D12ExperimentalShaderModelsID);

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

static bool enableGlobalAgilitySDK() {
  using namespace hlsl_test;

  std::optional<AgilitySDKConfiguration> C = getAgilitySDKConfiguration();
  if (!C)
    return false;

  if (C->SDKVersion == 0)
    return true;

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
    LogErrorFmt(L"D3D12EnableExperimentalFeatures(0...) failed: 0x%08x", HR);
    return !C->MustFind;
  }

  return true;
}

static bool enableGlobalExperimentalMode() {
  using namespace hlsl_test;

  const bool ExperimentalShaders = GetTestParamBool(L"ExperimentalShaders");

  if (!ExperimentalShaders)
    return false;

  HRESULT HR;
  if (FAILED(HR = D3D12EnableExperimentalFeatures(
                 1, &D3D12ExperimentalShaderModels, nullptr, nullptr))) {
    LogErrorFmt(L"D3D12EnableExperimentalFeatures("
                L"D3D12ExperimentalShaderModels) failed: 0x%08x",
                HR);
    return false;
  }

  return true;
}

static void setGlobalConfiguration() {
  using namespace hlsl_test;

  if (enableGlobalAgilitySDK())
    LogCommentFmt(L"Agility SDK enabled.");
  else
    LogCommentFmt(L"Agility SDK not enabled.");

  if (enableGlobalExperimentalMode())
    LogCommentFmt(L"Experimental mode enabled.");
  else
    LogCommentFmt(L"Experimental mode not enabled.");
}

std::optional<D3D12SDK> D3D12SDK::create() {
  using namespace hlsl_test;

  if (enableDebugLayer())
    LogCommentFmt(L"Debug layer enabled");
  else
    LogCommentFmt(L"Debug layer not enabled");

  //   CComPtr<ID3D12SDKConfiguration1> Config1;
  //   if (FAILED(D3D12GetInterface(CLSID_D3D12SDKConfiguration,
  //                                IID_PPV_ARGS(&Config1))))
  { return D3D12SDK(nullptr); }

  CComPtr<ID3D12DeviceFactory> DeviceFactory;

  // ...

  return D3D12SDK(DeviceFactory);
}

D3D12SDK::D3D12SDK(CComPtr<ID3D12DeviceFactory> DeviceFactory)
    : DeviceFactory(std::move(DeviceFactory)) {}

D3D12SDK::~D3D12SDK() = default;

bool D3D12SDK::createDevice(ID3D12Device **D3DDevice,
                            ExecTestUtils::D3D_SHADER_MODEL TestModel,
                            bool SkipUnsupported) {
  return ::createDevice(D3DDevice, TestModel, SkipUnsupported);
}