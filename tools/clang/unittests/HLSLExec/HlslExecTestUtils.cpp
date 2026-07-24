#include "HlslExecTestUtils.h"

#include "ShaderOpTest.h"
#include "dxc/Support/dxcapi.use.h"

#include "HlslTestUtils.h"

#include <Verify.h>
#include <atlcomcli.h>
#include <cstddef>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <filesystem>
#include <limits>
#include <optional>

// D3D12_FEATURE_D3D12_OPTIONS_PREVIEW and its data struct are not yet in
// the released Windows SDK. Define locally so the test can query variable
// group shared memory capabilities from the Agility SDK runtime.
// This should be removed once widely supported.
#if defined(D3D12_PREVIEW_SDK_VERSION) && D3D12_PREVIEW_SDK_VERSION < 720

#ifndef D3D12_FEATURE_D3D12_OPTIONS_PREVIEW
#define D3D12_FEATURE_D3D12_OPTIONS_PREVIEW ((D3D12_FEATURE)72)
#endif

typedef struct D3D12_FEATURE_DATA_D3D12_OPTIONS_PREVIEW {
  UINT MaxGroupSharedMemoryPerGroupCS;
  UINT MaxGroupSharedMemoryPerGroupAS;
  UINT MaxGroupSharedMemoryPerGroupMS;
} D3D12_FEATURE_DATA_D3D12_OPTIONS_PREVIEW;

#endif

namespace {

constexpr D3D12_FEATURE LinearAlgebraSupportFeature =
    static_cast<D3D12_FEATURE>(77);
constexpr D3D12_FEATURE LinearAlgebraOperationSupportFeature =
    static_cast<D3D12_FEATURE>(78);

struct RuntimeLinearAlgebraTierSupport {
  UINT LinearAlgebraTier;
};

struct RuntimeMatrixConstructionSupport {
  UINT ComponentType;
  UINT WaveSize;
  UINT MinM;
  UINT MinK;
  UINT MinN;
};

struct RuntimeMatrixMultiplyShape {
  UINT M;
  UINT K;
  UINT N;
};

struct RuntimeWaveMatrixMultiplyInputs {
  UINT WaveSize;
  UINT MatrixAComponentType;
  UINT MatrixBComponentType;
  UINT AccumulatorComponentType;
};

struct RuntimeWaveMatrixMultiplySupport {
  RuntimeWaveMatrixMultiplyInputs Inputs;
  UINT SupportFlags;
  UINT NumShapes;
  RuntimeMatrixMultiplyShape *Shapes;
};

struct RuntimeThreadGroupMatrixMultiplySupport {
  RuntimeWaveMatrixMultiplyInputs WaveInputs;
  RuntimeMatrixMultiplyShape Shape;
  UINT SupportFlags;
  UINT MinThreadGroupSize;
  UINT MaxThreadGroupSize;
  UINT PreferredThreadGroupSize;
};

struct RuntimeThreadVectorMatrixMultiplySupport {
  UINT VectorInputType;
  UINT MatrixInputType;
  UINT BiasInputType;
  UINT VectorResultType;
  UINT SupportFlags;
};

struct RuntimeThreadOuterProductSupport {
  UINT InputComponentType;
  UINT ResultComponentType;
  BOOL Supported;
};

struct RuntimeAtomicAccumulateStoreSupport {
  UINT ComponentType;
  BOOL RWByteAddressBufferSupported;
  BOOL GroupSharedSupported;
};

struct RuntimeLinearAlgebraOperationSupport {
  UINT OperationType;
  union {
    RuntimeMatrixConstructionSupport MatrixConstruction;
    RuntimeWaveMatrixMultiplySupport WaveMatrixMultiply;
    RuntimeThreadGroupMatrixMultiplySupport ThreadGroupMatrixMultiply;
    RuntimeThreadVectorMatrixMultiplySupport ThreadVectorMatrixMultiply;
    RuntimeThreadOuterProductSupport ThreadOuterProduct;
    RuntimeAtomicAccumulateStoreSupport AccumulateStore;
  };
};

#if defined(DIRECT3D_LINEAR_ALGEBRA)
static_assert(static_cast<UINT>(D3D12_FEATURE_LINEAR_ALGEBRA_SUPPORT) ==
                  static_cast<UINT>(LinearAlgebraSupportFeature),
              "Linear algebra tier feature ID changed");
static_assert(
    static_cast<UINT>(
        D3D12_FEATURE_LINEAR_ALGEBRA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT) ==
        static_cast<UINT>(LinearAlgebraOperationSupportFeature),
    "Linear algebra operation feature ID changed");
static_assert(static_cast<UINT>(D3D12_LINEAR_ALGEBRA_TIER_1_0) == 0x10,
              "Linear algebra tier ABI changed");

#define ASSERT_RUNTIME_ABI(RuntimeType, D3DType)                               \
  static_assert(sizeof(RuntimeType) == sizeof(D3DType),                        \
                "Linear algebra runtime ABI size changed");                    \
  static_assert(alignof(RuntimeType) == alignof(D3DType),                      \
                "Linear algebra runtime ABI alignment changed")

ASSERT_RUNTIME_ABI(RuntimeLinearAlgebraTierSupport,
                   D3D12_FEATURE_DATA_LINEAR_ALGEBRA_SUPPORT);
ASSERT_RUNTIME_ABI(RuntimeMatrixConstructionSupport,
                   D3D12_LINEAR_ALGEBRA_MATRIX_CONSTRUCTION_SUPPORT);
ASSERT_RUNTIME_ABI(RuntimeMatrixMultiplyShape,
                   D3D12_LINEAR_ALGEBRA_MATRIX_MULTIPLY_SHAPE);
ASSERT_RUNTIME_ABI(RuntimeWaveMatrixMultiplyInputs,
                   D3D12_LINEAR_ALGEBRA_WAVE_MATRIX_MULTIPLY_INPUTS);
ASSERT_RUNTIME_ABI(RuntimeWaveMatrixMultiplySupport,
                   D3D12_LINEAR_ALGEBRA_WAVE_MATRIX_MULTIPLY_SUPPORT);
ASSERT_RUNTIME_ABI(RuntimeThreadGroupMatrixMultiplySupport,
                   D3D12_LINEAR_ALGEBRA_THREADGROUP_MATRIX_MULTIPLY_SUPPORT);
ASSERT_RUNTIME_ABI(RuntimeThreadVectorMatrixMultiplySupport,
                   D3D12_LINEAR_ALGEBRA_THREAD_VECTOR_MATRIX_MULTIPLY_SUPPORT);
ASSERT_RUNTIME_ABI(RuntimeThreadOuterProductSupport,
                   D3D12_LINEAR_ALGEBRA_THREAD_OUTER_PRODUCT_SUPPORT);
ASSERT_RUNTIME_ABI(RuntimeAtomicAccumulateStoreSupport,
                   D3D12_LINEAR_ALGEBRA_ATOMIC_ACCUMULATE_STORE_SUPPORT);
ASSERT_RUNTIME_ABI(RuntimeLinearAlgebraOperationSupport,
                   D3D12_FEATURE_DATA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT);

#undef ASSERT_RUNTIME_ABI

#define ASSERT_RUNTIME_OFFSET(RuntimeType, RuntimeField, D3DType, D3DField)    \
  static_assert(offsetof(RuntimeType, RuntimeField) ==                         \
                    offsetof(D3DType, D3DField),                               \
                "Linear algebra runtime ABI field offset changed")

ASSERT_RUNTIME_OFFSET(RuntimeMatrixConstructionSupport, ComponentType,
                      D3D12_LINEAR_ALGEBRA_MATRIX_CONSTRUCTION_SUPPORT,
                      ComponentType);
ASSERT_RUNTIME_OFFSET(RuntimeMatrixConstructionSupport, MinN,
                      D3D12_LINEAR_ALGEBRA_MATRIX_CONSTRUCTION_SUPPORT, MinN);
ASSERT_RUNTIME_OFFSET(RuntimeWaveMatrixMultiplySupport, Shapes,
                      D3D12_LINEAR_ALGEBRA_WAVE_MATRIX_MULTIPLY_SUPPORT,
                      Shapes);
ASSERT_RUNTIME_OFFSET(RuntimeThreadGroupMatrixMultiplySupport,
                      PreferredThreadGroupSize,
                      D3D12_LINEAR_ALGEBRA_THREADGROUP_MATRIX_MULTIPLY_SUPPORT,
                      PreferredThreadGroupSize);
ASSERT_RUNTIME_OFFSET(
    RuntimeThreadVectorMatrixMultiplySupport, SupportFlags,
    D3D12_LINEAR_ALGEBRA_THREAD_VECTOR_MATRIX_MULTIPLY_SUPPORT, SupportFlags);
ASSERT_RUNTIME_OFFSET(RuntimeThreadOuterProductSupport, Supported,
                      D3D12_LINEAR_ALGEBRA_THREAD_OUTER_PRODUCT_SUPPORT,
                      Supported);
ASSERT_RUNTIME_OFFSET(RuntimeAtomicAccumulateStoreSupport, GroupSharedSupported,
                      D3D12_LINEAR_ALGEBRA_ATOMIC_ACCUMULATE_STORE_SUPPORT,
                      GroupSharedSupported);
ASSERT_RUNTIME_OFFSET(
    RuntimeLinearAlgebraOperationSupport, MatrixConstruction,
    D3D12_FEATURE_DATA_LINEAR_ALGEBRA_MATRIX_OPERATION_SUPPORT,
    MatrixConstruction);

#undef ASSERT_RUNTIME_OFFSET
#endif

constexpr UINT KnownMultiplicationFlags = 0xf;

bool isValidMultiplicationFlags(UINT Flags) {
  if ((Flags & ~KnownMultiplicationFlags) != 0)
    return false;
  return Flags == 0 ||
         (Flags &
          static_cast<UINT>(linalg_test::MultiplicationFlags::Supported)) != 0;
}

LPCWSTR dataTypeName(linalg_test::DataType Type) {
  using linalg_test::DataType;
  switch (Type) {
  case DataType::None:
    return L"None";
  case DataType::SInt16:
    return L"SInt16";
  case DataType::UInt16:
    return L"UInt16";
  case DataType::SInt32:
    return L"SInt32";
  case DataType::UInt32:
    return L"UInt32";
  case DataType::Float16:
    return L"Float16";
  case DataType::Float32:
    return L"Float32";
  case DataType::SInt8:
    return L"SInt8";
  case DataType::UInt8:
    return L"UInt8";
  case DataType::Float8E4M3FN:
    return L"Float8E4M3FN";
  case DataType::Float8E5M2:
    return L"Float8E5M2";
  }
  return L"Unknown";
}

void setWaveInputs(RuntimeWaveMatrixMultiplyInputs &RuntimeInputs,
                   const linalg_test::WaveMatrixMultiplyQuery &Query) {
  RuntimeInputs.WaveSize = Query.WaveSize;
  RuntimeInputs.MatrixAComponentType =
      static_cast<UINT>(Query.MatrixAComponentType);
  RuntimeInputs.MatrixBComponentType =
      static_cast<UINT>(Query.MatrixBComponentType);
  RuntimeInputs.AccumulatorComponentType =
      static_cast<UINT>(Query.AccumulatorComponentType);
}

} // namespace

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

namespace linalg_test {

bool MatrixConstructionSupport::valid() const {
  const bool AllZero = MinM == 0 && MinK == 0 && MinN == 0;
  const bool AllNonZero = MinM != 0 && MinK != 0 && MinN != 0;
  return AllZero || AllNonZero;
}

bool MatrixConstructionSupport::supported() const {
  return valid() && MinM != 0;
}

bool MatrixConstructionSupport::supports(MatrixRole Role, UINT Rows,
                                         UINT Columns) const {
  if (!supported())
    return false;

  switch (Role) {
  case MatrixRole::A:
    return Rows >= MinM && Columns >= MinK;
  case MatrixRole::B:
    return Rows >= MinK && Columns >= MinN;
  case MatrixRole::Accumulator:
    return Rows >= MinM && Columns >= MinN;
  }
  return false;
}

bool hasFlag(MultiplicationFlags Value, MultiplicationFlags Flag) {
  return (static_cast<UINT>(Value) & static_cast<UINT>(Flag)) != 0;
}

bool WaveMatrixMultiplySupport::valid() const {
  if (!isValidMultiplicationFlags(static_cast<UINT>(SupportFlags)))
    return false;
  if (!hasFlag(SupportFlags, MultiplicationFlags::Supported))
    return Shapes.empty();
  if (Shapes.empty())
    return false;
  for (const MatrixMultiplyShape &Shape : Shapes) {
    if (Shape.M == 0 || Shape.K == 0 || Shape.N == 0)
      return false;
  }
  return true;
}

bool WaveMatrixMultiplySupport::supported() const {
  return valid() && hasFlag(SupportFlags, MultiplicationFlags::Supported);
}

bool WaveMatrixMultiplySupport::supportsShape(UINT M, UINT K, UINT N) const {
  if (!supported())
    return false;
  for (const MatrixMultiplyShape &Shape : Shapes) {
    if (Shape.M != 0 && Shape.K != 0 && Shape.N != 0 && M % Shape.M == 0 &&
        K % Shape.K == 0 && N % Shape.N == 0)
      return true;
  }
  return false;
}

bool ThreadGroupMatrixMultiplySupport::supported() const {
  return valid() && hasFlag(SupportFlags, MultiplicationFlags::Supported);
}

bool ThreadGroupMatrixMultiplySupport::valid() const {
  if (!isValidMultiplicationFlags(static_cast<UINT>(SupportFlags)))
    return false;
  if (!hasFlag(SupportFlags, MultiplicationFlags::Supported))
    return true;
  return MinThreadGroupSize != 0 && MaxThreadGroupSize >= MinThreadGroupSize &&
         MaxThreadGroupSize % MinThreadGroupSize == 0 &&
         (PreferredThreadGroupSize == 0 ||
          (PreferredThreadGroupSize >= MinThreadGroupSize &&
           PreferredThreadGroupSize <= MaxThreadGroupSize &&
           PreferredThreadGroupSize % MinThreadGroupSize == 0));
}

bool ThreadGroupMatrixMultiplySupport::supportsThreadGroupSize(
    UINT ThreadGroupSize) const {
  return supported() && MinThreadGroupSize != 0 &&
         ThreadGroupSize >= MinThreadGroupSize &&
         ThreadGroupSize <= MaxThreadGroupSize &&
         ThreadGroupSize % MinThreadGroupSize == 0;
}

bool ThreadVectorMatrixMultiplySupport::supported() const {
  return valid() && hasFlag(SupportFlags, MultiplicationFlags::Supported);
}

bool ThreadVectorMatrixMultiplySupport::valid() const {
  return isValidMultiplicationFlags(static_cast<UINT>(SupportFlags));
}

bool AtomicAccumulateStoreSupport::supports(
    AtomicDestination Destination) const {
  switch (Destination) {
  case AtomicDestination::RWByteAddressBuffer:
    return RWByteAddressBufferSupported;
  case AtomicDestination::GroupShared:
    return GroupSharedSupported;
  }
  return false;
}

ScopeFlags legalScopes(OperationType Operation) {
  const ScopeFlags Thread = static_cast<ScopeFlags>(ExecutionScope::Thread);
  const ScopeFlags Wave = static_cast<ScopeFlags>(ExecutionScope::Wave);
  const ScopeFlags ThreadGroup =
      static_cast<ScopeFlags>(ExecutionScope::ThreadGroup);
  switch (Operation) {
  case OperationType::MatrixConstruction:
    return Wave | ThreadGroup;
  case OperationType::WaveMatrixMultiply:
    return Wave;
  case OperationType::ThreadGroupMatrixMultiply:
    return ThreadGroup;
  case OperationType::ThreadVectorMatrixMultiply:
  case OperationType::ThreadOuterProduct:
    return Thread;
  case OperationType::AtomicAccumulateStore:
    // The query category spans thread vector/outer-product accumulation and
    // Wave/ThreadGroup matrix forms. Individual operations narrow this mask.
    return Thread | Wave | ThreadGroup;
  }
  return 0;
}

bool isLegalScope(OperationType Operation, ExecutionScope Scope) {
  return (legalScopes(Operation) & static_cast<ScopeFlags>(Scope)) != 0;
}

Applicability classifyApplicability(HRESULT QueryResult, bool Supported,
                                    CapabilityRequirement Requirement) {
  if (FAILED(QueryResult))
    return Applicability::Fail;
  if (Supported)
    return Applicability::Execute;
  if (Requirement == CapabilityRequirement::CapabilityGated)
    return Applicability::NotApplicable;
  return Applicability::Fail;
}

HRESULT queryTierSupport(ID3D12Device *Device, TierSupport &Support) {
  Support = {};
  if (!Device)
    return E_INVALIDARG;

  RuntimeLinearAlgebraTierSupport RuntimeSupport = {};
  const HRESULT HR = Device->CheckFeatureSupport(
      LinearAlgebraSupportFeature, &RuntimeSupport, sizeof(RuntimeSupport));
  if (FAILED(HR)) {
    LogCommentFmt(L"Linear algebra tier query failed: 0x%08x", HR);
    return HR;
  }

  if (RuntimeSupport.LinearAlgebraTier !=
          static_cast<UINT>(Tier::NotSupported) &&
      RuntimeSupport.LinearAlgebraTier != static_cast<UINT>(Tier::Tier1_0)) {
    LogCommentFmt(L"Linear algebra tier query returned invalid tier: 0x%x",
                  RuntimeSupport.LinearAlgebraTier);
    return E_UNEXPECTED;
  }

  Support.LinearAlgebraTier =
      static_cast<Tier>(RuntimeSupport.LinearAlgebraTier);
  LogCommentFmt(L"Linear algebra tier: 0x%x",
                static_cast<UINT>(Support.LinearAlgebraTier));
  return S_OK;
}

HRESULT queryMatrixConstruction(ID3D12Device *Device,
                                const MatrixConstructionQuery &Query,
                                MatrixConstructionSupport &Support) {
  Support = {};
  if (!Device)
    return E_INVALIDARG;

  RuntimeLinearAlgebraOperationSupport RuntimeSupport = {};
  RuntimeSupport.OperationType =
      static_cast<UINT>(OperationType::MatrixConstruction);
  RuntimeSupport.MatrixConstruction.ComponentType =
      static_cast<UINT>(Query.ComponentType);
  RuntimeSupport.MatrixConstruction.WaveSize = Query.WaveSize;

  const HRESULT HR =
      Device->CheckFeatureSupport(LinearAlgebraOperationSupportFeature,
                                  &RuntimeSupport, sizeof(RuntimeSupport));
  if (FAILED(HR)) {
    LogCommentFmt(
        L"MatrixConstruction query failed: type=%s, wave=%u, hr=0x%08x",
        dataTypeName(Query.ComponentType), Query.WaveSize, HR);
    return HR;
  }

  const UINT MinM = RuntimeSupport.MatrixConstruction.MinM;
  const UINT MinK = RuntimeSupport.MatrixConstruction.MinK;
  const UINT MinN = RuntimeSupport.MatrixConstruction.MinN;
  const bool AllZero = MinM == 0 && MinK == 0 && MinN == 0;
  const bool AllNonZero = MinM != 0 && MinK != 0 && MinN != 0;
  if (!AllZero && !AllNonZero) {
    LogCommentFmt(
        L"MatrixConstruction query returned malformed minima: type=%s, "
        L"wave=%u, MinM=%u, MinK=%u, MinN=%u",
        dataTypeName(Query.ComponentType), Query.WaveSize, MinM, MinK, MinN);
    return E_UNEXPECTED;
  }

  Support.MinM = MinM;
  Support.MinK = MinK;
  Support.MinN = MinN;
  LogCommentFmt(
      L"MatrixConstruction query: type=%s, wave=%u, supported=%u, MinM=%u, "
      L"MinK=%u, MinN=%u",
      dataTypeName(Query.ComponentType), Query.WaveSize, Support.supported(),
      Support.MinM, Support.MinK, Support.MinN);
  return S_OK;
}

HRESULT queryWaveMatrixMultiply(ID3D12Device *Device,
                                const WaveMatrixMultiplyQuery &Query,
                                WaveMatrixMultiplySupport &Support) {
  Support = {};
  if (!Device)
    return E_INVALIDARG;

  RuntimeLinearAlgebraOperationSupport RuntimeSupport = {};
  RuntimeSupport.OperationType =
      static_cast<UINT>(OperationType::WaveMatrixMultiply);
  setWaveInputs(RuntimeSupport.WaveMatrixMultiply.Inputs, Query);

  HRESULT HR =
      Device->CheckFeatureSupport(LinearAlgebraOperationSupportFeature,
                                  &RuntimeSupport, sizeof(RuntimeSupport));
  if (FAILED(HR)) {
    LogCommentFmt(
        L"WaveMatrixMultiply query failed: wave=%u, A=%s, B=%s, Acc=%s, "
        L"hr=0x%08x",
        Query.WaveSize, dataTypeName(Query.MatrixAComponentType),
        dataTypeName(Query.MatrixBComponentType),
        dataTypeName(Query.AccumulatorComponentType), HR);
    return HR;
  }

  if (!isValidMultiplicationFlags(
          RuntimeSupport.WaveMatrixMultiply.SupportFlags)) {
    LogCommentFmt(L"WaveMatrixMultiply query returned invalid flags: 0x%x",
                  RuntimeSupport.WaveMatrixMultiply.SupportFlags);
    return E_UNEXPECTED;
  }

  const bool Supported =
      (RuntimeSupport.WaveMatrixMultiply.SupportFlags &
       static_cast<UINT>(MultiplicationFlags::Supported)) != 0;
  if (!Supported) {
    if (RuntimeSupport.WaveMatrixMultiply.NumShapes != 0) {
      LogCommentFmt(L"Unsupported WaveMatrixMultiply query returned %u shapes",
                    RuntimeSupport.WaveMatrixMultiply.NumShapes);
      return E_UNEXPECTED;
    }
    Support.SupportFlags = static_cast<MultiplicationFlags>(
        RuntimeSupport.WaveMatrixMultiply.SupportFlags);
    LogCommentFmt(L"WaveMatrixMultiply query: wave=%u, A=%s, B=%s, Acc=%s, "
                  L"flags=0x%x, shapes=0",
                  Query.WaveSize, dataTypeName(Query.MatrixAComponentType),
                  dataTypeName(Query.MatrixBComponentType),
                  dataTypeName(Query.AccumulatorComponentType),
                  static_cast<UINT>(Support.SupportFlags));
    return S_OK;
  }

  const UINT RequestedShapes = RuntimeSupport.WaveMatrixMultiply.NumShapes;
  if (RequestedShapes == 0) {
    LogCommentFmt(
        L"Supported WaveMatrixMultiply query returned no native shapes");
    return E_UNEXPECTED;
  }

  std::vector<RuntimeMatrixMultiplyShape> RuntimeShapes(RequestedShapes);
  RuntimeSupport = {};
  RuntimeSupport.OperationType =
      static_cast<UINT>(OperationType::WaveMatrixMultiply);
  setWaveInputs(RuntimeSupport.WaveMatrixMultiply.Inputs, Query);
  RuntimeSupport.WaveMatrixMultiply.NumShapes = RequestedShapes;
  RuntimeSupport.WaveMatrixMultiply.Shapes = RuntimeShapes.data();

  HR = Device->CheckFeatureSupport(LinearAlgebraOperationSupportFeature,
                                   &RuntimeSupport, sizeof(RuntimeSupport));
  if (FAILED(HR)) {
    LogCommentFmt(
        L"WaveMatrixMultiply shape query failed: wave=%u, A=%s, B=%s, "
        L"Acc=%s, requested=%u, hr=0x%08x",
        Query.WaveSize, dataTypeName(Query.MatrixAComponentType),
        dataTypeName(Query.MatrixBComponentType),
        dataTypeName(Query.AccumulatorComponentType), RequestedShapes, HR);
    return HR;
  }

  if (!isValidMultiplicationFlags(
          RuntimeSupport.WaveMatrixMultiply.SupportFlags) ||
      (RuntimeSupport.WaveMatrixMultiply.SupportFlags &
       static_cast<UINT>(MultiplicationFlags::Supported)) == 0 ||
      RuntimeSupport.WaveMatrixMultiply.NumShapes == 0 ||
      RuntimeSupport.WaveMatrixMultiply.NumShapes > RequestedShapes) {
    LogCommentFmt(
        L"WaveMatrixMultiply shape query returned malformed flags/count: "
        L"flags=0x%x, returned=%u, requested=%u",
        RuntimeSupport.WaveMatrixMultiply.SupportFlags,
        RuntimeSupport.WaveMatrixMultiply.NumShapes, RequestedShapes);
    return E_UNEXPECTED;
  }

  Support.SupportFlags = static_cast<MultiplicationFlags>(
      RuntimeSupport.WaveMatrixMultiply.SupportFlags);
  for (UINT I = 0; I < RuntimeSupport.WaveMatrixMultiply.NumShapes; ++I) {
    const RuntimeMatrixMultiplyShape &Shape = RuntimeShapes[I];
    if (Shape.M == 0 || Shape.K == 0 || Shape.N == 0) {
      LogCommentFmt(
          L"WaveMatrixMultiply query returned zero native shape at index %u: "
          L"M=%u, K=%u, N=%u",
          I, Shape.M, Shape.K, Shape.N);
      return E_UNEXPECTED;
    }
    Support.Shapes.push_back({Shape.M, Shape.K, Shape.N});
  }

  LogCommentFmt(
      L"WaveMatrixMultiply query: wave=%u, A=%s, B=%s, Acc=%s, flags=0x%x, "
      L"shapes=%zu",
      Query.WaveSize, dataTypeName(Query.MatrixAComponentType),
      dataTypeName(Query.MatrixBComponentType),
      dataTypeName(Query.AccumulatorComponentType),
      static_cast<UINT>(Support.SupportFlags), Support.Shapes.size());
  for (size_t I = 0; I < Support.Shapes.size(); ++I) {
    const MatrixMultiplyShape &Shape = Support.Shapes[I];
    LogCommentFmt(L"  Native shape %zu: M=%u, K=%u, N=%u", I, Shape.M, Shape.K,
                  Shape.N);
  }
  return S_OK;
}

HRESULT
queryThreadGroupMatrixMultiply(ID3D12Device *Device,
                               const ThreadGroupMatrixMultiplyQuery &Query,
                               ThreadGroupMatrixMultiplySupport &Support) {
  Support = {};
  if (!Device)
    return E_INVALIDARG;

  RuntimeLinearAlgebraOperationSupport RuntimeSupport = {};
  RuntimeSupport.OperationType =
      static_cast<UINT>(OperationType::ThreadGroupMatrixMultiply);
  setWaveInputs(RuntimeSupport.ThreadGroupMatrixMultiply.WaveInputs,
                Query.WaveInputs);
  RuntimeSupport.ThreadGroupMatrixMultiply.Shape = {
      Query.Shape.M,
      Query.Shape.K,
      Query.Shape.N,
  };

  const HRESULT HR =
      Device->CheckFeatureSupport(LinearAlgebraOperationSupportFeature,
                                  &RuntimeSupport, sizeof(RuntimeSupport));
  if (FAILED(HR)) {
    LogCommentFmt(
        L"ThreadGroupMatrixMultiply query failed: wave=%u, A=%s, B=%s, "
        L"Acc=%s, shape=(%u,%u,%u), hr=0x%08x",
        Query.WaveInputs.WaveSize,
        dataTypeName(Query.WaveInputs.MatrixAComponentType),
        dataTypeName(Query.WaveInputs.MatrixBComponentType),
        dataTypeName(Query.WaveInputs.AccumulatorComponentType), Query.Shape.M,
        Query.Shape.K, Query.Shape.N, HR);
    return HR;
  }

  const UINT Flags = RuntimeSupport.ThreadGroupMatrixMultiply.SupportFlags;
  if (!isValidMultiplicationFlags(Flags)) {
    LogCommentFmt(
        L"ThreadGroupMatrixMultiply query returned invalid flags: 0x%x", Flags);
    return E_UNEXPECTED;
  }

  const bool Supported =
      (Flags & static_cast<UINT>(MultiplicationFlags::Supported)) != 0;
  const UINT MinSize =
      RuntimeSupport.ThreadGroupMatrixMultiply.MinThreadGroupSize;
  const UINT MaxSize =
      RuntimeSupport.ThreadGroupMatrixMultiply.MaxThreadGroupSize;
  const UINT PreferredSize =
      RuntimeSupport.ThreadGroupMatrixMultiply.PreferredThreadGroupSize;
  if (Supported &&
      (MinSize == 0 || MaxSize < MinSize || MaxSize % MinSize != 0 ||
       (PreferredSize != 0 &&
        (PreferredSize < MinSize || PreferredSize > MaxSize ||
         PreferredSize % MinSize != 0)))) {
    LogCommentFmt(
        L"ThreadGroupMatrixMultiply query returned malformed group sizes: "
        L"min=%u, max=%u, preferred=%u",
        MinSize, MaxSize, PreferredSize);
    return E_UNEXPECTED;
  }

  Support.SupportFlags = static_cast<MultiplicationFlags>(Flags);
  Support.MinThreadGroupSize = MinSize;
  Support.MaxThreadGroupSize = MaxSize;
  Support.PreferredThreadGroupSize = PreferredSize;
  LogCommentFmt(
      L"ThreadGroupMatrixMultiply query: wave=%u, A=%s, B=%s, Acc=%s, "
      L"shape=(%u,%u,%u), flags=0x%x, minGroup=%u, maxGroup=%u, "
      L"preferredGroup=%u",
      Query.WaveInputs.WaveSize,
      dataTypeName(Query.WaveInputs.MatrixAComponentType),
      dataTypeName(Query.WaveInputs.MatrixBComponentType),
      dataTypeName(Query.WaveInputs.AccumulatorComponentType), Query.Shape.M,
      Query.Shape.K, Query.Shape.N, Flags, MinSize, MaxSize, PreferredSize);
  return S_OK;
}

HRESULT
queryThreadVectorMatrixMultiply(ID3D12Device *Device,
                                const ThreadVectorMatrixMultiplyQuery &Query,
                                ThreadVectorMatrixMultiplySupport &Support) {
  Support = {};
  if (!Device)
    return E_INVALIDARG;

  RuntimeLinearAlgebraOperationSupport RuntimeSupport = {};
  RuntimeSupport.OperationType =
      static_cast<UINT>(OperationType::ThreadVectorMatrixMultiply);
  RuntimeSupport.ThreadVectorMatrixMultiply.VectorInputType =
      static_cast<UINT>(Query.VectorInputType);
  RuntimeSupport.ThreadVectorMatrixMultiply.MatrixInputType =
      static_cast<UINT>(Query.MatrixInputType);
  RuntimeSupport.ThreadVectorMatrixMultiply.BiasInputType =
      static_cast<UINT>(Query.BiasInputType);
  RuntimeSupport.ThreadVectorMatrixMultiply.VectorResultType =
      static_cast<UINT>(Query.VectorResultType);

  const HRESULT HR =
      Device->CheckFeatureSupport(LinearAlgebraOperationSupportFeature,
                                  &RuntimeSupport, sizeof(RuntimeSupport));
  if (FAILED(HR)) {
    LogCommentFmt(
        L"ThreadVectorMatrixMultiply query failed: vector=%s, matrix=%s, "
        L"bias=%s, result=%s, hr=0x%08x",
        dataTypeName(Query.VectorInputType),
        dataTypeName(Query.MatrixInputType), dataTypeName(Query.BiasInputType),
        dataTypeName(Query.VectorResultType), HR);
    return HR;
  }

  const UINT Flags = RuntimeSupport.ThreadVectorMatrixMultiply.SupportFlags;
  if (!isValidMultiplicationFlags(Flags)) {
    LogCommentFmt(
        L"ThreadVectorMatrixMultiply query returned invalid flags: 0x%x",
        Flags);
    return E_UNEXPECTED;
  }

  Support.SupportFlags = static_cast<MultiplicationFlags>(Flags);
  LogCommentFmt(
      L"ThreadVectorMatrixMultiply query: vector=%s, matrix=%s, bias=%s, "
      L"result=%s, flags=0x%x",
      dataTypeName(Query.VectorInputType), dataTypeName(Query.MatrixInputType),
      dataTypeName(Query.BiasInputType), dataTypeName(Query.VectorResultType),
      Flags);
  return S_OK;
}

HRESULT queryThreadOuterProduct(ID3D12Device *Device,
                                const ThreadOuterProductQuery &Query,
                                ThreadOuterProductSupport &Support) {
  Support = {};
  if (!Device)
    return E_INVALIDARG;

  RuntimeLinearAlgebraOperationSupport RuntimeSupport = {};
  RuntimeSupport.OperationType =
      static_cast<UINT>(OperationType::ThreadOuterProduct);
  RuntimeSupport.ThreadOuterProduct.InputComponentType =
      static_cast<UINT>(Query.InputComponentType);
  RuntimeSupport.ThreadOuterProduct.ResultComponentType =
      static_cast<UINT>(Query.ResultComponentType);

  const HRESULT HR =
      Device->CheckFeatureSupport(LinearAlgebraOperationSupportFeature,
                                  &RuntimeSupport, sizeof(RuntimeSupport));
  if (FAILED(HR)) {
    LogCommentFmt(
        L"ThreadOuterProduct query failed: input=%s, result=%s, hr=0x%08x",
        dataTypeName(Query.InputComponentType),
        dataTypeName(Query.ResultComponentType), HR);
    return HR;
  }

  Support.Supported = RuntimeSupport.ThreadOuterProduct.Supported != FALSE;
  LogCommentFmt(L"ThreadOuterProduct query: input=%s, result=%s, supported=%u",
                dataTypeName(Query.InputComponentType),
                dataTypeName(Query.ResultComponentType), Support.Supported);
  return S_OK;
}

HRESULT queryAtomicAccumulateStore(ID3D12Device *Device,
                                   const AtomicAccumulateStoreQuery &Query,
                                   AtomicAccumulateStoreSupport &Support) {
  Support = {};
  if (!Device)
    return E_INVALIDARG;

  RuntimeLinearAlgebraOperationSupport RuntimeSupport = {};
  RuntimeSupport.OperationType =
      static_cast<UINT>(OperationType::AtomicAccumulateStore);
  RuntimeSupport.AccumulateStore.ComponentType =
      static_cast<UINT>(Query.ComponentType);

  const HRESULT HR =
      Device->CheckFeatureSupport(LinearAlgebraOperationSupportFeature,
                                  &RuntimeSupport, sizeof(RuntimeSupport));
  if (FAILED(HR)) {
    LogCommentFmt(L"AtomicAccumulateStore query failed: type=%s, hr=0x%08x",
                  dataTypeName(Query.ComponentType), HR);
    return HR;
  }

  Support.RWByteAddressBufferSupported =
      RuntimeSupport.AccumulateStore.RWByteAddressBufferSupported != FALSE;
  Support.GroupSharedSupported =
      RuntimeSupport.AccumulateStore.GroupSharedSupported != FALSE;
  LogCommentFmt(L"AtomicAccumulateStore query: type=%s, UAV=%u, groupshared=%u",
                dataTypeName(Query.ComponentType),
                Support.RWByteAddressBufferSupported,
                Support.GroupSharedSupported);
  return S_OK;
}

} // namespace linalg_test

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

void addSRVBuffer(st::ShaderOp *Op, const char *Name, UINT64 Width,
                  const char *Init) {
  st::ShaderOpResource Res = {};
  Res.Name = Op->Strings.insert(Name);
  Res.Init = Op->Strings.insert(Init);
  Res.ReadBack = FALSE;

  Res.HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
  Res.HeapFlags = D3D12_HEAP_FLAG_NONE;
  Res.Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  Res.Desc.Width = Width;
  Res.Desc.Height = 1;
  Res.Desc.DepthOrArraySize = 1;
  Res.Desc.MipLevels = 1;
  Res.Desc.SampleDesc.Count = 1;
  Res.Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  Res.Desc.Flags = D3D12_RESOURCE_FLAG_NONE;
  Res.InitialResourceState = D3D12_RESOURCE_STATE_COPY_DEST;
  Res.TransitionTo = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

  Op->Resources.push_back(Res);
}

void addRawBufferDescriptorTable(st::ShaderOp *Op, UINT RootIndex,
                                 const char *HeapName,
                                 std::initializer_list<RawBufferView> Views) {
  if (!Op || !HeapName || Views.size() == 0 ||
      Views.size() > (std::numeric_limits<UINT>::max)()) {
    VERIFY_IS_TRUE(false, "Invalid raw buffer descriptor table");
    return;
  }

  st::ShaderOpDescriptorHeap Heap = {};
  Heap.Name = Op->Strings.insert(HeapName);
  Heap.Desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  Heap.Desc.NumDescriptors = static_cast<UINT>(Views.size());
  Heap.Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

  for (const RawBufferView &View : Views) {
    st::ShaderOpResource *Resource =
        View.ResourceName ? Op->GetResourceByName(View.ResourceName) : nullptr;
    const bool Valid =
        View.DescriptorName && View.ResourceName && View.NumBytes != 0 &&
        Resource &&
        (View.Kind == RawBufferViewKind::SRV ||
         View.Kind == RawBufferViewKind::UAV) &&
        View.FirstByte % sizeof(UINT32) == 0 &&
        View.NumBytes % sizeof(UINT32) == 0 &&
        View.FirstByte / sizeof(UINT32) <= (std::numeric_limits<UINT>::max)() &&
        View.NumBytes / sizeof(UINT32) <= (std::numeric_limits<UINT>::max)() &&
        View.FirstByte <= Resource->Desc.Width &&
        View.NumBytes <= Resource->Desc.Width - View.FirstByte;
    if (!Valid) {
      VERIFY_IS_TRUE(false, "Invalid raw buffer descriptor");
      return;
    }

    st::ShaderOpDescriptor Descriptor = {};
    Descriptor.Name = Op->Strings.insert(View.DescriptorName);
    Descriptor.ResName = Op->Strings.insert(View.ResourceName);
    if (View.Kind == RawBufferViewKind::SRV) {
      Descriptor.Kind = Op->Strings.insert("SRV");
      Descriptor.SrvDescPresent = true;
      Descriptor.SrvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
      Descriptor.SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
      Descriptor.SrvDesc.Shader4ComponentMapping =
          D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      Descriptor.SrvDesc.Buffer.FirstElement =
          static_cast<UINT>(View.FirstByte / sizeof(UINT32));
      Descriptor.SrvDesc.Buffer.NumElements =
          static_cast<UINT>(View.NumBytes / sizeof(UINT32));
      Descriptor.SrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    } else {
      Descriptor.Kind = Op->Strings.insert("UAV");
      Descriptor.UavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
      Descriptor.UavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
      Descriptor.UavDesc.Buffer.FirstElement =
          static_cast<UINT>(View.FirstByte / sizeof(UINT32));
      Descriptor.UavDesc.Buffer.NumElements =
          static_cast<UINT>(View.NumBytes / sizeof(UINT32));
      Descriptor.UavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    }
    Heap.Descriptors.push_back(Descriptor);
  }

  Op->DescriptorHeaps.push_back(std::move(Heap));

  st::ShaderOpRootValue RootValue = {};
  RootValue.HeapName = Op->Strings.insert(HeapName);
  RootValue.Index = RootIndex;
  Op->RootValues.push_back(RootValue);
}

void addRootView(st::ShaderOp *Op, UINT Index, const char *ResName) {
  st::ShaderOpRootValue RV = {};
  RV.ResName = Op->Strings.insert(ResName);
  RV.HeapName = nullptr;
  RV.Index = Index;
  Op->RootValues.push_back(RV);
}

std::shared_ptr<st::ShaderOpTestResult>
runShaderOp(ID3D12Device *Device, dxc::SpecificDllLoader &DxcSupport,
            std::unique_ptr<st::ShaderOp> Op,
            st::ShaderOpTest::TInitCallbackFn InitCallback,
            st::ShaderOpTest::TCommandCallbackFn PostDispatchCallback) {
  auto OpSet = std::make_shared<st::ShaderOpSet>();
  OpSet->ShaderOps.push_back(std::move(Op));

  return st::RunShaderOpTestAfterParse(
      Device, DxcSupport, nullptr, std::move(InitCallback),
      /*pShaderCallback=*/nullptr, std::move(PostDispatchCallback),
      std::move(OpSet));
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
    hlsl_test::LogCommentFmt(L"%S", Source);
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

#if defined(DIRECT3D_LINEAR_ALGEBRA)
UINT getLinAlgMatrixByteSize(ID3D12Device *Device, UINT NumRows,
                             UINT NumColumns,
                             D3D12_LINEAR_ALGEBRA_DATATYPE DataType,
                             D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT Layout,
                             UINT Stride) {
  CComPtr<ID3D12DevicePreview> DevicePreview;
  VERIFY_SUCCEEDED(Device->QueryInterface(IID_PPV_ARGS(&DevicePreview)));

  D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_DEST_INFO Info = {};
  Info.DestSize = 0;
  Info.DestLayout = Layout;
  Info.DestStride = Stride;
  Info.NumRows = NumRows;
  Info.NumColumns = NumColumns;
  Info.DestDataType = DataType;
  DevicePreview->GetLinearAlgebraMatrixConversionDestinationInfo(&Info);
  return Info.DestSize;
}

void recordLinAlgMatrixConversion(
    ID3D12GraphicsCommandList *List, ID3D12Resource *SrcBuffer, UINT SrcSize,
    ID3D12Resource *DestBuffer, UINT DestSize, UINT NumRows, UINT NumColumns,
    D3D12_LINEAR_ALGEBRA_DATATYPE DataType,
    D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT SrcLayout, UINT SrcStride,
    D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT DestLayout, UINT DestStride) {
  CComPtr<ID3D12GraphicsCommandListPreview> PreviewList;
  VERIFY_SUCCEEDED(List->QueryInterface(IID_PPV_ARGS(&PreviewList)));

  // Per the linear-algebra spec, ConvertLinearAlgebraMatrix (legacy barriers)
  // requires the source buffer in NON_PIXEL_SHADER_RESOURCE and the destination
  // in UNORDERED_ACCESS. The caller passes both in UNORDERED_ACCESS (the
  // ShaderOp default UAV state), so transition the source to the required read
  // state; the destination is already in UNORDERED_ACCESS.
  D3D12_RESOURCE_BARRIER Barrier = {};
  Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  Barrier.Transition.pResource = SrcBuffer;
  Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  Barrier.Transition.StateAfter =
      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  List->ResourceBarrier(1, &Barrier);

  D3D12_LINEAR_ALGEBRA_MATRIX_CONVERSION_INFO Info = {};
  Info.DestInfo.DestSize = DestSize;
  Info.DestInfo.DestLayout = DestLayout;
  Info.DestInfo.DestStride = DestStride;
  Info.DestInfo.NumRows = NumRows;
  Info.DestInfo.NumColumns = NumColumns;
  Info.DestInfo.DestDataType = DataType;
  Info.SrcInfo.SrcSize = SrcSize;
  Info.SrcInfo.SrcDataType = DataType;
  Info.SrcInfo.SrcLayout = SrcLayout;
  Info.SrcInfo.SrcStride = SrcStride;
  Info.DataDesc.DestVA = DestBuffer->GetGPUVirtualAddress();
  Info.DataDesc.SrcVA = SrcBuffer->GetGPUVirtualAddress();
  PreviewList->ConvertLinearAlgebraMatrix(&Info, 1);
}
#endif // defined(DIRECT3D_LINEAR_ALGEBRA)
