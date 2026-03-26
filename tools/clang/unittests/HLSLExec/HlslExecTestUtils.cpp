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

// D3D12_FEATURE_D3D12_OPTIONS_PREVIEW and its data struct are not yet in
// the released Windows SDK. Define locally so the test can query variable
// group shared memory capabilities from the Agility SDK runtime.
// Once the public SDK ships with these, a compile break (redefinition error)
// will signal that these local definitions should be removed.
#ifndef D3D12_FEATURE_D3D12_OPTIONS_PREVIEW
#define D3D12_FEATURE_D3D12_OPTIONS_PREVIEW ((D3D12_FEATURE)72)
#endif

typedef struct D3D12_FEATURE_DATA_D3D12_OPTIONS_PREVIEW {
  UINT MaxGroupSharedMemoryPerGroupCS;
  UINT MaxGroupSharedMemoryPerGroupAS;
  UINT MaxGroupSharedMemoryPerGroupMS;
} D3D12_FEATURE_DATA_D3D12_OPTIONS_PREVIEW;

using namespace hlsl_test;

static bool useDebugIfaces() { return true; }

bool useDxbc() {
#ifdef _HLK_CONF
  return false;
#else
  return GetTestParamBool(L"DXBC");
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
    ID3D12Device **D3DDevice, D3D_SHADER_MODEL TestModel, bool SkipUnsupported,
    std::function<HRESULT(IUnknown *, D3D_FEATURE_LEVEL, REFIID, void **)>
        CreateDeviceFn

) {
  if (*D3DDevice)
    LogErrorFmt(L"createDevice called with non-null *D3DDevice - "
                L"this will likely leak the previous device");
  if (TestModel > DXC_HIGHEST_SHADER_MODEL) {
    const UINT Minor = (UINT)TestModel & 0x0f;
    LogCommentFmt(L"Installed SDK does not support "
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
  if (GetTestParamUseWARP(useWarpByDefault())) {
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
    HRESULT CreateHR = CreateDeviceFn(WarpAdapter, D3D_FEATURE_LEVEL_11_0,
                                      IID_PPV_ARGS(&D3DDeviceCom));
    if (FAILED(CreateHR)) {
      LogCommentFmt(L"Failed to create WARP device: 0x%08x", CreateHR);

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

    VERIFY_SUCCEEDED(CreateDeviceFn(HardwareAdapter, D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&D3DDeviceCom)));
  }
  // retrieve adapter information
  const LUID AdapterID = D3DDeviceCom->GetAdapterLuid();
  CComPtr<IDXGIAdapter> DXGIAdapter;
  DXGIFactory->EnumAdapterByLuid(AdapterID, IID_PPV_ARGS(&DXGIAdapter));
  DXGI_ADAPTER_DESC AdapterDesc;
  VERIFY_SUCCEEDED(DXGIAdapter->GetDesc(&AdapterDesc));
  LogCommentFmt(L"Using Adapter:%s", AdapterDesc.Description);

  if (D3DDeviceCom == nullptr)
    return false;

  if (!useDxbc()) {
    D3D12_FEATURE_DATA_SHADER_MODEL SMData;
    SMData.HighestShaderModel = TestModel;
    if (FAILED(D3DDeviceCom->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,
                                                 &SMData, sizeof(SMData))) ||
        SMData.HighestShaderModel < TestModel) {
      const UINT Minor = (UINT)TestModel & 0x0f;
      LogCommentFmt(L"The selected device does not support "
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
  std::wstring Path = GetPathToHlslDataFile(RelativePath, HLSLDATAFILEPARAM,
                                            DEFAULT_EXEC_TEST_DIR);
  VERIFY_SUCCEEDED(Support.CreateInstance(CLSID_DxcLibrary, &Library));
  VERIFY_SUCCEEDED(Library->CreateBlobFromFile(Path.c_str(), nullptr, &Blob));
  VERIFY_SUCCEEDED(Library->CreateStreamFromBlobReadOnly(Blob, &StreamCom));
  *Stream = StreamCom.Detach();
}

static bool enableDebugLayer() {
  CComPtr<ID3D12Debug> DebugController;
  HRESULT HR;
  if (FAILED(HR = D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController)))) {
    LogWarningFmt(L"Failed to get ID3D12Debug: 0x%08x", HR);
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
  using WEX::TestExecution::RuntimeParameters;

  AgilitySDKConfiguration C;

  // For global configuration, D3D12SDKPath must be a relative path from the
  // .exe, meaning it should be relative to the TE.exe location and must start
  // with ".\", such as with the default: ".\D3D12\".
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
        LogErrorFmt(L"Agility SDK not found in path: %s",
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
  if (!C)
    return false;

  if (C->SDKVersion == 0)
    return false;

  CComPtr<ID3D12SDKConfiguration> SDKConfig;
  HRESULT HR;
  if (FAILED(HR = D3D12GetInterface(CLSID_D3D12SDKConfiguration,
                                    IID_PPV_ARGS(&SDKConfig)))) {
    LogWarningFmt(L"Failed to get ID3D12SDKConfiguration instance: 0x%08x", HR);
    return !C->MustFind;
  }

  if (FAILED(HR = SDKConfig->SetSDKVersion(C->SDKVersion, CW2A(C->SDKPath)))) {
    LogWarningFmt(L"SetSDKVersion(%d, %s) failed: 0x%08x", C->SDKVersion,
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
  return GetTestParamBool(L"ExperimentalShaders");
}

static bool enableGlobalExperimentalMode() {
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

D3D12SDKSelector::D3D12SDKSelector() {
  if (enableDebugLayer())
    LogCommentFmt(L"Debug layer enabled");
  else
    LogCommentFmt(L"Debug layer not enabled");

  std::optional<AgilitySDKConfiguration> C = getAgilitySDKConfiguration();

  if (C && C->SDKVersion > 0) {
    CComPtr<ID3D12DeviceFactory> DeviceFactory = createDeviceFactorySDK(*C);
    if (DeviceFactory) {
      this->DeviceFactory = DeviceFactory;
      return;
    }
  }

  setGlobalConfiguration(C);
}

D3D12SDKSelector::~D3D12SDKSelector() {
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

bool D3D12SDKSelector::createDevice(ID3D12Device **D3DDevice,
                                    D3D_SHADER_MODEL TestModel,
                                    bool SkipUnsupported) {

  if (DeviceFactory) {
    LogCommentFmt(L"Creating device using DeviceFactory");
    return ::createDevice(
        D3DDevice, TestModel, SkipUnsupported,
        [&](IUnknown *A, D3D_FEATURE_LEVEL FL, REFIID R, void **P) {
          LogCommentFmt(L"Calling DeviceFactory->CreateDevice");
          HRESULT HR = DeviceFactory->CreateDevice(A, FL, R, P);
          LogCommentFmt(L" Result: 0x%x", HR);
          return HR;
        });
  }

  return ::createDevice(D3DDevice, TestModel, SkipUnsupported,
                        D3D12CreateDevice);
}

bool doesDeviceSupportInt64(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 O;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &O, sizeof(O))))
    return false;
  return O.Int64ShaderOps != FALSE;
}

bool doesDeviceSupportDouble(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS O;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS, &O, sizeof(O))))
    return false;
  return O.DoublePrecisionFloatShaderOps != FALSE;
}

bool doesDeviceSupportWaveOps(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 O;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &O, sizeof(O))))
    return false;
  return O.WaveOps != FALSE;
}

bool doesDeviceSupportBarycentrics(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS3 O;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS3, &O, sizeof(O))))
    return false;
  return O.BarycentricsSupported != FALSE;
}

bool doesDeviceSupportNative16bitOps(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS4 O;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS4, &O, sizeof(O))))
    return false;
  return O.Native16BitShaderOpsSupported != FALSE;
}

bool doesDeviceSupportMeshShaders(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS7 O7;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS7, &O7, sizeof(O7))))
    return false;
  return O7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
}

bool doesDeviceSupportRayTracing(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS5 O5;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS5, &O5, sizeof(O5))))
    return false;
  return O5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

bool doesDeviceSupportMeshAmpDerivatives(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS7 O7;
  D3D12_FEATURE_DATA_D3D12_OPTIONS9 O9;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS7, &O7, sizeof(O7))) ||
      FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS9, &O9, sizeof(O9))))
    return false;
  return O7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED &&
         O9.DerivativesInMeshAndAmplificationShadersSupported != FALSE;
}

bool doesDeviceSupportTyped64Atomics(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS9 O9;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS9, &O9, sizeof(O9))))
    return false;
  return O9.AtomicInt64OnTypedResourceSupported != FALSE;
}

bool doesDeviceSupportHeap64Atomics(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS11 O11;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS11, &O11, sizeof(O11))))
    return false;
  return O11.AtomicInt64OnDescriptorHeapResourceSupported != FALSE;
}

bool doesDeviceSupportShared64Atomics(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS9 O9;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS9, &O9, sizeof(O9))))
    return false;
  return O9.AtomicInt64OnGroupSharedSupported != FALSE;
}

bool doesDeviceSupportAdvancedTexOps(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS14 O14;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS14, &O14, sizeof(O14))))
    return false;
  return O14.AdvancedTextureOpsSupported != FALSE;
}

bool doesDeviceSupportWritableMSAA(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS14 O14;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS14, &O14, sizeof(O14))))
    return false;
  return O14.WriteableMSAATexturesSupported != FALSE;
}

bool doesDeviceSupportEnhancedBarriers(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS12 O12;
  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS12, &O12, sizeof(O12))))
    return false;
  return O12.EnhancedBarriersSupported != FALSE;
}

bool doesDeviceSupportRelaxedFormatCasting(ID3D12Device *pDevice) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS12 O12;
  if (!doesDeviceSupportEnhancedBarriers(pDevice))
    return false;

  if (FAILED(pDevice->CheckFeatureSupport(
          (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS12, &O12, sizeof(O12))))
    return false;
  return O12.RelaxedFormatCastingSupported != FALSE;
}

bool isFallbackPathEnabled() {
  // Enable fallback paths with: /p:"EnableFallback=1"
  UINT EnableFallbackValue = 0;
  WEX::TestExecution::RuntimeParameters::TryGetValue(L"EnableFallback",
                                                     EnableFallbackValue);
  return EnableFallbackValue != 0;
}

UINT getMaxGroupSharedMemoryCS(ID3D12Device *Device) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS_PREVIEW O = {};
  VERIFY_SUCCEEDED(Device->CheckFeatureSupport(
      D3D12_FEATURE_D3D12_OPTIONS_PREVIEW, &O, sizeof(O)));
  return O.MaxGroupSharedMemoryPerGroupCS;
}

UINT getMaxGroupSharedMemoryAS(ID3D12Device *Device) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS_PREVIEW O = {};
  VERIFY_SUCCEEDED(Device->CheckFeatureSupport(
      D3D12_FEATURE_D3D12_OPTIONS_PREVIEW, &O, sizeof(O)));
  return O.MaxGroupSharedMemoryPerGroupAS;
}

UINT getMaxGroupSharedMemoryMS(ID3D12Device *Device) {
  D3D12_FEATURE_DATA_D3D12_OPTIONS_PREVIEW O = {};
  VERIFY_SUCCEEDED(Device->CheckFeatureSupport(
      D3D12_FEATURE_D3D12_OPTIONS_PREVIEW, &O, sizeof(O)));
  return O.MaxGroupSharedMemoryPerGroupMS;
}

std::unique_ptr<st::ShaderOp> createComputeOp(const char *Source,
                                              const char *Target,
                                              const char *RootSig,
                                              const char *Args, UINT DispatchX,
                                              UINT DispatchY, UINT DispatchZ) {
  auto Op = std::make_unique<st::ShaderOp>();
  LPCSTR CSName = Op->Strings.insert("CS");
  Op->Name = CSName;
  Op->CS = CSName;
  Op->RootSignature = Op->Strings.insert(RootSig);
  Op->DispatchX = DispatchX;
  Op->DispatchY = DispatchY;
  Op->DispatchZ = DispatchZ;
  Op->UseWarpDevice = true;

  st::ShaderOpShader Shader = {};
  Shader.Name = CSName;
  Shader.Target = Op->Strings.insert(Target);
  Shader.EntryPoint = Op->Strings.insert("main");
  Shader.Text = Op->Strings.insert(Source);
  Shader.Arguments = Args ? Op->Strings.insert(Args) : nullptr;
  Shader.Compiled = FALSE;
  Shader.Callback = FALSE;
  Op->Shaders.push_back(Shader);

  return Op;
}

void addUAVBuffer(st::ShaderOp *Op, const char *Name, UINT64 Width,
                  bool ReadBack, const char *Init) {
  st::ShaderOpResource Res = {};
  Res.Name = Op->Strings.insert(Name);
  Res.Init = Op->Strings.insert(Init);
  Res.ReadBack = ReadBack ? TRUE : FALSE;

  Res.HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
  Res.HeapFlags = D3D12_HEAP_FLAG_NONE;
  Res.Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  Res.Desc.Width = Width;
  Res.Desc.Height = 1;
  Res.Desc.DepthOrArraySize = 1;
  Res.Desc.MipLevels = 1;
  Res.Desc.SampleDesc.Count = 1;
  Res.Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  Res.Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  Res.InitialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
  Res.TransitionTo = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

  Op->Resources.push_back(Res);
}

void addRootUAV(st::ShaderOp *Op, UINT Index, const char *ResName) {
  st::ShaderOpRootValue RV = {};
  RV.ResName = Op->Strings.insert(ResName);
  RV.HeapName = nullptr;
  RV.Index = Index;
  Op->RootValues.push_back(RV);
}

std::shared_ptr<st::ShaderOpTestResult>
runShaderOp(ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
            std::unique_ptr<st::ShaderOp> Op,
            st::ShaderOpTest::TInitCallbackFn InitCallback) {
  auto OpSet = std::make_shared<st::ShaderOpSet>();
  OpSet->ShaderOps.push_back(std::move(Op));

  return st::RunShaderOpTestAfterParse(
      Device, DxcSupport, nullptr, std::move(InitCallback), std::move(OpSet));
}

void compileShader(dxc::SpecificDllLoader &DxcSupport, const char *Source,
                   const char *Target, const std::string &Args,
                   bool VerboseLogging) {
  CComPtr<IDxcCompiler3> Compiler;
  VERIFY_SUCCEEDED(DxcSupport.CreateInstance(CLSID_DxcCompiler, &Compiler));

  CComPtr<IDxcUtils> Utils;
  VERIFY_SUCCEEDED(DxcSupport.CreateInstance(CLSID_DxcUtils, &Utils));

  CComPtr<IDxcBlobEncoding> SourceBlob;
  VERIFY_SUCCEEDED(Utils->CreateBlobFromPinned(
      Source, static_cast<UINT32>(strlen(Source)), DXC_CP_UTF8, &SourceBlob));

  // Build wide-string argument list: -T <target> -E main <extra args>.
  std::vector<std::wstring> WArgStorage;
  WArgStorage.push_back(L"-T");
  WArgStorage.push_back(std::wstring(Target, Target + strlen(Target)));
  WArgStorage.push_back(L"-E");
  WArgStorage.push_back(L"main");

  // Tokenize the additional arguments string.
  std::istringstream SS(Args);
  std::string Tok;
  while (SS >> Tok)
    WArgStorage.push_back(std::wstring(Tok.begin(), Tok.end()));

  std::vector<LPCWSTR> WArgPtrs;
  std::wstringstream LogFlags;
  LogFlags << L"Compiling with flags:";
  for (const auto &A : WArgStorage) {
    WArgPtrs.push_back(A.c_str());
    LogFlags << L" " << A;
  }

  DxcBuffer Buf = {};
  Buf.Ptr = SourceBlob->GetBufferPointer();
  Buf.Size = SourceBlob->GetBufferSize();
  Buf.Encoding = DXC_CP_UTF8;

  if (VerboseLogging) {
      hlsl_test::LogCommentFmt(L"Shader Source:");
      hlsl_test::LogCommentFmt(L"%c", Source);
  }

  hlsl_test::LogCommentFmt(LogFlags.str().c_str());

  CComPtr<IDxcResult> Result;
  VERIFY_SUCCEEDED(Compiler->Compile(&Buf, WArgPtrs.data(),
                                     static_cast<UINT32>(WArgPtrs.size()),
                                     nullptr, IID_PPV_ARGS(&Result)));

  HRESULT HR;
  VERIFY_SUCCEEDED(Result->GetStatus(&HR));

  if (FAILED(HR)) {
    CComPtr<IDxcBlobUtf8> Errors;
    Result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&Errors), nullptr);
    if (Errors && Errors->GetStringLength() > 0)
      hlsl_test::LogErrorFmt(L"Shader compilation failed:\n%S",
                             Errors->GetStringPointer());
    VERIFY_SUCCEEDED(HR);
  }
}
