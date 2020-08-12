///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ExecutionTest.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// These tests run by executing compiled programs, and thus involve more     //
// moving parts, like the runtime and drivers.                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <unordered_set>
#include <strstream>
#include <iomanip>
#include "dxc/Test/CompilationResult.h"
#include "dxc/Test/HLSLTestData.h"
#include <Shlwapi.h>
#include <atlcoll.h>
#include <locale>
#include <algorithm>

#undef _read
#include "WexTestClass.h"
#include "dxc/Test/HlslTestUtils.h"
#include "dxc/Test/DxcTestUtils.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Unicode.h"

//
// d3d12.h and dxgi1_4.h are included in the Windows 10 SDK
// https://msdn.microsoft.com/en-us/library/windows/desktop/dn899120(v=vs.85).aspx
// https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk
//
#include <d3d12.h>
#include <dxgi1_4.h>
#include <DXGIDebug.h>
#include "dxc/Support/d3dx12.h"
#include <DirectXMath.h>
#include <strsafe.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include "ShaderOpTest.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "version.lib")

// A more recent Windows SDK than currently required is needed for these.
typedef HRESULT(WINAPI *D3D12EnableExperimentalFeaturesFn)(
  UINT                                    NumFeatures,
  __in_ecount(NumFeatures) const IID*     pIIDs,
  __in_ecount_opt(NumFeatures) void*      pConfigurationStructs,
  __in_ecount_opt(NumFeatures) UINT*      pConfigurationStructSizes);

static const GUID D3D12ExperimentalShaderModelsID = { /* 76f5573e-f13a-40f5-b297-81ce9e18933f */
  0x76f5573e,
  0xf13a,
  0x40f5,
  { 0xb2, 0x97, 0x81, 0xce, 0x9e, 0x18, 0x93, 0x3f }
};

using namespace DirectX;
using namespace hlsl_test;


template <typename TSequence, typename T>
static bool contains(TSequence s, const T &val) {
  return std::cend(s) != std::find(std::cbegin(s), std::cend(s), val);
}

template <typename InputIterator, typename T>
static bool contains(InputIterator b, InputIterator e, const T &val) {
  return e != std::find(b, e, val);
}

static HRESULT EnableExperimentalShaderModels() {
  HMODULE hRuntime = LoadLibraryW(L"d3d12.dll");
  if (hRuntime == NULL) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  D3D12EnableExperimentalFeaturesFn pD3D12EnableExperimentalFeatures =
    (D3D12EnableExperimentalFeaturesFn)GetProcAddress(hRuntime, "D3D12EnableExperimentalFeatures");
  if (pD3D12EnableExperimentalFeatures == nullptr) {
    FreeLibrary(hRuntime);
    return HRESULT_FROM_WIN32(GetLastError());
  }

  HRESULT hr = pD3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModelsID, nullptr, nullptr);
  FreeLibrary(hRuntime);
  return hr;
}

static HRESULT ReportLiveObjects() {
  CComPtr<IDXGIDebug1> pDebug;
  IFR(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug)));
  IFR(pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL));
  return S_OK;
}

static void WriteInfoQueueMessages(void *pStrCtx, st::OutputStringFn pOutputStrFn, ID3D12InfoQueue *pInfoQueue) {
  bool allMessagesOK = true;
  UINT64 count = pInfoQueue->GetNumStoredMessages();
  CAtlArray<BYTE> message;
  for (UINT64 i = 0; i < count; ++i) {
    // 'GetMessageA' rather than 'GetMessage' is an artifact of user32 headers.
    SIZE_T msgLen = 0;
    if (FAILED(pInfoQueue->GetMessageA(i, nullptr, &msgLen))) {
      allMessagesOK = false;
      continue;
    }
    if (message.GetCount() < msgLen) {
      if (!message.SetCount(msgLen)) {
        allMessagesOK = false;
        continue;
      }
    }
    D3D12_MESSAGE *pMessage = (D3D12_MESSAGE *)message.GetData();
    if (FAILED(pInfoQueue->GetMessageA(i, pMessage, &msgLen))) {
      allMessagesOK = false;
      continue;
    }
    CA2W msgW(pMessage->pDescription, CP_ACP);
    pOutputStrFn(pStrCtx, msgW.m_psz);
    pOutputStrFn(pStrCtx, L"\r\n");
  }
  if (!allMessagesOK) {
    pOutputStrFn(pStrCtx, L"Failed to retrieve some messages.\r\n");
  }
}

class CComContext {
private:
  bool m_init;
public:
  CComContext() : m_init(false) {}
  ~CComContext() { Dispose(); }
  void Dispose() { if (!m_init) return; m_init = false; CoUninitialize(); }
  HRESULT Init() { HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED); if (SUCCEEDED(hr)) { m_init = true; } return hr; }
};

static void SavePixelsToFile(LPCVOID pPixels, DXGI_FORMAT format, UINT32 m_width, UINT32 m_height, LPCWSTR pFileName) {
  CComContext ctx;
  CComPtr<IWICImagingFactory> pFactory;
  CComPtr<IWICBitmap> pBitmap;
  CComPtr<IWICBitmapEncoder> pEncoder;
  CComPtr<IWICBitmapFrameEncode> pFrameEncode;
  CComPtr<hlsl::AbstractMemoryStream> pStream;
  CComPtr<IMalloc> pMalloc;

  struct PF {
    DXGI_FORMAT Format;
    GUID PixelFormat;
    UINT32 PixelSize;
    bool operator==(DXGI_FORMAT F) const {
      return F == Format;
    }
  } Vals[] = {
    // Add more pixel format mappings as needed.
    { DXGI_FORMAT_R8G8B8A8_UNORM, GUID_WICPixelFormat32bppRGBA, 4 }
  };
  PF *pFormat = std::find(Vals, Vals + _countof(Vals), format);

  VERIFY_SUCCEEDED(ctx.Init());
  VERIFY_SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*)&pFactory));
  VERIFY_SUCCEEDED(CoGetMalloc(1, &pMalloc));
  VERIFY_SUCCEEDED(hlsl::CreateMemoryStream(pMalloc, &pStream));
  VERIFY_ARE_NOT_EQUAL(pFormat, Vals + _countof(Vals));
  VERIFY_SUCCEEDED(pFactory->CreateBitmapFromMemory(m_width, m_height, pFormat->PixelFormat, m_width * pFormat->PixelSize, m_width * m_height * pFormat->PixelSize, (BYTE *)pPixels, &pBitmap));
  VERIFY_SUCCEEDED(pFactory->CreateEncoder(GUID_ContainerFormatBmp, nullptr, &pEncoder));
  VERIFY_SUCCEEDED(pEncoder->Initialize(pStream, WICBitmapEncoderNoCache));
  VERIFY_SUCCEEDED(pEncoder->CreateNewFrame(&pFrameEncode, nullptr));
  VERIFY_SUCCEEDED(pFrameEncode->Initialize(nullptr));
  VERIFY_SUCCEEDED(pFrameEncode->WriteSource(pBitmap, nullptr));
  VERIFY_SUCCEEDED(pFrameEncode->Commit());
  VERIFY_SUCCEEDED(pEncoder->Commit());
  hlsl::WriteBinaryFile(pFileName, pStream->GetPtr(), pStream->GetPtrSize());
}

// Checks if the given warp version supports the given operation.
bool IsValidWarpDllVersion(unsigned int minBuildNumber) {
    HMODULE pLibrary = LoadLibrary("D3D10Warp.dll");
    if (pLibrary) {
        char path[MAX_PATH];
        DWORD length = GetModuleFileName(pLibrary, path, MAX_PATH);
        if (length) {
            DWORD dwVerHnd = 0;
            DWORD dwVersionInfoSize = GetFileVersionInfoSize(path, &dwVerHnd);
            std::unique_ptr<int[]> VffInfo(new int[dwVersionInfoSize]);
            if (GetFileVersionInfo(path, NULL, dwVersionInfoSize, VffInfo.get())) {
                LPVOID versionInfo;
                UINT size;
                if (VerQueryValue(VffInfo.get(), "\\", &versionInfo, &size)) {
                    if (size) {
                        VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)versionInfo;
                        unsigned int warpBuildNumber = verInfo->dwFileVersionLS >> 16 & 0xffff;
                        if (verInfo->dwSignature == 0xFEEF04BD && warpBuildNumber >= minBuildNumber) {
                            return true;
                        }
                    }
                }
            }
        }
        FreeLibrary(pLibrary);
    }
    return false;
}

#if WDK_NTDDI_VERSION <= NTDDI_WIN10_RS2
#define D3D12_FEATURE_D3D12_OPTIONS3 ((D3D12_FEATURE)21)
#define NTDDI_WIN10_RS3                     0x0A000004  /* ABRACADABRA_WIN10_RS2 */
typedef
enum D3D12_COMMAND_LIST_SUPPORT_FLAGS
{
  D3D12_COMMAND_LIST_SUPPORT_FLAG_NONE = 0,
  D3D12_COMMAND_LIST_SUPPORT_FLAG_DIRECT = (1 << D3D12_COMMAND_LIST_TYPE_DIRECT),
  D3D12_COMMAND_LIST_SUPPORT_FLAG_BUNDLE = (1 << D3D12_COMMAND_LIST_TYPE_BUNDLE),
  D3D12_COMMAND_LIST_SUPPORT_FLAG_COMPUTE = (1 << D3D12_COMMAND_LIST_TYPE_COMPUTE),
  D3D12_COMMAND_LIST_SUPPORT_FLAG_COPY = (1 << D3D12_COMMAND_LIST_TYPE_COPY),
  D3D12_COMMAND_LIST_SUPPORT_FLAG_VIDEO_DECODE = (1 << 4),
  D3D12_COMMAND_LIST_SUPPORT_FLAG_VIDEO_PROCESS = (1 << 5)
} D3D12_COMMAND_LIST_SUPPORT_FLAGS;

typedef
enum D3D12_VIEW_INSTANCING_TIER
{
  D3D12_VIEW_INSTANCING_TIER_NOT_SUPPORTED = 0,
  D3D12_VIEW_INSTANCING_TIER_1 = 1,
  D3D12_VIEW_INSTANCING_TIER_2 = 2,
  D3D12_VIEW_INSTANCING_TIER_3 = 3
} D3D12_VIEW_INSTANCING_TIER;

typedef struct D3D12_FEATURE_DATA_D3D12_OPTIONS3
{
  _Out_  BOOL CopyQueueTimestampQueriesSupported;
  _Out_  BOOL CastingFullyTypedFormatSupported;
  _Out_  DWORD WriteBufferImmediateSupportFlags;
  _Out_  D3D12_VIEW_INSTANCING_TIER ViewInstancingTier;
  _Out_  BOOL BarycentricsSupported;
} D3D12_FEATURE_DATA_D3D12_OPTIONS3;
#endif

#if WDK_NTDDI_VERSION <= NTDDI_WIN10_RS3
#define D3D12_FEATURE_D3D12_OPTIONS4 ((D3D12_FEATURE)23)
typedef enum D3D12_SHARED_RESOURCE_COMPATIBILITY_TIER
{
    D3D12_SHARED_RESOURCE_COMPATIBILITY_TIER_0,
    D3D12_SHARED_RESOURCE_COMPATIBILITY_TIER_1,
} D3D12_SHARED_RESOURCE_COMPATIBILITY_TIER;

typedef struct D3D12_FEATURE_DATA_D3D12_OPTIONS4
{
    _Out_ BOOL ReservedBufferPlacementSupported;
    _Out_ D3D12_SHARED_RESOURCE_COMPATIBILITY_TIER SharedResourceCompatibilityTier;
    _Out_ BOOL Native16BitShaderOpsSupported;
} D3D12_FEATURE_DATA_D3D12_OPTIONS4;

#endif

// Virtual class to compute the expected result given a set of inputs
struct TableParameter;

class ExecutionTest {
public:
  // By default, ignore these tests, which require a recent build to run properly.
  BEGIN_TEST_CLASS(ExecutionTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_CLASS_PROPERTY(L"Ignore", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()
  TEST_CLASS_SETUP(ExecutionTestClassSetup)

  TEST_METHOD(BasicComputeTest);
  TEST_METHOD(BasicTriangleTest);
  TEST_METHOD(BasicTriangleOpTest);

  BEGIN_TEST_METHOD(BasicTriangleOpTestHalf)
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()

  TEST_METHOD(OutOfBoundsTest);
  TEST_METHOD(SaturateTest);
  TEST_METHOD(SignTest);
  TEST_METHOD(Int64Test);
  TEST_METHOD(WaveIntrinsicsTest);
  TEST_METHOD(WaveIntrinsicsDDITest);
  TEST_METHOD(WaveIntrinsicsInPSTest);
  TEST_METHOD(PartialDerivTest);

  BEGIN_TEST_METHOD(CBufferTestHalf)
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()

  TEST_METHOD(BasicShaderModel61);
  BEGIN_TEST_METHOD(BasicShaderModel63)
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.3
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(WaveIntrinsicsActiveIntTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#WaveIntrinsicsActiveIntTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(WaveIntrinsicsActiveUintTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#WaveIntrinsicsActiveUintTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(WaveIntrinsicsPrefixIntTest)
  TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#WaveIntrinsicsPrefixIntTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(WaveIntrinsicsPrefixUintTest)
  TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#WaveIntrinsicsPrefixUintTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(WaveIntrinsicsSM65IntTest)
  TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#WaveIntrinsicsMultiPrefixIntTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(WaveIntrinsicsSM65UintTest)
  TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#WaveIntrinsicsMultiPrefixUintTable")
  END_TEST_METHOD()

  // TAEF data-driven tests.
  BEGIN_TEST_METHOD(UnaryFloatOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#UnaryFloatOpTable")
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(BinaryFloatOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#BinaryFloatOpTable")
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(TertiaryFloatOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#TertiaryFloatOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(UnaryHalfOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#UnaryHalfOpTable")
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(BinaryHalfOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#BinaryHalfOpTable")
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(TertiaryHalfOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#TertiaryHalfOpTable")
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(UnaryIntOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#UnaryIntOpTable")
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(BinaryIntOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#BinaryIntOpTable")
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(TertiaryIntOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#TertiaryIntOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(UnaryUintOpTest)
     TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#UnaryUintOpTable")
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(BinaryUintOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#BinaryUintOpTable")
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(TertiaryUintOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#TertiaryUintOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(UnaryInt16OpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#UnaryInt16OpTable")
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(BinaryInt16OpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#BinaryInt16OpTable")
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(TertiaryInt16OpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#TertiaryInt16OpTable")
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(UnaryUint16OpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#UnaryUint16OpTable")
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(BinaryUint16OpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#BinaryUint16OpTable")
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(TertiaryUint16OpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#TertiaryUint16OpTable")
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(DotTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#DotOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(Dot2AddHalfTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#Dot2AddHalfOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(Dot4AddI8PackedTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#Dot4AddI8PackedOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(Dot4AddU8PackedTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#Dot4AddU8PackedOpTable")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(Msad4Test)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#Msad4Table")
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(DenormBinaryFloatOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#DenormBinaryFloatOpTable")
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(DenormTertiaryFloatOpTest)
    TEST_METHOD_PROPERTY(L"DataSource", L"Table:ShaderOpArithTable.xml#DenormTertiaryFloatOpTable")
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.2
  END_TEST_METHOD()

  TEST_METHOD(BarycentricsTest);
  
  TEST_METHOD(ComputeRawBufferLdStI32);
  TEST_METHOD(ComputeRawBufferLdStFloat);

  BEGIN_TEST_METHOD(ComputeRawBufferLdStI64)
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.3
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(ComputeRawBufferLdStDouble)
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.3
  END_TEST_METHOD()
    
  BEGIN_TEST_METHOD(ComputeRawBufferLdStI16)
    TEST_METHOD_PROPERTY(L"Priority", L"2") // This test is disabled because of a bug in WARP; TODO: enable once the bug is fixed
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(ComputeRawBufferLdStHalf)
    TEST_METHOD_PROPERTY(L"Priority", L"2") // This test is disabled because of a bug in WARP; TODO: enable once the bug is fixed
  END_TEST_METHOD()

  TEST_METHOD(GraphicsRawBufferLdStI32);
  TEST_METHOD(GraphicsRawBufferLdStFloat);

  BEGIN_TEST_METHOD(GraphicsRawBufferLdStI64)
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.3
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(GraphicsRawBufferLdStDouble)
    TEST_METHOD_PROPERTY(L"Priority", L"2") // Remove this line once warp supports this feature in Shader Model 6.3
  END_TEST_METHOD()

  BEGIN_TEST_METHOD(GraphicsRawBufferLdStI16)
    TEST_METHOD_PROPERTY(L"Priority", L"2") // This test is disabled because of a bug in WARP; TODO: enable once the bug is fixed
  END_TEST_METHOD()
  BEGIN_TEST_METHOD(GraphicsRawBufferLdStHalf)
    TEST_METHOD_PROPERTY(L"Priority", L"2") // This test is disabled because of a bug in WARP; TODO: enable once the bug is fixed
  END_TEST_METHOD()

  dxc::DxcDllSupport m_support;
  VersionSupportInfo m_ver;
  bool m_ExperimentalModeEnabled = false;

  const float ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };

// Do not remove the following line - it is used by TranslateExecutionTest.py
// MARKER: ExecutionTest/DxilConf Shared Implementation Start

 // This is defined in d3d.h for Windows 10 Anniversary Edition SDK, but we only
  // require the Windows 10 SDK.
  typedef enum D3D_SHADER_MODEL {
    D3D_SHADER_MODEL_5_1 = 0x51,
    D3D_SHADER_MODEL_6_0 = 0x60,
    D3D_SHADER_MODEL_6_1 = 0x61,
    D3D_SHADER_MODEL_6_2 = 0x62,
    D3D_SHADER_MODEL_6_3 = 0x63,
    D3D_SHADER_MODEL_6_4 = 0x64,
    D3D_SHADER_MODEL_6_5 = 0x65,
} D3D_SHADER_MODEL;

#if WDK_NTDDI_VERSION == NTDDI_WIN10_RS2
  static const D3D_SHADER_MODEL HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_0;
#elif WDK_NTDDI_VERSION == NTDDI_WIN10_RS3
  static const D3D_SHADER_MODEL HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_1;
#elif WDK_NTDDI_VERSION == NTDDI_WIN10_RS4
  static const D3D_SHADER_MODEL HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_2;
#elif WDK_NTDDI_VERSION == NTDDI_WIN10_RS5
  static const D3D_SHADER_MODEL HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_3;
#elif WDK_NTDDI_VERSION == NTDDI_WIN10_19H1
  static const D3D_SHADER_MODEL HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_4;
#elif WDK_NTDDI_VERSION == NTDDI_WIN10_VB
  static const D3D_SHADER_MODEL HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_5;
#elif WDK_NTDDI_VERSION == NTDDI_WIN10_MN
  static const D3D_SHADER_MODEL HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_5;
#else
  static const D3D_SHADER_MODEL HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_5;
#endif

  bool UseDxbc() {
#ifdef _HLK_CONF
    return false;
#else
    return GetTestParamBool(L"DXBC");
#endif
  }

  bool UseWarpByDefault() {
#ifdef _HLK_CONF
    return false;
#else
    return true;
#endif
  }

  bool UseDebugIfaces() {
    return true;
  }

  bool SaveImages() {
    return GetTestParamBool(L"SaveImages");
  }

  template <class T1, class T2>
  void WaveIntrinsicsActivePrefixTest(TableParameter *pParameterList,
                                      size_t numParameter, bool isPrefix);

  template <typename T>
  void WaveIntrinsicsMultiPrefixOpTest(TableParameter *pParameterList,
                                       size_t numParameters);

  void BasicTriangleTestSetup(LPCSTR OpName, LPCWSTR FileName, D3D_SHADER_MODEL testModel);

  void RunBasicShaderModelTest(D3D_SHADER_MODEL shaderModel);

  enum class RawBufferLdStType {
     I32,
     Float,
     I64,
     Double,
     I16,
     Half
  };

  template <class Ty>
  struct RawBufferLdStTestData {
    Ty v1, v2[2], v3[3], v4[4];
  };

  template <class Ty>
  struct RawBufferLdStUavData {
    RawBufferLdStTestData<Ty> input, output, srvOut;
  };

  template <class Ty>
  void RunComputeRawBufferLdStTest(D3D_SHADER_MODEL shaderModel, RawBufferLdStType dataType,
                            const char *shaderOpName, const RawBufferLdStTestData<Ty> &testData);

  template <class Ty>
  void RunGraphicsRawBufferLdStTest(D3D_SHADER_MODEL shaderModel, RawBufferLdStType dataType,
                            const char *shaderOpName, const RawBufferLdStTestData<Ty> &testData);

  template <class Ty>
  void VerifyRawBufferLdStTestResults(const std::shared_ptr<st::ShaderOpTest> test, const RawBufferLdStTestData<Ty> &testData);
                                      
  bool SetupRawBufferLdStTest(D3D_SHADER_MODEL shaderModel, RawBufferLdStType dataType, CComPtr<ID3D12Device> &pDevice, 
                              CComPtr<IStream> &pStream, char *&sTy, char *&additionalOptions);

  template <class Ty>
  void RunBasicShaderModelTest(CComPtr<ID3D12Device> pDevice, const char *pShaderModelStr, const char *pShader, Ty *pInputDataPairs, unsigned inputDataCount);
  
  template <class Ty>
  const wchar_t* BasicShaderModelTest_GetFormatString();
                                      
  void CompileFromText(LPCSTR pText, LPCWSTR pEntryPoint, LPCWSTR pTargetProfile, ID3DBlob **ppBlob) {
    VERIFY_SUCCEEDED(m_support.Initialize());
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcLibrary> pLibrary;
    CComPtr<IDxcBlobEncoding> pTextBlob;
    CComPtr<IDxcOperationResult> pResult;
    HRESULT resultCode;
    VERIFY_SUCCEEDED(m_support.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    VERIFY_SUCCEEDED(m_support.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    VERIFY_SUCCEEDED(pLibrary->CreateBlobWithEncodingFromPinned(pText, (UINT32)strlen(pText), CP_UTF8, &pTextBlob));
    VERIFY_SUCCEEDED(pCompiler->Compile(pTextBlob, L"hlsl.hlsl", pEntryPoint, pTargetProfile, nullptr, 0, nullptr, 0, nullptr, &pResult));
    VERIFY_SUCCEEDED(pResult->GetStatus(&resultCode));
    if (FAILED(resultCode)) {
      CComPtr<IDxcBlobEncoding> errors;
      VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&errors));
#ifndef _HLK_CONF
      LogCommentFmt(L"Failed to compile shader: %s", BlobToUtf16(errors).data());
#endif
    }
    VERIFY_SUCCEEDED(resultCode);
    VERIFY_SUCCEEDED(pResult->GetResult((IDxcBlob **)ppBlob));
  }

  void CreateComputeCommandQueue(ID3D12Device *pDevice, LPCWSTR pName, ID3D12CommandQueue **ppCommandQueue) {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    VERIFY_SUCCEEDED(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(ppCommandQueue)));
    VERIFY_SUCCEEDED((*ppCommandQueue)->SetName(pName));
  }

  void CreateComputePSO(ID3D12Device *pDevice, ID3D12RootSignature *pRootSignature, LPCSTR pShader, ID3D12PipelineState **ppComputeState) {
    CComPtr<ID3DBlob> pComputeShader;

    // Load and compile shaders.
    if (UseDxbc()) {
#ifndef _HLK_CONF
      DXBCFromText(pShader, L"main", L"cs_6_0", &pComputeShader);
#endif
    }
    else {
      CompileFromText(pShader, L"main", L"cs_6_0", &pComputeShader);
    }

    // Describe and create the compute pipeline state object (PSO).
    D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
    computePsoDesc.pRootSignature = pRootSignature;
    computePsoDesc.CS = CD3DX12_SHADER_BYTECODE(pComputeShader);

    VERIFY_SUCCEEDED(pDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(ppComputeState)));
  }

  bool CreateDevice(_COM_Outptr_ ID3D12Device **ppDevice,
                    D3D_SHADER_MODEL testModel = D3D_SHADER_MODEL_6_0, bool skipUnsupported = true) {
    if (testModel > HIGHEST_SHADER_MODEL) {
      UINT minor = (UINT)testModel & 0x0f;
      LogCommentFmt(L"Installed SDK does not support "
          L"shader model 6.%1u", minor);

      if (skipUnsupported) {
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
      }

      return false;
    }
    const D3D_FEATURE_LEVEL FeatureLevelRequired = D3D_FEATURE_LEVEL_11_0;
    CComPtr<IDXGIFactory4> factory;
    CComPtr<ID3D12Device> pDevice;

    *ppDevice = nullptr;

    VERIFY_SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
    if (GetTestParamUseWARP(UseWarpByDefault())) {
      CComPtr<IDXGIAdapter> warpAdapter;
      VERIFY_SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
      HRESULT createHR = D3D12CreateDevice(warpAdapter, FeatureLevelRequired,
                                           IID_PPV_ARGS(&pDevice));
      if (FAILED(createHR)) {
        LogCommentFmt(L"The available version of WARP does not support d3d12.");

        if (skipUnsupported) {
          WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
        }

        return false;
      }
    } else {
      CComPtr<IDXGIAdapter1> hardwareAdapter;
      WEX::Common::String AdapterValue;
      HRESULT hr = WEX::TestExecution::RuntimeParameters::TryGetValue(L"Adapter",
                                                             AdapterValue);
      if (SUCCEEDED(hr)) {
          GetHardwareAdapter(factory, AdapterValue, &hardwareAdapter);
      } else {
        WEX::Logging::Log::Comment(
            L"Using default hardware adapter with D3D12 support.");
      }
      VERIFY_SUCCEEDED(D3D12CreateDevice(hardwareAdapter, FeatureLevelRequired,
                                         IID_PPV_ARGS(&pDevice)));
    }
    // retrieve adapter information
    LUID adapterID = pDevice->GetAdapterLuid();
    CComPtr<IDXGIAdapter> adapter;
    factory->EnumAdapterByLuid(adapterID, IID_PPV_ARGS(&adapter));
    DXGI_ADAPTER_DESC AdapterDesc;
    VERIFY_SUCCEEDED(adapter->GetDesc(&AdapterDesc));
    LogCommentFmt(L"Using Adapter:%s", AdapterDesc.Description);
    
    if (pDevice == nullptr)
      return false;

    if (!UseDxbc()) {
      // Check for DXIL support.
      typedef struct D3D12_FEATURE_DATA_SHADER_MODEL {
        _Inout_ D3D_SHADER_MODEL HighestShaderModel;
      } D3D12_FEATURE_DATA_SHADER_MODEL;
      const UINT D3D12_FEATURE_SHADER_MODEL = 7;
      D3D12_FEATURE_DATA_SHADER_MODEL SMData;
      SMData.HighestShaderModel = testModel;
      VERIFY_SUCCEEDED(pDevice->CheckFeatureSupport(
        (D3D12_FEATURE)D3D12_FEATURE_SHADER_MODEL, &SMData, sizeof(SMData)));
      if (SMData.HighestShaderModel < testModel) {
        UINT minor = (UINT)testModel & 0x0f;
        LogCommentFmt(L"The selected device does not support "
                      L"shader model 6.%1u", minor);

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

  void CreateGraphicsCommandQueue(ID3D12Device *pDevice, ID3D12CommandQueue **ppCommandQueue) {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;;
    VERIFY_SUCCEEDED(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(ppCommandQueue)));
  }

  void CreateGraphicsCommandQueueAndList(
      ID3D12Device *pDevice, ID3D12CommandQueue **ppCommandQueue,
      ID3D12CommandAllocator **ppAllocator,
      ID3D12GraphicsCommandList **ppCommandList, ID3D12PipelineState *pPSO) {
    CreateGraphicsCommandQueue(pDevice, ppCommandQueue);
    VERIFY_SUCCEEDED(pDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(ppAllocator)));
    VERIFY_SUCCEEDED(pDevice->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, *ppAllocator, pPSO,
        IID_PPV_ARGS(ppCommandList)));
  }

  void CreateGraphicsPSO(ID3D12Device *pDevice,
                         D3D12_INPUT_LAYOUT_DESC *pInputLayout,
                         ID3D12RootSignature *pRootSignature, LPCSTR pShaders,
                         ID3D12PipelineState **ppPSO) {
    CComPtr<ID3DBlob> vertexShader;
    CComPtr<ID3DBlob> pixelShader;

    if (UseDxbc()) {
#ifndef _HLK_CONF
      DXBCFromText(pShaders, L"VSMain", L"vs_6_0", &vertexShader);
      DXBCFromText(pShaders, L"PSMain", L"ps_6_0", &pixelShader);
#endif
    } else {
      CompileFromText(pShaders, L"VSMain", L"vs_6_0", &vertexShader);
      CompileFromText(pShaders, L"PSMain", L"ps_6_0", &pixelShader);
    }

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = *pInputLayout;
    psoDesc.pRootSignature = pRootSignature;
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    VERIFY_SUCCEEDED(
        pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(ppPSO)));
  }

  void CreateRenderTargetAndReadback(ID3D12Device *pDevice,
                                     ID3D12DescriptorHeap *pHeap, UINT width,
                                     UINT height,
                                     ID3D12Resource **ppRenderTarget,
                                     ID3D12Resource **ppBuffer) {
    const DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    const size_t formatElementSize = 4;
    CComPtr<ID3D12Resource> pRenderTarget;
    CComPtr<ID3D12Resource> pBuffer;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        pHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_HEAP_PROPERTIES rtHeap(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC rtDesc(
        CD3DX12_RESOURCE_DESC::Tex2D(format, width, height));
    CD3DX12_CLEAR_VALUE rtClearVal(format, ClearColor);
    rtDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    VERIFY_SUCCEEDED(pDevice->CreateCommittedResource(
        &rtHeap, D3D12_HEAP_FLAG_NONE, &rtDesc, D3D12_RESOURCE_STATE_COPY_DEST,
        &rtClearVal, IID_PPV_ARGS(&pRenderTarget)));
    pDevice->CreateRenderTargetView(pRenderTarget, nullptr, rtvHandle);
    // rtvHandle.Offset(1, rtvDescriptorSize);  // Not needed for a single
    // resource.

    CD3DX12_HEAP_PROPERTIES readHeap(D3D12_HEAP_TYPE_READBACK);
    CD3DX12_RESOURCE_DESC readDesc(
        CD3DX12_RESOURCE_DESC::Buffer(width * height * formatElementSize));
    VERIFY_SUCCEEDED(pDevice->CreateCommittedResource(
        &readHeap, D3D12_HEAP_FLAG_NONE, &readDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pBuffer)));

    *ppRenderTarget = pRenderTarget.Detach();
    *ppBuffer = pBuffer.Detach();
  }

  void CreateRootSignatureFromDesc(ID3D12Device *pDevice,
                                   const D3D12_ROOT_SIGNATURE_DESC *pDesc,
                                   ID3D12RootSignature **pRootSig) {
    CComPtr<ID3DBlob> signature;
    CComPtr<ID3DBlob> error;
    VERIFY_SUCCEEDED(D3D12SerializeRootSignature(pDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    VERIFY_SUCCEEDED(pDevice->CreateRootSignature(
        0, signature->GetBufferPointer(), signature->GetBufferSize(),
        IID_PPV_ARGS(pRootSig)));
  }

  void CreateRtvDescriptorHeap(ID3D12Device *pDevice, UINT numDescriptors,
                               ID3D12DescriptorHeap **pRtvHeap, UINT *rtvDescriptorSize) {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = numDescriptors;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    VERIFY_SUCCEEDED(
        pDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(pRtvHeap)));

    if (rtvDescriptorSize != nullptr) {
      *rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(
          D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }
  }

  void CreateTestUavs(ID3D12Device *pDevice,
                      ID3D12GraphicsCommandList *pCommandList, LPCVOID values,
                      UINT32 valueSizeInBytes, ID3D12Resource **ppUavResource,
                      ID3D12Resource **ppReadBuffer,
                      ID3D12Resource **ppUploadResource) {
    CComPtr<ID3D12Resource> pUavResource;
    CComPtr<ID3D12Resource> pReadBuffer;
    CComPtr<ID3D12Resource> pUploadResource;
    D3D12_SUBRESOURCE_DATA transferData;
    D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(valueSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(valueSizeInBytes);
    CD3DX12_HEAP_PROPERTIES readHeap(D3D12_HEAP_TYPE_READBACK);
    CD3DX12_RESOURCE_DESC readDesc(CD3DX12_RESOURCE_DESC::Buffer(valueSizeInBytes));

    VERIFY_SUCCEEDED(pDevice->CreateCommittedResource(
      &defaultHeapProperties,
      D3D12_HEAP_FLAG_NONE,
      &bufferDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&pUavResource)));

    VERIFY_SUCCEEDED(pDevice->CreateCommittedResource(
      &uploadHeapProperties,
      D3D12_HEAP_FLAG_NONE,
      &uploadBufferDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&pUploadResource)));

    VERIFY_SUCCEEDED(pDevice->CreateCommittedResource(
      &readHeap, D3D12_HEAP_FLAG_NONE, &readDesc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pReadBuffer)));

    transferData.pData = values;
    transferData.RowPitch = valueSizeInBytes;
    transferData.SlicePitch = transferData.RowPitch;

    UpdateSubresources<1>(pCommandList, pUavResource.p, pUploadResource.p, 0, 0, 1, &transferData);
    RecordTransitionBarrier(pCommandList, pUavResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    *ppUavResource = pUavResource.Detach();
    *ppReadBuffer = pReadBuffer.Detach();
    *ppUploadResource = pUploadResource.Detach();
  }

  template <typename TVertex, int len>
  void CreateVertexBuffer(ID3D12Device *pDevice, TVertex(&vertices)[len],
                          ID3D12Resource **ppVertexBuffer,
                          D3D12_VERTEX_BUFFER_VIEW *pVertexBufferView) {
    size_t vertexBufferSize = sizeof(vertices);
    CComPtr<ID3D12Resource> pVertexBuffer;
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc(
        CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize));
    VERIFY_SUCCEEDED(pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&pVertexBuffer)));

    UINT8 *pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);
    VERIFY_SUCCEEDED(pVertexBuffer->Map(
        0, &readRange, reinterpret_cast<void **>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, vertices, vertexBufferSize);
    pVertexBuffer->Unmap(0, nullptr);

    // Initialize the vertex buffer view.
    pVertexBufferView->BufferLocation = pVertexBuffer->GetGPUVirtualAddress();
    pVertexBufferView->StrideInBytes = sizeof(TVertex);
    pVertexBufferView->SizeInBytes = (UINT)vertexBufferSize;

    *ppVertexBuffer = pVertexBuffer.Detach();
  }

  // Requires Anniversary Edition headers, so simplifying things for current setup.
  const UINT D3D12_FEATURE_D3D12_OPTIONS1 = 8;
  struct D3D12_FEATURE_DATA_D3D12_OPTIONS1 {
    BOOL WaveOps;
    UINT WaveLaneCountMin;
    UINT WaveLaneCountMax;
    UINT TotalLaneCount;
    BOOL ExpandedComputeResourceStates;
    BOOL Int64ShaderOps;
  };

  bool DoesDeviceSupportInt64(ID3D12Device *pDevice) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 O;
    if (FAILED(pDevice->CheckFeatureSupport((D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &O, sizeof(O))))
      return false;
    return O.Int64ShaderOps != FALSE;
  }

  bool DoesDeviceSupportDouble(ID3D12Device *pDevice) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS O;
    if (FAILED(pDevice->CheckFeatureSupport((D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS, &O, sizeof(O))))
      return false;
    return O.DoublePrecisionFloatShaderOps != FALSE;
  }

  bool DoesDeviceSupportWaveOps(ID3D12Device *pDevice) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 O;
    if (FAILED(pDevice->CheckFeatureSupport((D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &O, sizeof(O))))
      return false;
    return O.WaveOps != FALSE;
  }

  bool DoesDeviceSupportBarycentrics(ID3D12Device *pDevice) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS3 O;
    if (FAILED(pDevice->CheckFeatureSupport((D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS3, &O, sizeof(O))))
      return false;
    return O.BarycentricsSupported != FALSE;
  }

  bool DoesDeviceSupportNative16bitOps(ID3D12Device *pDevice) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS4 O;
    if (FAILED(pDevice->CheckFeatureSupport((D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS4, &O, sizeof(O))))
      return false;
    return O.Native16BitShaderOpsSupported != FALSE;
  }
    
#ifndef _HLK_CONF
  void DXBCFromText(LPCSTR pText, LPCWSTR pEntryPoint, LPCWSTR pTargetProfile, ID3DBlob **ppBlob) {
    CW2A pEntryPointA(pEntryPoint, CP_UTF8);
    CW2A pTargetProfileA(pTargetProfile, CP_UTF8);
    CComPtr<ID3DBlob> pErrors;
    D3D_SHADER_MACRO d3dMacro[2];
    ZeroMemory(d3dMacro, sizeof(d3dMacro));
    d3dMacro[0].Definition = "1";
    d3dMacro[0].Name = "USING_DXBC";
    HRESULT hr = D3DCompile(pText, strlen(pText), "hlsl.hlsl", d3dMacro, nullptr, pEntryPointA, pTargetProfileA, 0, 0, ppBlob, &pErrors);
    if (pErrors != nullptr) {
      CA2W errors((char *)pErrors->GetBufferPointer(), CP_ACP);
      LogCommentFmt(L"Compilation failure: %s", errors.m_szBuffer);
    }
    VERIFY_SUCCEEDED(hr);
  }
#endif

  HRESULT EnableDebugLayer() {
    // The debug layer does net yet validate DXIL programs that require rewriting,
    // but basic logging should work properly.
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

#ifndef _HLK_CONF
  HRESULT EnableExperimentalMode() {
    if (m_ExperimentalModeEnabled) {
      return S_OK;
    }
    if (!GetTestParamBool(L"ExperimentalShaders")) {
      return S_FALSE;
    }
    HRESULT hr = EnableExperimentalShaderModels();
    if (SUCCEEDED(hr)) {
      m_ExperimentalModeEnabled = true;
    }
    return hr;
  }
#endif

  struct FenceObj {
    HANDLE m_fenceEvent = NULL;
    CComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
    ~FenceObj() {
      if (m_fenceEvent) CloseHandle(m_fenceEvent);
    }
  };

  void InitFenceObj(ID3D12Device *pDevice, FenceObj *pObj) {
    pObj->m_fenceValue = 1;
    VERIFY_SUCCEEDED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                          IID_PPV_ARGS(&pObj->m_fence)));
    // Create an event handle to use for frame synchronization.
    pObj->m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (pObj->m_fenceEvent == nullptr) {
      VERIFY_SUCCEEDED(HRESULT_FROM_WIN32(GetLastError()));
    }
  }

  void ReadHlslDataIntoNewStream(LPCWSTR relativePath, IStream **ppStream) {
    VERIFY_SUCCEEDED(m_support.Initialize());
    CComPtr<IDxcLibrary> pLibrary;
    CComPtr<IDxcBlobEncoding> pBlob;
    CComPtr<IStream> pStream;
    std::wstring path = GetPathToHlslDataFile(relativePath);
    VERIFY_SUCCEEDED(m_support.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    VERIFY_SUCCEEDED(pLibrary->CreateBlobFromFile(path.c_str(), nullptr, &pBlob));
    VERIFY_SUCCEEDED(pLibrary->CreateStreamFromBlobReadOnly(pBlob, &pStream));
    *ppStream = pStream.Detach();
  }

  void RecordRenderAndReadback(ID3D12GraphicsCommandList *pList,
                               ID3D12DescriptorHeap *pRtvHeap,
                               UINT rtvDescriptorSize,
                               UINT instanceCount,
                               D3D12_VERTEX_BUFFER_VIEW *pVertexBufferView,
                               ID3D12RootSignature *pRootSig,
                               ID3D12Resource *pRenderTarget,
                               ID3D12Resource *pReadBuffer) {
    D3D12_RESOURCE_DESC rtDesc = pRenderTarget->GetDesc();
    D3D12_VIEWPORT viewport;
    D3D12_RECT scissorRect;

    memset(&viewport, 0, sizeof(viewport));
    viewport.Height = (float)rtDesc.Height;
    viewport.Width = (float)rtDesc.Width;
    viewport.MaxDepth = 1.0f;
    memset(&scissorRect, 0, sizeof(scissorRect));
    scissorRect.right = (long)rtDesc.Width;
    scissorRect.bottom = rtDesc.Height;
    if (pRootSig != nullptr) {
      pList->SetGraphicsRootSignature(pRootSig);
    }
    pList->RSSetViewports(1, &viewport);
    pList->RSSetScissorRects(1, &scissorRect);

    // Indicate that the buffer will be used as a render target.
    RecordTransitionBarrier(pList, pRenderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pRtvHeap->GetCPUDescriptorHandleForHeapStart(), 0, rtvDescriptorSize);
    pList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    pList->ClearRenderTargetView(rtvHandle, ClearColor, 0, nullptr);
    pList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pList->IASetVertexBuffers(0, 1, pVertexBufferView);
    pList->DrawInstanced(3, instanceCount, 0, 0);

    // Transition to copy source and copy into read-back buffer.
    RecordTransitionBarrier(pList, pRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);

    // Copy into read-back buffer.
    UINT64 rowPitch = rtDesc.Width * 4;
    if (rowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
      rowPitch += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - (rowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
    Footprint.Offset = 0;
    Footprint.Footprint = CD3DX12_SUBRESOURCE_FOOTPRINT(DXGI_FORMAT_R8G8B8A8_UNORM, (UINT)rtDesc.Width, rtDesc.Height, 1, (UINT)rowPitch);
    CD3DX12_TEXTURE_COPY_LOCATION DstLoc(pReadBuffer, Footprint);
    CD3DX12_TEXTURE_COPY_LOCATION SrcLoc(pRenderTarget, 0);
    pList->CopyTextureRegion(&DstLoc, 0, 0, 0, &SrcLoc, nullptr);
  }

  void RunRWByteBufferComputeTest(ID3D12Device *pDevice, LPCSTR shader, std::vector<uint32_t> &values);

  void SetDescriptorHeap(ID3D12GraphicsCommandList *pCommandList, ID3D12DescriptorHeap *pHeap) {
    ID3D12DescriptorHeap *const pHeaps[1] = { pHeap };
    pCommandList->SetDescriptorHeaps(1, pHeaps);
  }

  void WaitForSignal(ID3D12CommandQueue *pCQ, FenceObj &FO) {
    ::WaitForSignal(pCQ, FO.m_fence, FO.m_fenceEvent, FO.m_fenceValue++);
  }
};
#define WAVE_INTRINSIC_DXBC_GUARD \
  "#ifdef USING_DXBC\r\n" \
  "uint WaveGetLaneIndex() { return 1; }\r\n" \
  "uint WaveReadLaneFirst(uint u) { return u; }\r\n" \
  "bool WaveIsFirstLane() { return true; }\r\n" \
  "uint WaveGetLaneCount() { return 1; }\r\n" \
  "uint WaveReadLaneAt(uint n, uint u) { return u; }\r\n" \
  "bool WaveActiveAnyTrue(bool b) { return b; }\r\n" \
  "bool WaveActiveAllTrue(bool b) { return false; }\r\n" \
  "uint WaveActiveAllEqual(uint u) { return u; }\r\n" \
  "uint4 WaveActiveBallot(bool b) { return 1; }\r\n" \
  "uint WaveActiveCountBits(uint u) { return 1; }\r\n" \
  "uint WaveActiveSum(uint u) { return 1; }\r\n" \
  "uint WaveActiveProduct(uint u) { return 1; }\r\n" \
  "uint WaveActiveBitAnd(uint u) { return 1; }\r\n" \
  "uint WaveActiveBitOr(uint u) { return 1; }\r\n" \
  "uint WaveActiveBitXor(uint u) { return 1; }\r\n" \
  "uint WaveActiveMin(uint u) { return 1; }\r\n" \
  "uint WaveActiveMax(uint u) { return 1; }\r\n" \
  "uint WavePrefixCountBits(uint u) { return 1; }\r\n" \
  "uint WavePrefixSum(uint u) { return 1; }\r\n" \
  "uint WavePrefixProduct(uint u) { return 1; }\r\n" \
  "uint QuadReadLaneAt(uint a, uint u) { return 1; }\r\n" \
  "uint QuadReadAcrossX(uint u) { return 1; }\r\n" \
  "uint QuadReadAcrossY(uint u) { return 1; }\r\n" \
  "uint QuadReadAcrossDiagonal(uint u) { return 1; }\r\n" \
  "#endif\r\n"

static void SetupComputeValuePattern(std::vector<uint32_t> &values,
                                     size_t count) {
  values.resize(count); // one element per dispatch group, in bytes
  for (size_t i = 0; i < count; ++i) {
    values[i] = (uint32_t)i;
  }
}

bool ExecutionTest::ExecutionTestClassSetup() {
#ifdef _HLK_CONF
// TODO: Enabling the D3D driver verifier. Check out the logic in the D3DConf_12_Core test.
    VERIFY_SUCCEEDED(m_support.Initialize());
    m_UseWarp = hlsl_test::GetTestParamUseWARP(false);
    m_EnableDebugLayer = hlsl_test::GetTestParamBool(L"DebugLayer");
    if (m_EnableDebugLayer) {
        EnableDebugLayer();
    }
    return true;
#else
  HRESULT hr = EnableExperimentalMode();
  if (FAILED(hr)) {
    LogCommentFmt(L"Unable to enable shader experimental mode - 0x%08x.", hr);
  }
  else if (hr == S_FALSE) {
    LogCommentFmt(L"Experimental mode not enabled.");
  }
  else {
    LogCommentFmt(L"Experimental mode enabled.");
  }
  hr = EnableDebugLayer();
  if (FAILED(hr)) {
    LogCommentFmt(L"Unable to enable debug layer - 0x%08x.", hr);
  }
  else {
    LogCommentFmt(L"Debug layer enabled.");
  }
  return true;
#endif
}

void ExecutionTest::RunRWByteBufferComputeTest(ID3D12Device *pDevice, LPCSTR pShader, std::vector<uint32_t> &values) {
  static const int DispatchGroupX = 1;
  static const int DispatchGroupY = 1;
  static const int DispatchGroupZ = 1;

  CComPtr<ID3D12GraphicsCommandList> pCommandList;
  CComPtr<ID3D12CommandQueue> pCommandQueue;
  CComPtr<ID3D12DescriptorHeap> pUavHeap;
  CComPtr<ID3D12CommandAllocator> pCommandAllocator;
  UINT uavDescriptorSize;
  FenceObj FO;

  const UINT valueSizeInBytes = (UINT)values.size() * sizeof(uint32_t);
  CreateComputeCommandQueue(pDevice, L"RunRWByteBufferComputeTest Command Queue", &pCommandQueue);
  InitFenceObj(pDevice, &FO);

  // Describe and create a UAV descriptor heap.
  D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
  heapDesc.NumDescriptors = 1;
  heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  VERIFY_SUCCEEDED(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pUavHeap)));
  uavDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(heapDesc.Type);

  // Create root signature.
  CComPtr<ID3D12RootSignature> pRootSignature;
  {
    CD3DX12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[1];
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    CreateRootSignatureFromDesc(pDevice, &rootSignatureDesc, &pRootSignature);
  }

  // Create pipeline state object.
  CComPtr<ID3D12PipelineState> pComputeState;
  CreateComputePSO(pDevice, pRootSignature, pShader, &pComputeState);

  // Create a command allocator and list for compute.
  VERIFY_SUCCEEDED(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&pCommandAllocator)));
  VERIFY_SUCCEEDED(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, pCommandAllocator, pComputeState, IID_PPV_ARGS(&pCommandList)));
  pCommandList->SetName(L"ExecutionTest::RunRWByteButterComputeTest Command List");

  // Set up UAV resource.
  CComPtr<ID3D12Resource> pUavResource;
  CComPtr<ID3D12Resource> pReadBuffer;
  CComPtr<ID3D12Resource> pUploadResource;
  CreateTestUavs(pDevice, pCommandList, values.data(), valueSizeInBytes, &pUavResource, &pReadBuffer, &pUploadResource);
  VERIFY_SUCCEEDED(pUavResource->SetName(L"RunRWByteBufferComputeText UAV"));
  VERIFY_SUCCEEDED(pReadBuffer->SetName(L"RunRWByteBufferComputeText UAV Read Buffer"));
  VERIFY_SUCCEEDED(pUploadResource->SetName(L"RunRWByteBufferComputeText UAV Upload Buffer"));

  // Close the command list and execute it to perform the GPU setup.
  pCommandList->Close();
  ExecuteCommandList(pCommandQueue, pCommandList);
  WaitForSignal(pCommandQueue, FO);
  VERIFY_SUCCEEDED(pCommandAllocator->Reset());
  VERIFY_SUCCEEDED(pCommandList->Reset(pCommandAllocator, pComputeState));

  // Run the compute shader and copy the results back to readable memory.
  {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = (UINT)values.size();
    uavDesc.Buffer.StructureByteStride = 0;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(pUavHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandleGpu(pUavHeap->GetGPUDescriptorHandleForHeapStart());
    pDevice->CreateUnorderedAccessView(pUavResource, nullptr, &uavDesc, uavHandle);
    SetDescriptorHeap(pCommandList, pUavHeap);
    pCommandList->SetComputeRootSignature(pRootSignature);
    pCommandList->SetComputeRootDescriptorTable(0, uavHandleGpu);
  }
  pCommandList->Dispatch(DispatchGroupX, DispatchGroupY, DispatchGroupZ);
  RecordTransitionBarrier(pCommandList, pUavResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
  pCommandList->CopyResource(pReadBuffer, pUavResource);
  pCommandList->Close();
  ExecuteCommandList(pCommandQueue, pCommandList);
  WaitForSignal(pCommandQueue, FO);
  {
    MappedData mappedData(pReadBuffer, valueSizeInBytes);
    uint32_t *pData = (uint32_t *)mappedData.data();
    memcpy(values.data(), pData, (size_t)valueSizeInBytes);
  }
  WaitForSignal(pCommandQueue, FO);
}

TEST_F(ExecutionTest, BasicComputeTest) {
#ifndef _HLK_CONF
  //
  // BasicComputeTest is a simple compute shader that can be used as the basis
  // for more interesting compute execution tests.
  // The HLSL is compatible with shader models <=5.1 to allow using the DXBC
  // rendering code paths for comparison.
  //
  static const char pShader[] =
    "RWByteAddressBuffer g_bab : register(u0);\r\n"
    "[numthreads(8,8,1)]\r\n"
    "void main(uint GI : SV_GroupIndex) {"
    "  uint addr = GI * 4;\r\n"
    "  uint val = g_bab.Load(addr);\r\n"
    "  DeviceMemoryBarrierWithGroupSync();\r\n"
    "  g_bab.Store(addr, val + 1);\r\n"
    "}";
  static const int NumThreadsX = 8;
  static const int NumThreadsY = 8;
  static const int NumThreadsZ = 1;
  static const int ThreadsPerGroup = NumThreadsX * NumThreadsY * NumThreadsZ;
  static const int DispatchGroupCount = 1;

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;

  std::vector<uint32_t> values;
  SetupComputeValuePattern(values, ThreadsPerGroup * DispatchGroupCount);
  VERIFY_ARE_EQUAL(values[0], (uint32_t)0);
  RunRWByteBufferComputeTest(pDevice, pShader, values);
  VERIFY_ARE_EQUAL(values[0], (uint32_t)1);
#endif
}

TEST_F(ExecutionTest, BasicTriangleTest) {
#ifndef _HLK_CONF
  static const UINT FrameCount = 2;
  static const UINT m_width = 320;
  static const UINT m_height = 200;
  static const float m_aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);

  struct Vertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
  };

  // Pipeline objects.
  CComPtr<ID3D12Device> pDevice;
  CComPtr<ID3D12Resource> pRenderTarget;
  CComPtr<ID3D12CommandAllocator> pCommandAllocator;
  CComPtr<ID3D12CommandQueue> pCommandQueue;
  CComPtr<ID3D12RootSignature> pRootSig;
  CComPtr<ID3D12DescriptorHeap> pRtvHeap;
  CComPtr<ID3D12PipelineState> pPipelineState;
  CComPtr<ID3D12GraphicsCommandList> pCommandList;
  CComPtr<ID3D12Resource> pReadBuffer;
  UINT rtvDescriptorSize;

  CComPtr<ID3D12Resource> pVertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

  // Synchronization objects.
  FenceObj FO;

  // Shaders.
  static const char pShaders[] =
    "struct PSInput {\r\n"
    "  float4 position : SV_POSITION;\r\n"
    "  float4 color : COLOR;\r\n"
    "};\r\n\r\n"
    "PSInput VSMain(float4 position : POSITION, float4 color : COLOR) {\r\n"
    "  PSInput result;\r\n"
    "\r\n"
    "  result.position = position;\r\n"
    "  result.color = color;\r\n"
    "  return result;\r\n"
    "}\r\n\r\n"
    "float4 PSMain(PSInput input) : SV_TARGET {\r\n"
    "  return 1; //input.color;\r\n"
    "};\r\n";

  if (!CreateDevice(&pDevice))
    return;

  struct BasicTestChecker {
    CComPtr<ID3D12Device> m_pDevice;
    CComPtr<ID3D12InfoQueue> m_pInfoQueue;
    bool m_OK = false;
    void SetOK(bool value) { m_OK = value; }
    BasicTestChecker(ID3D12Device *pDevice) : m_pDevice(pDevice) {
      if (FAILED(m_pDevice.QueryInterface(&m_pInfoQueue)))
        return;
      m_pInfoQueue->PushEmptyStorageFilter();
      m_pInfoQueue->PushEmptyRetrievalFilter();
    }
    ~BasicTestChecker() {
      if (!m_OK && m_pInfoQueue != nullptr) {
        UINT64 count = m_pInfoQueue->GetNumStoredMessages();
        bool invalidBytecodeFound = false;
        CAtlArray<BYTE> m_pBytes;
        for (UINT64 i = 0; i < count; ++i) {
          SIZE_T len = 0;
          if (FAILED(m_pInfoQueue->GetMessageA(i, nullptr, &len)))
            continue;
          if (m_pBytes.GetCount() < len && !m_pBytes.SetCount(len))
            continue;
          D3D12_MESSAGE *pMsg = (D3D12_MESSAGE *)m_pBytes.GetData();
          if (FAILED(m_pInfoQueue->GetMessageA(i, pMsg, &len)))
            continue;
          if (pMsg->ID == D3D12_MESSAGE_ID_CREATEVERTEXSHADER_INVALIDSHADERBYTECODE ||
              pMsg->ID == D3D12_MESSAGE_ID_CREATEPIXELSHADER_INVALIDSHADERBYTECODE) {
            invalidBytecodeFound = true;
            break;
          }
        }
        if (invalidBytecodeFound) {
          LogCommentFmt(L"%s", L"Found an invalid bytecode message. This "
            L"typically indicates that experimental mode "
            L"is not set up properly.");
          if (!GetTestParamBool(L"ExperimentalShaders")) {
            LogCommentFmt(L"Note that the ExperimentalShaders test parameter isn't set.");
          }
        }
        else {
          LogCommentFmt(L"Did not find corrupt pixel or vertex shaders in "
                        L"queue - dumping complete queue.");
          WriteInfoQueueMessages(nullptr, OutputFn, m_pInfoQueue);
        }
      }
    }
    static void __stdcall OutputFn(void *pCtx, const wchar_t *pMsg) {
      LogCommentFmt(L"%s", pMsg);
    }
  };
  BasicTestChecker BTC(pDevice);
  {
    InitFenceObj(pDevice, &FO);
    CreateRtvDescriptorHeap(pDevice, FrameCount, &pRtvHeap, &rtvDescriptorSize);
    CreateRenderTargetAndReadback(pDevice, pRtvHeap, m_width, m_height, &pRenderTarget, &pReadBuffer);

    // Create an empty root signature.
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(
      0, nullptr, 0, nullptr,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    CreateRootSignatureFromDesc(pDevice, &rootSignatureDesc, &pRootSig);

    // Create the pipeline state, which includes compiling and loading shaders.
    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
    D3D12_INPUT_LAYOUT_DESC InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    CreateGraphicsPSO(pDevice, &InputLayout, pRootSig, pShaders, &pPipelineState);

    CreateGraphicsCommandQueueAndList(pDevice, &pCommandQueue,
                                      &pCommandAllocator, &pCommandList,
                                      pPipelineState);

    // Define the geometry for a triangle.
    Vertex triangleVertices[] = {
      { { 0.0f, 0.25f * m_aspectRatio, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f } },
      { { 0.25f, -0.25f * m_aspectRatio, 0.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } },
      { { -0.25f, -0.25f * m_aspectRatio, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } } };

    CreateVertexBuffer(pDevice, triangleVertices, &pVertexBuffer, &vertexBufferView);
    WaitForSignal(pCommandQueue, FO);
  }

  // Render and execute the command list.
  RecordRenderAndReadback(pCommandList, pRtvHeap, rtvDescriptorSize, 1,
                          &vertexBufferView, pRootSig, pRenderTarget,
                          pReadBuffer);
  VERIFY_SUCCEEDED(pCommandList->Close());
  ExecuteCommandList(pCommandQueue, pCommandList);

  // Wait for previous frame.
  WaitForSignal(pCommandQueue, FO);

  // At this point, we've verified that execution succeeded with DXIL.
  BTC.SetOK(true);

  // Read back to CPU and examine contents.
  {
    MappedData data(pReadBuffer, m_width * m_height * 4);
    const uint32_t *pPixels = (uint32_t *)data.data();
    if (SaveImages()) {
      SavePixelsToFile(pPixels, DXGI_FORMAT_R8G8B8A8_UNORM, m_width, m_height, L"basic.bmp");
    }
    uint32_t top = pPixels[m_width / 2]; // Top center.
    uint32_t mid = pPixels[m_width / 2 + m_width * (m_height / 2)]; // Middle center.
    VERIFY_ARE_EQUAL(0xff663300, top); // clear color
    VERIFY_ARE_EQUAL(0xffffffff, mid); // white
  }
#endif
}

TEST_F(ExecutionTest, Int64Test) {
  static const char pShader[] =
    "RWByteAddressBuffer g_bab : register(u0);\r\n"
    "[numthreads(8,8,1)]\r\n"
    "void main(uint GI : SV_GroupIndex) {"
    "  uint addr = GI * 4;\r\n"
    "  uint val = g_bab.Load(addr);\r\n"
    "  uint64_t u64 = val;\r\n"
    "  u64 *= val;\r\n"
    "  g_bab.Store(addr, (uint)(u64 >> 32));\r\n"
    "}";
  static const int NumThreadsX = 8;
  static const int NumThreadsY = 8;
  static const int NumThreadsZ = 1;
  static const int ThreadsPerGroup = NumThreadsX * NumThreadsY * NumThreadsZ;
  static const int DispatchGroupCount = 1;

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;

  if (!DoesDeviceSupportInt64(pDevice)) {
    // Optional feature, so it's correct to not support it if declared as such.
    WEX::Logging::Log::Comment(L"Device does not support int64 operations.");
    return;
  }
  std::vector<uint32_t> values;
  SetupComputeValuePattern(values, ThreadsPerGroup * DispatchGroupCount);
  VERIFY_ARE_EQUAL(values[0], (uint32_t)0);
  RunRWByteBufferComputeTest(pDevice, pShader, values);
  VERIFY_ARE_EQUAL(values[0], (uint32_t)0);
}

TEST_F(ExecutionTest, SignTest) {
  static const char pShader[] =
    "RWByteAddressBuffer g_bab : register(u0);\r\n"
    "[numthreads(8,1,1)]\r\n"
    "void main(uint GI : SV_GroupIndex) {"
    "  uint addr = GI * 4;\r\n"
    "  int val = g_bab.Load(addr);\r\n"
    "  g_bab.Store(addr, (uint)(sign(val)));\r\n"
    "}";
  static const int NumThreadsX = 8;
  static const int NumThreadsY = 1;
  static const int NumThreadsZ = 1;
  static const int ThreadsPerGroup = NumThreadsX * NumThreadsY * NumThreadsZ;
  static const int DispatchGroupCount = 1;

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;

  const uint32_t neg1 = (uint32_t)-1;
  uint32_t origValues[] = { (uint32_t)-3, (uint32_t)-2, neg1, 0, 1, 2, 3, 4 };
  std::vector<uint32_t> values(origValues, origValues + _countof(origValues));

  RunRWByteBufferComputeTest(pDevice, pShader, values);
  VERIFY_ARE_EQUAL(values[0], neg1);
  VERIFY_ARE_EQUAL(values[1], neg1);
  VERIFY_ARE_EQUAL(values[2], neg1);
  VERIFY_ARE_EQUAL(values[3], (uint32_t)0);
  VERIFY_ARE_EQUAL(values[4], (uint32_t)1);
  VERIFY_ARE_EQUAL(values[5], (uint32_t)1);
  VERIFY_ARE_EQUAL(values[6], (uint32_t)1);
  VERIFY_ARE_EQUAL(values[7], (uint32_t)1);
}

TEST_F(ExecutionTest, WaveIntrinsicsDDITest) {
#ifndef _HLK_CONF 
  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 O;
  if (FAILED(pDevice->CheckFeatureSupport((D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &O, sizeof(O))))
    return;
  bool waveSupported = O.WaveOps;
  UINT laneCountMin = O.WaveLaneCountMin;
  UINT laneCountMax = O.WaveLaneCountMax;
  LogCommentFmt(L"WaveOps %i, WaveLaneCountMin %u, WaveLaneCountMax %u", waveSupported, laneCountMin, laneCountMax);
  VERIFY_IS_TRUE(laneCountMin <= laneCountMax);
  if (waveSupported) {
    VERIFY_IS_TRUE(laneCountMin > 0 && laneCountMax > 0);
  }
  else {
    VERIFY_IS_TRUE(laneCountMin == 0 && laneCountMax == 0);
  }
#endif
}

TEST_F(ExecutionTest, WaveIntrinsicsTest) {
#ifndef _HLK_CONF 
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  struct PerThreadData {
    uint32_t id, flags, laneIndex, laneCount, firstLaneId, preds, firstlaneX, lane1X;
    uint32_t allBC, allSum, allProd, allAND, allOR, allXOR, allMin, allMax;
    uint32_t pfBC, pfSum, pfProd;
    uint32_t ballot[4];
    uint32_t diver;   // divergent value, used in calculation
    int32_t i_diver;  // divergent value, used in calculation
    int32_t i_allMax, i_allMin, i_allSum, i_allProd;
    int32_t i_pfSum, i_pfProd;
  };
  static const char pShader[] =
    WAVE_INTRINSIC_DXBC_GUARD
    "struct PerThreadData {\r\n"
    " uint id, flags, laneIndex, laneCount, firstLaneId, preds, firstlaneX, lane1X;\r\n"
    " uint allBC, allSum, allProd, allAND, allOR, allXOR, allMin, allMax;\r\n"
    " uint pfBC, pfSum, pfProd;\r\n"
    " uint4 ballot;\r\n"
    " uint diver;\r\n"
    " int i_diver;\r\n"
    " int i_allMax, i_allMin, i_allSum, i_allProd;\r\n"
    " int i_pfSum, i_pfProd;\r\n"
    "};\r\n"
    "RWStructuredBuffer<PerThreadData> g_sb : register(u0);\r\n"
    "[numthreads(8,8,1)]\r\n"
    "void main(uint GI : SV_GroupIndex, uint3 GTID : SV_GroupThreadID) {"
    "  PerThreadData pts = g_sb[GI];\r\n"
    "  uint diver = GTID.x + 2;\r\n"
    "  pts.diver = diver;\r\n"
    "  pts.flags = 0;\r\n"
    "  pts.preds = 0;\r\n"
    "  if (WaveIsFirstLane()) pts.flags |= 1;\r\n"
    "  pts.laneIndex = WaveGetLaneIndex();\r\n"
    "  pts.laneCount = WaveGetLaneCount();\r\n"
    "  pts.firstLaneId = WaveReadLaneFirst(pts.id);\r\n"
    "  pts.preds |= ((WaveActiveAnyTrue(diver == 1) ? 1 : 0) << 0);\r\n"
    "  pts.preds |= ((WaveActiveAllTrue(diver == 1) ? 1 : 0) << 1);\r\n"
    "  pts.preds |= ((WaveActiveAllEqual(diver) ? 1 : 0) << 2);\r\n"
    "  pts.preds |= ((WaveActiveAllEqual(GTID.z) ? 1 : 0) << 3);\r\n"
    "  pts.preds |= ((WaveActiveAllEqual(WaveReadLaneFirst(diver)) ? 1 : 0) << 4);\r\n"
    "  pts.ballot = WaveActiveBallot(diver > 3);\r\n"
    "  pts.firstlaneX = WaveReadLaneFirst(GTID.x);\r\n"
    "  pts.lane1X = WaveReadLaneAt(GTID.x, 1);\r\n"
    "\r\n"
    "  pts.allBC = WaveActiveCountBits(diver > 3);\r\n"
    "  pts.allSum = WaveActiveSum(diver);\r\n"
    "  pts.allProd = WaveActiveProduct(diver);\r\n"
    "  pts.allAND = WaveActiveBitAnd(diver);\r\n"
    "  pts.allOR = WaveActiveBitOr(diver);\r\n"
    "  pts.allXOR = WaveActiveBitXor(diver);\r\n"
    "  pts.allMin = WaveActiveMin(diver);\r\n"
    "  pts.allMax = WaveActiveMax(diver);\r\n"
    "\r\n"
    "  pts.pfBC = WavePrefixCountBits(diver > 3);\r\n"
    "  pts.pfSum = WavePrefixSum(diver);\r\n"
    "  pts.pfProd = WavePrefixProduct(diver);\r\n"
    "\r\n"
    "  int i_diver = pts.i_diver;\r\n"
    "  pts.i_allMax = WaveActiveMax(i_diver);\r\n"
    "  pts.i_allMin = WaveActiveMin(i_diver);\r\n"
    "  pts.i_allSum = WaveActiveSum(i_diver);\r\n"
    "  pts.i_allProd = WaveActiveProduct(i_diver);\r\n"
    "  pts.i_pfSum = WavePrefixSum(i_diver);\r\n"
    "  pts.i_pfProd = WavePrefixProduct(i_diver);\r\n"
    "\r\n"
    "  g_sb[GI] = pts;\r\n"
    "}";
  static const int NumtheadsX = 8;
  static const int NumtheadsY = 8;
  static const int NumtheadsZ = 1;
  static const int ThreadsPerGroup = NumtheadsX * NumtheadsY * NumtheadsZ;
  static const int DispatchGroupCount = 1;

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;

  if (!DoesDeviceSupportWaveOps(pDevice)) {
    // Optional feature, so it's correct to not support it if declared as such.
    WEX::Logging::Log::Comment(L"Device does not support wave operations.");
    return;
  }

  std::vector<PerThreadData> values;
  values.resize(ThreadsPerGroup * DispatchGroupCount);
  for (size_t i = 0; i < values.size(); ++i) {
    memset(&values[i], 0, sizeof(PerThreadData));
    values[i].id = i;
    values[i].i_diver = (int)i;
    values[i].i_diver *= (i % 2) ? 1 : -1;
  }

  static const int DispatchGroupX = 1;
  static const int DispatchGroupY = 1;
  static const int DispatchGroupZ = 1;

  CComPtr<ID3D12GraphicsCommandList> pCommandList;
  CComPtr<ID3D12CommandQueue> pCommandQueue;
  CComPtr<ID3D12DescriptorHeap> pUavHeap;
  CComPtr<ID3D12CommandAllocator> pCommandAllocator;
  UINT uavDescriptorSize;
  FenceObj FO;
  bool dxbc = UseDxbc();

  const size_t valueSizeInBytes = values.size() * sizeof(PerThreadData);
  CreateComputeCommandQueue(pDevice, L"WaveIntrinsicsTest Command Queue", &pCommandQueue);
  InitFenceObj(pDevice, &FO);

  // Describe and create a UAV descriptor heap.
  D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
  heapDesc.NumDescriptors = 1;
  heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  VERIFY_SUCCEEDED(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pUavHeap)));
  uavDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(heapDesc.Type);

  // Create root signature.
  CComPtr<ID3D12RootSignature> pRootSignature;
  {
    CD3DX12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[1];
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    CComPtr<ID3DBlob> signature;
    CComPtr<ID3DBlob> error;
    VERIFY_SUCCEEDED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    VERIFY_SUCCEEDED(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pRootSignature)));
  }

  // Create pipeline state object.
  CComPtr<ID3D12PipelineState> pComputeState;
  CreateComputePSO(pDevice, pRootSignature, pShader, &pComputeState);

  // Create a command allocator and list for compute.
  VERIFY_SUCCEEDED(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&pCommandAllocator)));
  VERIFY_SUCCEEDED(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, pCommandAllocator, pComputeState, IID_PPV_ARGS(&pCommandList)));

  // Set up UAV resource.
  CComPtr<ID3D12Resource> pUavResource;
  CComPtr<ID3D12Resource> pReadBuffer;
  CComPtr<ID3D12Resource> pUploadResource;
  CreateTestUavs(pDevice, pCommandList, values.data(), valueSizeInBytes, &pUavResource, &pReadBuffer, &pUploadResource);

  // Close the command list and execute it to perform the GPU setup.
  pCommandList->Close();
  ExecuteCommandList(pCommandQueue, pCommandList);
  WaitForSignal(pCommandQueue, FO);
  VERIFY_SUCCEEDED(pCommandAllocator->Reset());
  VERIFY_SUCCEEDED(pCommandList->Reset(pCommandAllocator, pComputeState));

  // Run the compute shader and copy the results back to readable memory.
  {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = values.size();
    uavDesc.Buffer.StructureByteStride = sizeof(PerThreadData);
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(pUavHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandleGpu(pUavHeap->GetGPUDescriptorHandleForHeapStart());
    pDevice->CreateUnorderedAccessView(pUavResource, nullptr, &uavDesc, uavHandle);
    SetDescriptorHeap(pCommandList, pUavHeap);
    pCommandList->SetComputeRootSignature(pRootSignature);
    pCommandList->SetComputeRootDescriptorTable(0, uavHandleGpu);
  }
  pCommandList->Dispatch(DispatchGroupX, DispatchGroupY, DispatchGroupZ);
  RecordTransitionBarrier(pCommandList, pUavResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
  pCommandList->CopyResource(pReadBuffer, pUavResource);
  pCommandList->Close();
  ExecuteCommandList(pCommandQueue, pCommandList);
  WaitForSignal(pCommandQueue, FO);
  {
    MappedData mappedData(pReadBuffer, valueSizeInBytes);
    PerThreadData *pData = (PerThreadData *)mappedData.data();
    memcpy(values.data(), pData, valueSizeInBytes);

    // Gather some general data.
    // The 'firstLaneId' captures a unique number per first-lane per wave.
    // Counting the number distinct firstLaneIds gives us the number of waves.
    std::vector<uint32_t> firstLaneIds;
    for (size_t i = 0; i < values.size(); ++i) {
      PerThreadData &pts = values[i];
      uint32_t firstLaneId = pts.firstLaneId;
      if (!contains(firstLaneIds, firstLaneId)) {
        firstLaneIds.push_back(firstLaneId);
      }
    }

    // Waves should cover 4 threads or more.
    LogCommentFmt(L"Found %u distinct lane ids: %u", firstLaneIds.size());
    if (!dxbc) {
      VERIFY_IS_GREATER_THAN_OR_EQUAL(values.size() / 4, firstLaneIds.size());
    }

    // Now, group threads into waves.
    std::map<uint32_t, std::unique_ptr<std::vector<PerThreadData *> > > waves;
    for (size_t i = 0; i < firstLaneIds.size(); ++i) {
      waves[firstLaneIds[i]] = std::make_unique<std::vector<PerThreadData *> >();
    }
    for (size_t i = 0; i < values.size(); ++i) {
      PerThreadData &pts = values[i];
      std::unique_ptr<std::vector<PerThreadData *> > &wave = waves[pts.firstLaneId];
      wave->push_back(&pts);
    }

    // Verify that all the wave values are coherent across the wave.
    for (size_t i = 0; i < values.size(); ++i) {
      PerThreadData &pts = values[i];
      std::unique_ptr<std::vector<PerThreadData *> > &wave = waves[pts.firstLaneId];
      // Sort the lanes by increasing lane ID.
      struct LaneIdOrderPred {
        bool operator()(PerThreadData *a, PerThreadData *b) {
          return a->laneIndex < b->laneIndex;
        }
      };
      std::sort(wave.get()->begin(), wave.get()->end(), LaneIdOrderPred());

      // Verify some interesting properties of the first lane.
      uint32_t pfBC, pfSum, pfProd;
      int32_t i_pfSum, i_pfProd;
      int32_t i_allMax, i_allMin;
      {
        PerThreadData *ptdFirst = wave->front();
        VERIFY_IS_TRUE(0 != (ptdFirst->flags & 1)); // FirstLane sets this bit.
        VERIFY_IS_TRUE(0 == ptdFirst->pfBC);
        VERIFY_IS_TRUE(0 == ptdFirst->pfSum);
        VERIFY_IS_TRUE(1 == ptdFirst->pfProd);
        VERIFY_IS_TRUE(0 == ptdFirst->i_pfSum);
        VERIFY_IS_TRUE(1 == ptdFirst->i_pfProd);
        pfBC = (ptdFirst->diver > 3) ? 1 : 0;
        pfSum = ptdFirst->diver;
        pfProd = ptdFirst->diver;
        i_pfSum = ptdFirst->i_diver;
        i_pfProd = ptdFirst->i_diver;
        i_allMax = i_allMin = ptdFirst->i_diver;
      }

      // Calculate values which take into consideration all lanes.
      uint32_t preds = 0;
      preds |= 1 << 1; // AllTrue starts true, switches to false if needed.
      preds |= 1 << 2; // AllEqual starts true, switches to false if needed.
      preds |= 1 << 3; // WaveActiveAllEqual(GTID.z) is always true
      preds |= 1 << 4; // (WaveActiveAllEqual(WaveReadLaneFirst(diver)) is always true
      uint32_t ballot[4] = { 0, 0, 0, 0 };
      int32_t i_allSum = 0, i_allProd = 1;
      for (size_t n = 0; n < wave->size(); ++n) {
        std::vector<PerThreadData *> &lanes = *wave.get();
        // pts.preds |= ((WaveActiveAnyTrue(diver == 1) ? 1 : 0) << 0);
        if (lanes[n]->diver == 1) preds |= (1 << 0);
        // pts.preds |= ((WaveActiveAllTrue(diver == 1) ? 1 : 0) << 1);
        if (lanes[n]->diver != 1) preds &= ~(1 << 1);
        // pts.preds |= ((WaveActiveAllEqual(diver) ? 1 : 0) << 2);
        if (lanes[0]->diver != lanes[n]->diver) preds &= ~(1 << 2);
        // pts.ballot = WaveActiveBallot(diver > 3);\r\n"
        if (lanes[n]->diver > 3) {
          // This is the uint4 result layout:
          // .x -> bits  0 .. 31
          // .y -> bits 32 .. 63
          // .z -> bits 64 .. 95
          // .w -> bits 96 ..127
          uint32_t component = lanes[n]->laneIndex / 32;
          uint32_t bit = lanes[n]->laneIndex % 32;
          ballot[component] |= 1 << bit;
        }
        i_allMax = std::max(lanes[n]->i_diver, i_allMax);
        i_allMin = std::min(lanes[n]->i_diver, i_allMin);
        i_allProd *= lanes[n]->i_diver;
        i_allSum += lanes[n]->i_diver;
      }

      for (size_t n = 1; n < wave->size(); ++n) {
        // 'All' operations are uniform across the wave.
        std::vector<PerThreadData *> &lanes = *wave.get();
        VERIFY_IS_TRUE(0 == (lanes[n]->flags & 1)); // non-firstlanes do not set this bit
        VERIFY_ARE_EQUAL(lanes[0]->allBC, lanes[n]->allBC);
        VERIFY_ARE_EQUAL(lanes[0]->allSum, lanes[n]->allSum);
        VERIFY_ARE_EQUAL(lanes[0]->allProd, lanes[n]->allProd);
        VERIFY_ARE_EQUAL(lanes[0]->allAND, lanes[n]->allAND);
        VERIFY_ARE_EQUAL(lanes[0]->allOR, lanes[n]->allOR);
        VERIFY_ARE_EQUAL(lanes[0]->allXOR, lanes[n]->allXOR);
        VERIFY_ARE_EQUAL(lanes[0]->allMin, lanes[n]->allMin);
        VERIFY_ARE_EQUAL(lanes[0]->allMax, lanes[n]->allMax);
        VERIFY_ARE_EQUAL(i_allMax, lanes[n]->i_allMax);
        VERIFY_ARE_EQUAL(i_allMin, lanes[n]->i_allMin);
        VERIFY_ARE_EQUAL(i_allProd, lanes[n]->i_allProd);
        VERIFY_ARE_EQUAL(i_allSum, lanes[n]->i_allSum);

        // first-lane reads and uniform reads are uniform across the wave.
        VERIFY_ARE_EQUAL(lanes[0]->firstlaneX, lanes[n]->firstlaneX);
        VERIFY_ARE_EQUAL(lanes[0]->lane1X, lanes[n]->lane1X);

        // the lane count is uniform across the wave.
        VERIFY_ARE_EQUAL(lanes[0]->laneCount, lanes[n]->laneCount);

        // The predicates are uniform across the wave.
        VERIFY_ARE_EQUAL(lanes[n]->preds, preds);

        // the lane index is distinct per thread.
        for (size_t prior = 0; prior < n; ++prior) {
          VERIFY_ARE_NOT_EQUAL(lanes[prior]->laneIndex, lanes[n]->laneIndex);
        }
        // Ballot results are uniform across the wave.
        VERIFY_ARE_EQUAL(0, memcmp(ballot, lanes[n]->ballot, sizeof(ballot)));

        // Keep running total of prefix calculation. Prefix values are exclusive to
        // the executing lane.
        VERIFY_ARE_EQUAL(pfBC, lanes[n]->pfBC);
        VERIFY_ARE_EQUAL(pfSum, lanes[n]->pfSum);
        VERIFY_ARE_EQUAL(pfProd, lanes[n]->pfProd);
        VERIFY_ARE_EQUAL(i_pfSum, lanes[n]->i_pfSum);
        VERIFY_ARE_EQUAL(i_pfProd, lanes[n]->i_pfProd);
        pfBC += (lanes[n]->diver > 3) ? 1 : 0;
        pfSum += lanes[n]->diver;
        pfProd *= lanes[n]->diver;
        i_pfSum += lanes[n]->i_diver;
        i_pfProd *= lanes[n]->i_diver;
      }
      // TODO: add divergent branching and verify that the otherwise uniform values properly diverge
    }

    // Compare each value of each per-thread element.
    for (size_t i = 0; i < values.size(); ++i) {
      PerThreadData &pts = values[i];
      VERIFY_ARE_EQUAL(i, pts.id); // ID is unchanged.
    }
  }
#endif
}

// This test is assuming that the adapter implements WaveReadLaneFirst correctly
TEST_F(ExecutionTest, WaveIntrinsicsInPSTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  struct Vertex {
    XMFLOAT3 position;
  };

  struct PerPixelData {
    XMFLOAT4 position;
    uint32_t id, flags, laneIndex, laneCount, firstLaneId, sum1;
    uint32_t id0, id1, id2, id3;
    uint32_t acrossX, acrossY, acrossDiag, quadActiveCount;
  };

  const UINT RTWidth = 128;
  const UINT RTHeight = 128;

  // Shaders.
  static const char pShaders[] =
    WAVE_INTRINSIC_DXBC_GUARD
    "struct PSInput {\r\n"
    "  float4 position : SV_POSITION;\r\n"
    "};\r\n\r\n"
    "PSInput VSMain(float4 position : POSITION) {\r\n"
    "  PSInput result;\r\n"
    "\r\n"
    "  result.position = position;\r\n"
    "  return result;\r\n"
    "}\r\n\r\n"
    "uint pos_to_id(float4 pos) { return pos.x * 128 + pos.y; }\r\n"
    "struct PerPixelData {\r\n"
    " float4 position;\r\n"
    " uint id, flags, laneIndex, laneCount, firstLaneId, sum1;\r\n"
    " uint id0, id1, id2, id3;\r\n"
    " uint acrossX, acrossY, acrossDiag, quadActiveCount;\r\n"
    "};\r\n"
    "AppendStructuredBuffer<PerPixelData> g_sb : register(u1);\r\n"
    "float4 PSMain(PSInput input) : SV_TARGET {\r\n"
    "  uint one = 1;\r\n"
    "  PerPixelData d;\r\n"
    "  d.position = input.position;\r\n"
    "  d.id = pos_to_id(input.position);\r\n"
    "  d.flags = 0;\r\n"
    "  if (WaveIsFirstLane()) d.flags |= 1;\r\n"
    "  d.laneIndex = WaveGetLaneIndex();\r\n"
    "  d.laneCount = WaveGetLaneCount();\r\n"
    "  d.firstLaneId = WaveReadLaneFirst(d.id);\r\n"
    "  d.sum1 = WaveActiveSum(one);\r\n"
    "  d.id0 = QuadReadLaneAt(d.id, 0);\r\n"
    "  d.id1 = QuadReadLaneAt(d.id, 1);\r\n"
    "  d.id2 = QuadReadLaneAt(d.id, 2);\r\n"
    "  d.id3 = QuadReadLaneAt(d.id, 3);\r\n"
    "  d.acrossX = QuadReadAcrossX(d.id);\r\n"
    "  d.acrossY = QuadReadAcrossY(d.id);\r\n"
    "  d.acrossDiag = QuadReadAcrossDiagonal(d.id);\r\n"
    "  d.quadActiveCount = one + QuadReadAcrossX(one) + QuadReadAcrossY(one) + QuadReadAcrossDiagonal(one);\r\n"
    "  g_sb.Append(d);\r\n"
    "  return 1;\r\n"
    "};\r\n";

  CComPtr<ID3D12Device> pDevice;
  CComPtr<ID3D12CommandQueue> pCommandQueue;
  CComPtr<ID3D12DescriptorHeap> pUavHeap, pRtvHeap;
  CComPtr<ID3D12CommandAllocator> pCommandAllocator;
  CComPtr<ID3D12GraphicsCommandList> pCommandList;
  CComPtr<ID3D12PipelineState> pPSO;
  CComPtr<ID3D12Resource> pRenderTarget, pReadBuffer;
  UINT uavDescriptorSize, rtvDescriptorSize;
  CComPtr<ID3D12Resource> pVertexBuffer;
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

  if (!CreateDevice(&pDevice))
    return;
  if (!DoesDeviceSupportWaveOps(pDevice)) {
    // Optional feature, so it's correct to not support it if declared as such.
    WEX::Logging::Log::Comment(L"Device does not support wave operations.");
    return;
  }

  FenceObj FO;
  InitFenceObj(pDevice, &FO);

  // Describe and create a UAV descriptor heap.
  D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
  heapDesc.NumDescriptors = 1;
  heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  VERIFY_SUCCEEDED(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pUavHeap)));
  uavDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(heapDesc.Type);

  CreateRtvDescriptorHeap(pDevice, 1, &pRtvHeap, &rtvDescriptorSize);
  CreateRenderTargetAndReadback(pDevice, pRtvHeap, RTHeight, RTWidth, &pRenderTarget, &pReadBuffer);

  // Create root signature: one UAV.
  CComPtr<ID3D12RootSignature> pRootSignature;
  {
    CD3DX12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, 0);

    CD3DX12_ROOT_PARAMETER rootParameters[1];
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    CreateRootSignatureFromDesc(pDevice, &rootSignatureDesc, &pRootSignature);
  }

  D3D12_INPUT_ELEMENT_DESC elementDesc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
       D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
  D3D12_INPUT_LAYOUT_DESC InputLayout = {elementDesc, _countof(elementDesc)};
  CreateGraphicsPSO(pDevice, &InputLayout, pRootSignature, pShaders, &pPSO);

  CreateGraphicsCommandQueueAndList(pDevice, &pCommandQueue, &pCommandAllocator,
                                    &pCommandList, pPSO);

  // Single triangle covering half the target.
  Vertex vertices[] = {
    { { -1.0f,  1.0f, 0.0f } },
    { {  1.0f,  1.0f, 0.0f } },
    { { -1.0f, -1.0f, 0.0f } } };
  const UINT TriangleCount = _countof(vertices) / 3;

  CreateVertexBuffer(pDevice, vertices, &pVertexBuffer, &vertexBufferView);

  bool dxbc = UseDxbc();

  // Set up UAV resource.
  std::vector<PerPixelData> values;
  values.resize(RTWidth * RTHeight * 2);
  UINT valueSizeInBytes = (UINT)values.size() * sizeof(PerPixelData);
  memset(values.data(), 0, valueSizeInBytes);
  CComPtr<ID3D12Resource> pUavResource;
  CComPtr<ID3D12Resource> pUavReadBuffer;
  CComPtr<ID3D12Resource> pUploadResource;
  CreateTestUavs(pDevice, pCommandList, values.data(), valueSizeInBytes, &pUavResource, &pUavReadBuffer, &pUploadResource);

  // Set up the append counter resource.
  CComPtr<ID3D12Resource> pUavCounterResource;
  CComPtr<ID3D12Resource> pReadCounterBuffer;
  CComPtr<ID3D12Resource> pUploadCounterResource;
  BYTE zero[sizeof(UINT)] = { 0 };
  CreateTestUavs(pDevice, pCommandList, zero, sizeof(zero), &pUavCounterResource, &pReadCounterBuffer, &pUploadCounterResource);

  // Close the command list and execute it to perform the GPU setup.
  pCommandList->Close();
  ExecuteCommandList(pCommandQueue, pCommandList);
  WaitForSignal(pCommandQueue, FO);
  VERIFY_SUCCEEDED(pCommandAllocator->Reset());
  VERIFY_SUCCEEDED(pCommandList->Reset(pCommandAllocator, pPSO));

  pCommandList->SetGraphicsRootSignature(pRootSignature);
  SetDescriptorHeap(pCommandList, pUavHeap);
  {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = (UINT)values.size();
    uavDesc.Buffer.StructureByteStride = sizeof(PerPixelData);
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(pUavHeap->GetCPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandleGpu(pUavHeap->GetGPUDescriptorHandleForHeapStart());
    pDevice->CreateUnorderedAccessView(pUavResource, pUavCounterResource, &uavDesc, uavHandle);
    pCommandList->SetGraphicsRootDescriptorTable(0, uavHandleGpu);
  }
  RecordRenderAndReadback(pCommandList, pRtvHeap, rtvDescriptorSize, TriangleCount, &vertexBufferView, nullptr, pRenderTarget, pReadBuffer);
  RecordTransitionBarrier(pCommandList, pUavResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
  RecordTransitionBarrier(pCommandList, pUavCounterResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
  pCommandList->CopyResource(pUavReadBuffer, pUavResource);
  pCommandList->CopyResource(pReadCounterBuffer, pUavCounterResource);
  VERIFY_SUCCEEDED(pCommandList->Close());
  LogCommentFmt(L"Rendering to %u by %u", RTWidth, RTHeight);
  ExecuteCommandList(pCommandQueue, pCommandList);
  WaitForSignal(pCommandQueue, FO);
  {
    MappedData data(pReadBuffer, RTWidth * RTHeight * 4);
    const uint32_t *pPixels = (uint32_t *)data.data();
    if (SaveImages()) {
      SavePixelsToFile(pPixels, DXGI_FORMAT_R8G8B8A8_UNORM, RTWidth, RTHeight, L"psintrin.bmp");
    }
  }

  uint32_t appendCount;
  {
    MappedData mappedData(pReadCounterBuffer, sizeof(uint32_t));
    appendCount = *((uint32_t *)mappedData.data());
    LogCommentFmt(L"%u elements in append buffer", appendCount);
  }

  {
    MappedData mappedData(pUavReadBuffer, (UINT32)values.size());
    PerPixelData *pData = (PerPixelData *)mappedData.data();
    memcpy(values.data(), pData, valueSizeInBytes);

    // DXBC is handy to test pipeline setup, but interesting functions are
    // stubbed out, so there is no point in further validation.
    if (dxbc)
      return;

    uint32_t maxActiveLaneCount = 0;
    uint32_t maxLaneCount = 0;
    for (uint32_t i = 0; i < appendCount; ++i) {
      maxActiveLaneCount = std::max(maxActiveLaneCount, values[i].sum1);
      maxLaneCount = std::max(maxLaneCount, values[i].laneCount);
    }

    uint32_t peerOfHelperLanes = 0;
    for (uint32_t i = 0; i < appendCount; ++i) {
      if (values[i].sum1 != maxActiveLaneCount) {
        ++peerOfHelperLanes;
      }
    }

    LogCommentFmt(
        L"Found: %u threads. Waves reported up to %u total lanes, up "
        L"to %u active lanes, and %u threads had helper/inactive lanes.",
        appendCount, maxLaneCount, maxActiveLaneCount, peerOfHelperLanes);

    // Group threads into quad invocations.
    uint32_t singlePixelCount = 0;
    uint32_t multiPixelCount = 0;
    std::unordered_set<uint32_t> ids;
    std::multimap<uint32_t, PerPixelData *> idGroups;
    std::multimap<uint32_t, PerPixelData *> firstIdGroups;
    for (uint32_t i = 0; i < appendCount; ++i) {
      ids.insert(values[i].id);
      idGroups.insert(std::make_pair(values[i].id, &values[i]));
      firstIdGroups.insert(std::make_pair(values[i].firstLaneId, &values[i]));
    }
    for (uint32_t id : ids) {
      if (idGroups.count(id) == 1)
        ++singlePixelCount;
      else
        ++multiPixelCount;
    }
    LogCommentFmt(L"%u pixels were processed by a single thread. %u invocations were for shared pixels.",
      singlePixelCount, multiPixelCount);

    // Multiple threads may have tried to shade the same pixel. (Is this true even if we have only one triangle?)
    // Where every pixel is distinct, it's very straightforward to validate.
    {
      auto cur = firstIdGroups.begin(), end = firstIdGroups.end();
      while (cur != end) {
        bool simpleWave = true;
        uint32_t firstId = (*cur).first;
        auto groupEnd = cur;
        while (groupEnd != end && (*groupEnd).first == firstId) {
          if (idGroups.count((*groupEnd).second->id) > 1)
            simpleWave = false;
          ++groupEnd;
        }
        if (simpleWave) {
          // Break the wave into quads.
          struct QuadData {
            unsigned count;
            PerPixelData *data[4];
          };
          std::map<uint32_t, QuadData> quads;
          for (auto i = cur; i != groupEnd; ++i) {
            // assuming that it is a simple wave, idGroups has a unique id for each entry.
            uint32_t laneId = (*i).second->id;
            uint32_t laneIds[4] = {(*i).second->id0, (*i).second->id1,
                                   (*i).second->id2, (*i).second->id3};
            // Since this is a simple wave, each lane has an unique id and
            // therefore should not have any ids in there.
            VERIFY_IS_TRUE(quads.find(laneId) == quads.end());
            // check if QuadReadLaneAt is returning same values in a single quad.
            bool newQuad = true;
            for (unsigned quadIndex = 0; quadIndex < 4; ++quadIndex) {
              auto match = quads.find(laneIds[quadIndex]);
              if (match != quads.end()) {
                (*match).second.data[(*match).second.count++] = (*i).second;
                newQuad = false;
                break;
              }
              auto quadMemberData = idGroups.find(laneIds[quadIndex]);
              if (quadMemberData != idGroups.end()) {
                VERIFY_IS_TRUE((*quadMemberData).second->id0 == laneIds[0]);
                VERIFY_IS_TRUE((*quadMemberData).second->id1 == laneIds[1]);
                VERIFY_IS_TRUE((*quadMemberData).second->id2 == laneIds[2]);
                VERIFY_IS_TRUE((*quadMemberData).second->id3 == laneIds[3]);
              }
            }
            if (newQuad) {
              QuadData qdata;
              qdata.count = 1;
              qdata.data[0] = (*i).second;
              quads.insert(std::make_pair(laneId, qdata));
            }
          }
          for (auto quadPair : quads) {
            unsigned count = quadPair.second.count;
            // There could be only one pixel data on the edge of the triangle
            if (count < 2) continue;
            PerPixelData **data = quadPair.second.data;
            bool isTop[4];
            bool isLeft[4];
            PerPixelData helperData;
            memset(&helperData, sizeof(helperData), 0);
            PerPixelData *layout[4]; // tl,tr,bl,br
            memset(layout, sizeof(layout), 0);
            auto fnToLayout = [&](bool top, bool left) -> PerPixelData ** {
              int idx = top ? 0 : 2;
              idx += left ? 0 : 1;
              return &layout[idx];
            };
            auto fnToLayoutData = [&](bool top, bool left) -> PerPixelData * {
              PerPixelData **pResult = fnToLayout(top, left);
              if (*pResult == nullptr) return &helperData;
              return *pResult;
            };
            VERIFY_IS_TRUE(count <= 4);
            if (count == 2) {
              isTop[0] = data[0]->position.y < data[1]->position.y;
              isTop[1] = (data[0]->position.y == data[1]->position.y) ? isTop[0] : !isTop[0];
              isLeft[0] = data[0]->position.x < data[1]->position.x;
              isLeft[1] = (data[0]->position.x == data[1]->position.x) ? isLeft[0] : !isLeft[0];
            }
            else {
              // with at least three samples, we have distinct x and y coordinates.
              float left = std::min(data[0]->position.x, data[1]->position.x);
              left = std::min(data[2]->position.x, left);
              float top = std::min(data[0]->position.y, data[1]->position.y);
              top = std::min(data[2]->position.y, top);
              for (unsigned i = 0; i < count; ++i) {
                isTop[i] = data[i]->position.y == top;
                isLeft[i] = data[i]->position.x == left;
              }
            }
            for (unsigned i = 0; i < count; ++i) {
              *(fnToLayout(isTop[i], isLeft[i])) = data[i];
            }

            // Finally, we have a proper quad reconstructed. Validate.
            for (unsigned i = 0; i < count; ++i) {
              PerPixelData *d = data[i];
              VERIFY_ARE_EQUAL(d->id0, fnToLayoutData(true, true)->id);
              VERIFY_ARE_EQUAL(d->id1, fnToLayoutData(true, false)->id);
              VERIFY_ARE_EQUAL(d->id2, fnToLayoutData(false, true)->id);
              VERIFY_ARE_EQUAL(d->id3, fnToLayoutData(false, false)->id);
              VERIFY_ARE_EQUAL(d->acrossX, fnToLayoutData(isTop[i], !isLeft[i])->id);
              VERIFY_ARE_EQUAL(d->acrossY, fnToLayoutData(!isTop[i], isLeft[i])->id);
              VERIFY_ARE_EQUAL(d->acrossDiag, fnToLayoutData(!isTop[i], !isLeft[i])->id);
              VERIFY_ARE_EQUAL(d->quadActiveCount, count);
            }
          }
        }
        cur = groupEnd;
      }
    }

    // TODO: provide validation for quads where the same pixel was shaded multiple times
    //
    // Consider: for pixels that were shaded multiple times, check whether
    // some grouping of threads into quads satisfies all value requirements.
  }
}

struct ShaderOpTestResult {
  st::ShaderOp *ShaderOp;
  std::shared_ptr<st::ShaderOpSet> ShaderOpSet;
  std::shared_ptr<st::ShaderOpTest> Test;
};

struct SPrimitives {
  float f_float;
  float f_float2;
  float f_float_o;
  float f_float2_o;
};

std::shared_ptr<ShaderOpTestResult>
RunShaderOpTestAfterParse(ID3D12Device *pDevice, dxc::DxcDllSupport &support,
                          LPCSTR pName,
                          st::ShaderOpTest::TInitCallbackFn pInitCallback,
                          std::shared_ptr<st::ShaderOpSet> ShaderOpSet) {
  st::ShaderOp *pShaderOp;
  if (pName == nullptr) {
    if (ShaderOpSet->ShaderOps.size() != 1) {
      VERIFY_FAIL(L"Expected a single shader operation.");
    }
    pShaderOp = ShaderOpSet->ShaderOps[0].get();
  }
  else {
    pShaderOp = ShaderOpSet->GetShaderOp(pName);
  }
  if (pShaderOp == nullptr) {
    std::string msg = "Unable to find shader op ";
    msg += pName;
    msg += "; available ops";
    const char sep = ':';
    for (auto &pAvailOp : ShaderOpSet->ShaderOps) {
      msg += sep;
      msg += pAvailOp->Name ? pAvailOp->Name : "[n/a]";
    }
    CA2W msgWide(msg.c_str());
    VERIFY_FAIL(msgWide.m_psz);
  }

  // This won't actually be used since we're supplying the device,
  // but let's make it consistent.
  pShaderOp->UseWarpDevice = GetTestParamUseWARP(true);

  std::shared_ptr<st::ShaderOpTest> test = std::make_shared<st::ShaderOpTest>();
  test->SetDxcSupport(&support);
  test->SetInitCallback(pInitCallback);
  test->SetDevice(pDevice);
  test->RunShaderOp(pShaderOp);

  std::shared_ptr<ShaderOpTestResult> result =
      std::make_shared<ShaderOpTestResult>();
  result->ShaderOpSet = ShaderOpSet;
  result->Test = test;
  result->ShaderOp = pShaderOp;
  return result;
}

std::shared_ptr<ShaderOpTestResult>
RunShaderOpTest(ID3D12Device *pDevice, dxc::DxcDllSupport &support,
                IStream *pStream, LPCSTR pName,
                st::ShaderOpTest::TInitCallbackFn pInitCallback) {
  DXASSERT_NOMSG(pStream != nullptr);
  std::shared_ptr<st::ShaderOpSet> ShaderOpSet =
        std::make_shared<st::ShaderOpSet>();
  st::ParseShaderOpSetFromStream(pStream, ShaderOpSet.get());
  return RunShaderOpTestAfterParse(pDevice, support, pName, pInitCallback, ShaderOpSet);
}

TEST_F(ExecutionTest, OutOfBoundsTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  // Single operation test at the moment.
  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(pDevice, m_support, pStream, "OOB", nullptr);
  MappedData data;
  // Read back to CPU and examine contents - should get pure red.
  {
    MappedData data;
    test->Test->GetReadBackData("RTarget", &data);
    const uint32_t *pPixels = (uint32_t *)data.data();
    uint32_t first = *pPixels;
    VERIFY_ARE_EQUAL(0xff0000ff, first); // pure red - only first component is read
  }
}

TEST_F(ExecutionTest, SaturateTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  // Single operation test at the moment.
  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(pDevice, m_support, pStream, "Saturate", nullptr);
  MappedData data;
  test->Test->GetReadBackData("U0", &data);
  const float *pValues = (float *)data.data();
  // Everything is zero except for 1.5f and +Inf, which saturate to 1.0f
  const float ExpectedCases[9] = {
    0.0f, 0.0f, 0.0f, 0.0f, // -inf, -1.5, -denorm, -0
    0.0f, 0.0f, 1.0f, 1.0f, // 0, denorm, 1.5f, inf
    0.0f                    // nan
  };
  for (size_t i = 0; i < _countof(ExpectedCases); ++i) {
    VERIFY_IS_TRUE(ifdenorm_flushf_eq(*pValues, ExpectedCases[i]));
    ++pValues;
  }
}

void ExecutionTest::BasicTriangleTestSetup(LPCSTR ShaderOpName, LPCWSTR FileName, D3D_SHADER_MODEL testModel) {
#ifdef _HLK_CONF
  UNREFERENCED_PARAMETER(ShaderOpName);
  UNREFERENCED_PARAMETER(FileName);
  UNREFERENCED_PARAMETER(testModel);
#else
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  // Single operation test at the moment.
  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice, testModel))
    return;

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(pDevice, m_support, pStream, ShaderOpName, nullptr);
  MappedData data;
  D3D12_RESOURCE_DESC &D = test->ShaderOp->GetResourceByName("RTarget")->Desc;
  UINT width = (UINT64)D.Width;
  UINT height = (UINT64)D.Height;
  test->Test->GetReadBackData("RTarget", &data);
  const uint32_t *pPixels = (uint32_t *)data.data();
  if (SaveImages()) {
    SavePixelsToFile(pPixels, DXGI_FORMAT_R8G8B8A8_UNORM, 320, 200, FileName);
  }
  uint32_t top = pPixels[width / 2]; // Top center.
  uint32_t mid = pPixels[width / 2 + width * (height / 2)]; // Middle center.
  VERIFY_ARE_EQUAL(0xff663300, top); // clear color
  VERIFY_ARE_EQUAL(0xffffffff, mid); // white

  // This is the basic validation test for shader operations, so it's good to
  // check this here at least for this one test case.
  data.reset();
  test.reset();
  ReportLiveObjects();
#endif
}

TEST_F(ExecutionTest, BasicTriangleOpTest) {
  BasicTriangleTestSetup("Triangle", L"basic-triangle.bmp", D3D_SHADER_MODEL_6_0);
}

TEST_F(ExecutionTest, BasicTriangleOpTestHalf) {
  BasicTriangleTestSetup("TriangleHalf", L"basic-triangle-half.bmp", D3D_SHADER_MODEL_6_2);
}

// Rendering two right triangles forming a square and assigning a texture value
// for each pixel to calculate derivates.
TEST_F(ExecutionTest, PartialDerivTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
      return;

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(pDevice, m_support, pStream, "DerivFine", nullptr);
  MappedData data;
  D3D12_RESOURCE_DESC &D = test->ShaderOp->GetResourceByName("RTarget")->Desc;
  UINT width = (UINT)D.Width;
  UINT height = D.Height;
  UINT pixelSize = GetByteSizeForFormat(D.Format) / 4;

  test->Test->GetReadBackData("RTarget", &data);
  const float *pPixels = (float *)data.data();

  UINT centerIndex = (UINT64)width * height / 2 - width / 2;

  // pixel at the center
  UINT offsetCenter = centerIndex * pixelSize;
  float CenterDDXFine = pPixels[offsetCenter];
  float CenterDDYFine = pPixels[offsetCenter + 1];
  float CenterDDXCoarse = pPixels[offsetCenter + 2];
  float CenterDDYCoarse = pPixels[offsetCenter + 3];

  LogCommentFmt(
      L"center  ddx_fine: %8f, ddy_fine: %8f, ddx_coarse: %8f, ddy_coarse: %8f",
      CenterDDXFine, CenterDDYFine, CenterDDXCoarse, CenterDDYCoarse);

  // The texture for the 9 pixels in the center should look like the following

  // 256   32  64
  // 2048 256 512
  // 1   .125 .25

  // In D3D12 there is no guarantee of how the adapter is grouping 2x2 pixels
  // So for fine derivatives there can be up to two possible results for the center pixel,
  // while for coarse derivatives there can be up to six possible results.
  int ulpTolerance = 1;
  // 512 - 256 or 2048 - 256
  bool left = CompareFloatULP(CenterDDXFine, -1792.0f, ulpTolerance);
  VERIFY_IS_TRUE(left || CompareFloatULP(CenterDDXFine, 256.0f, ulpTolerance));
  // 256 - 32 or 256 - .125
  bool top = CompareFloatULP(CenterDDYFine, 224.0f, ulpTolerance);
  VERIFY_IS_TRUE(top || CompareFloatULP(CenterDDYFine, -255.875, ulpTolerance));

  if (top && left) {
    VERIFY_IS_TRUE((CompareFloatULP(CenterDDXCoarse, -224.0f, ulpTolerance) ||
                   CompareFloatULP(CenterDDXCoarse, -1792.0f, ulpTolerance)) &&
                   (CompareFloatULP(CenterDDYCoarse, 224.0f, ulpTolerance) ||
                   CompareFloatULP(CenterDDYCoarse, 1792.0f, ulpTolerance)));
  }
  else if (top) { // top right quad
    VERIFY_IS_TRUE((CompareFloatULP(CenterDDXCoarse, 256.0f, ulpTolerance)  ||
                   CompareFloatULP(CenterDDXCoarse, 32.0f, ulpTolerance))   &&
                   (CompareFloatULP(CenterDDYCoarse, 224.0f, ulpTolerance) ||
                   CompareFloatULP(CenterDDYCoarse, 448.0f, ulpTolerance)));
  }
  else if (left) { // bottom left quad
    VERIFY_IS_TRUE((CompareFloatULP(CenterDDXCoarse, -1792.0f, ulpTolerance) ||
                   CompareFloatULP(CenterDDXCoarse, -.875f, ulpTolerance))   &&
                   (CompareFloatULP(CenterDDYCoarse, -2047.0f, ulpTolerance) ||
                   CompareFloatULP(CenterDDYCoarse, -255.875f, ulpTolerance)));
  }
  else { // bottom right
    VERIFY_IS_TRUE((CompareFloatULP(CenterDDXCoarse, 256.0f, ulpTolerance) ||
                   CompareFloatULP(CenterDDXCoarse, .125f, ulpTolerance))  &&
                   (CompareFloatULP(CenterDDYCoarse, -255.875f, ulpTolerance) ||
                   CompareFloatULP(CenterDDYCoarse, -511.75f, ulpTolerance)));
  }
}

// Executing a simple binop to verify shadel model 6.1 support; runs with
// ShaderModel61.CoreRequirement
TEST_F(ExecutionTest, BasicShaderModel61) {
  RunBasicShaderModelTest(D3D_SHADER_MODEL_6_1);
}

// Executing a simple binop to verify shadel model 6.3 support; runs with
// ShaderModel63.CoreRequirement
TEST_F(ExecutionTest, BasicShaderModel63) {
  RunBasicShaderModelTest(D3D_SHADER_MODEL_6_3);
}

void ExecutionTest::RunBasicShaderModelTest(D3D_SHADER_MODEL shaderModel) {

  WEX::TestExecution::SetVerifyOutput verifySettings(
      WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice, shaderModel)) {
    return;
  }

  char *pShaderModelStr;
  if (shaderModel == D3D_SHADER_MODEL_6_1) {
    pShaderModelStr = "cs_6_1";
  } else if (shaderModel == D3D_SHADER_MODEL_6_3) {
    pShaderModelStr = "cs_6_3";
  } else {
    DXASSERT_NOMSG("Invalid Shader Model Parameter");
    pShaderModelStr = nullptr;
  }

  const char shaderTemplate[] =
      "struct SBinaryOp { %s input1; %s input2; %s output; };"
      "RWStructuredBuffer<SBinaryOp> g_buf : register(u0);"
      "[numthreads(8,8,1)]"
      "void main(uint GI : SV_GroupIndex) {"
      "    SBinaryOp l = g_buf[GI];"
      "    l.output = l.input1 + l.input2;"
      "    g_buf[GI] = l;"
      "}";
  char shader[sizeof(shaderTemplate) + 50];

  // Run simple shader with float data types
  char* sTy = "float";
  float inputFloatPairs[] = { 1.5f, -2.8f, 3.23e-5f, 6.0f, 181.621f, 14.978f };
  VERIFY_IS_TRUE(sprintf(shader, shaderTemplate, sTy, sTy, sTy) > 0);
  WEX::Logging::Log::Comment(L"BasicShaderModel float");
  RunBasicShaderModelTest<float>(pDevice, pShaderModelStr, shader, inputFloatPairs, sizeof(inputFloatPairs) / (2 * sizeof(float)));

   // Run simple shader with double data types
  if (DoesDeviceSupportDouble(pDevice)) {
    sTy = "double";
    double inputDoublePairs[] = { 1.5891020, -2.8, 3.23e-5, 1 / 3, 181.91621, 14.654978 };
    VERIFY_IS_TRUE(sprintf(shader, shaderTemplate, sTy, sTy, sTy) > 0);
    WEX::Logging::Log::Comment(L"BasicShaderModel double");
    RunBasicShaderModelTest<double>(pDevice, pShaderModelStr, shader, inputDoublePairs, sizeof(inputDoublePairs) / (2 * sizeof(double)));
   }
   else {
     // Optional feature, so it's correct to not support it if declared as such.
     WEX::Logging::Log::Comment(L"Device does not support double operations.");
   }

   // Run simple shader with int64 types
   if (DoesDeviceSupportInt64(pDevice)) {
     sTy = "int64_t";
     int64_t inputInt64Pairs[] = { 1, -100, 6814684, -9814810, 654, 1021248900 };
     VERIFY_IS_TRUE(sprintf(shader, shaderTemplate, sTy, sTy, sTy) > 0);
     WEX::Logging::Log::Comment(L"BasicShaderModel int64_t");
     RunBasicShaderModelTest<int64_t>(pDevice, pShaderModelStr, shader, inputInt64Pairs, sizeof(inputInt64Pairs) / (2 * sizeof(int64_t)));
   }
   else {
     // Optional feature, so it's correct to not support it if declared as such.
     WEX::Logging::Log::Comment(L"Device does not support int64 operations.");
   }
}

template <class Ty>
const wchar_t* ExecutionTest::BasicShaderModelTest_GetFormatString() {
  DXASSERT_NOMSG("Unsupported type");
  return "";
}

template <>
const wchar_t* ExecutionTest::BasicShaderModelTest_GetFormatString<float>() {
  return L"element #%u: input1 = %6.8f, input1 = %6.8f, output = %6.8f, expected = %6.8f";
}

template <>
const wchar_t* ExecutionTest::BasicShaderModelTest_GetFormatString<double>() {
  return BasicShaderModelTest_GetFormatString<float>();
}

template <>
const wchar_t* ExecutionTest::BasicShaderModelTest_GetFormatString<int64_t>() {
  return L"element #%u: input1 = %ld, input1 = %ld, output = %ld, expected = %ld";
}

template <class Ty>
void ExecutionTest::RunBasicShaderModelTest(CComPtr<ID3D12Device> pDevice, const char *pShaderModelStr, const char *pShader,
                                           Ty *pInputDataPairs, unsigned inputDataCount) {
  struct SBinaryOp {
    Ty input1;
    Ty input2;
    Ty output;
  };

  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
    pDevice, m_support, pStream, "BinaryFPOp",
    // this callbacked is called when the test is creating the resource to run the test
    [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
      UNREFERENCED_PARAMETER(Name);
      pShaderOp->Shaders.at(0).Target = pShaderModelStr;
      pShaderOp->Shaders.at(0).Text = pShader;
      size_t size = sizeof(SBinaryOp) * inputDataCount;
      Data.resize(size);
      SBinaryOp *pPrimitives = (SBinaryOp*)Data.data();
      Ty *pIn = pInputDataPairs;
      for (size_t i = 0; i < inputDataCount; i++, pIn += 2) {
        SBinaryOp *p = &pPrimitives[i];
        p->input1 = pIn[0];
        p->input2 = pIn[1];
      }
  });

  VERIFY_SUCCEEDED(S_OK);

  MappedData data;
  test->Test->GetReadBackData("SBinaryFPOp", &data);
  SBinaryOp *pPrimitives = (SBinaryOp*)data.data();

  const wchar_t* formatStr = BasicShaderModelTest_GetFormatString<Ty>();
  Ty *pIn = pInputDataPairs;

  for (unsigned i = 0; i < inputDataCount; i++, pIn += 2) {
    Ty expValue = pIn[0] + pIn[1];
    SBinaryOp *p = &pPrimitives[i];
    
    LogCommentFmt(formatStr, i,  pIn[0], pIn[1], p->output, expValue);
    VERIFY_ARE_EQUAL(p->output, expValue);
  }
}


// Resource structure for data-driven tests.

struct SUnaryFPOp {
    float input;
    float output;
};

struct SBinaryFPOp {
    float input1;
    float input2;
    float output1;
    float output2;
};

struct STertiaryFPOp {
    float input1;
    float input2;
    float input3;
    float output;
};

struct SUnaryHalfOp {
  uint16_t input;
  uint16_t output;
};

struct SBinaryHalfOp {
  uint16_t input1;
  uint16_t input2;
  uint16_t output1;
  uint16_t output2;
};

struct STertiaryHalfOp {
  uint16_t input1;
  uint16_t input2;
  uint16_t input3;
  uint16_t output;
};

struct SUnaryIntOp {
    int input;
    int output;
};

struct SUnaryUintOp {
    unsigned int input;
    unsigned int output;
};

struct SBinaryIntOp {
    int input1;
    int input2;
    int output1;
    int output2;
};

struct STertiaryIntOp {
    int input1;
    int input2;
    int input3;
    int output;
};

struct SBinaryUintOp {
    unsigned int input1;
    unsigned int input2;
    unsigned int output1;
    unsigned int output2;
};

struct STertiaryUintOp {
    unsigned int input1;
    unsigned int input2;
    unsigned int input3;
    unsigned int output;
};

struct SUnaryInt16Op {
  short input;
  short output;
};

struct SUnaryUint16Op {
  unsigned short input;
  unsigned short output;
};

struct SBinaryInt16Op {
  short input1;
  short input2;
  short output1;
  short output2;
};

struct STertiaryInt16Op {
  short input1;
  short input2;
  short input3;
  short output;
};

struct SBinaryUint16Op {
  unsigned short input1;
  unsigned short input2;
  unsigned short output1;
  unsigned short output2;
};

struct STertiaryUint16Op {
  unsigned short input1;
  unsigned short input2;
  unsigned short input3;
  unsigned short output;
};
// representation for HLSL float vectors
struct SDotOp {
    XMFLOAT4 input1;
    XMFLOAT4 input2;
    float o_dot2;
    float o_dot3;
    float o_dot4;
};

struct Half2
{
    uint16_t x;
    uint16_t y;

    Half2() = default;

    Half2(const Half2&) = default;
    Half2& operator=(const Half2&) = default;

    Half2(Half2&&) = default;
    Half2& operator=(Half2&&) = default;

    constexpr Half2(uint16_t _x, uint16_t _y) : x(_x), y(_y) {}
    explicit Half2(_In_reads_(2) const uint16_t *pArray) : x(pArray[0]), y(pArray[1]) {}
};

struct SDot2AddHalfOp {
    Half2 input1;
    Half2 input2;
    float acc;
    float result;
};

struct SDot4AddI8PackedOp {
    uint32_t input1;
    uint32_t input2;
    int32_t acc;
    int32_t result;
};

struct SDot4AddU8PackedOp {
    uint32_t input1;
    uint32_t input2;
    uint32_t acc;
    uint32_t result;
};

struct SMsad4 {
    unsigned int ref;
    XMUINT2 src;
    XMUINT4 accum;
    XMUINT4 result;
};
// Parameter representation for taef data-driven tests
struct TableParameter {
    LPCWSTR m_name;
    enum TableParameterType {
        INT8,
        INT16,
        INT32,
        UINT,
        FLOAT,
        HALF,
        DOUBLE,
        STRING,
        BOOL,
        INT8_TABLE,
        INT16_TABLE,
        INT32_TABLE,
        FLOAT_TABLE,
        HALF_TABLE,
        DOUBLE_TABLE,
        STRING_TABLE,
        UINT8_TABLE,
        UINT16_TABLE,
        UINT32_TABLE,
        BOOL_TABLE
    };
    TableParameterType m_type;
    bool m_required; // required parameter
    int8_t m_int8;
    int16_t m_int16;
    int m_int32;
    unsigned int m_uint;
    float m_float;
    uint16_t m_half; // no such thing as half type in c++. Use int16 instead
    double m_double;
    bool m_bool;
    WEX::Common::String m_str;
    std::vector<int8_t> m_int8Table;
    std::vector<int16_t> m_int16Table;
    std::vector<int> m_int32Table;
    std::vector<uint8_t> m_uint8Table;
    std::vector<uint16_t> m_uint16Table;
    std::vector<unsigned int> m_uint32Table;
    std::vector<float> m_floatTable;
    std::vector<uint16_t> m_halfTable; // no such thing as half type in c++
    std::vector<double> m_doubleTable;
    std::vector<bool> m_boolTable;
    std::vector<WEX::Common::String> m_StringTable;
};

class TableParameterHandler {
private:
  HRESULT ParseTableRow();
public:
  TableParameter* m_table;
  size_t m_tableSize;
  TableParameterHandler(TableParameter *pTable, size_t size) : m_table(pTable), m_tableSize(size) {
    clearTableParameter();
    VERIFY_SUCCEEDED(ParseTableRow());
  }

  TableParameter* GetTableParamByName(LPCWSTR name) {
    for (size_t i = 0; i < m_tableSize; ++i) {
      if (_wcsicmp(name, m_table[i].m_name) == 0) {
        return &m_table[i];
      }
    }
    DXASSERT_ARGS(false, "Invalid Table Parameter Name %s", name);
    return nullptr;
  }

  void clearTableParameter() {
    for (size_t i = 0; i < m_tableSize; ++i) {
      m_table[i].m_int32 = 0;
      m_table[i].m_uint = 0;
      m_table[i].m_double = 0;
      m_table[i].m_bool = false;
      m_table[i].m_str = WEX::Common::String();
    }
  }

  template <class T1>
  std::vector<T1> *GetDataArray(LPCWSTR name) {
    return nullptr;
  }

  template <>
  std::vector<int> *GetDataArray(LPCWSTR name) {
    for (size_t i = 0; i < m_tableSize; ++i) {
      if (_wcsicmp(name, m_table[i].m_name) == 0) {
        return &(m_table[i].m_int32Table);
      }
    }
    DXASSERT_ARGS(false, "Invalid Table Parameter Name %s", name);
    return nullptr;
  }

  template <>
  std::vector<int8_t> *GetDataArray(LPCWSTR name) {
    for (size_t i = 0; i < m_tableSize; ++i) {
      if (_wcsicmp(name, m_table[i].m_name) == 0) {
        return &(m_table[i].m_int8Table);
      }
    }
    DXASSERT_ARGS(false, "Invalid Table Parameter Name %s", name);
    return nullptr;
  }

  template <>
  std::vector<int16_t> *GetDataArray(LPCWSTR name) {
    for (size_t i = 0; i < m_tableSize; ++i) {
      if (_wcsicmp(name, m_table[i].m_name) == 0) {
        return &(m_table[i].m_int16Table);
      }
    }
    DXASSERT_ARGS(false, "Invalid Table Parameter Name %s", name);
    return nullptr;
  }

  template <>
  std::vector<unsigned int> *GetDataArray(LPCWSTR name) {
    for (size_t i = 0; i < m_tableSize; ++i) {
      if (_wcsicmp(name, m_table[i].m_name) == 0) {
        return &(m_table[i].m_uint32Table);
      }
    }
    DXASSERT_ARGS(false, "Invalid Table Parameter Name %s", name);
    return nullptr;
  }

  template <>
  std::vector<float> *GetDataArray(LPCWSTR name) {
    for (size_t i = 0; i < m_tableSize; ++i) {
      if (_wcsicmp(name, m_table[i].m_name) == 0) {
        return &(m_table[i].m_floatTable);
      }
    }
    DXASSERT_ARGS(false, "Invalid Table Parameter Name %s", name);
    return nullptr;
  }

  // TODO: uin16_t may be used to represent two different types when we introduce uint16
  template <>
  std::vector<uint16_t> *GetDataArray(LPCWSTR name) {
    for (size_t i = 0; i < m_tableSize; ++i) {
      if (_wcsicmp(name, m_table[i].m_name) == 0) {
        return &(m_table[i].m_halfTable);
      }
    }
    DXASSERT_ARGS(false, "Invalid Table Parameter Name %s", name);
    return nullptr;
  }

  template <>
  std::vector<double> *GetDataArray(LPCWSTR name) {
    for (size_t i = 0; i < m_tableSize; ++i) {
      if (_wcsicmp(name, m_table[i].m_name) == 0) {
        return &(m_table[i].m_doubleTable);
      }
    }
    DXASSERT_ARGS(false, "Invalid Table Parameter Name %s", name);
    return nullptr;
  }

  template <>
  std::vector<bool> *GetDataArray(LPCWSTR name) {
    for (size_t i = 0; i < m_tableSize; ++i) {
      if (_wcsicmp(name, m_table[i].m_name) == 0) {
        return &(m_table[i].m_boolTable);
      }
    }
    DXASSERT_ARGS(false, "Invalid Table Parameter Name %s", name);
    return nullptr;
  }

};

static TableParameter UnaryFPOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::FLOAT_TABLE, true },
    { L"Validation.Expected1", TableParameter::FLOAT_TABLE, true },
    { L"Validation.Type", TableParameter::STRING, true },
    { L"Validation.Tolerance", TableParameter::DOUBLE, true },
    { L"Warp.Version", TableParameter::UINT, false }
};

static TableParameter BinaryFPOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::FLOAT_TABLE, true },
    { L"Validation.Input2", TableParameter::FLOAT_TABLE, true },
    { L"Validation.Expected1", TableParameter::FLOAT_TABLE, true },
    { L"Validation.Expected2", TableParameter::FLOAT_TABLE, false },
    { L"Validation.Type", TableParameter::STRING, true },
    { L"Validation.Tolerance", TableParameter::DOUBLE, true },
};

static TableParameter TertiaryFPOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::FLOAT_TABLE, true },
    { L"Validation.Input2", TableParameter::FLOAT_TABLE, true },
    { L"Validation.Input3", TableParameter::FLOAT_TABLE, true },
    { L"Validation.Expected1", TableParameter::FLOAT_TABLE, true },
    { L"Validation.Type", TableParameter::STRING, true },
    { L"Validation.Tolerance", TableParameter::DOUBLE, true },
};

static TableParameter UnaryHalfOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"ShaderOp.Arguments", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::HALF_TABLE, true },
    { L"Validation.Expected1", TableParameter::HALF_TABLE, true },
    { L"Validation.Type", TableParameter::STRING, true },
    { L"Validation.Tolerance", TableParameter::DOUBLE, true },
    { L"Warp.Version", TableParameter::UINT, false }
};

static TableParameter BinaryHalfOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"ShaderOp.Arguments", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::HALF_TABLE, true },
    { L"Validation.Input2", TableParameter::HALF_TABLE, true },
    { L"Validation.Expected1", TableParameter::HALF_TABLE, true },
    { L"Validation.Expected2", TableParameter::HALF_TABLE, false },
    { L"Validation.Type", TableParameter::STRING, true },
    { L"Validation.Tolerance", TableParameter::DOUBLE, true },
};

static TableParameter TertiaryHalfOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"ShaderOp.Arguments", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::HALF_TABLE, true },
    { L"Validation.Input2", TableParameter::HALF_TABLE, true },
    { L"Validation.Input3", TableParameter::HALF_TABLE, true },
    { L"Validation.Expected1", TableParameter::HALF_TABLE, true },
    { L"Validation.Type", TableParameter::STRING, true },
    { L"Validation.Tolerance", TableParameter::DOUBLE, true },
};

static TableParameter UnaryIntOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::INT32_TABLE, true },
    { L"Validation.Expected1", TableParameter::INT32_TABLE, true },
    { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter UnaryUintOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::UINT32_TABLE, true },
    { L"Validation.Expected1", TableParameter::UINT32_TABLE, true },
    { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter BinaryIntOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::INT32_TABLE, true },
    { L"Validation.Input2", TableParameter::INT32_TABLE, true },
    { L"Validation.Expected1", TableParameter::INT32_TABLE, true },
    { L"Validation.Expected2", TableParameter::INT32_TABLE, false },
    { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter TertiaryIntOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::INT32_TABLE, true },
    { L"Validation.Input2", TableParameter::INT32_TABLE, true },
    { L"Validation.Input3", TableParameter::INT32_TABLE, true },
    { L"Validation.Expected1", TableParameter::INT32_TABLE, true },
    { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter BinaryUintOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::UINT32_TABLE, true },
    { L"Validation.Input2", TableParameter::UINT32_TABLE, true },
    { L"Validation.Expected1", TableParameter::UINT32_TABLE, true },
    { L"Validation.Expected2", TableParameter::UINT32_TABLE, false },
    { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter TertiaryUintOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::UINT32_TABLE, true },
    { L"Validation.Input2", TableParameter::UINT32_TABLE, true },
    { L"Validation.Input3", TableParameter::UINT32_TABLE, true },
    { L"Validation.Expected1", TableParameter::UINT32_TABLE, true },
    { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter UnaryInt16OpParameters[] = {
  { L"ShaderOp.Target", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"ShaderOp.Arguments", TableParameter::STRING, true },
  { L"Validation.Input1", TableParameter::INT16_TABLE, true },
  { L"Validation.Expected1", TableParameter::INT16_TABLE, true },
  { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter UnaryUint16OpParameters[] = {
  { L"ShaderOp.Target", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"ShaderOp.Arguments", TableParameter::STRING, true },
  { L"Validation.Input1", TableParameter::UINT16_TABLE, true },
  { L"Validation.Expected1", TableParameter::UINT16_TABLE, true },
  { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter BinaryInt16OpParameters[] = {
  { L"ShaderOp.Target", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"ShaderOp.Arguments", TableParameter::STRING, true },
  { L"Validation.Input1", TableParameter::INT16_TABLE, true },
  { L"Validation.Input2", TableParameter::INT16_TABLE, true },
  { L"Validation.Expected1", TableParameter::INT16_TABLE, true },
  { L"Validation.Expected2", TableParameter::INT16_TABLE, false },
  { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter TertiaryInt16OpParameters[] = {
  { L"ShaderOp.Target", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"ShaderOp.Arguments", TableParameter::STRING, true },
  { L"Validation.Input1", TableParameter::INT16_TABLE, true },
  { L"Validation.Input2", TableParameter::INT16_TABLE, true },
  { L"Validation.Input3", TableParameter::INT16_TABLE, true },
  { L"Validation.Expected1", TableParameter::INT16_TABLE, true },
  { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter BinaryUint16OpParameters[] = {
  { L"ShaderOp.Target", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"ShaderOp.Arguments", TableParameter::STRING, true },
  { L"Validation.Input1", TableParameter::UINT16_TABLE, true },
  { L"Validation.Input2", TableParameter::UINT16_TABLE, true },
  { L"Validation.Expected1", TableParameter::UINT16_TABLE, true },
  { L"Validation.Expected2", TableParameter::UINT16_TABLE, false },
  { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter TertiaryUint16OpParameters[] = {
  { L"ShaderOp.Target", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"ShaderOp.Arguments", TableParameter::STRING, true },
  { L"Validation.Input1", TableParameter::UINT16_TABLE, true },
  { L"Validation.Input2", TableParameter::UINT16_TABLE, true },
  { L"Validation.Input3", TableParameter::UINT16_TABLE, true },
  { L"Validation.Expected1", TableParameter::UINT16_TABLE, true },
  { L"Validation.Tolerance", TableParameter::INT32, true },
};

static TableParameter DotOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::STRING_TABLE, true },
    { L"Validation.Input2", TableParameter::STRING_TABLE, true },
    { L"Validation.Expected1", TableParameter::STRING_TABLE, true },
    { L"Validation.Expected2", TableParameter::STRING_TABLE, true },
    { L"Validation.Expected3", TableParameter::STRING_TABLE, true },
    { L"Validation.Type", TableParameter::STRING, true },
    { L"Validation.Tolerance", TableParameter::DOUBLE, true },
};

static TableParameter Dot2AddHalfOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"ShaderOp.Arguments", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::STRING_TABLE, true },
    { L"Validation.Input2", TableParameter::STRING_TABLE, true },
    { L"Validation.Input3", TableParameter::FLOAT_TABLE, true },
    { L"Validation.Expected1", TableParameter::FLOAT_TABLE, true },
    { L"Validation.Type", TableParameter::STRING, true },
    { L"Validation.Tolerance", TableParameter::DOUBLE, true },
};

static TableParameter Dot4AddI8PackedOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::UINT32_TABLE, true },
    { L"Validation.Input2", TableParameter::UINT32_TABLE, true },
    { L"Validation.Input3", TableParameter::INT32_TABLE, true },
    { L"Validation.Expected1", TableParameter::INT32_TABLE, true },
};

static TableParameter Dot4AddU8PackedOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::UINT32_TABLE, true },
    { L"Validation.Input2", TableParameter::UINT32_TABLE, true },
    { L"Validation.Input3", TableParameter::UINT32_TABLE, true },
    { L"Validation.Expected1", TableParameter::UINT32_TABLE, true },
};

static TableParameter Msad4OpParameters[] = {
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.Tolerance", TableParameter::DOUBLE, true },
    { L"Validation.Input1", TableParameter::UINT32_TABLE, true},
    { L"Validation.Input2", TableParameter::STRING_TABLE, true },
    { L"Validation.Input3", TableParameter::STRING_TABLE, true },
    { L"Validation.Expected1", TableParameter::STRING_TABLE, true }
};

static TableParameter WaveIntrinsicsActiveIntParameters[] = {
    { L"ShaderOp.Name", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"Validation.NumInputSet", TableParameter::UINT, true },
    { L"Validation.InputSet1", TableParameter::INT32_TABLE, true },
    { L"Validation.InputSet2", TableParameter::INT32_TABLE, false },
    { L"Validation.InputSet3", TableParameter::INT32_TABLE, false },
    { L"Validation.InputSet4", TableParameter::INT32_TABLE, false }
};

static TableParameter WaveIntrinsicsPrefixIntParameters[] = {
  { L"ShaderOp.Name", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"Validation.NumInputSet", TableParameter::UINT, true },
  { L"Validation.InputSet1", TableParameter::INT32_TABLE, true },
  { L"Validation.InputSet2", TableParameter::INT32_TABLE, false },
  { L"Validation.InputSet3", TableParameter::INT32_TABLE, false },
  { L"Validation.InputSet4", TableParameter::INT32_TABLE, false }
};

static TableParameter WaveIntrinsicsActiveUintParameters[] = {
  { L"ShaderOp.Name", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"Validation.NumInputSet", TableParameter::UINT, true },
  { L"Validation.InputSet1", TableParameter::UINT32_TABLE, true },
  { L"Validation.InputSet2", TableParameter::UINT32_TABLE, false },
  { L"Validation.InputSet3", TableParameter::UINT32_TABLE, false },
  { L"Validation.InputSet4", TableParameter::UINT32_TABLE, false }
};

static TableParameter WaveIntrinsicsPrefixUintParameters[] = {
  { L"ShaderOp.Name", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"Validation.NumInputSet", TableParameter::UINT, true },
  { L"Validation.InputSet1", TableParameter::UINT32_TABLE, true },
  { L"Validation.InputSet2", TableParameter::UINT32_TABLE, false },
  { L"Validation.InputSet3", TableParameter::UINT32_TABLE, false },
  { L"Validation.InputSet4", TableParameter::UINT32_TABLE, false }
};

static TableParameter WaveIntrinsicsMultiPrefixIntParameters[] = {
  { L"ShaderOp.Name", TableParameter::STRING, true },
  { L"ShaderOp.Target", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"Validation.Keys", TableParameter::INT32_TABLE, true },
  { L"Validation.Values", TableParameter::INT32_TABLE, true },
};

static TableParameter WaveIntrinsicsMultiPrefixUintParameters[] = {
  { L"ShaderOp.Name", TableParameter::STRING, true },
  { L"ShaderOp.Target", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"Validation.Keys", TableParameter::UINT32_TABLE, true },
  { L"Validation.Values", TableParameter::UINT32_TABLE, true },
};

static TableParameter WaveIntrinsicsActiveBoolParameters[] = {
  { L"ShaderOp.Name", TableParameter::STRING, true },
  { L"ShaderOp.Text", TableParameter::STRING, true },
  { L"Validation.NumInputSet", TableParameter::UINT, true },
  { L"Validation.InputSet1", TableParameter::BOOL_TABLE, true },
  { L"Validation.InputSet2", TableParameter::BOOL_TABLE, false },
  { L"Validation.InputSet3", TableParameter::BOOL_TABLE, false },
};

static TableParameter CBufferTestHalfParameters[] = {
  { L"Validation.InputSet", TableParameter::HALF_TABLE, true },
};

static TableParameter DenormBinaryFPOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"ShaderOp.Arguments", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::STRING_TABLE, true },
    { L"Validation.Input2", TableParameter::STRING_TABLE, true },
    { L"Validation.Expected1", TableParameter::STRING_TABLE, true },
    { L"Validation.Expected2", TableParameter::STRING_TABLE, false },
    { L"Validation.Type", TableParameter::STRING, true },
    { L"Validation.Tolerance", TableParameter::DOUBLE, true },
};

static TableParameter DenormTertiaryFPOpParameters[] = {
    { L"ShaderOp.Target", TableParameter::STRING, true },
    { L"ShaderOp.Text", TableParameter::STRING, true },
    { L"ShaderOp.Arguments", TableParameter::STRING, true },
    { L"Validation.Input1", TableParameter::STRING_TABLE, true },
    { L"Validation.Input2", TableParameter::STRING_TABLE, true },
    { L"Validation.Input3", TableParameter::STRING_TABLE, true },
    { L"Validation.Expected1", TableParameter::STRING_TABLE, true },
    { L"Validation.Expected2", TableParameter::STRING_TABLE, false },
    { L"Validation.Type", TableParameter::STRING, true },
    { L"Validation.Tolerance", TableParameter::DOUBLE, true },
};

static bool IsHexString(PCWSTR str, uint16_t *value) {
  std::wstring wString(str);
  wString.erase(std::remove(wString.begin(), wString.end(), L' '), wString.end());
  LPCWSTR wstr = wString.c_str();
  if (wcsncmp(wstr, L"0x", 2) == 0 || wcsncmp(wstr, L"0b", 2) == 0) {
    *value = (uint16_t)wcstol(wstr, NULL, 0);
    return true;
  }
  return false;
}

static HRESULT ParseDataToFloat(PCWSTR str, float &value) {
  std::wstring wString(str);
  wString.erase(std::remove(wString.begin(), wString.end(), L' '), wString.end());
  PCWSTR wstr = wString.data();
  if (_wcsicmp(wstr, L"NaN") == 0) {
    value = NAN;
  } else if (_wcsicmp(wstr, L"-inf") == 0) {
    value = -(INFINITY);
  } else if (_wcsicmp(wstr, L"inf") == 0) {
    value = INFINITY;
  } else if (_wcsicmp(wstr, L"-denorm") == 0) {
    value = -(FLT_MIN / 2);
  } else if (_wcsicmp(wstr, L"denorm") == 0) {
    value = FLT_MIN / 2;
  } else if (_wcsicmp(wstr, L"-0.0f") == 0 || _wcsicmp(wstr, L"-0.0") == 0 ||
             _wcsicmp(wstr, L"-0") == 0) {
    value = -0.0f;
  } else if (_wcsicmp(wstr, L"0.0f") == 0 || _wcsicmp(wstr, L"0.0") == 0 ||
             _wcsicmp(wstr, L"0") == 0) {
    value = 0.0f;
  } else if (_wcsnicmp(wstr, L"0x", 2) == 0) { // For hex values, take values literally
    unsigned temp_i = std::stoul(wstr, nullptr, 16);
    value = (float&)temp_i;
  }
  else {
    // evaluate the expression of wstring
    double val = _wtof(wstr);
    if (val == 0) {
      LogErrorFmt(L"Failed to parse parameter %s to float", wstr);
      return E_FAIL;
    }
    value = (float)val;
  }
  return S_OK;
}

static HRESULT ParseDataToInt(PCWSTR str, int &value) {
  std::wstring wString(str);
  wString.erase(std::remove(wString.begin(), wString.end(), L' '), wString.end());
  PCWSTR wstr = wString.data();
  // evaluate the expression of string
  if (_wcsicmp(wstr, L"0.0") == 0 || _wcsicmp(wstr, L"0") == 0) {
      value = 0;
      return S_OK;
  }
  int val = _wtoi(wstr);
  if (val == 0) {
      LogErrorFmt(L"Failed to parse parameter %s to int", wstr);
      return E_FAIL;
  }
  value = val;
  return S_OK;
}

static HRESULT ParseDataToUint(PCWSTR str, unsigned int &value) {
    std::wstring wString(str);
    wString.erase(std::remove(wString.begin(), wString.end(), L' '), wString.end());
    PCWSTR wstr = wString.data();
    // evaluate the expression of string
    if (_wcsicmp(wstr, L"0") == 0 || _wcsicmp(wstr, L"0x00000000") == 0) {
        value = 0;
        return S_OK;
    }
    wchar_t *end;
    unsigned int val = std::wcstoul(wstr, &end, 0);
    if (val == 0) {
        LogErrorFmt(L"Failed to parse parameter %s to int", wstr);
        return E_FAIL;
    }
    value = val;
    return S_OK;
}

static HRESULT ParseDataToVectorFloat(PCWSTR str, float *ptr, size_t count) {
    std::wstring wstr(str);
    size_t curPosition = 0;
    // parse a string of dot product separated by commas
    for (size_t i = 0; i < count; ++i) {
        size_t nextPosition = wstr.find(L",", curPosition);
        if (FAILED(ParseDataToFloat(
            wstr.substr(curPosition, nextPosition - curPosition).data(),
            *(ptr + i)))) {
            return E_FAIL;
        }
        curPosition = nextPosition + 1;
    }
    return S_OK;
}

static HRESULT ParseDataToVectorHalf(PCWSTR str, uint16_t *ptr, size_t count) {
    std::wstring wstr(str);
    size_t curPosition = 0;
    // parse a string of dot product separated by commas
    for (size_t i = 0; i < count; ++i) {
        size_t nextPosition = wstr.find(L",", curPosition);
        float floatValue;
        if (FAILED(ParseDataToFloat(
            wstr.substr(curPosition, nextPosition - curPosition).data(), floatValue))) {
            return E_FAIL;
        }
        *(ptr + i) = ConvertFloat32ToFloat16(floatValue);
        curPosition = nextPosition + 1;
    }
    return S_OK;
}

static HRESULT ParseDataToVectorUint(PCWSTR str, unsigned int *ptr, size_t count) {
    std::wstring wstr(str);
    size_t curPosition = 0;
    // parse a string of dot product separated by commas
    for (size_t i = 0; i < count; ++i) {
        size_t nextPosition = wstr.find(L",", curPosition);
        if (FAILED(ParseDataToUint(
            wstr.substr(curPosition, nextPosition - curPosition).data(),
            *(ptr + i)))) {
            return E_FAIL;
        }
        curPosition = nextPosition + 1;
    }
    return S_OK;
}

HRESULT TableParameterHandler::ParseTableRow() {
  TableParameter *table = m_table;
  for (unsigned int i = 0; i < m_tableSize; ++i) {
    switch (table[i].m_type) {
    case TableParameter::INT8:
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(table[i].m_name,
        table[i].m_int32)) && table[i].m_required) {
        // TryGetValue does not suppport reading from int16
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      table[i].m_int8 = (int8_t)(table[i].m_int32);
      break;
    case TableParameter::INT16:
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(table[i].m_name,
        table[i].m_int32)) && table[i].m_required) {
        // TryGetValue does not suppport reading from int16
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      table[i].m_int16 = (short)(table[i].m_int32);
      break;
    case TableParameter::INT32:
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(table[i].m_name,
        table[i].m_int32)) && table[i].m_required) {
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      break;
    case TableParameter::UINT:
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(table[i].m_name,
        table[i].m_uint)) && table[i].m_required) {
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      break;
    case TableParameter::DOUBLE:
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, table[i].m_double)) && table[i].m_required) {
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      break;
    case TableParameter::STRING:
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(table[i].m_name,
        table[i].m_str)) && table[i].m_required) {
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      break;
    case TableParameter::BOOL:
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(table[i].m_name,
        table[i].m_str)) && table[i].m_bool) {
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      break;
    case TableParameter::INT8_TABLE: {
      WEX::TestExecution::TestDataArray<int> tempTable;
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, tempTable)) && table[i].m_required) {

        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      // TryGetValue does not suppport reading from int8
      table[i].m_int8Table.resize(tempTable.GetSize());
      for (size_t j = 0, end = tempTable.GetSize(); j != end; ++j) {
        table[i].m_int8Table[j] = (int8_t)tempTable[j];
      }
      break;
    }
    case TableParameter::INT16_TABLE: {
      WEX::TestExecution::TestDataArray<int> tempTable;
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, tempTable)) && table[i].m_required) {
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      // TryGetValue does not suppport reading from int8
      table[i].m_int16Table.resize(tempTable.GetSize());
      for (size_t j = 0, end = tempTable.GetSize(); j != end; ++j) {
        table[i].m_int16Table[j] = (int16_t)tempTable[j];
      }
      break;
    }case TableParameter::INT32_TABLE: {
      WEX::TestExecution::TestDataArray<int> tempTable;
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, tempTable)) && table[i].m_required) {
        // TryGetValue does not suppport reading from int8
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      table[i].m_int32Table.resize(tempTable.GetSize());
      for (size_t j = 0, end = tempTable.GetSize(); j != end; ++j) {
        table[i].m_int32Table[j] = tempTable[j];
      }
      break;
    }
    case TableParameter::UINT8_TABLE: {
      WEX::TestExecution::TestDataArray<int> tempTable;
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, tempTable)) && table[i].m_required) {

        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      // TryGetValue does not suppport reading from int8
      table[i].m_int8Table.resize(tempTable.GetSize());
      for (size_t j = 0, end = tempTable.GetSize(); j != end; ++j) {
        table[i].m_int8Table[j] = (uint8_t)tempTable[j];
      }
      break;
    }
    case TableParameter::UINT16_TABLE: {
      WEX::TestExecution::TestDataArray<int> tempTable;
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, tempTable)) && table[i].m_required) {
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      // TryGetValue does not suppport reading from int8
      table[i].m_uint16Table.resize(tempTable.GetSize());
      for (size_t j = 0, end = tempTable.GetSize(); j != end; ++j) {
        table[i].m_uint16Table[j] = (uint16_t)tempTable[j];
      }
      break;
    }
    case TableParameter::UINT32_TABLE: {
      WEX::TestExecution::TestDataArray<unsigned int> tempTable;
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, tempTable)) && table[i].m_required) {
        // TryGetValue does not suppport reading from int8
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      table[i].m_uint32Table.resize(tempTable.GetSize());
      for (size_t j = 0, end = tempTable.GetSize(); j != end; ++j) {
        table[i].m_uint32Table[j] = tempTable[j];
      }
      break;
    }
    case TableParameter::FLOAT_TABLE: {
      WEX::TestExecution::TestDataArray<WEX::Common::String> tempTable;
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, tempTable)) && table[i].m_required) {
        // TryGetValue does not suppport reading from int8
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      table[i].m_floatTable.resize(tempTable.GetSize());
      for (size_t j = 0, end = tempTable.GetSize(); j != end; ++j) {
        ParseDataToFloat(tempTable[j], table[i].m_floatTable[j]);
      }
      break;
    }
    case TableParameter::HALF_TABLE: {
      WEX::TestExecution::TestDataArray<WEX::Common::String> tempTable;
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, tempTable)) && table[i].m_required) {
        // TryGetValue does not suppport reading from int8
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      table[i].m_halfTable.resize(tempTable.GetSize());
      for (size_t j = 0, end = tempTable.GetSize(); j != end; ++j) {
        uint16_t value = 0;
        if (IsHexString(tempTable[j], &value)) {
          table[i].m_halfTable[j] = value;
        }
        else {
          float val;
          ParseDataToFloat(tempTable[j], val);
          if (isdenorm(val))
            table[i].m_halfTable[j] = signbit(val) ? Float16NegDenorm : Float16PosDenorm;
          else
            table[i].m_halfTable[j] = ConvertFloat32ToFloat16(val);
        }
     }
      break;
    }
    case TableParameter::DOUBLE_TABLE: {
      WEX::TestExecution::TestDataArray<double> tempTable;
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, tempTable)) && table[i].m_required) {
        // TryGetValue does not suppport reading from int8
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      table[i].m_doubleTable.resize(tempTable.GetSize());
      for (size_t j = 0, end = tempTable.GetSize(); j != end; ++j) {
        table[i].m_doubleTable[j] = tempTable[j];
      }
      break;
    }
    case TableParameter::BOOL_TABLE: {
      WEX::TestExecution::TestDataArray<bool> tempTable;
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, tempTable)) && table[i].m_required) {
        // TryGetValue does not suppport reading from int8
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      table[i].m_boolTable.resize(tempTable.GetSize());
      for (size_t j = 0, end = tempTable.GetSize(); j != end; ++j) {
        table[i].m_boolTable[j] = tempTable[j];
      }
      break;
    }
    case TableParameter::STRING_TABLE: {
      WEX::TestExecution::TestDataArray<WEX::Common::String> tempTable;
      if (FAILED(WEX::TestExecution::TestData::TryGetValue(
        table[i].m_name, tempTable)) && table[i].m_required) {
        // TryGetValue does not suppport reading from int8
        LogErrorFmt(L"Failed to get %s", table[i].m_name);
        return E_FAIL;
      }
      table[i].m_StringTable.resize(tempTable.GetSize());
      for (size_t j = 0, end = tempTable.GetSize(); j != end; ++j) {
        table[i].m_StringTable[j] = tempTable[j];
      }
      break;
    }
    default:
      DXASSERT_NOMSG("Invalid Parameter Type");
    }
    if (errno == ERANGE) {
      LogErrorFmt(L"got out of range value for table %s", table[i].m_name);
      return E_FAIL;
    }
  }
  return S_OK;
}

static void VerifyOutputWithExpectedValueInt(int output, int ref, int tolerance) {
    VERIFY_IS_TRUE(output - ref <= tolerance && ref - output <= tolerance);
}

static void VerifyOutputWithExpectedValueUInt(uint32_t output, uint32_t ref, uint32_t tolerance) {
    VERIFY_IS_TRUE(output - ref <= tolerance && ref - output <= tolerance);
}

static void VerifyOutputWithExpectedValueFloat(
    float output, float ref, LPCWSTR type, double tolerance,
    hlsl::DXIL::Float32DenormMode mode = hlsl::DXIL::Float32DenormMode::Any) {
  if (_wcsicmp(type, L"Relative") == 0) {
    VERIFY_IS_TRUE(CompareFloatRelativeEpsilon(output, ref, (int)tolerance, mode));
  } else if (_wcsicmp(type, L"Epsilon") == 0) {
    VERIFY_IS_TRUE(CompareFloatEpsilon(output, ref, (float)tolerance, mode));
  } else if (_wcsicmp(type, L"ULP") == 0) {
    VERIFY_IS_TRUE(CompareFloatULP(output, ref, (int)tolerance, mode));
  } else {
    LogErrorFmt(L"Failed to read comparison type %S", type);
  }
}

static bool CompareOutputWithExpectedValueFloat(
    float output, float ref, LPCWSTR type, double tolerance,
    hlsl::DXIL::Float32DenormMode mode = hlsl::DXIL::Float32DenormMode::Any) {
  if (_wcsicmp(type, L"Relative") == 0) {
    return CompareFloatRelativeEpsilon(output, ref, (int)tolerance, mode);
  } else if (_wcsicmp(type, L"Epsilon") == 0) {
    return CompareFloatEpsilon(output, ref, (float)tolerance, mode);
  } else if (_wcsicmp(type, L"ULP") == 0) {
    return CompareFloatULP(output, ref, (int)tolerance, mode);
  } else {
    LogErrorFmt(L"Failed to read comparison type %S", type);
    return false;
  }
}

static void VerifyOutputWithExpectedValueHalf(
  uint16_t output, uint16_t ref, LPCWSTR type, double tolerance) {
  if (_wcsicmp(type, L"Relative") == 0) {
    VERIFY_IS_TRUE(CompareHalfRelativeEpsilon(output, ref, (int)tolerance));
  }
  else if (_wcsicmp(type, L"Epsilon") == 0) {
    VERIFY_IS_TRUE(CompareHalfEpsilon(output, ref, (float)tolerance));
  }
  else if (_wcsicmp(type, L"ULP") == 0) {
    VERIFY_IS_TRUE(CompareHalfULP(output, ref, (float)tolerance));
  }
  else {
    LogErrorFmt(L"Failed to read comparison type %S", type);
  }
}

TEST_F(ExecutionTest, UnaryFloatOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice)) {
      return;
    }
    // Read data from the table
    int tableSize = sizeof(UnaryFPOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(UnaryFPOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);

    unsigned int WarpVersion = handler.GetTableParamByName(L"Warp.Version")->m_uint;
    if (GetTestParamUseWARP(true) && !IsValidWarpDllVersion(WarpVersion)) {
        return;
    }

    std::vector<float> *Validation_Input =
        &(handler.GetTableParamByName(L"Validation.Input1")->m_floatTable);
    std::vector<float> *Validation_Expected =
        &(handler.GetTableParamByName(L"Validation.Expected1")->m_floatTable);

    LPCWSTR Validation_Type = handler.GetTableParamByName(L"Validation.Type")->m_str;
    double Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_double;

    size_t count = Validation_Input->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "UnaryFPOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
          VERIFY_IS_TRUE(0 == _stricmp(Name, "SUnaryFPOp"));
          size_t size = sizeof(SUnaryFPOp) * count;
          Data.resize(size);
          SUnaryFPOp *pPrimitives = (SUnaryFPOp *)Data.data();
          for (size_t i = 0; i < count; ++i) {
            SUnaryFPOp *p = &pPrimitives[i];
            p->input = (*Validation_Input)[i % Validation_Input->size()];
          }
          // use shader from data table
          pShaderOp->Shaders.at(0).Target = Target.m_psz;
          pShaderOp->Shaders.at(0).Text = Text.m_psz;
        });

    MappedData data;
    test->Test->GetReadBackData("SUnaryFPOp", &data);

    SUnaryFPOp *pPrimitives = (SUnaryFPOp*)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    for (unsigned i = 0; i < count; ++i) {
        SUnaryFPOp *p = &pPrimitives[i];
        float val = (*Validation_Expected)[i % Validation_Expected->size()];
        LogCommentFmt(
            L"element #%u, input = %6.8f, output = %6.8f, expected = %6.8f", i,
            p->input, p->output, val);
        VerifyOutputWithExpectedValueFloat(p->output, val, Validation_Type, Validation_Tolerance);
    }
}

TEST_F(ExecutionTest, BinaryFloatOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice)) {
        return;
    }
    // Read data from the table
    int tableSize = sizeof(BinaryFPOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(BinaryFPOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);

    std::vector<float> *Validation_Input1 =
        &(handler.GetTableParamByName(L"Validation.Input1")->m_floatTable);
    std::vector<float> *Validation_Input2 =
        &(handler.GetTableParamByName(L"Validation.Input2")->m_floatTable);

    std::vector<float> *Validation_Expected1 =
        &(handler.GetTableParamByName(L"Validation.Expected1")->m_floatTable);

    std::vector<float> *Validation_Expected2 =
        &(handler.GetTableParamByName(L"Validation.Expected2")->m_floatTable);

    LPCWSTR Validation_Type = handler.GetTableParamByName(L"Validation.Type")->m_str;
    double Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_double;
    size_t count = Validation_Input1->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "BinaryFPOp", 
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "SBinaryFPOp"));
        size_t size = sizeof(SBinaryFPOp) * count;
        Data.resize(size);
        SBinaryFPOp *pPrimitives = (SBinaryFPOp *)Data.data();
        for (size_t i = 0; i < count; ++i) {
            SBinaryFPOp *p = &pPrimitives[i];
            p->input1 = (*Validation_Input1)[i % Validation_Input1->size()];
            p->input2 = (*Validation_Input2)[i % Validation_Input2->size()];
        }

        // use shader from data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("SBinaryFPOp", &data);

    SBinaryFPOp *pPrimitives = (SBinaryFPOp *)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    unsigned numExpected = Validation_Expected2->size() == 0 ? 1 : 2;
    if (numExpected == 2) {
      for (unsigned i = 0; i < count; ++i) {
        SBinaryFPOp *p = &pPrimitives[i];
        float val1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
        float val2 = (*Validation_Expected2)[i % Validation_Expected2->size()];
        LogCommentFmt(L"element #%u, input1 = %6.8f, input2 = %6.8f, output1 = "
            L"%6.8f, expected1 = %6.8f, output2 = %6.8f, expected2 = %6.8f",
            i, p->input1, p->input2, p->output1, val1, p->output2,
            val2);
        VerifyOutputWithExpectedValueFloat(p->output1, val1, Validation_Type,
          Validation_Tolerance);
        VerifyOutputWithExpectedValueFloat(p->output2, val2, Validation_Type,
          Validation_Tolerance);
      }
    }
    else if (numExpected == 1) {
      for (unsigned i = 0; i < count; ++i) {
        SBinaryFPOp *p = &pPrimitives[i];
        float val1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
        LogCommentFmt(L"element #%u, input1 = %6.8f, input2 = %6.8f, output1 = "
          L"%6.8f, expected1 = %6.8f",
          i, p->input1, p->input2, p->output1, val1);
        VerifyOutputWithExpectedValueFloat(p->output1, val1, Validation_Type,
          Validation_Tolerance);
      }
    }
    else {
      LogErrorFmt(L"Unexpected number of expected values for operation %i", numExpected);
    }
}

TEST_F(ExecutionTest, TertiaryFloatOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice)) {
        return;
    }
    // Read data from the table
    
    int tableSize = sizeof(TertiaryFPOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(TertiaryFPOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);

    std::vector<float> *Validation_Input1 =
        &(handler.GetTableParamByName(L"Validation.Input1")->m_floatTable);
    std::vector<float> *Validation_Input2 =
        &(handler.GetTableParamByName(L"Validation.Input2")->m_floatTable);
    std::vector<float> *Validation_Input3 =
        &(handler.GetTableParamByName(L"Validation.Input3")->m_floatTable);

    std::vector<float> *Validation_Expected =
        &(handler.GetTableParamByName(L"Validation.Expected1")->m_floatTable);

    LPCWSTR Validation_Type = handler.GetTableParamByName(L"Validation.Type")->m_str;
    double Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_double;
    size_t count = Validation_Input1->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "TertiaryFPOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "STertiaryFPOp"));
        size_t size = sizeof(STertiaryFPOp) * count;
        Data.resize(size);
        STertiaryFPOp *pPrimitives = (STertiaryFPOp *)Data.data();
        for (size_t i = 0; i < count; ++i) {
            STertiaryFPOp *p = &pPrimitives[i];
            p->input1 = (*Validation_Input1)[i % Validation_Input1->size()];
            p->input2 = (*Validation_Input2)[i % Validation_Input2->size()];
            p->input3 = (*Validation_Input3)[i % Validation_Input3->size()];
        }

        // use shader from data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("STertiaryFPOp", &data);

    STertiaryFPOp *pPrimitives = (STertiaryFPOp *)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;

    for (unsigned i = 0; i < count; ++i) {
      STertiaryFPOp *p = &pPrimitives[i];
      float val = (*Validation_Expected)[i % Validation_Expected->size()];
      LogCommentFmt(L"element #%u, input1 = %6.8f, input2 = %6.8f, input3 = %6.8f, output1 = "
                    L"%6.8f, expected = %6.8f",
                    i, p->input1, p->input2, p->input3, p->output, val);
      VerifyOutputWithExpectedValueFloat(p->output, val, Validation_Type,
                               Validation_Tolerance);
    }
}

TEST_F(ExecutionTest, UnaryHalfOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_2)) {
      return;
    }
    // Read data from the table
    int tableSize = sizeof(UnaryHalfOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(UnaryHalfOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
    CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

    unsigned int WarpVersion = handler.GetTableParamByName(L"Warp.Version")->m_uint;
    if (GetTestParamUseWARP(true) && !IsValidWarpDllVersion(WarpVersion)) {
        return;
    }

    std::vector<uint16_t> *Validation_Input =
        &(handler.GetTableParamByName(L"Validation.Input1")->m_halfTable);
    std::vector<uint16_t> *Validation_Expected =
        &(handler.GetTableParamByName(L"Validation.Expected1")->m_halfTable);

    LPCWSTR Validation_Type = handler.GetTableParamByName(L"Validation.Type")->m_str;
    double Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_double;

    size_t count = Validation_Input->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "UnaryFPOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
          VERIFY_IS_TRUE(0 == _stricmp(Name, "SUnaryFPOp"));
          size_t size = sizeof(SUnaryHalfOp) * count;
          Data.resize(size);
          SUnaryHalfOp *pPrimitives = (SUnaryHalfOp *)Data.data();
          for (size_t i = 0; i < count; ++i) {
            SUnaryHalfOp *p = &pPrimitives[i];
            p->input = (*Validation_Input)[i % Validation_Input->size()];
          }
          // use shader from data table
          pShaderOp->Shaders.at(0).Target = Target.m_psz;
          pShaderOp->Shaders.at(0).Text = Text.m_psz;
          pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
        });

    MappedData data;
    test->Test->GetReadBackData("SUnaryFPOp", &data);

    SUnaryHalfOp *pPrimitives = (SUnaryHalfOp*)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    for (unsigned i = 0; i < count; ++i) {
        SUnaryHalfOp *p = &pPrimitives[i];
        uint16_t expected = (*Validation_Expected)[i % Validation_Input->size()];
        LogCommentFmt(L"element #%u, input = %6.8f(0x%04x), output = "
                      L"%6.8f(0x%04x), expected = %6.8f(0x%04x)",
                      i, ConvertFloat16ToFloat32(p->input), p->input,
                      ConvertFloat16ToFloat32(p->output), p->output,
                      ConvertFloat16ToFloat32(expected), expected);
        VerifyOutputWithExpectedValueHalf(p->output, expected, Validation_Type, Validation_Tolerance);
    }
}

TEST_F(ExecutionTest, BinaryHalfOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_2)) {
        return;
    }
    // Read data from the table
    int tableSize = sizeof(BinaryHalfOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(BinaryHalfOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
    CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

    std::vector<uint16_t> *Validation_Input1 =
        &(handler.GetTableParamByName(L"Validation.Input1")->m_halfTable);
    std::vector<uint16_t> *Validation_Input2 =
        &(handler.GetTableParamByName(L"Validation.Input2")->m_halfTable);

    std::vector<uint16_t> *Validation_Expected1 =
        &(handler.GetTableParamByName(L"Validation.Expected1")->m_halfTable);

    std::vector<uint16_t> *Validation_Expected2 =
        &(handler.GetTableParamByName(L"Validation.Expected2")->m_halfTable);

    LPCWSTR Validation_Type = handler.GetTableParamByName(L"Validation.Type")->m_str;
    double Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_double;
    size_t count = Validation_Input1->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "BinaryFPOp", 
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "SBinaryFPOp"));
        size_t size = sizeof(SBinaryHalfOp) * count;
        Data.resize(size);
        SBinaryHalfOp *pPrimitives = (SBinaryHalfOp *)Data.data();
        for (size_t i = 0; i < count; ++i) {
            SBinaryHalfOp *p = &pPrimitives[i];
            p->input1 = (*Validation_Input1)[i % Validation_Input1->size()];
            p->input2 = (*Validation_Input2)[i % Validation_Input2->size()];
        }

        // use shader from data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
        pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("SBinaryFPOp", &data);

    SBinaryHalfOp *pPrimitives = (SBinaryHalfOp *)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    unsigned numExpected = Validation_Expected2->size() == 0 ? 1 : 2;
    if (numExpected == 2) {
      for (unsigned i = 0; i < count; ++i) {
        SBinaryHalfOp *p = &pPrimitives[i];
        uint16_t expected1 = (*Validation_Expected1)[i % Validation_Input1->size()];
        uint16_t expected2 = (*Validation_Expected2)[i % Validation_Input2->size()];
        LogCommentFmt(L"element #%u, input1 = %6.8f(0x%04x), input2 = %6.8f(0x%04x), output1 = "
          L"%6.8f(0x%04x), expected1 = %6.8f(0x%04x), output2 = %6.8f(0x%04x), expected2 = %6.8f(0x%04x)",
          i, ConvertFloat16ToFloat32(p->input1), p->input1,
          ConvertFloat16ToFloat32(p->input2), p->input2,
          ConvertFloat16ToFloat32(p->output1), p->output1,
          ConvertFloat16ToFloat32(p->output2), p->output2,
          ConvertFloat16ToFloat32(expected1), expected1,
          ConvertFloat16ToFloat32(expected2), expected2);
        VerifyOutputWithExpectedValueHalf(p->output1, expected1, Validation_Type, Validation_Tolerance);
        VerifyOutputWithExpectedValueHalf(p->output2, expected2, Validation_Type, Validation_Tolerance);
      }
    }
    else if (numExpected == 1) {
      for (unsigned i = 0; i < count; ++i) {
        uint16_t expected = (*Validation_Expected1)[i % Validation_Input1->size()];
        SBinaryHalfOp *p = &pPrimitives[i];
        LogCommentFmt(L"element #%u, input = %6.8f(0x%04x), output = "
          L"%6.8f(0x%04x), expected = %6.8f(0x%04x)",
          i, ConvertFloat16ToFloat32(p->input1), p->input1,
          ConvertFloat16ToFloat32(p->output1), p->output1,
          ConvertFloat16ToFloat32(expected), expected);
        VerifyOutputWithExpectedValueHalf(p->output1, expected, Validation_Type, Validation_Tolerance);
      }
    }
    else {
      LogErrorFmt(L"Unexpected number of expected values for operation %i", numExpected);
    }
}

TEST_F(ExecutionTest, TertiaryHalfOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_2)) {
        return;
    }
    // Read data from the table
    
    int tableSize = sizeof(TertiaryHalfOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(TertiaryHalfOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
    CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

    std::vector<uint16_t> *Validation_Input1 =
        &(handler.GetTableParamByName(L"Validation.Input1")->m_halfTable);
    std::vector<uint16_t> *Validation_Input2 =
        &(handler.GetTableParamByName(L"Validation.Input2")->m_halfTable);
    std::vector<uint16_t> *Validation_Input3 =
        &(handler.GetTableParamByName(L"Validation.Input3")->m_halfTable);

    std::vector<uint16_t> *Validation_Expected =
        &(handler.GetTableParamByName(L"Validation.Expected1")->m_halfTable);

    LPCWSTR Validation_Type = handler.GetTableParamByName(L"Validation.Type")->m_str;
    double Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_double;
    size_t count = Validation_Input1->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "TertiaryFPOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "STertiaryFPOp"));
        size_t size = sizeof(STertiaryHalfOp) * count;
        Data.resize(size);
        STertiaryHalfOp *pPrimitives = (STertiaryHalfOp *)Data.data();
        for (size_t i = 0; i < count; ++i) {
            STertiaryHalfOp *p = &pPrimitives[i];
            p->input1 = (*Validation_Input1)[i % Validation_Input1->size()];
            p->input2 = (*Validation_Input2)[i % Validation_Input2->size()];
            p->input3 = (*Validation_Input3)[i % Validation_Input3->size()];
        }

        // use shader from data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
        pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("STertiaryFPOp", &data);

    STertiaryHalfOp *pPrimitives = (STertiaryHalfOp *)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;

    for (unsigned i = 0; i < count; ++i) {
      STertiaryHalfOp *p = &pPrimitives[i];
      uint16_t expected = (*Validation_Expected)[i % Validation_Expected->size()];
      LogCommentFmt(L"element #%u,  input1 = %6.8f(0x%04x), input2 = %6.8f(0x%04x), input3 = %6.8f(0x%04x), output = "
        L"%6.8f(0x%04x), expected = %6.8f(0x%04x)",
        i, ConvertFloat16ToFloat32(p->input1), p->input1,
        ConvertFloat16ToFloat32(p->input2), p->input2,
        ConvertFloat16ToFloat32(p->input3), p->input3,
        ConvertFloat16ToFloat32(p->output), p->output,
        ConvertFloat16ToFloat32(expected), expected);
      VerifyOutputWithExpectedValueHalf(p->output, expected, Validation_Type, Validation_Tolerance);
    }
}

TEST_F(ExecutionTest, UnaryIntOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice)) {
        return;
    }
    // Read data from the table

    int tableSize = sizeof(UnaryIntOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(UnaryIntOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);

    std::vector<int> *Validation_Input =
        &handler.GetTableParamByName(L"Validation.Input1")->m_int32Table;
    std::vector<int> *Validation_Expected =
        &handler.GetTableParamByName(L"Validation.Expected1")->m_int32Table;
    int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
    size_t count = Validation_Input->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "UnaryIntOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
          VERIFY_IS_TRUE(0 == _stricmp(Name, "SUnaryIntOp"));
          size_t size = sizeof(SUnaryIntOp) * count;
          Data.resize(size);
          SUnaryIntOp *pPrimitives = (SUnaryIntOp *)Data.data();
          for (size_t i = 0; i < count; ++i) {
            SUnaryIntOp *p = &pPrimitives[i];
            int val = (*Validation_Input)[i % Validation_Input->size()];
            p->input = val;
          }
          // use shader data table
          pShaderOp->Shaders.at(0).Target = Target.m_psz;
          pShaderOp->Shaders.at(0).Text = Text.m_psz;
        });

    MappedData data;
    test->Test->GetReadBackData("SUnaryIntOp", &data);

    SUnaryIntOp *pPrimitives = (SUnaryIntOp *)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    for (unsigned i = 0; i < count; ++i) {
      SUnaryIntOp *p = &pPrimitives[i];
      int val = (*Validation_Expected)[i % Validation_Expected->size()];
      LogCommentFmt(L"element #%u, input = %11i(0x%08x), output = %11i(0x%08x), "
                    L"expected = %11i(0x%08x)",
                    i, p->input, p->input, p->output, p->output, val, val);
      VerifyOutputWithExpectedValueInt(p->output, val, Validation_Tolerance);
    }
}

TEST_F(ExecutionTest, UnaryUintOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice)) {
        return;
    }
    // Read data from the table

    int tableSize = sizeof(UnaryUintOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(UnaryUintOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);

    std::vector<unsigned int> *Validation_Input =
        &handler.GetTableParamByName(L"Validation.Input1")->m_uint32Table;
    std::vector<unsigned int> *Validation_Expected =
        &handler.GetTableParamByName(L"Validation.Expected1")->m_uint32Table;
    int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
    size_t count = Validation_Input->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "UnaryUintOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "SUnaryUintOp"));
        size_t size = sizeof(SUnaryUintOp) * count;
        Data.resize(size);
        SUnaryUintOp *pPrimitives = (SUnaryUintOp *)Data.data();
        for (size_t i = 0; i < count; ++i) {
            SUnaryUintOp *p = &pPrimitives[i];
            unsigned int val = (*Validation_Input)[i % Validation_Input->size()];
            p->input = val;
        }
        // use shader data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("SUnaryUintOp", &data);

    SUnaryUintOp *pPrimitives = (SUnaryUintOp *)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    for (unsigned i = 0; i < count; ++i) {
        SUnaryUintOp *p = &pPrimitives[i];
        unsigned int val = (*Validation_Expected)[i % Validation_Expected->size()];
        LogCommentFmt(L"element #%u, input = %11u(0x%08x), output = %11u(0x%08x), "
            L"expected = %11u(0x%08x)",
            i, p->input, p->input, p->output, p->output, val, val);
        VerifyOutputWithExpectedValueInt(p->output, val, Validation_Tolerance);
    }
}

TEST_F(ExecutionTest, BinaryIntOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice)) {
      return;
    }
    // Read data from the table
    size_t tableSize = sizeof(BinaryIntOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(BinaryIntOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);


    std::vector<int> *Validation_Input1 =
        &handler.GetTableParamByName(L"Validation.Input1")->m_int32Table;
    std::vector<int> *Validation_Input2 =
        &handler.GetTableParamByName(L"Validation.Input2")->m_int32Table;
    std::vector<int> *Validation_Expected1 =
        &handler.GetTableParamByName(L"Validation.Expected1")->m_int32Table;
    std::vector<int> *Validation_Expected2 =
        &handler.GetTableParamByName(L"Validation.Expected2")->m_int32Table;
    int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
    size_t count = Validation_Input1->size();

    size_t numExpected = Validation_Expected2->size() == 0 ? 1 : 2;

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "BinaryIntOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
          VERIFY_IS_TRUE(0 == _stricmp(Name, "SBinaryIntOp"));
          size_t size = sizeof(SBinaryIntOp) * count;
          Data.resize(size);
          SBinaryIntOp *pPrimitives = (SBinaryIntOp *)Data.data();
          for (size_t i = 0; i < count; ++i) {
            SBinaryIntOp *p = &pPrimitives[i];
            int val1 = (*Validation_Input1)[i % Validation_Input1->size()];
            int val2 = (*Validation_Input2)[i % Validation_Input2->size()];
            p->input1 = val1;
            p->input2 = val2;
          }

          // use shader from data table
          pShaderOp->Shaders.at(0).Target = Target.m_psz;
          pShaderOp->Shaders.at(0).Text = Text.m_psz;
        });

    MappedData data;
    test->Test->GetReadBackData("SBinaryIntOp", &data);

    SBinaryIntOp *pPrimitives = (SBinaryIntOp *)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;

    if (numExpected == 2) {
        for (unsigned i = 0; i < count; ++i) {
            SBinaryIntOp *p = &pPrimitives[i];
            int val1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
            int val2 = (*Validation_Expected2)[i % Validation_Expected2->size()];
            LogCommentFmt(L"element #%u, input1 = %11i(0x%08x), input2 = "
                L"%11i(0x%08x), output1 = "
                L"%11i(0x%08x), expected1 = %11i(0x%08x), output2 = "
                L"%11i(0x%08x), expected2 = %11i(0x%08x)",
                i, p->input1, p->input1, p->input2, p->input2, p->output1,
                p->output1, val1, val1, p->output2, p->output2, val2,
                val2);
            VerifyOutputWithExpectedValueInt(p->output1, val1, Validation_Tolerance);
            VerifyOutputWithExpectedValueInt(p->output2, val2, Validation_Tolerance);
        }
    }
    else if (numExpected == 1) {
        for (unsigned i = 0; i < count; ++i) {
            SBinaryIntOp *p = &pPrimitives[i];
            int val1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
            LogCommentFmt(L"element #%u, input1 = %11i(0x%08x), input2 = "
                          L"%11i(0x%08x), output = "
                          L"%11i(0x%08x), expected = %11i(0x%08x)", i,
                          p->input1, p->input1, p->input2, p->input2,
                          p->output1, p->output1, val1, val1);
            VerifyOutputWithExpectedValueInt(p->output1, val1, Validation_Tolerance);
        }
    }
    else {
        LogErrorFmt(L"Unexpected number of expected values for operation %i", numExpected);
    }
}

TEST_F(ExecutionTest, TertiaryIntOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice)) {
        return;
    }
    // Read data from the table
    size_t tableSize = sizeof(TertiaryIntOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(TertiaryIntOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);

    std::vector<int> *Validation_Input1 =
        &handler.GetTableParamByName(L"Validation.Input1")->m_int32Table;
    std::vector<int> *Validation_Input2 =
        &handler.GetTableParamByName(L"Validation.Input2")->m_int32Table;
    std::vector<int> *Validation_Input3 =
        &handler.GetTableParamByName(L"Validation.Input3")->m_int32Table;
    std::vector<int> *Validation_Expected =
        &handler.GetTableParamByName(L"Validation.Expected1")->m_int32Table;
    int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
    size_t count = Validation_Input1->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "TertiaryIntOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "STertiaryIntOp"));
        size_t size = sizeof(STertiaryIntOp) * count;
        Data.resize(size);
        STertiaryIntOp *pPrimitives = (STertiaryIntOp *)Data.data();
        for (size_t i = 0; i < count; ++i) {
            STertiaryIntOp *p = &pPrimitives[i];
            int val1 = (*Validation_Input1)[i % Validation_Input1->size()];
            int val2 = (*Validation_Input2)[i % Validation_Input2->size()];
            int val3 = (*Validation_Input3)[i % Validation_Input3->size()];
            p->input1 = val1;
            p->input2 = val2;
            p->input3 = val3;
        }

        // use shader from data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("STertiaryIntOp", &data);

    STertiaryIntOp *pPrimitives = (STertiaryIntOp *)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    for (unsigned i = 0; i < count; ++i) {
        STertiaryIntOp *p = &pPrimitives[i];
        int val1 = (*Validation_Expected)[i % Validation_Expected->size()];
        LogCommentFmt(L"element #%u, input1 = %11i(0x%08x), input2 = "
            L"%11i(0x%08x), input3= %11i(0x%08x), output = "
            L"%11i(0x%08x), expected = %11i(0x%08x)",
            i, p->input1, p->input1, p->input2, p->input2,
            p->input3, p->input3, p->output, p->output, val1,
            val1);
        VerifyOutputWithExpectedValueInt(p->output, val1, Validation_Tolerance);
    }
}

TEST_F(ExecutionTest, BinaryUintOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice)) {
        return;
    }
    // Read data from the table
    size_t tableSize = sizeof(BinaryUintOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(BinaryUintOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);


    std::vector<unsigned int> *Validation_Input1 =
        &handler.GetTableParamByName(L"Validation.Input1")->m_uint32Table;
    std::vector<unsigned int> *Validation_Input2 =
        &handler.GetTableParamByName(L"Validation.Input2")->m_uint32Table;
    std::vector<unsigned int> *Validation_Expected1 =
        &handler.GetTableParamByName(L"Validation.Expected1")->m_uint32Table;
    std::vector<unsigned int> *Validation_Expected2 =
        &handler.GetTableParamByName(L"Validation.Expected2")->m_uint32Table;
    int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
    size_t count = Validation_Input1->size();
    int numExpected = Validation_Expected2->size() == 0 ? 1 : 2;
    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "BinaryUintOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "SBinaryUintOp"));
        size_t size = sizeof(SBinaryUintOp) * count;
        Data.resize(size);
        SBinaryUintOp *pPrimitives = (SBinaryUintOp *)Data.data();
        for (size_t i = 0; i < count; ++i) {
            SBinaryUintOp *p = &pPrimitives[i];
            unsigned int val1 = (*Validation_Input1)[i % Validation_Input1->size()];
            unsigned int val2 = (*Validation_Input2)[i % Validation_Input2->size()];
            p->input1 = val1;
            p->input2 = val2;
        }

        // use shader from data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("SBinaryUintOp", &data);

    SBinaryUintOp *pPrimitives = (SBinaryUintOp *)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    if (numExpected == 2) {
        for (unsigned i = 0; i < count; ++i) {
            SBinaryUintOp *p = &pPrimitives[i];
            unsigned int val1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
            unsigned int val2 = (*Validation_Expected2)[i % Validation_Expected2->size()];
            LogCommentFmt(L"element #%u, input1 = %11u(0x%08x), input2 = "
                L"%11u(0x%08x), output1 = "
                L"%11u(0x%08x), expected1 = %11u(0x%08x), output2 = "
                L"%11u(0x%08x), expected2 = %11u(0x%08x)",
                i, p->input1, p->input1, p->input2, p->input2, p->output1,
                p->output1, val1, val1, p->output2, p->output2, val2,
                val2);
            VerifyOutputWithExpectedValueInt(p->output1, val1, Validation_Tolerance);
            VerifyOutputWithExpectedValueInt(p->output2, val2, Validation_Tolerance);
        }
    }
    else if (numExpected == 1) {
        for (unsigned i = 0; i < count; ++i) {
            SBinaryUintOp *p = &pPrimitives[i];
            unsigned int val1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
            LogCommentFmt(L"element #%u, input1 = %11u(0x%08x), input2 = "
                L"%11u(0x%08x), output = "
                L"%11u(0x%08x), expected = %11u(0x%08x)", i,
                p->input1, p->input1, p->input2, p->input2,
                p->output1, p->output1, val1, val1);
            VerifyOutputWithExpectedValueInt(p->output1, val1, Validation_Tolerance);
        }
    }
    else {
        LogErrorFmt(L"Unexpected number of expected values for operation %i", numExpected);
    }
}

TEST_F(ExecutionTest, TertiaryUintOpTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice)) {
        return;
    }
    // Read data from the table
    size_t tableSize = sizeof(TertiaryUintOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(TertiaryUintOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);

    std::vector<unsigned int> *Validation_Input1 =
        &handler.GetTableParamByName(L"Validation.Input1")->m_uint32Table;
    std::vector<unsigned int> *Validation_Input2 =
        &handler.GetTableParamByName(L"Validation.Input2")->m_uint32Table;
    std::vector<unsigned int> *Validation_Input3 =
        &handler.GetTableParamByName(L"Validation.Input3")->m_uint32Table;
    std::vector<unsigned int> *Validation_Expected =
        &handler.GetTableParamByName(L"Validation.Expected1")->m_uint32Table;
    int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
    size_t count = Validation_Input1->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "TertiaryUintOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "STertiaryUintOp"));
        size_t size = sizeof(STertiaryUintOp) * count;
        Data.resize(size);
        STertiaryUintOp *pPrimitives = (STertiaryUintOp *)Data.data();
        for (size_t i = 0; i < count; ++i) {
            STertiaryUintOp *p = &pPrimitives[i];
            unsigned int val1 = (*Validation_Input1)[i % Validation_Input1->size()];
            unsigned int val2 = (*Validation_Input2)[i % Validation_Input2->size()];
            unsigned int val3 = (*Validation_Input3)[i % Validation_Input3->size()];
            p->input1 = val1;
            p->input2 = val2;
            p->input3 = val3;
        }

        // use shader from data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("STertiaryUintOp", &data);

    STertiaryUintOp *pPrimitives = (STertiaryUintOp *)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    for (unsigned i = 0; i < count; ++i) {
        STertiaryUintOp *p = &pPrimitives[i];
        unsigned int val1 = (*Validation_Expected)[i % Validation_Expected->size()];
        LogCommentFmt(L"element #%u, input1 = %11u(0x%08x), input2 = "
            L"%11u(0x%08x), input3 = %11u(0x%08x), output = "
            L"%11u(0x%08x), expected = %11u(0x%08x)", i,
            p->input1, p->input1, p->input2, p->input2, p->input3, p->input3,
            p->output, p->output, val1, val1);
        VerifyOutputWithExpectedValueInt(p->output, val1, Validation_Tolerance);
    }
}

// 16 bit integer type tests
TEST_F(ExecutionTest, UnaryInt16OpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
    WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_2)) {
    return;
  }
  // Read data from the table

  int tableSize = sizeof(UnaryInt16OpParameters) / sizeof(TableParameter);
  TableParameterHandler handler(UnaryInt16OpParameters, tableSize);

  CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
  CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
  CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

  std::vector<short> *Validation_Input =
    &handler.GetTableParamByName(L"Validation.Input1")->m_int16Table;
  std::vector<short> *Validation_Expected =
    &handler.GetTableParamByName(L"Validation.Expected1")->m_int16Table;
  int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
  size_t count = Validation_Input->size();

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
    pDevice, m_support, pStream, "UnaryIntOp",
    // this callbacked is called when the test
    // is creating the resource to run the test
    [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
    VERIFY_IS_TRUE(0 == _stricmp(Name, "SUnaryIntOp"));
    size_t size = sizeof(SUnaryInt16Op) * count;
    Data.resize(size);
    SUnaryInt16Op *pPrimitives = (SUnaryInt16Op *)Data.data();
    for (size_t i = 0; i < count; ++i) {
      SUnaryInt16Op *p = &pPrimitives[i];
      p->input = (*Validation_Input)[i % Validation_Input->size()];
    }
    // use shader data table
    pShaderOp->Shaders.at(0).Target = Target.m_psz;
    pShaderOp->Shaders.at(0).Text = Text.m_psz;
    pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
  });

  MappedData data;
  test->Test->GetReadBackData("SUnaryIntOp", &data);

  SUnaryInt16Op *pPrimitives = (SUnaryInt16Op *)data.data();
  WEX::TestExecution::DisableVerifyExceptions dve;
  for (unsigned i = 0; i < count; ++i) {
    SUnaryInt16Op *p = &pPrimitives[i];
    short val = (*Validation_Expected)[i % Validation_Expected->size()];
    LogCommentFmt(L"element #%u, input = %5hi(0x%08x), output = %5hi(0x%08x), "
      L"expected = %5hi(0x%08x)",
      i, p->input, p->input, p->output, p->output, val, val);
    VerifyOutputWithExpectedValueInt(p->output, val, Validation_Tolerance);
  }
}

TEST_F(ExecutionTest, UnaryUint16OpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
    WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_2)) {
    return;
  }
  // Read data from the table

  int tableSize = sizeof(UnaryUint16OpParameters) / sizeof(TableParameter);
  TableParameterHandler handler(UnaryUint16OpParameters, tableSize);

  CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
  CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
  CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

  std::vector<unsigned short> *Validation_Input =
    &handler.GetTableParamByName(L"Validation.Input1")->m_uint16Table;
  std::vector<unsigned short> *Validation_Expected =
    &handler.GetTableParamByName(L"Validation.Expected1")->m_uint16Table;
  int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
  size_t count = Validation_Input->size();

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
    pDevice, m_support, pStream, "UnaryUintOp",
    // this callbacked is called when the test
    // is creating the resource to run the test
    [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
    VERIFY_IS_TRUE(0 == _stricmp(Name, "SUnaryUintOp"));
    size_t size = sizeof(SUnaryUint16Op) * count;
    Data.resize(size);
    SUnaryUint16Op *pPrimitives = (SUnaryUint16Op *)Data.data();
    for (size_t i = 0; i < count; ++i) {
      SUnaryUint16Op *p = &pPrimitives[i];
      p->input = (*Validation_Input)[i % Validation_Input->size()];
    }
    // use shader data table
    pShaderOp->Shaders.at(0).Target = Target.m_psz;
    pShaderOp->Shaders.at(0).Text = Text.m_psz;
    pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
  });

  MappedData data;
  test->Test->GetReadBackData("SUnaryUintOp", &data);

  SUnaryUint16Op *pPrimitives = (SUnaryUint16Op *)data.data();
  WEX::TestExecution::DisableVerifyExceptions dve;
  for (unsigned i = 0; i < count; ++i) {
    SUnaryUint16Op *p = &pPrimitives[i];
    unsigned short val = (*Validation_Expected)[i % Validation_Expected->size()];
    LogCommentFmt(L"element #%u, input = %5hu(0x%08x), output = %5hu(0x%08x), "
      L"expected = %5hu(0x%08x)",
      i, p->input, p->input, p->output, p->output, val, val);
    VerifyOutputWithExpectedValueInt(p->output, val, Validation_Tolerance);
  }
}

TEST_F(ExecutionTest, BinaryInt16OpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
    WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_2)) {
    return;
  }
  // Read data from the table
  size_t tableSize = sizeof(BinaryInt16OpParameters) / sizeof(TableParameter);
  TableParameterHandler handler(BinaryInt16OpParameters, tableSize);

  CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
  CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
  CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

  std::vector<short> *Validation_Input1 =
    &handler.GetTableParamByName(L"Validation.Input1")->m_int16Table;
  std::vector<short> *Validation_Input2 =
    &handler.GetTableParamByName(L"Validation.Input2")->m_int16Table;
  std::vector<short> *Validation_Expected1 =
    &handler.GetTableParamByName(L"Validation.Expected1")->m_int16Table;
  std::vector<short> *Validation_Expected2 =
    &handler.GetTableParamByName(L"Validation.Expected2")->m_int16Table;
  int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
  size_t count = Validation_Input1->size();

  size_t numExpected = Validation_Expected2->size() == 0 ? 1 : 2;

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
    pDevice, m_support, pStream, "BinaryIntOp",
    // this callbacked is called when the test
    // is creating the resource to run the test
    [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
    VERIFY_IS_TRUE(0 == _stricmp(Name, "SBinaryIntOp"));
    size_t size = sizeof(SBinaryInt16Op) * count;
    Data.resize(size);
    SBinaryInt16Op *pPrimitives = (SBinaryInt16Op *)Data.data();
    for (size_t i = 0; i < count; ++i) {
      SBinaryInt16Op *p = &pPrimitives[i];
      p->input1 = (*Validation_Input1)[i % Validation_Input1->size()];
      p->input2 = (*Validation_Input2)[i % Validation_Input2->size()];
    }

    // use shader from data table
    pShaderOp->Shaders.at(0).Target = Target.m_psz;
    pShaderOp->Shaders.at(0).Text = Text.m_psz;
    pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
  });

  MappedData data;
  test->Test->GetReadBackData("SBinaryIntOp", &data);

  SBinaryInt16Op *pPrimitives = (SBinaryInt16Op *)data.data();
  WEX::TestExecution::DisableVerifyExceptions dve;

  if (numExpected == 2) {
    for (unsigned i = 0; i < count; ++i) {
      SBinaryInt16Op *p = &pPrimitives[i];
      short val1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
      short val2 = (*Validation_Expected2)[i % Validation_Expected2->size()];
      LogCommentFmt(L"element #%u, input1 = %5hi(0x%08x), input2 = "
        L"%5hi(0x%08x), output1 = "
        L"%5hi(0x%08x), expected1 = %5hi(0x%08x), output2 = "
        L"%5hi(0x%08x), expected2 = %5hi(0x%08x)",
        i, p->input1, p->input1, p->input2, p->input2, p->output1,
        p->output1, val1, val1, p->output2, p->output2, val2,
        val2);
      VerifyOutputWithExpectedValueInt(p->output1, val1, Validation_Tolerance);
      VerifyOutputWithExpectedValueInt(p->output2, val2, Validation_Tolerance);
    }
  }
  else if (numExpected == 1) {
    for (unsigned i = 0; i < count; ++i) {
      SBinaryInt16Op *p = &pPrimitives[i];
      short val1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
      LogCommentFmt(L"element #%u, input1 = %5hi(0x%08x), input2 = "
        L"%5hi(0x%08x), output = "
        L"%5hi(0x%08x), expected = %5hi(0x%08x)", i,
        p->input1, p->input1, p->input2, p->input2,
        p->output1, p->output1, val1, val1);
      VerifyOutputWithExpectedValueInt(p->output1, val1, Validation_Tolerance);
    }
  }
  else {
    LogErrorFmt(L"Unexpected number of expected values for operation %i", numExpected);
  }
}

TEST_F(ExecutionTest, TertiaryInt16OpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
    WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_2)) {
    return;
  }
  // Read data from the table
  size_t tableSize = sizeof(TertiaryInt16OpParameters) / sizeof(TableParameter);
  TableParameterHandler handler(TertiaryInt16OpParameters, tableSize);

  CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
  CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
  CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

  std::vector<short> *Validation_Input1 =
    &handler.GetTableParamByName(L"Validation.Input1")->m_int16Table;
  std::vector<short> *Validation_Input2 =
    &handler.GetTableParamByName(L"Validation.Input2")->m_int16Table;
  std::vector<short> *Validation_Input3 =
    &handler.GetTableParamByName(L"Validation.Input3")->m_int16Table;
  std::vector<short> *Validation_Expected =
    &handler.GetTableParamByName(L"Validation.Expected1")->m_int16Table;
  int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
  size_t count = Validation_Input1->size();

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
    pDevice, m_support, pStream, "TertiaryIntOp",
    // this callbacked is called when the test
    // is creating the resource to run the test
    [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
    VERIFY_IS_TRUE(0 == _stricmp(Name, "STertiaryIntOp"));
    size_t size = sizeof(STertiaryInt16Op) * count;
    Data.resize(size);
    STertiaryInt16Op *pPrimitives = (STertiaryInt16Op *)Data.data();
    for (size_t i = 0; i < count; ++i) {
      STertiaryInt16Op *p = &pPrimitives[i];
      p->input1 = (*Validation_Input1)[i % Validation_Input1->size()];
      p->input2 = (*Validation_Input2)[i % Validation_Input2->size()];
      p->input3 = (*Validation_Input3)[i % Validation_Input3->size()];
    }

    // use shader from data table
    pShaderOp->Shaders.at(0).Target = Target.m_psz;
    pShaderOp->Shaders.at(0).Text = Text.m_psz;
    pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
  });

  MappedData data;
  test->Test->GetReadBackData("STertiaryIntOp", &data);

  STertiaryInt16Op *pPrimitives = (STertiaryInt16Op *)data.data();
  WEX::TestExecution::DisableVerifyExceptions dve;
  for (unsigned i = 0; i < count; ++i) {
    STertiaryInt16Op *p = &pPrimitives[i];
    short val1 = (*Validation_Expected)[i % Validation_Expected->size()];
    LogCommentFmt(L"element #%u, input1 = %11i(0x%08x), input2 = "
      L"%11i(0x%08x), input3= %11i(0x%08x), output = "
      L"%11i(0x%08x), expected = %11i(0x%08x)",
      i, p->input1, p->input1, p->input2, p->input2,
      p->input3, p->input3, p->output, p->output, val1,
      val1);
    VerifyOutputWithExpectedValueInt(p->output, val1, Validation_Tolerance);
  }
}

TEST_F(ExecutionTest, BinaryUint16OpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
    WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_2)) {
    return;
  }
  // Read data from the table
  size_t tableSize = sizeof(BinaryUint16OpParameters) / sizeof(TableParameter);
  TableParameterHandler handler(BinaryUint16OpParameters, tableSize);

  CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
  CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
  CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

  std::vector<unsigned short> *Validation_Input1 =
    &handler.GetTableParamByName(L"Validation.Input1")->m_uint16Table;
  std::vector<unsigned short> *Validation_Input2 =
    &handler.GetTableParamByName(L"Validation.Input2")->m_uint16Table;
  std::vector<unsigned short> *Validation_Expected1 =
    &handler.GetTableParamByName(L"Validation.Expected1")->m_uint16Table;
  std::vector<unsigned short> *Validation_Expected2 =
    &handler.GetTableParamByName(L"Validation.Expected2")->m_uint16Table;
  int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
  size_t count = Validation_Input1->size();
  int numExpected = Validation_Expected2->size() == 0 ? 1 : 2;
  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
    pDevice, m_support, pStream, "BinaryUintOp",
    // this callbacked is called when the test
    // is creating the resource to run the test
    [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
    VERIFY_IS_TRUE(0 == _stricmp(Name, "SBinaryUintOp"));
    size_t size = sizeof(SBinaryUint16Op) * count;
    Data.resize(size);
    SBinaryUint16Op *pPrimitives = (SBinaryUint16Op *)Data.data();
    for (size_t i = 0; i < count; ++i) {
      SBinaryUint16Op *p = &pPrimitives[i];
      p->input1 = (*Validation_Input1)[i % Validation_Input1->size()];
      p->input2 = (*Validation_Input2)[i % Validation_Input2->size()];
    }

    // use shader from data table
    pShaderOp->Shaders.at(0).Target = Target.m_psz;
    pShaderOp->Shaders.at(0).Text = Text.m_psz;
    pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
  });

  MappedData data;
  test->Test->GetReadBackData("SBinaryUintOp", &data);

  SBinaryUint16Op *pPrimitives = (SBinaryUint16Op *)data.data();
  WEX::TestExecution::DisableVerifyExceptions dve;
  if (numExpected == 2) {
    for (unsigned i = 0; i < count; ++i) {
      SBinaryUint16Op *p = &pPrimitives[i];
      unsigned short val1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
      unsigned short val2 = (*Validation_Expected2)[i % Validation_Expected2->size()];
      LogCommentFmt(L"element #%u, input1 = %5hu(0x%08x), input2 = "
        L"%5hu(0x%08x), output1 = "
        L"%5hu(0x%08x), expected1 = %5hu(0x%08x), output2 = "
        L"%5hu(0x%08x), expected2 = %5hu(0x%08x)",
        i, p->input1, p->input1, p->input2, p->input2, p->output1,
        p->output1, val1, val1, p->output2, p->output2, val2,
        val2);
      VerifyOutputWithExpectedValueInt(p->output1, val1, Validation_Tolerance);
      VerifyOutputWithExpectedValueInt(p->output2, val2, Validation_Tolerance);
    }
  }
  else if (numExpected == 1) {
    for (unsigned i = 0; i < count; ++i) {
      SBinaryUint16Op *p = &pPrimitives[i];
      unsigned short val1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
      LogCommentFmt(L"element #%u, input1 = %5hu(0x%08x), input2 = "
        L"%5hu(0x%08x), output = "
        L"%5hu(0x%08x), expected = %5hu(0x%08x)", i,
        p->input1, p->input1, p->input2, p->input2,
        p->output1, p->output1, val1, val1);
      VerifyOutputWithExpectedValueInt(p->output1, val1, Validation_Tolerance);
    }
  }
  else {
    LogErrorFmt(L"Unexpected number of expected values for operation %i", numExpected);
  }
}

TEST_F(ExecutionTest, TertiaryUint16OpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
    WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_2)) {
    return;
  }
  // Read data from the table
  size_t tableSize = sizeof(TertiaryUint16OpParameters) / sizeof(TableParameter);
  TableParameterHandler handler(TertiaryUint16OpParameters, tableSize);

  CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
  CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
  CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

  std::vector<unsigned short> *Validation_Input1 =
    &handler.GetTableParamByName(L"Validation.Input1")->m_uint16Table;
  std::vector<unsigned short> *Validation_Input2 =
    &handler.GetTableParamByName(L"Validation.Input2")->m_uint16Table;
  std::vector<unsigned short> *Validation_Input3 =
    &handler.GetTableParamByName(L"Validation.Input3")->m_uint16Table;
  std::vector<unsigned short> *Validation_Expected =
    &handler.GetTableParamByName(L"Validation.Expected1")->m_uint16Table;
  int Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_int32;
  size_t count = Validation_Input1->size();

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
    pDevice, m_support, pStream, "TertiaryUintOp",
    // this callbacked is called when the test
    // is creating the resource to run the test
    [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
    VERIFY_IS_TRUE(0 == _stricmp(Name, "STertiaryUintOp"));
    size_t size = sizeof(STertiaryUint16Op) * count;
    Data.resize(size);
    STertiaryUint16Op *pPrimitives = (STertiaryUint16Op *)Data.data();
    for (size_t i = 0; i < count; ++i) {
      STertiaryUint16Op *p = &pPrimitives[i];
      p->input1 = (*Validation_Input1)[i % Validation_Input1->size()];
      p->input2 = (*Validation_Input2)[i % Validation_Input2->size()];
      p->input3 = (*Validation_Input3)[i % Validation_Input3->size()];
    }

    // use shader from data table
    pShaderOp->Shaders.at(0).Target = Target.m_psz;
    pShaderOp->Shaders.at(0).Text = Text.m_psz;
    pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
  });

  MappedData data;
  test->Test->GetReadBackData("STertiaryUintOp", &data);

  STertiaryUint16Op *pPrimitives = (STertiaryUint16Op *)data.data();
  WEX::TestExecution::DisableVerifyExceptions dve;
  for (unsigned i = 0; i < count; ++i) {
    STertiaryUint16Op *p = &pPrimitives[i];
    unsigned short val1 = (*Validation_Expected)[i % Validation_Expected->size()];
    LogCommentFmt(L"element #%u, input1 = %5hu(0x%08x), input2 = "
      L"%5hu(0x%08x), input3 = %5hu(0x%08x), output = "
      L"%5hu(0x%08x), expected = %5hu(0x%08x)", i,
      p->input1, p->input1, p->input2, p->input2, p->input3, p->input3,
      p->output, p->output, val1, val1);
    VerifyOutputWithExpectedValueInt(p->output, val1, Validation_Tolerance);
  }
}

TEST_F(ExecutionTest, DotTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice)) {
        return;
    }

    int tableSize = sizeof(DotOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(DotOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);

    std::vector<WEX::Common::String> *Validation_Input1 =
        &handler.GetTableParamByName(L"Validation.Input1")->m_StringTable;
    std::vector<WEX::Common::String> *Validation_Input2 =
        &handler.GetTableParamByName(L"Validation.Input2")->m_StringTable;
    std::vector<WEX::Common::String> *Validation_dot2 =
        &handler.GetTableParamByName(L"Validation.Expected1")->m_StringTable;
    std::vector<WEX::Common::String> *Validation_dot3 =
        &handler.GetTableParamByName(L"Validation.Expected2")->m_StringTable;
    std::vector<WEX::Common::String> *Validation_dot4 =
        &handler.GetTableParamByName(L"Validation.Expected3")->m_StringTable;

    PCWSTR Validation_type = handler.GetTableParamByName(L"Validation.Type")->m_str;
    double tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_double;
    size_t count = Validation_Input1->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "DotOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "SDotOp"));
        size_t size = sizeof(SDotOp) * count;
        Data.resize(size);
        SDotOp *pPrimitives = (SDotOp*)Data.data();
        for (size_t i = 0; i < count; ++i) {
            SDotOp *p = &pPrimitives[i];
            XMFLOAT4 val1,val2;
            VERIFY_SUCCEEDED(ParseDataToVectorFloat((*Validation_Input1)[i],
                                                    (float *)&val1, 4));
            VERIFY_SUCCEEDED(ParseDataToVectorFloat((*Validation_Input2)[i],
                                                    (float *)&val2, 4));
            p->input1 = val1;
            p->input2 = val2;
        }
        // use shader from data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("SDotOp", &data);

    SDotOp *pPrimitives = (SDotOp*)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    for (size_t i = 0; i < count; ++i) {
        SDotOp *p = &pPrimitives[i];
        float dot2, dot3, dot4;
        VERIFY_SUCCEEDED(ParseDataToFloat((*Validation_dot2)[i], dot2));
        VERIFY_SUCCEEDED(ParseDataToFloat((*Validation_dot3)[i], dot3));
        VERIFY_SUCCEEDED(ParseDataToFloat((*Validation_dot4)[i], dot4));
        LogCommentFmt(
            L"element #%u, input1 = (%f, %f, %f, %f), input2 = (%f, %f, "
            L"%f, %f), \n dot2 = %f, dot2_expected = %f, dot3 = %f, "
            L"dot3_expected = %f, dot4 = %f, dot4_expected = %f",
            i, p->input1.x, p->input1.y, p->input1.z, p->input1.w, p->input2.x,
            p->input2.y, p->input2.z, p->input2.w, p->o_dot2, dot2, p->o_dot3, dot3,
            p->o_dot4, dot4);
        VerifyOutputWithExpectedValueFloat(p->o_dot2, dot2, Validation_type,
                                           tolerance);
        VerifyOutputWithExpectedValueFloat(p->o_dot3, dot3, Validation_type,
                                           tolerance);
        VerifyOutputWithExpectedValueFloat(p->o_dot4, dot4, Validation_type,
                                           tolerance);
    }
}

TEST_F(ExecutionTest, Dot2AddHalfTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_4, false)) {
        return;
    }

    if (!DoesDeviceSupportNative16bitOps(pDevice)) {
        WEX::Logging::Log::Comment(L"Device does not support native 16-bit operations.");
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
        return;
    }

    int tableSize = sizeof(Dot2AddHalfOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(Dot2AddHalfOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
    CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

    std::vector<WEX::Common::String> *validation_input1 =
        &handler.GetTableParamByName(L"Validation.Input1")->m_StringTable;
    std::vector<WEX::Common::String> *validation_input2 =
        &handler.GetTableParamByName(L"Validation.Input2")->m_StringTable;
    std::vector<float> *validation_acc = &handler.GetTableParamByName(L"Validation.Input3")->m_floatTable;
    std::vector<float> *validation_result = &handler.GetTableParamByName(L"Validation.Expected1")->m_floatTable;

    PCWSTR Validation_type = handler.GetTableParamByName(L"Validation.Type")->m_str;
    double tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_double;
    size_t count = validation_input1->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "Dot2AddHalfOp",
        // this callback is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "SDot2AddHalfOp"));
        size_t size = sizeof(SDot2AddHalfOp) * count;
        Data.resize(size);
        SDot2AddHalfOp *pPrimitives = (SDot2AddHalfOp*)Data.data();
        for (size_t i = 0; i < count; ++i) {
            SDot2AddHalfOp *p = &pPrimitives[i];
            Half2 val1,val2;
            VERIFY_SUCCEEDED(ParseDataToVectorHalf((*validation_input1)[i],
                                                    (uint16_t *)&val1, 2));
            VERIFY_SUCCEEDED(ParseDataToVectorHalf((*validation_input2)[i],
                                                    (uint16_t *)&val2, 2));
            p->input1 = val1;
            p->input2 = val2;
            p->acc = (*validation_acc)[i];
        }
        // use shader from data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
        pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("SDot2AddHalfOp", &data);

    SDot2AddHalfOp *pPrimitives = (SDot2AddHalfOp*)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    for (size_t i = 0; i < count; ++i) {
        SDot2AddHalfOp *p = &pPrimitives[i];
        float expectedResult = (*validation_result)[i];
        float input1x = ConvertFloat16ToFloat32(p->input1.x);
        float input1y = ConvertFloat16ToFloat32(p->input1.y);
        float input2x = ConvertFloat16ToFloat32(p->input2.x);
        float input2y = ConvertFloat16ToFloat32(p->input2.y);
        LogCommentFmt(
            L"element #%u, input1 = (%f, %f), input2 = (%f, %f), acc = %f\n"
            L"result = %f, result_expected = %f",
            i, input1x, input1y, input2x, input2y, p->acc, p->result, expectedResult);
        VerifyOutputWithExpectedValueFloat(p->result, expectedResult, Validation_type, tolerance);
    }
}

TEST_F(ExecutionTest, Dot4AddI8PackedTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_4, false)) {
        return;
    }

    int tableSize = sizeof(Dot4AddI8PackedOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(Dot4AddI8PackedOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);

    std::vector<uint32_t> *validation_input1 = &handler.GetTableParamByName(L"Validation.Input1")->m_uint32Table;
    std::vector<uint32_t> *validation_input2 = &handler.GetTableParamByName(L"Validation.Input2")->m_uint32Table;
    std::vector<int32_t> *validation_acc = &handler.GetTableParamByName(L"Validation.Input3")->m_int32Table;
    std::vector<int32_t> *validation_result = &handler.GetTableParamByName(L"Validation.Expected1")->m_int32Table;

    size_t count = validation_input1->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "Dot4AddI8PackedOp",
        // this callback is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "SDot4AddI8PackedOp"));
        size_t size = sizeof(SDot4AddI8PackedOp) * count;
        Data.resize(size);
        SDot4AddI8PackedOp *pPrimitives = (SDot4AddI8PackedOp*)Data.data();
        for (size_t i = 0; i < count; ++i) {
            SDot4AddI8PackedOp *p = &pPrimitives[i];
            p->input1 = (*validation_input1)[i];
            p->input2 = (*validation_input2)[i];
            p->acc = (*validation_acc)[i];
        }
        // use shader from data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("SDot4AddI8PackedOp", &data);

    SDot4AddI8PackedOp *pPrimitives = (SDot4AddI8PackedOp*)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    for (size_t i = 0; i < count; ++i) {
        SDot4AddI8PackedOp *p = &pPrimitives[i];
        int32_t expectedResult = (*validation_result)[i];
        LogCommentFmt(
            L"element #%u, input1 = %u, input2 = %u, acc = %d \n"
            L"result = %d, result_expected = %d",
            i, p->input1, p->input2, p->acc, p->result, expectedResult);
        VerifyOutputWithExpectedValueInt(p->result, expectedResult, 0);
    }
}

TEST_F(ExecutionTest, Dot4AddU8PackedTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_4, false)) {
        return;
    }

    int tableSize = sizeof(Dot4AddU8PackedOpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(Dot4AddU8PackedOpParameters, tableSize);

    CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);

    std::vector<uint32_t> *validation_input1 = &handler.GetTableParamByName(L"Validation.Input1")->m_uint32Table;
    std::vector<uint32_t> *validation_input2 = &handler.GetTableParamByName(L"Validation.Input2")->m_uint32Table;
    std::vector<uint32_t> *validation_acc = &handler.GetTableParamByName(L"Validation.Input3")->m_uint32Table;
    std::vector<uint32_t> *validation_result = &handler.GetTableParamByName(L"Validation.Expected1")->m_uint32Table;

    size_t count = validation_input1->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "Dot4AddU8PackedOp",
        // this callback is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "SDot4AddU8PackedOp"));
        size_t size = sizeof(SDot4AddU8PackedOp) * count;
        Data.resize(size);
        SDot4AddU8PackedOp *pPrimitives = (SDot4AddU8PackedOp*)Data.data();
        for (size_t i = 0; i < count; ++i) {
            SDot4AddU8PackedOp *p = &pPrimitives[i];
            p->input1 = (*validation_input1)[i];
            p->input2 = (*validation_input2)[i];
            p->acc = (*validation_acc)[i];
        }
        // use shader from data table
        pShaderOp->Shaders.at(0).Target = Target.m_psz;
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("SDot4AddU8PackedOp", &data);

    SDot4AddU8PackedOp *pPrimitives = (SDot4AddU8PackedOp*)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    for (size_t i = 0; i < count; ++i) {
        SDot4AddU8PackedOp *p = &pPrimitives[i];
        uint32_t expectedResult = (*validation_result)[i];
        LogCommentFmt(
            L"element #%u, input1 = %u, input2 = %u, acc = %u \n"
            L"result = %u, result_expected = %u, ",
            i, p->input1, p->input2, p->acc, p->result, expectedResult);
        VerifyOutputWithExpectedValueUInt(p->result, expectedResult, 0);
    }
}

TEST_F(ExecutionTest, Msad4Test) {
    WEX::TestExecution::SetVerifyOutput verifySettings(
        WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice)) {
        return;
    }
    size_t tableSize = sizeof(Msad4OpParameters) / sizeof(TableParameter);
    TableParameterHandler handler(Msad4OpParameters, tableSize);

    CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
    double tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_double;

    std::vector<unsigned int> *Validation_Reference =
        &handler.GetTableParamByName(L"Validation.Input1")->m_uint32Table;
    std::vector<WEX::Common::String> *Validation_Source =
        &handler.GetTableParamByName(L"Validation.Input2")->m_StringTable;
    std::vector<WEX::Common::String> *Validation_Accum =
        &handler.GetTableParamByName(L"Validation.Input3")->m_StringTable;
    std::vector<WEX::Common::String> *Validation_Expected =
        &handler.GetTableParamByName(L"Validation.Expected1")->m_StringTable;

    size_t count = Validation_Expected->size();

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
        pDevice, m_support, pStream, "Msad4",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "SMsad4"));
        size_t size = sizeof(SMsad4) * count;
        Data.resize(size);
        SMsad4 *pPrimitives = (SMsad4*)Data.data();
        for (size_t i = 0; i < count; ++i) {
            SMsad4 *p = &pPrimitives[i];
            XMUINT2 src;
            XMUINT4 accum;
            VERIFY_SUCCEEDED(ParseDataToVectorUint((*Validation_Source)[i], (unsigned int*)&src, 2));
            VERIFY_SUCCEEDED(ParseDataToVectorUint((*Validation_Accum)[i], (unsigned int*)&accum, 4));
            p->ref = (*Validation_Reference)[i];
            p->src = src;
            p->accum = accum;
        }
        // use shader from data table
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
    });

    MappedData data;
    test->Test->GetReadBackData("SMsad4", &data);

    SMsad4 *pPrimitives = (SMsad4*)data.data();
    WEX::TestExecution::DisableVerifyExceptions dve;
    for (size_t i = 0; i < count; ++i) {
        SMsad4 *p = &pPrimitives[i];
        XMUINT4 result;
        VERIFY_SUCCEEDED(ParseDataToVectorUint((*Validation_Expected)[i],
                                               (unsigned int *)&result, 4));
        LogCommentFmt(
            L"element #%u, ref = %u(0x%08x), src = %u(0x%08x), %u(0x%08x), "
            L"accum = %u(0x%08x), %u(0x%08x), %u(0x%08x), %u(0x%08x),\n"
            L"result = %u(0x%08x), %u(0x%08x), %u(0x%08x), %u(0x%08x),\n"
            L"expected = %u(0x%08x), %u(0x%08x), %u(0x%08x), %u(0x%08x)", i,
            p->ref, p->ref, p->src.x, p->src.x, p->src.y, p->src.y, p->accum.x,
            p->accum.x, p->accum.y, p->accum.y, p->accum.z, p->accum.z,
            p->accum.w, p->accum.w, p->result.x, p->result.x, p->result.y,
            p->result.y, p->result.z, p->result.z, p->result.w, p->result.w,
            result.x, result.x, result.y, result.y, result.z, result.z,
            result.w, result.w);

        int toleranceInt = (int)tolerance;
        VerifyOutputWithExpectedValueInt(p->result.x, result.x, toleranceInt);
        VerifyOutputWithExpectedValueInt(p->result.y, result.y, toleranceInt);
        VerifyOutputWithExpectedValueInt(p->result.z, result.z, toleranceInt);
        VerifyOutputWithExpectedValueInt(p->result.w, result.w, toleranceInt);
    }
}

TEST_F(ExecutionTest, DenormBinaryFloatOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
    WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_2)) {
    return;
  }
  // Read data from the table
  int tableSize = sizeof(DenormBinaryFPOpParameters) / sizeof(TableParameter);
  TableParameterHandler handler(DenormBinaryFPOpParameters, tableSize);

  CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
  CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
  CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

  std::vector<WEX::Common::String> *Validation_Input1 =
    &(handler.GetTableParamByName(L"Validation.Input1")->m_StringTable);
  std::vector<WEX::Common::String> *Validation_Input2 =
    &(handler.GetTableParamByName(L"Validation.Input2")->m_StringTable);

  std::vector<WEX::Common::String> *Validation_Expected1 =
    &(handler.GetTableParamByName(L"Validation.Expected1")->m_StringTable);
  // two expected outputs for any mode
  std::vector<WEX::Common::String> *Validation_Expected2 =
    &(handler.GetTableParamByName(L"Validation.Expected2")->m_StringTable);

  LPCWSTR Validation_Type = handler.GetTableParamByName(L"Validation.Type")->m_str;
  double Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_double;
  size_t count = Validation_Input1->size();

  using namespace hlsl::DXIL;
  Float32DenormMode mode = Float32DenormMode::Any;
  if (strcmp(Arguments.m_psz, "-denorm preserve") == 0) {
    mode = Float32DenormMode::Preserve;
  }
  else if (strcmp(Arguments.m_psz, "-denorm ftz") == 0) {
    mode = Float32DenormMode::FTZ;
  }
  if (mode == Float32DenormMode::Any) {
    DXASSERT(Validation_Expected2->size() == Validation_Expected1->size(),
             "must have same number of expected values");
  }
  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
    pDevice, m_support, pStream, "BinaryFPOp",
    // this callbacked is called when the test
    // is creating the resource to run the test
    [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
    VERIFY_IS_TRUE(0 == _stricmp(Name, "SBinaryFPOp"));
    size_t size = sizeof(SBinaryFPOp) * count;
    Data.resize(size);
    SBinaryFPOp *pPrimitives = (SBinaryFPOp *)Data.data();
    for (size_t i = 0; i < count; ++i) {
      SBinaryFPOp *p = &pPrimitives[i];
      PCWSTR str1 = (*Validation_Input1)[i % Validation_Input1->size()];
      PCWSTR str2 = (*Validation_Input2)[i % Validation_Input2->size()];
      float val1, val2;
      VERIFY_SUCCEEDED(ParseDataToFloat(str1, val1));
      VERIFY_SUCCEEDED(ParseDataToFloat(str2, val2));
      p->input1 = val1;
      p->input2 = val2;
    }

    // use shader from data table
    pShaderOp->Shaders.at(0).Target = Target.m_psz;
    pShaderOp->Shaders.at(0).Text = Text.m_psz;
    pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
  });

  MappedData data;
  test->Test->GetReadBackData("SBinaryFPOp", &data);

  SBinaryFPOp *pPrimitives = (SBinaryFPOp *)data.data();
  WEX::TestExecution::DisableVerifyExceptions dve;

  for (unsigned i = 0; i < count; ++i) {
    SBinaryFPOp *p = &pPrimitives[i];
    if (mode == Float32DenormMode::Any) {
       LPCWSTR str1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
       LPCWSTR str2 = (*Validation_Expected2)[i % Validation_Expected2->size()];
       float val1;
       float val2;
       VERIFY_SUCCEEDED(ParseDataToFloat(str1, val1));
       VERIFY_SUCCEEDED(ParseDataToFloat(str2, val2));
       LogCommentFmt(L"element #%u, input1 = %6.8f, input2 = %6.8f, output = "
         L"%6.8f, expected = %6.8f(%x) or %6.8f(%x)",
         i, p->input1, p->input2, p->output1, val1, *(int *)&val1, val2, *(int *)&val2);
       VERIFY_IS_TRUE(
           CompareOutputWithExpectedValueFloat(
               p->output1, val1, Validation_Type, Validation_Tolerance, mode) ||
           CompareOutputWithExpectedValueFloat(
               p->output1, val2, Validation_Type, Validation_Tolerance, mode));
    }
    else {
       LPCWSTR str1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
       float val1;
       VERIFY_SUCCEEDED(ParseDataToFloat(str1, val1));
       LogCommentFmt(L"element #%u, input1 = %6.8f, input2 = %6.8f, output = "
         L"%6.8f, expected = %6.8f(%a)",
         i, p->input1, p->input2, p->output1, val1, *(int *)&val1);
       VerifyOutputWithExpectedValueFloat(p->output1, val1, Validation_Type,
          Validation_Tolerance, mode);
    }
  }
}

TEST_F(ExecutionTest, DenormTertiaryFloatOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(
    WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice, D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_2)) {
    return;
  }
  // Read data from the table

  int tableSize = sizeof(DenormTertiaryFPOpParameters) / sizeof(TableParameter);
  TableParameterHandler handler(DenormTertiaryFPOpParameters, tableSize);

  CW2A Target(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
  CW2A Text(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
  CW2A Arguments(handler.GetTableParamByName(L"ShaderOp.Arguments")->m_str);

  std::vector<WEX::Common::String> *Validation_Input1 =
    &(handler.GetTableParamByName(L"Validation.Input1")->m_StringTable);
  std::vector<WEX::Common::String> *Validation_Input2 =
    &(handler.GetTableParamByName(L"Validation.Input2")->m_StringTable);
  std::vector<WEX::Common::String> *Validation_Input3 =
    &(handler.GetTableParamByName(L"Validation.Input3")->m_StringTable);

  std::vector<WEX::Common::String> *Validation_Expected1 =
    &(handler.GetTableParamByName(L"Validation.Expected1")->m_StringTable);
  
  // two expected outputs for any mode
  std::vector<WEX::Common::String> *Validation_Expected2 =
    &(handler.GetTableParamByName(L"Validation.Expected2")->m_StringTable);
  LPCWSTR Validation_Type = handler.GetTableParamByName(L"Validation.Type")->m_str;
  double Validation_Tolerance = handler.GetTableParamByName(L"Validation.Tolerance")->m_double;
  size_t count = Validation_Input1->size();

  using namespace hlsl::DXIL;
  Float32DenormMode mode = Float32DenormMode::Any;
  if (strcmp(Arguments.m_psz, "-denorm preserve") == 0) {
    mode = Float32DenormMode::Preserve;
  }
  else if (strcmp(Arguments.m_psz, "-denorm ftz") == 0) {
    mode = Float32DenormMode::FTZ;
  }
  if (mode == Float32DenormMode::Any) {
    DXASSERT(Validation_Expected2->size() == Validation_Expected1->size(),
      "must have same number of expected values");
  }
  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(
    pDevice, m_support, pStream, "TertiaryFPOp",
    // this callbacked is called when the test
    // is creating the resource to run the test
    [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
    VERIFY_IS_TRUE(0 == _stricmp(Name, "STertiaryFPOp"));
    size_t size = sizeof(STertiaryFPOp) * count;
    Data.resize(size);
    STertiaryFPOp *pPrimitives = (STertiaryFPOp *)Data.data();
    for (size_t i = 0; i < count; ++i) {
      STertiaryFPOp *p = &pPrimitives[i];
      PCWSTR str1 = (*Validation_Input1)[i % Validation_Input1->size()];
      PCWSTR str2 = (*Validation_Input2)[i % Validation_Input2->size()];
      PCWSTR str3 = (*Validation_Input3)[i % Validation_Input3->size()];
      float val1, val2, val3;
      VERIFY_SUCCEEDED(ParseDataToFloat(str1, val1));
      VERIFY_SUCCEEDED(ParseDataToFloat(str2, val2));
      VERIFY_SUCCEEDED(ParseDataToFloat(str3, val3));
      p->input1 = val1;
      p->input2 = val2;
      p->input3 = val3;
    }

    // use shader from data table
    pShaderOp->Shaders.at(0).Target = Target.m_psz;
    pShaderOp->Shaders.at(0).Text = Text.m_psz;
    pShaderOp->Shaders.at(0).Arguments = Arguments.m_psz;
  });

  MappedData data;
  test->Test->GetReadBackData("STertiaryFPOp", &data);

  STertiaryFPOp *pPrimitives = (STertiaryFPOp *)data.data();
  WEX::TestExecution::DisableVerifyExceptions dve;

  for (unsigned i = 0; i < count; ++i) {
    STertiaryFPOp *p = &pPrimitives[i];
    if (mode == Float32DenormMode::Any) {
        LPCWSTR str1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
        LPCWSTR str2 = (*Validation_Expected2)[i % Validation_Expected2->size()];
        float val1;
        float val2;
        VERIFY_SUCCEEDED(ParseDataToFloat(str1, val1));
        VERIFY_SUCCEEDED(ParseDataToFloat(str2, val2));
        LogCommentFmt(L"element #%u, input1 = %6.8f, input2 = %6.8f, input3 = %6.8f, output = "
            L"%6.8f, expected = %6.8f(%x) or %6.8f(%x)",
            i, p->input1, p->input2, p->input3, p->output, val1, *(int *)&val1, val2, *(int *)&val2);
        VERIFY_IS_TRUE(
            CompareOutputWithExpectedValueFloat(
                p->output, val1, Validation_Type, Validation_Tolerance, mode) ||
            CompareOutputWithExpectedValueFloat(
                p->output, val2, Validation_Type, Validation_Tolerance, mode));
    }
    else {
        LPCWSTR str1 = (*Validation_Expected1)[i % Validation_Expected1->size()];
        float val1;
        VERIFY_SUCCEEDED(ParseDataToFloat(str1, val1));
        LogCommentFmt(L"element #%u, input1 = %6.8f, input2 = %6.8f, input3 = %6.8f, output = "
            L"%6.8f, expected = %6.8f(%a)",
            i, p->input1, p->input2, p->input3, p->output, val1, *(int *)&val1);
        VerifyOutputWithExpectedValueFloat(p->output, val1, Validation_Type,
            Validation_Tolerance, mode);
    }
  }
}

// Setup for wave intrinsics tests
enum class ShaderOpKind {
  WaveSum,
  WaveProduct,
  WaveActiveMax,
  WaveActiveMin,
  WaveCountBits,
  WaveActiveAllEqual,
  WaveActiveAnyTrue,
  WaveActiveAllTrue,
  WaveActiveBitOr,
  WaveActiveBitAnd,
  WaveActiveBitXor,
  ShaderOpInvalid
};

struct ShaderOpKindPair {
  LPCWSTR name;
  ShaderOpKind kind;
};

static ShaderOpKindPair ShaderOpKindTable[] = {
  { L"WaveActiveSum", ShaderOpKind::WaveSum },
  { L"WaveActiveUSum", ShaderOpKind::WaveSum },
  { L"WaveActiveProduct", ShaderOpKind::WaveProduct },
  { L"WaveActiveUProduct", ShaderOpKind::WaveProduct },
  { L"WaveActiveMax", ShaderOpKind::WaveActiveMax },
  { L"WaveActiveUMax", ShaderOpKind::WaveActiveMax },
  { L"WaveActiveMin", ShaderOpKind::WaveActiveMin },
  { L"WaveActiveUMin", ShaderOpKind::WaveActiveMin },
  { L"WaveActiveCountBits", ShaderOpKind::WaveCountBits },
  { L"WaveActiveAllEqual", ShaderOpKind::WaveActiveAllEqual },
  { L"WaveActiveAnyTrue", ShaderOpKind::WaveActiveAnyTrue },
  { L"WaveActiveAllTrue", ShaderOpKind::WaveActiveAllTrue },
  { L"WaveActiveBitOr", ShaderOpKind::WaveActiveBitOr },
  { L"WaveActiveBitAnd", ShaderOpKind::WaveActiveBitAnd },
  { L"WaveActiveBitXor", ShaderOpKind::WaveActiveBitXor },
  { L"WavePrefixSum", ShaderOpKind::WaveSum },
  { L"WavePrefixUSum", ShaderOpKind::WaveSum },
  { L"WavePrefixProduct", ShaderOpKind::WaveProduct },
  { L"WavePrefixUProduct", ShaderOpKind::WaveProduct },
  { L"WavePrefixMax", ShaderOpKind::WaveActiveMax },
  { L"WavePrefixUMax", ShaderOpKind::WaveActiveMax },
  { L"WavePrefixMin", ShaderOpKind::WaveActiveMin },
  { L"WavePrefixUMin", ShaderOpKind::WaveActiveMin },
  { L"WavePrefixCountBits", ShaderOpKind::WaveCountBits }
};

ShaderOpKind GetShaderOpKind(LPCWSTR str) {
  for (size_t i = 0; i < sizeof(ShaderOpKindTable)/sizeof(ShaderOpKindPair); ++i) {
    if (_wcsicmp(ShaderOpKindTable[i].name, str) == 0) {
      return ShaderOpKindTable[i].kind;
    }
  }
  DXASSERT_ARGS(false, "Invalid ShaderOp name: %s", str);
  return ShaderOpKind::ShaderOpInvalid;
}

template <typename InType, typename OutType, ShaderOpKind kind>
struct computeExpected {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    return 0;
  }
};

template <typename InType, typename OutType>
struct computeExpected<InType, OutType, ShaderOpKind::WaveSum> {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    OutType sum = 0;
    for (size_t i = 0; i < index; ++i) {
      if (masks.at(i) == maskValue) {
        sum += inputs.at(i);
      }
    }
    return sum;
  }
};

template <typename InType, typename OutType>
struct computeExpected<InType, OutType, ShaderOpKind::WaveProduct> {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    OutType prod = 1;
    for (size_t i = 0; i < index; ++i) {
      if (masks.at(i) == maskValue) {
        prod *= inputs.at(i);
      }
    }
    return prod;
  }
};

template <typename InType, typename OutType>
struct computeExpected<InType, OutType, ShaderOpKind::WaveActiveMax> {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    OutType maximum = std::numeric_limits<OutType>::min();
    for (size_t i = 0; i < index; ++i) {
      if (masks.at(i) == maskValue && inputs.at(i) > maximum)
        maximum = inputs.at(i);
    }
    return maximum;
  }
};

template <typename InType, typename OutType>
struct computeExpected<InType, OutType, ShaderOpKind::WaveActiveMin> {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    OutType minimum = std::numeric_limits<OutType>::max();
    for (size_t i = 0; i < index; ++i) {
      if (masks.at(i) == maskValue && inputs.at(i) < minimum)
        minimum = inputs.at(i);
    }
    return minimum;
  }
};

template <typename InType, typename OutType>
struct computeExpected<InType, OutType, ShaderOpKind::WaveCountBits> {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    OutType count = 0;
    for (size_t i = 0; i < index; ++i) {
      if (masks.at(i) == maskValue && inputs.at(i) > 3) {
        count++;
      }
    }
    return count;
  }
};

// In HLSL, boolean is represented in a 4 byte (uint32) format,
// So we cannot use c++ bool type to represent bool in HLSL
// HLSL returns 0 for false and 1 for true
template <typename InType, typename OutType>
struct computeExpected<InType, OutType, ShaderOpKind::WaveActiveAnyTrue> {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    for (size_t i = 0; i < index; ++i) {
      if (masks.at(i) == maskValue && inputs.at(i) != 0) {
        return 1;
      }
    }
    return 0;
  }
};

template <typename InType, typename OutType>
struct computeExpected<InType, OutType, ShaderOpKind::WaveActiveAllTrue> {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    for (size_t i = 0; i < index; ++i) {
      if (masks.at(i) == maskValue && inputs.at(i) == 0) {
        return 0;
      }
    }
    return 1;
  }
};

template <typename InType, typename OutType>
struct computeExpected<InType, OutType, ShaderOpKind::WaveActiveAllEqual> {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    const InType *val = nullptr;
    for (size_t i = 0; i < index; ++i) {
      if (masks.at(i) == maskValue) {
        if (val && *val != inputs.at(i)) {
          return 0;
        }
        val = &inputs.at(i);
      }
    }
    return 1;
  }
};

template <typename InType, typename OutType>
struct computeExpected<InType, OutType, ShaderOpKind::WaveActiveBitOr> {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    OutType bits = 0x00000000;
    for (size_t i = 0; i < index; ++i) {
      if (masks.at(i) == maskValue) {
        bits |= inputs.at(i);
      }
    }
    return bits;
  }
};

template <typename InType, typename OutType>
struct computeExpected<InType, OutType, ShaderOpKind::WaveActiveBitAnd> {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    OutType bits = 0xffffffff;
    for (size_t i = 0; i < index; ++i) {
      if (masks.at(i) == maskValue) {
        bits &= inputs.at(i);
      }
    }
    return bits;
  }
};

template <typename InType, typename OutType>
struct computeExpected<InType, OutType, ShaderOpKind::WaveActiveBitXor> {
  OutType operator()(const std::vector<InType> &inputs,
                     const std::vector<int> &masks, int maskValue,
                     unsigned int index) {
    OutType bits = 0x00000000;
    for (size_t i = 0; i < index; ++i) {
      if (masks.at(i) == maskValue) {
        bits ^= inputs.at(i);
      }
    }
    return bits;
  }
};

// Mask functions used to control active lanes
static int MaskAll(int i) {
  UNREFERENCED_PARAMETER(i);
  return 1;
}

static int MaskEveryOther(int i) {
  return i % 2 == 0 ? 1 : 0;
}

static int MaskEveryThird(int i) {
  return i % 3 == 0 ? 1 : 0;
}

typedef int(*MaskFunction)(int);
static MaskFunction MaskFunctionTable[] = {
  MaskAll, MaskEveryOther, MaskEveryThird
};

template <typename InType, typename OutType>
static OutType computeExpectedWithShaderOp(const std::vector<InType> &inputs,
                                           const std::vector<int> &masks,
                                           int maskValue, unsigned int index,
                                           LPCWSTR str) {
  ShaderOpKind kind = GetShaderOpKind(str);
  switch (kind) {
  case ShaderOpKind::WaveSum:
    return computeExpected<InType, OutType, ShaderOpKind::WaveSum>()(inputs, masks, maskValue, index);
  case ShaderOpKind::WaveProduct:
    return computeExpected<InType, OutType, ShaderOpKind::WaveProduct>()(inputs, masks, maskValue, index);
  case ShaderOpKind::WaveActiveMax:
    return computeExpected<InType, OutType, ShaderOpKind::WaveActiveMax>()(inputs, masks, maskValue, index);
  case ShaderOpKind::WaveActiveMin:
    return computeExpected<InType, OutType, ShaderOpKind::WaveActiveMin>()(inputs, masks, maskValue, index);
  case ShaderOpKind::WaveCountBits:
    return computeExpected<InType, OutType, ShaderOpKind::WaveCountBits>()(inputs, masks, maskValue, index);
  case ShaderOpKind::WaveActiveBitOr:
    return computeExpected<InType, OutType, ShaderOpKind::WaveActiveBitOr>()(inputs, masks, maskValue, index);
  case ShaderOpKind::WaveActiveBitAnd:
    return computeExpected<InType, OutType, ShaderOpKind::WaveActiveBitAnd>()(inputs, masks, maskValue, index);
  case ShaderOpKind::WaveActiveBitXor:
    return computeExpected<InType, OutType, ShaderOpKind::WaveActiveBitXor>()(inputs, masks, maskValue, index);
  case ShaderOpKind::WaveActiveAnyTrue:
    return computeExpected<InType, OutType, ShaderOpKind::WaveActiveAnyTrue>()(inputs, masks, maskValue, index);
  case ShaderOpKind::WaveActiveAllTrue:
    return computeExpected<InType, OutType, ShaderOpKind::WaveActiveAllTrue>()(inputs, masks, maskValue, index);
  case ShaderOpKind::WaveActiveAllEqual:
    return computeExpected<InType, OutType, ShaderOpKind::WaveActiveAllEqual>()(inputs, masks, maskValue, index);
  default:
    DXASSERT_ARGS(false, "Invalid ShaderOp Name: %s", str);
    return (OutType) 0;
  }
};

// A framework for testing individual wave intrinsics tests.
// This test case is assuming that functions 1) WaveIsFirstLane and 2) WaveGetLaneIndex are correct for all lanes.
template <class T1, class T2>
void ExecutionTest::WaveIntrinsicsActivePrefixTest(
    TableParameter *pParameterList, size_t numParameter, bool isPrefix) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  // Resource representation for compute shader
  // firstLaneId is used to group different waves
  // laneIndex is used to identify lane within the wave.
  // Lane ids are not necessarily in same order as thread ids.
  struct PerThreadData {
      unsigned firstLaneId;
      unsigned laneIndex;
      int mask;
      T1 input;
      T2 output;
  };

  unsigned int NumThreadsX = 8;
  unsigned int NumThreadsY = 12;
  unsigned int NumThreadsZ = 1;

  static const unsigned int ThreadsPerGroup = NumThreadsX * NumThreadsY * NumThreadsZ;
  static const unsigned int DispatchGroupCount = 1;
  static const unsigned int ThreadCount = ThreadsPerGroup * DispatchGroupCount;
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice)) {
    return;
  }
  if (!DoesDeviceSupportWaveOps(pDevice)) {
    // Optional feature, so it's correct to not support it if declared as such.
    WEX::Logging::Log::Comment(L"Device does not support wave operations.");
    return;
  }

  TableParameterHandler handler(pParameterList, numParameter);

  unsigned int numInputSet = handler.GetTableParamByName(L"Validation.NumInputSet")->m_uint;

  // Obtain the list of input lists
  std::vector<std::vector<T1>*> InputDataList;
  for (unsigned int i = 0;
    i < numInputSet; ++i) {
    std::wstring inputName = L"Validation.InputSet";
    inputName.append(std::to_wstring(i + 1));
    InputDataList.push_back(handler.GetDataArray<T1>(inputName.data()));
  }
  CW2A Text(handler.GetTableParamByName(L"ShaderOp.text")->m_str);

  std::shared_ptr<st::ShaderOpSet> ShaderOpSet = std::make_shared<st::ShaderOpSet>();
  st::ParseShaderOpSetFromStream(pStream, ShaderOpSet.get());

  // Running compute shader for each input set with different masks
  for (size_t setIndex = 0; setIndex < numInputSet; ++setIndex) {
    for (size_t maskIndex = 0; maskIndex < sizeof(MaskFunctionTable) / sizeof(MaskFunction); ++maskIndex) {
      std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTestAfterParse(
        pDevice, m_support, "WaveIntrinsicsOp",
        // this callbacked is called when the test
        // is creating the resource to run the test
        [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
        VERIFY_IS_TRUE(0 == _stricmp(Name, "SWaveIntrinsicsOp"));
        size_t size = sizeof(PerThreadData) * ThreadCount;
        Data.resize(size);
        PerThreadData *pPrimitives = (PerThreadData*)Data.data();
        // 4 different inputs for each operation test
        size_t index = 0;
        std::vector<T1> *IntList = InputDataList[setIndex];
        while (index < ThreadCount) {
          PerThreadData *p = &pPrimitives[index];
          p->firstLaneId = 0xFFFFBFFF;
          p->laneIndex = 0xFFFFBFFF;
          p->mask = MaskFunctionTable[maskIndex]((int)index);
          p->input = (*IntList)[index % IntList->size()];
          p->output = 0xFFFFBFFF;
          index++;
        }
        // use shader from data table
        pShaderOp->Shaders.at(0).Text = Text.m_psz;
      }, ShaderOpSet);

      // Check the value
      MappedData data;
      test->Test->GetReadBackData("SWaveIntrinsicsOp", &data);

      PerThreadData *pPrimitives = (PerThreadData*)data.data();
      WEX::TestExecution::DisableVerifyExceptions dve;

      // Grouping data by waves
      std::vector<int> firstLaneIds;
      for (size_t i = 0; i < ThreadCount; ++i) {
        PerThreadData *p = &pPrimitives[i];
        int firstLaneId = p->firstLaneId;
        if (!contains(firstLaneIds, firstLaneId)) {
          firstLaneIds.push_back(firstLaneId);
        }
      }

      std::map<int, std::unique_ptr<std::vector<PerThreadData *>>> waves;
      for (size_t i = 0; i < firstLaneIds.size(); ++i) {
        waves[firstLaneIds.at(i)] = std::make_unique<std::vector<PerThreadData*>>();
      }

      for (size_t i = 0; i < ThreadCount; ++i) {
        PerThreadData *p = &pPrimitives[i];
        waves[p->firstLaneId].get()->push_back(p);
      }

      // validate for each wave
      for (size_t i = 0; i < firstLaneIds.size(); ++i) {
        // collect inputs and masks for a given wave
        std::vector<PerThreadData *> *waveData = waves[firstLaneIds.at(i)].get();
        std::vector<T1> inputList(waveData->size());
        std::vector<int> maskList(waveData->size(), -1);
        std::vector<T2> outputList(waveData->size());
        // sort inputList and masklist by lane id. input for each lane can be computed for its group index
        for (size_t j = 0, end = waveData->size(); j < end; ++j) {
          unsigned laneID = waveData->at(j)->laneIndex;
          // ensure that each lane ID is unique and within the range
          VERIFY_IS_TRUE(0 <= laneID && laneID < waveData->size());
          VERIFY_IS_TRUE(maskList.at(laneID) == -1);
          maskList.at(laneID) = waveData->at(j)->mask;
          inputList.at(laneID) = waveData->at(j)->input;
          outputList.at(laneID) = waveData->at(j)->output;
        }
        std::wstring inputStr = L"Wave Inputs:  ";
        std::wstring maskStr =  L"Wave Masks:   ";
        std::wstring outputStr = L"Wave Outputs: ";
        // append input string and mask string in lane id order
        for (size_t j = 0, end = waveData->size(); j < end; ++j) {
          maskStr.append(std::to_wstring(maskList.at(j)));
          maskStr.append(L" ");
          inputStr.append(std::to_wstring(inputList.at(j)));
          inputStr.append(L" ");
          outputStr.append(std::to_wstring(outputList.at(j)));
          outputStr.append(L" ");
        }

        LogCommentFmt(inputStr.data());
        LogCommentFmt(maskStr.data());
        LogCommentFmt(outputStr.data());
        LogCommentFmt(L"\n");
        // Compute expected output for a given inputs, masks, and index
        for (size_t laneIndex = 0, laneEnd = inputList.size(); laneIndex < laneEnd; ++laneIndex) {
          T2 expected;
          // WaveActive is equivalent to WavePrefix lane # lane count
          unsigned index = isPrefix ? (unsigned)laneIndex : (unsigned)inputList.size();
          if (maskList.at(laneIndex) == 1) {
            expected = computeExpectedWithShaderOp<T1, T2>(
              inputList, maskList, 1, index,
              handler.GetTableParamByName(L"ShaderOp.Name")->m_str);
          }
          else {
            expected = computeExpectedWithShaderOp<T1, T2>(
              inputList, maskList, 0, index,
              handler.GetTableParamByName(L"ShaderOp.Name")->m_str);
          }
          // TODO: use different comparison for floating point inputs
          bool equal = outputList.at(laneIndex) == expected;
          if (!equal) {
            LogCommentFmt(L"lane%d: %4d, Expected : %4d", laneIndex, outputList.at(laneIndex), expected);
          }
          VERIFY_IS_TRUE(equal);
        }
      }
    }
  }
}

static const unsigned int MinWarpVersionForWaveIntrinsics = 16202;

TEST_F(ExecutionTest, WaveIntrinsicsActiveIntTest) {
  if (GetTestParamUseWARP(true) &&
      !IsValidWarpDllVersion(MinWarpVersionForWaveIntrinsics)) {
    return;
  }
  WaveIntrinsicsActivePrefixTest<int, int>(
      WaveIntrinsicsActiveIntParameters,
      sizeof(WaveIntrinsicsActiveIntParameters) / sizeof(TableParameter),
      /*isPrefix*/ false);
}

TEST_F(ExecutionTest, WaveIntrinsicsActiveUintTest) {
  if (GetTestParamUseWARP(true) &&
      !IsValidWarpDllVersion(MinWarpVersionForWaveIntrinsics)) {
    return;
  }
  WaveIntrinsicsActivePrefixTest<unsigned int, unsigned int>(
      WaveIntrinsicsActiveUintParameters,
      sizeof(WaveIntrinsicsActiveUintParameters) / sizeof(TableParameter),
      /*isPrefix*/ false);
}

TEST_F(ExecutionTest, WaveIntrinsicsPrefixIntTest) {
  if (GetTestParamUseWARP(true) &&
      !IsValidWarpDllVersion(MinWarpVersionForWaveIntrinsics)) {
    return;
  }
  WaveIntrinsicsActivePrefixTest<int, int>(
      WaveIntrinsicsPrefixIntParameters,
      sizeof(WaveIntrinsicsPrefixIntParameters) / sizeof(TableParameter),
      /*isPrefix*/ true);
}

TEST_F(ExecutionTest, WaveIntrinsicsPrefixUintTest) {
  if (GetTestParamUseWARP(true) &&
      !IsValidWarpDllVersion(MinWarpVersionForWaveIntrinsics)) {
    return;
  }
  WaveIntrinsicsActivePrefixTest<unsigned int, unsigned int>(
      WaveIntrinsicsPrefixUintParameters,
      sizeof(WaveIntrinsicsPrefixUintParameters) / sizeof(TableParameter),
      /*isPrefix*/ true);
}

template <typename T>
static T GetWaveMultiPrefixInitialAccumValue(LPCWSTR testName) {
  if (_wcsicmp(testName, L"WaveMultiPrefixProduct") == 0 ||
      _wcsicmp(testName, L"WaveMultiPrefixUProduct") == 0) {
    return static_cast<T>(1);
  } else if (_wcsicmp(testName, L"WaveMultiPrefixSum") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixUSum") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixBitOr") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixUBitOr") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixBitXor") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixUBitXor") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixCountBits") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixUCountBits") == 0) {
    return static_cast<T>(0);
  } else if (_wcsicmp(testName, L"WaveMultiPrefixBitAnd") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixUBitAnd") == 0) {
    return static_cast<T>(-1);
  } else {
    return static_cast<T>(0);
  }
}

template <typename T>
std::function<T(T, T)> GetWaveMultiPrefixReferenceFunction(LPCWSTR testName) {
  if (_wcsicmp(testName, L"WaveMultiPrefixProduct") == 0 ||
      _wcsicmp(testName, L"WaveMultiPrefixUProduct") == 0) {
    return [] (T lhs, T rhs) -> T { return lhs * rhs; };
  } else if (_wcsicmp(testName, L"WaveMultiPrefixSum") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixUSum") == 0) {
    return [] (T lhs, T rhs) -> T { return lhs + rhs; };
  } else if (_wcsicmp(testName, L"WaveMultiPrefixBitAnd") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixUBitAnd") == 0) {
    return [] (T lhs, T rhs) -> T { return lhs & rhs; };
  } else if (_wcsicmp(testName, L"WaveMultiPrefixBitOr") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixUBitOr") == 0) {
    return [] (T lhs, T rhs) -> T { return lhs | rhs; };
  } else if (_wcsicmp(testName, L"WaveMultiPrefixBitXor") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixUBitXor") == 0) {
    return [] (T lhs, T rhs) -> T { return lhs ^ rhs; };
  } else if (_wcsicmp(testName, L"WaveMultiPrefixCountBits") == 0 ||
             _wcsicmp(testName, L"WaveMultiPrefixUCountBits") == 0) {
    // For CountBits, each lane contributes a boolean value. The test input is
    // a zero or non-zero integer. If the input is a non-zero value then the
    // condition is true, thus we contribute one to the bit count.
    return [] (T lhs, T rhs) -> T { return lhs + (rhs ? 1 : 0); };
  } else {
    return [] (T lhs, T rhs) -> T { UNREFERENCED_PARAMETER(lhs); UNREFERENCED_PARAMETER(rhs); return 0; };
  }
}

template <class T>
void
ExecutionTest::WaveIntrinsicsMultiPrefixOpTest(TableParameter *pParameterList,
                                               size_t numParameters) {
  WEX::TestExecution::SetVerifyOutput
    verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  struct PerThreadData {
    uint32_t key;
    uint32_t firstLaneId;
    uint32_t laneId;
    uint32_t mask;
    T value;
    T result;
  };

  constexpr size_t NumThreadsX = 8;
  constexpr size_t NumThreadsY = 12;
  constexpr size_t NumThreadsZ = 1;

  constexpr size_t ThreadsPerGroup = NumThreadsX * NumThreadsY * NumThreadsZ;
  constexpr size_t DispatchGroupSize = 1;
  constexpr size_t ThreadCount = ThreadsPerGroup * DispatchGroupSize;

  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;

  if (!CreateDevice(&pDevice, D3D_SHADER_MODEL_6_5)) {
    return;
  }

  if (!DoesDeviceSupportWaveOps(pDevice)) {
    // Optional feature, so it's correct to not support it if declared as such.
    WEX::Logging::Log::Comment(L"Device does not support wave operations.");
    return;
  }

  std::shared_ptr<st::ShaderOpSet>
    ShaderOpSet = std::make_shared<st::ShaderOpSet>();
  st::ParseShaderOpSetFromStream(pStream, ShaderOpSet.get());

  TableParameterHandler handler(pParameterList, numParameters);
  CW2A shaderSource(handler.GetTableParamByName(L"ShaderOp.Text")->m_str);
  CW2A shaderProfile(handler.GetTableParamByName(L"ShaderOp.Target")->m_str);
  auto testName = handler.GetTableParamByName(L"ShaderOp.Name")->m_str;

  std::vector<T> *keys = handler.GetDataArray<T>(L"Validation.Keys");
  std::vector<T> *values = handler.GetDataArray<T>(L"Validation.Values");

  for (size_t maskIndex = 0; maskIndex < _countof(MaskFunctionTable); ++maskIndex) {
    std::shared_ptr<ShaderOpTestResult> test =
      RunShaderOpTestAfterParse(pDevice, m_support, "WaveIntrinsicsOp",
      [&] (LPCSTR name, std::vector<BYTE> &data, st::ShaderOp *pShaderOp) {
        UNREFERENCED_PARAMETER(name);

        const size_t dataSize = sizeof(PerThreadData) * ThreadCount;

        data.resize(dataSize);
        PerThreadData *pThreadData = reinterpret_cast<PerThreadData *>(data.data());

        for (size_t i = 0; i != ThreadCount; ++i) {
          pThreadData[i].key = keys->at(i % keys->size());
          pThreadData[i].value = values->at(i % values->size());
          pThreadData[i].firstLaneId = 0xdeadbeef;
          pThreadData[i].laneId = 0xdeadbeef;
          pThreadData[i].mask = MaskFunctionTable[maskIndex]((int)i);
          pThreadData[i].result = 0xdeadbeef;
        }

        pShaderOp->Shaders.at(0).Text = shaderSource;
        pShaderOp->Shaders.at(0).Target = shaderProfile;
      }, ShaderOpSet);

    MappedData mappedData;
    test->Test->GetReadBackData("SWaveIntrinsicsOp", &mappedData);
    PerThreadData *resultData = reinterpret_cast<PerThreadData *>(mappedData.data());

    // Partition our data into waves
    std::map<uint32_t, std::vector<PerThreadData *>> waves;

    for (size_t i = 0, e = ThreadCount; i != e; ++i) {
      PerThreadData *elt = &resultData[i];

      // Basic sanity checks
      VERIFY_IS_TRUE(elt->firstLaneId != 0xdeadbeef);
      VERIFY_IS_TRUE(elt->laneId != 0xdeadbeef);

      waves[elt->firstLaneId].push_back(elt);
    }

    // Verify each wave
    auto refFn = GetWaveMultiPrefixReferenceFunction<T>(testName);

    for (auto &w : waves) {
      std::vector<PerThreadData *> &waveData = w.second;
      
      struct {
        bool operator()(PerThreadData *a, PerThreadData *b) const {
          return (a->laneId < b->laneId);
        }
      } compare;
	  // Need to sort based on the lane id
      std::sort(waveData.begin(), waveData.end(), compare);

      LogCommentFmt(L"LaneId    Mask      Key       Value     Result    Expected");
      LogCommentFmt(L"--------  --------  --------  --------  --------  --------");
      for (size_t i = 0, e = waveData.size(); i != e; ++i) {
        PerThreadData *data = waveData[i];

        // Compute prefix operation over each previous lane element that has the
        // same key value, and is part of the same active thread group
        T accum = GetWaveMultiPrefixInitialAccumValue<T>(testName);
        for (unsigned j = 0; j < i; ++j) {
          if (waveData[j]->key == data->key && waveData[j]->mask == data->mask) {
            accum = refFn(accum, waveData[j]->value);
          }
        }

        LogCommentFmt(L"%08X  %08X  %08X  %08X  %08X  %08X", data->laneId, data->mask, data->key, data->value, data->result, accum);

        VERIFY_IS_TRUE(accum == data->result);
      }
      LogCommentFmt(L"\n");
    }
  }
}

TEST_F(ExecutionTest, WaveIntrinsicsSM65IntTest) {
  WaveIntrinsicsMultiPrefixOpTest<int>(WaveIntrinsicsMultiPrefixIntParameters,
                                       _countof(WaveIntrinsicsMultiPrefixIntParameters));
}

TEST_F(ExecutionTest, WaveIntrinsicsSM65UintTest) {
  WaveIntrinsicsMultiPrefixOpTest<unsigned>(WaveIntrinsicsMultiPrefixUintParameters,
                                            _countof(WaveIntrinsicsMultiPrefixUintParameters));
}

TEST_F(ExecutionTest, CBufferTestHalf) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  // Single operation test at the moment.
  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice, D3D_SHADER_MODEL_6_2))
    return;

  uint16_t InputData[] = { 0x3F80, 0x3F00, 0x3D80, 0x7BFF };

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(pDevice, m_support, pStream, "CBufferTestHalf",
    [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
    UNREFERENCED_PARAMETER(pShaderOp);
    VERIFY_IS_TRUE(0 == _stricmp(Name, "CB0"));
    // use shader from data table.
    Data.resize(sizeof(InputData));
    uint16_t *pData = (uint16_t *)Data.data();
    for (size_t i = 0; i < 4; ++i, ++pData) {
      *pData = InputData[i];
    }
  });
  {
    MappedData data;
    test->Test->GetReadBackData("RTarget", &data);
    const uint16_t *pPixels = (uint16_t *)data.data();

    for (int i = 0; i < 4; ++i) {
      uint16_t output = *(pPixels + i);
      float outputFloat = ConvertFloat16ToFloat32(output);
      float inputFloat = ConvertFloat16ToFloat32(InputData[i]);
      LogCommentFmt(L"element #%u: input = %6.8f(0x%04x), output = %6.8f(0x%04x)",
          i, inputFloat, InputData[i], outputFloat, output);
      VERIFY_ARE_EQUAL(inputFloat, outputFloat);
    }
  }
}

TEST_F(ExecutionTest, BarycentricsTest) {
    WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
    CComPtr<IStream> pStream;
    ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

    CComPtr<ID3D12Device> pDevice;
    if (!CreateDevice(&pDevice, D3D_SHADER_MODEL_6_1))
        return;

    if (!DoesDeviceSupportBarycentrics(pDevice)) {
      WEX::Logging::Log::Comment(L"Device does not support barycentrics.");
      WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
      return;
    }

    std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(pDevice, m_support, pStream, "Barycentrics", nullptr);
    MappedData data;
    D3D12_RESOURCE_DESC &D = test->ShaderOp->GetResourceByName("RTarget")->Desc;
    UINT width = (UINT)D.Width;
    UINT height = D.Height;
    UINT pixelSize = GetByteSizeForFormat(D.Format);

    test->Test->GetReadBackData("RTarget", &data);
    //const uint8_t *pPixels = (uint8_t *)data.data();
    const float *pPixels = (float *)data.data();
    // Get the vertex of barycentric coordinate using VBuffer
    MappedData triangleData;
    test->Test->GetReadBackData("VBuffer", &triangleData);
    const float *pTriangleData = (float*)triangleData.data();
    // get the size of the input data
    unsigned triangleVertexSizeInFloat = 0;
    for (auto element : test->ShaderOp->InputElements)
        triangleVertexSizeInFloat += GetByteSizeForFormat(element.Format) / 4;

    XMFLOAT2 p0(pTriangleData[0], pTriangleData[1]);
    XMFLOAT2 p1(pTriangleData[triangleVertexSizeInFloat], pTriangleData[triangleVertexSizeInFloat + 1]);
    XMFLOAT2 p2(pTriangleData[triangleVertexSizeInFloat * 2], pTriangleData[triangleVertexSizeInFloat * 2 + 1]);

    XMFLOAT3 barycentricWeights[4] = {
        XMFLOAT3(0.3333f, 0.3333f, 0.3333f),
        XMFLOAT3(0.5f, 0.25f, 0.25f),
        XMFLOAT3(0.25f, 0.5f, 0.25f),
        XMFLOAT3(0.25f, 0.25f, 0.50f)
    };

    float tolerance = 0.001f;
    for (unsigned i = 0; i < sizeof(barycentricWeights) / sizeof(XMFLOAT3); ++i) {
        float w0 = barycentricWeights[i].x;
        float w1 = barycentricWeights[i].y;
        float w2 = barycentricWeights[i].z;
        float x1 = w0 * p0.x + w1 * p1.x + w2 * p2.x;
        float y1 = w0 * p0.y + w1 * p1.y + w2 * p2.y;
        // map from x1 y1 to rtv pixels
        int pixelX = (int)((x1 + 1) * (width - 1) / 2);
        int pixelY = (int)((1 - y1) * (height - 1) / 2);
        int offset = pixelSize * (pixelX + pixelY * width) / sizeof(pPixels[0]);
        LogCommentFmt(L"location  %u %u, value %f, %f, %f", pixelX, pixelY, pPixels[offset], pPixels[offset + 1], pPixels[offset + 2]);
        VERIFY_IS_TRUE(CompareFloatEpsilon(pPixels[offset], w0, tolerance));
        VERIFY_IS_TRUE(CompareFloatEpsilon(pPixels[offset + 1], w1, tolerance));
        VERIFY_IS_TRUE(CompareFloatEpsilon(pPixels[offset + 2], w2, tolerance));
    }
    //SavePixelsToFile(pPixels, DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, L"barycentric.bmp");
}

static const char RawBufferTestShaderDeclarations[] =
"// Note: COMPONENT_TYPE and COMPONENT_SIZE will be defined via compiler option -D\r\n"
"typedef COMPONENT_TYPE scalar; \r\n"
"typedef vector<COMPONENT_TYPE, 2> vector2; \r\n"
"typedef vector<COMPONENT_TYPE, 3> vector3; \r\n"
"typedef vector<COMPONENT_TYPE, 4> vector4; \r\n"
"\r\n"
"struct TestData { \r\n"
"  scalar  v1; \r\n"
"  vector2 v2; \r\n"
"  vector3 v3; \r\n"
"  vector4 v4; \r\n"
"}; \r\n"
"\r\n"
"struct UavData {\r\n"
"  TestData input; \r\n"
"  TestData output; \r\n"
"  TestData srvOut; \r\n"
"}; \r\n"
"\r\n"
"ByteAddressBuffer           srv0 : register(t0); \r\n"
"StructuredBuffer<TestData>  srv1 : register(t1); \r\n"
"ByteAddressBuffer           srv2 : register(t2); \r\n"
"StructuredBuffer<TestData>  srv3 : register(t3); \r\n"
"\r\n"
"RWByteAddressBuffer         uav0 : register(u0); \r\n"
"RWStructuredBuffer<UavData> uav1 : register(u1); \r\n"
"RWByteAddressBuffer         uav2 : register(u2); \r\n"
"RWStructuredBuffer<UavData> uav3 : register(u3); \r\n";

static const char RawBufferTestShaderBody[] =
"  // offset of 'out' in 'UavData'\r\n"
"  const int out_offset = COMPONENT_SIZE * 10; \r\n"
"\r\n"
"  // offset of 'srv_out' in 'UavData'\r\n"
"  const int srv_out_offset = COMPONENT_SIZE * 10 * 2; \r\n"
"\r\n"
"  // offsets within the 'Data' struct\r\n"
"  const int v1_offset = 0; \r\n"
"  const int v2_offset = COMPONENT_SIZE; \r\n"
"  const int v3_offset = COMPONENT_SIZE * 3; \r\n"
"  const int v4_offset = COMPONENT_SIZE * 6; \r\n"
"\r\n"
"  uav0.Store(srv_out_offset + v1_offset, srv0.Load<scalar>(v1_offset)); \r\n"
"  uav0.Store(srv_out_offset + v2_offset, srv0.Load<vector2>(v2_offset)); \r\n"
"  uav0.Store(srv_out_offset + v3_offset, srv0.Load<vector3>(v3_offset)); \r\n"
"  uav0.Store(srv_out_offset + v4_offset, srv0.Load<vector4>(v4_offset)); \r\n"
"\r\n"
"  uav1[0].srvOut.v1 = srv1[0].v1; \r\n"
"  uav1[0].srvOut.v2 = srv1[0].v2; \r\n"
"  uav1[0].srvOut.v3 = srv1[0].v3; \r\n"
"  uav1[0].srvOut.v4 = srv1[0].v4; \r\n"
"\r\n"
"  uav2.Store(srv_out_offset + v1_offset, srv2.Load<scalar>(v1_offset)); \r\n"
"  uav2.Store(srv_out_offset + v2_offset, srv2.Load<vector2>(v2_offset)); \r\n"
"  uav2.Store(srv_out_offset + v3_offset, srv2.Load<vector3>(v3_offset)); \r\n"
"  uav2.Store(srv_out_offset + v4_offset, srv2.Load<vector4>(v4_offset)); \r\n"
"\r\n"
"  uav3[0].srvOut.v1 = srv3[0].v1; \r\n"
"  uav3[0].srvOut.v2 = srv3[0].v2; \r\n"
"  uav3[0].srvOut.v3 = srv3[0].v3; \r\n"
"  uav3[0].srvOut.v4 = srv3[0].v4; \r\n"
"\r\n"
"  uav0.Store(out_offset + v1_offset, uav0.Load<scalar>(v1_offset)); \r\n"
"  uav0.Store(out_offset + v2_offset, uav0.Load<vector2>(v2_offset)); \r\n"
"  uav0.Store(out_offset + v3_offset, uav0.Load<vector3>(v3_offset)); \r\n"
"  uav0.Store(out_offset + v4_offset, uav0.Load<vector4>(v4_offset)); \r\n"
"\r\n"
"  uav1[0].output.v1 = uav1[0].input.v1; \r\n"
"  uav1[0].output.v2 = uav1[0].input.v2; \r\n"
"  uav1[0].output.v3 = uav1[0].input.v3; \r\n"
"  uav1[0].output.v4 = uav1[0].input.v4; \r\n"
"\r\n"
"  uav2.Store(out_offset + v1_offset, uav2.Load<scalar>(v1_offset)); \r\n"
"  uav2.Store(out_offset + v2_offset, uav2.Load<vector2>(v2_offset)); \r\n"
"  uav2.Store(out_offset + v3_offset, uav2.Load<vector3>(v3_offset)); \r\n"
"  uav2.Store(out_offset + v4_offset, uav2.Load<vector4>(v4_offset)); \r\n"
"\r\n"
"  uav3[0].output.v1 = uav3[0].input.v1; \r\n"
"  uav3[0].output.v2 = uav3[0].input.v2; \r\n"
"  uav3[0].output.v3 = uav3[0].input.v3; \r\n"
"  uav3[0].output.v4 = uav3[0].input.v4; \r\n";


static const char RawBufferTestComputeShaderTemplate[] =
"%s\r\n" // <- RawBufferTestShaderDeclarations
"[numthreads(1, 1, 1)]\r\n"
"void main(uint GI : SV_GroupIndex) {\r\n"
"%s\r\n" // <- RawBufferTestShaderBody
"};";

static const char RawBufferTestGraphicsPixelShaderTemplate[] =
"%s\r\n" // <- RawBufferTestShaderDeclarations
"struct PSInput { \r\n"
"  float4 pos : SV_POSITION; \r\n"
"}; \r\n"
"uint4 main(PSInput input) : SV_TARGET{ \r\n"
"  if (input.pos.x + input.pos.y == 1.0f) { // pixel { 0.5, 0.5, 0 } \r\n"
"%s\r\n" // <- RawBufferTestShaderBody
"  } \r\n"
"  return uint4(1, 2, 3, 4); \r\n"
"};";

TEST_F(ExecutionTest, ComputeRawBufferLdStI32) {
  RawBufferLdStTestData<int32_t> data = { { 1 }, { 2, -1 }, { 256, -10517, 980 }, { 465, 13, -89, MAXUINT32 / 2 } };
  RunComputeRawBufferLdStTest<int32_t>(D3D_SHADER_MODEL_6_2, RawBufferLdStType::I32, "ComputeRawBufferLdSt32Bit", data);
}

TEST_F(ExecutionTest, ComputeRawBufferLdStFloat)  {
  RawBufferLdStTestData<float> data = { { 3e-10f }, { 1.5f, -1.99988f }, { 256.0f, -105.17f, 980.0f }, { 465.1652f, -1.5694e2f, -0.8543e-2f, 1333.5f } };
  RunComputeRawBufferLdStTest<float>(D3D_SHADER_MODEL_6_2, RawBufferLdStType::Float, "ComputeRawBufferLdSt32Bit", data);
}

TEST_F(ExecutionTest,  ComputeRawBufferLdStI64)  {
  RawBufferLdStTestData<int64_t> data = { { 1 }, { 2, -1 }, { 256, -105171532, 980 }, { 465, 13, -89, MAXUINT64 / 2 } };
  RunComputeRawBufferLdStTest<int64_t>(D3D_SHADER_MODEL_6_3, RawBufferLdStType::I64, "ComputeRawBufferLdSt64Bit", data);
}

TEST_F(ExecutionTest,  ComputeRawBufferLdStDouble)  {
  RawBufferLdStTestData<double> data = { { 3e-10 }, { 1.5, -1.99988 }, { 256.0, -105.17, 980.0 }, { 465.1652, -1.5694e2, -0.8543e-2, 1333.5 } };
  RunComputeRawBufferLdStTest<double>(D3D_SHADER_MODEL_6_3, RawBufferLdStType::I64, "ComputeRawBufferLdSt64Bit", data);
}

TEST_F(ExecutionTest, ComputeRawBufferLdStI16) {
  RawBufferLdStTestData<int16_t> data = { { 1 }, { 2, -1 }, { 256, -10517, 980 }, { 465, 13, -89, MAXUINT16 / 2 } };
  RunComputeRawBufferLdStTest<int16_t>(D3D_SHADER_MODEL_6_2, RawBufferLdStType::I16, "ComputeRawBufferLdSt16Bit", data);
}

TEST_F(ExecutionTest,  ComputeRawBufferLdStHalf)  {
  RawBufferLdStTestData<float> floatData = { { 3e-10f }, { 1.5f, -1.99988f }, { 256.0f, 105.17f, 980.0f }, { 465.1652f, -1.5694e2f, -0.8543e-2f, 1333.5f } };
  RawBufferLdStTestData<uint16_t> halfData;
  for (int i = 0; i < sizeof(floatData)/sizeof(float); i++) {
    ((uint16_t*)&halfData)[i] = ConvertFloat32ToFloat16(((float*)&floatData)[i]);
  }
  RunComputeRawBufferLdStTest<uint16_t>(D3D_SHADER_MODEL_6_2, RawBufferLdStType::Half, "ComputeRawBufferLdSt16Bit", halfData);
}

TEST_F(ExecutionTest,  GraphicsRawBufferLdStI32)  {
  RawBufferLdStTestData<int32_t> data = { { 1 }, { 2, -1 }, { 256, -10517, 980 }, { 465, 13, -89, MAXUINT32 / 2 } };
  RunGraphicsRawBufferLdStTest<int32_t>(D3D_SHADER_MODEL_6_2, RawBufferLdStType::I32, "GraphicsRawBufferLdSt32Bit", data);
}

TEST_F(ExecutionTest,  GraphicsRawBufferLdStFloat)  {
  RawBufferLdStTestData<float> data = { { 3e-10f }, { 1.5f, -1.99988f }, { 256.0f, -105.17f, 980.0f }, { 465.1652f, -1.5694e2f, -0.8543e-2f, 1333.5f } };
  RunGraphicsRawBufferLdStTest<float>(D3D_SHADER_MODEL_6_2, RawBufferLdStType::Float, "GraphicsRawBufferLdSt32Bit", data);
}

TEST_F(ExecutionTest,  GraphicsRawBufferLdStI64)  {
  RawBufferLdStTestData<int64_t> data = { { 1 }, { 2, -1 }, { 256, -105171532, 980 }, { 465, 13, -89, MAXUINT64 / 2 } };
  RunGraphicsRawBufferLdStTest<int64_t>(D3D_SHADER_MODEL_6_3, RawBufferLdStType::I64, "GraphicsRawBufferLdSt64Bit", data);
}

TEST_F(ExecutionTest,  GraphicsRawBufferLdStDouble)  {
  RawBufferLdStTestData<double> data = { { 3e-10 }, { 1.5, -1.99988 }, { 256.0, -105.17, 980.0 }, { 465.1652, -1.5694e2, -0.8543e-2, 1333.5 } };
  RunGraphicsRawBufferLdStTest<double>(D3D_SHADER_MODEL_6_3, RawBufferLdStType::Double, "GraphicsRawBufferLdSt64Bit", data);
}

TEST_F(ExecutionTest, GraphicsRawBufferLdStI16) {
  RawBufferLdStTestData<int16_t> data = { { 1 }, { 2, -1 }, { 256, -10517, 980 }, { 465, 13, -89, MAXUINT16 / 2 } };
  RunGraphicsRawBufferLdStTest<int16_t>(D3D_SHADER_MODEL_6_2, RawBufferLdStType::I16, "GraphicsRawBufferLdSt16Bit", data);
}

TEST_F(ExecutionTest, GraphicsRawBufferLdStHalf) {
  RawBufferLdStTestData<float> floatData = { { 3e-10f }, { 1.5f, -1.99988f }, { 256.0f, 105.17f, 0.0f }, { 465.1652f, -1.5694e2f, -0.8543e-2f, 1333.5f } };
  RawBufferLdStTestData<uint16_t> halfData;
  for (int i = 0; i < sizeof(floatData) / sizeof(float); i++) {
    ((uint16_t*)&halfData)[i] = ConvertFloat32ToFloat16(((float*)&floatData)[i]);
  }
  RunGraphicsRawBufferLdStTest<uint16_t>(D3D_SHADER_MODEL_6_2, RawBufferLdStType::Half, "GraphicsRawBufferLdSt16Bit", halfData);
}

bool ExecutionTest::SetupRawBufferLdStTest(D3D_SHADER_MODEL shaderModel, RawBufferLdStType dataType,
                                           CComPtr<ID3D12Device> &pDevice, CComPtr<IStream> &pStream, 
                                           char *&sTy, char *&additionalOptions) {
  if (!CreateDevice(&pDevice, shaderModel)) {
    return false;
  }

  additionalOptions = "";

  switch (dataType) {
  case RawBufferLdStType::I64:
    if (!DoesDeviceSupportInt64(pDevice)) {
      WEX::Logging::Log::Comment(L"Device does not support int64 operations.");
      WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
      return false;
    }
    sTy = "int64_t";
    break;
  case RawBufferLdStType::Double:
    if (!DoesDeviceSupportDouble(pDevice)) {
      WEX::Logging::Log::Comment(L"Device does not support double operations.");
      WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
      return false;
    }
    sTy = "double";
    break;
  case RawBufferLdStType::I16:
  case RawBufferLdStType::Half:
    if (!DoesDeviceSupportNative16bitOps(pDevice)) {
      WEX::Logging::Log::Comment(L"Device does not support native 16-bit operations.");
      WEX::Logging::Log::Result(WEX::Logging::TestResults::Skipped);
      return false;
    }
    additionalOptions = "-enable-16bit-types";
    sTy = (dataType == RawBufferLdStType::I16 ? "int16_t" : "half");
    break;
  case RawBufferLdStType::I32:
    sTy = "int32_t";
    break;
  case RawBufferLdStType::Float:
    sTy = "float";
    break;
  default:
    DXASSERT_NOMSG("Invalid RawBufferLdStType");
  }

  // read shader config
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  return true;
}

template <class Ty>
void ExecutionTest::VerifyRawBufferLdStTestResults(const std::shared_ptr<st::ShaderOpTest> test, const RawBufferLdStTestData<Ty> &testData) {
  // read buffers back & verify expected values
  static const int UavBufferCount = 4;
  char bufferName[11] = "UAVBufferX";

  for (unsigned i = 0; i < UavBufferCount; i++) {
    MappedData dataUav;
    RawBufferLdStUavData<Ty> *pOutData;

    bufferName[sizeof(bufferName) - 2] = (char)(i + '0');

    test->GetReadBackData(bufferName, &dataUav);
    VERIFY_ARE_EQUAL(sizeof(RawBufferLdStUavData<Ty>), dataUav.size());
    pOutData = (RawBufferLdStUavData<Ty> *)dataUav.data();

    LogCommentFmt(L"Verifying UAVBuffer%d Load -> UAVBuffer%d Store", i, i);
    // scalar
    VERIFY_ARE_EQUAL(pOutData->output.v1, testData.v1);
    // vector 2
    VERIFY_ARE_EQUAL(pOutData->output.v2[0], testData.v2[0]);
    VERIFY_ARE_EQUAL(pOutData->output.v2[1], testData.v2[1]);
    // vector 3
    VERIFY_ARE_EQUAL(pOutData->output.v3[0], testData.v3[0]);
    VERIFY_ARE_EQUAL(pOutData->output.v3[1], testData.v3[1]);
    VERIFY_ARE_EQUAL(pOutData->output.v3[2], testData.v3[2]);
    // vector 4
    VERIFY_ARE_EQUAL(pOutData->output.v4[0], testData.v4[0]);
    VERIFY_ARE_EQUAL(pOutData->output.v4[1], testData.v4[1]);
    VERIFY_ARE_EQUAL(pOutData->output.v4[2], testData.v4[2]);
    VERIFY_ARE_EQUAL(pOutData->output.v4[3], testData.v4[3]);

    // verify SRV Store
    LogCommentFmt(L"Verifying SRVBuffer%d Load -> UAVBuffer%d Store", i, i);
    // scalar
    VERIFY_ARE_EQUAL(pOutData->srvOut.v1, testData.v1);
    // vector 2
    VERIFY_ARE_EQUAL(pOutData->srvOut.v2[0], testData.v2[0]);
    VERIFY_ARE_EQUAL(pOutData->srvOut.v2[1], testData.v2[1]);
    // vector 3
    VERIFY_ARE_EQUAL(pOutData->srvOut.v3[0], testData.v3[0]);
    VERIFY_ARE_EQUAL(pOutData->srvOut.v3[1], testData.v3[1]);
    VERIFY_ARE_EQUAL(pOutData->srvOut.v3[2], testData.v3[2]);
    // vector 4
    VERIFY_ARE_EQUAL(pOutData->srvOut.v4[0], testData.v4[0]);
    VERIFY_ARE_EQUAL(pOutData->srvOut.v4[1], testData.v4[1]);
    VERIFY_ARE_EQUAL(pOutData->srvOut.v4[2], testData.v4[2]);
    VERIFY_ARE_EQUAL(pOutData->srvOut.v4[3], testData.v4[3]);
  }
}

template <class Ty>
void ExecutionTest::RunComputeRawBufferLdStTest(D3D_SHADER_MODEL shaderModel, RawBufferLdStType dataType, 
                                                const char *shaderOpName, const RawBufferLdStTestData<Ty> &testData) {
   WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

   CComPtr<ID3D12Device> pDevice;
   CComPtr<IStream> pStream;
   char *sTy, *additionalOptions;

   if (!SetupRawBufferLdStTest(shaderModel, dataType, pDevice, pStream, sTy, additionalOptions)) {
     return;
   }

   // format shader source
   char rawBufferTestShaderText[sizeof(RawBufferTestComputeShaderTemplate) + sizeof(RawBufferTestShaderDeclarations) + sizeof(RawBufferTestShaderBody)];
   VERIFY_IS_TRUE(sprintf_s(rawBufferTestShaderText, sizeof(rawBufferTestShaderText), 
                            RawBufferTestComputeShaderTemplate, RawBufferTestShaderDeclarations, RawBufferTestShaderBody) != -1);

   // format compiler args
   char compilerOptions[256];
   VERIFY_IS_TRUE(sprintf_s(compilerOptions, sizeof(compilerOptions), "-D COMPONENT_TYPE=%s -D COMPONENT_SIZE=%d %s", sTy, (int)sizeof(Ty), additionalOptions) != -1);

   // run the shader
   std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(pDevice, m_support, pStream, shaderOpName,
     [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
     VERIFY_IS_TRUE(((0 == strncmp(Name, "SRVBuffer", 9)) || (0 == strncmp(Name, "UAVBuffer", 9))) &&
                    (Name[9] >= '0' && Name[9] <= '3'));
     pShaderOp->Shaders.at(0).Arguments = compilerOptions;
     pShaderOp->Shaders.at(0).Text = rawBufferTestShaderText;

     VERIFY_IS_TRUE(sizeof(RawBufferLdStTestData<Ty>) <= Data.size());
     RawBufferLdStTestData<Ty> *pInData = (RawBufferLdStTestData<Ty>*)Data.data();
     memcpy(pInData, &testData, sizeof(RawBufferLdStTestData<Ty>));
   });

   // verify expected values
   VerifyRawBufferLdStTestResults<Ty>(test->Test, testData);
}

template <class Ty>
void ExecutionTest::RunGraphicsRawBufferLdStTest(D3D_SHADER_MODEL shaderModel, RawBufferLdStType dataType,
                                                 const char *shaderOpName, const RawBufferLdStTestData<Ty> &testData) {

  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

  CComPtr<ID3D12Device> pDevice;
  CComPtr<IStream> pStream;
  char *sTy, *additionalOptions;

  if (!SetupRawBufferLdStTest(shaderModel, dataType, pDevice, pStream, sTy, additionalOptions)) {
    return;
  }

  // format shader source
  char rawBufferTestPixelShaderText[sizeof(RawBufferTestGraphicsPixelShaderTemplate) + sizeof(RawBufferTestShaderDeclarations) + sizeof(RawBufferTestShaderBody)];
  VERIFY_IS_TRUE(sprintf_s(rawBufferTestPixelShaderText, sizeof(rawBufferTestPixelShaderText),
                           RawBufferTestGraphicsPixelShaderTemplate, RawBufferTestShaderDeclarations, RawBufferTestShaderBody) != -1);

  // format compiler args
  char compilerOptions[256];
  VERIFY_IS_TRUE(sprintf_s(compilerOptions, sizeof(compilerOptions), "-D COMPONENT_TYPE=%s -D COMPONENT_SIZE=%d %s", sTy, (int)sizeof(Ty), additionalOptions) != -1);

  // run the shader
  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(pDevice, m_support, pStream, shaderOpName,
    [&](LPCSTR Name, std::vector<BYTE> &Data, st::ShaderOp *pShaderOp) {
    VERIFY_IS_TRUE(((0 == strncmp(Name, "SRVBuffer", 9)) || (0 == strncmp(Name, "UAVBuffer", 9))) &&
      (Name[9] >= '0' && Name[9] <= '3'));
    // pixel shader is at index 1, vertex shader at index 0
    pShaderOp->Shaders.at(1).Arguments = compilerOptions;
    pShaderOp->Shaders.at(1).Text = rawBufferTestPixelShaderText;

    VERIFY_IS_TRUE(sizeof(RawBufferLdStTestData<Ty>) <= Data.size());
    RawBufferLdStTestData<Ty> *pInData = (RawBufferLdStTestData<Ty>*)Data.data();
    memcpy(pInData, &testData, sizeof(RawBufferLdStTestData<Ty>));
  });

  // verify expected values
  VerifyRawBufferLdStTestResults<Ty>(test->Test, testData);
}

#ifndef _HLK_CONF
static void WriteReadBackDump(st::ShaderOp *pShaderOp, st::ShaderOpTest *pTest,
                              char **pReadBackDump) {
  std::stringstream str;

  unsigned count = 0;
  for (auto &R : pShaderOp->Resources) {
    if (!R.ReadBack)
      continue;
    ++count;
    str << "Resource: " << R.Name << "\r\n";
    // Find a descriptor that can tell us how to dump this resource.
    bool found = false;
    for (auto &Heaps : pShaderOp->DescriptorHeaps) {
      for (auto &D : Heaps.Descriptors) {
        if (_stricmp(D.ResName, R.Name) != 0) {
          continue;
        }
        found = true;
        if (_stricmp(D.Kind, "UAV") != 0) {
          str << "Resource dump for kind " << D.Kind << " not implemented yet.\r\n";
          break;
        }
        if (D.UavDesc.ViewDimension != D3D12_UAV_DIMENSION_BUFFER) {
          str << "Resource dump for this kind of view dimension not implemented yet.\r\n";
          break;
        }
        // We can map back to the structure if a structured buffer via the shader, but
        // we'll keep this simple and simply dump out 32-bit uint/float representations.
        MappedData data;
        pTest->GetReadBackData(R.Name, &data);
        uint32_t *pData = (uint32_t *)data.data();
        size_t u32_count = R.Desc.Width / sizeof(uint32_t);
        for (size_t i = 0; i < u32_count; ++i) {
          float f = *(float *)pData;
          str << i << ": 0n" << *pData << "   0x" << std::hex << *pData
              << std::dec << "   " << f << "\r\n";
          ++pData;
        }
        break;
      }
      if (found) break;
    }
    if (!found) {
      str << "Unable to find a view for the resource.\r\n";
    }
  }

  str << "Resources read back: " << count << "\r\n";

  std::string s(str.str());
  CComHeapPtr<char> pDump;
  if (!pDump.Allocate(s.size() + 1))
    throw std::bad_alloc();
  memcpy(pDump.m_pData, s.data(), s.size());
  pDump.m_pData[s.size()] = '\0';
  *pReadBackDump = pDump.Detach();
}

// This is the exported interface by use from HLSLHost.exe.
// It's exclusive with the use of the DLL as a TAEF target.
extern "C" {
  __declspec(dllexport) HRESULT WINAPI InitializeOpTests(void *pStrCtx, st::OutputStringFn pOutputStrFn) {
    HRESULT hr = EnableExperimentalShaderModels();
    if (FAILED(hr)) {
      pOutputStrFn(pStrCtx, L"Unable to enable experimental shader models.\r\n.");
    }
    return S_OK;
  }

  __declspec(dllexport) HRESULT WINAPI
      RunOpTest(void *pStrCtx, st::OutputStringFn pOutputStrFn, LPCSTR pText,
                ID3D12Device *pDevice, ID3D12CommandQueue *pCommandQueue,
                ID3D12Resource *pRenderTarget, char **pReadBackDump) {

    HRESULT hr;
    if (pReadBackDump) *pReadBackDump = nullptr;
    st::SetOutputFn(pStrCtx, pOutputStrFn);
    CComPtr<ID3D12InfoQueue> pInfoQueue;
    CComHeapPtr<char> pDump;
    bool FilterCreation = false;
    if (SUCCEEDED(pDevice->QueryInterface(&pInfoQueue))) {
      // Creation is largely driven by inputs, so don't log create/destroy messages.
      pInfoQueue->PushEmptyStorageFilter();
      pInfoQueue->PushEmptyRetrievalFilter();
      if (FilterCreation) {
        D3D12_INFO_QUEUE_FILTER filter;
        D3D12_MESSAGE_CATEGORY denyCategories[] = { D3D12_MESSAGE_CATEGORY_STATE_CREATION };
        ZeroMemory(&filter, sizeof(filter));
        filter.DenyList.NumCategories = _countof(denyCategories);
        filter.DenyList.pCategoryList = denyCategories;
        pInfoQueue->PushStorageFilter(&filter);
      }
    }
    else {
      pOutputStrFn(pStrCtx, L"Unable to enable info queue for D3D.\r\n.");
    }
    try {
      dxc::DxcDllSupport m_support;
      m_support.Initialize();

      const char *pName = nullptr;
      CComPtr<IStream> pStream = SHCreateMemStream((BYTE *)pText, strlen(pText));
      std::shared_ptr<st::ShaderOpSet> ShaderOpSet =
        std::make_shared<st::ShaderOpSet>();
      st::ParseShaderOpSetFromStream(pStream, ShaderOpSet.get());
      st::ShaderOp *pShaderOp;
      if (pName == nullptr) {
        if (ShaderOpSet->ShaderOps.size() != 1) {
          pOutputStrFn(pStrCtx, L"Expected a single shader operation.\r\n");
          return E_FAIL;
        }
        pShaderOp = ShaderOpSet->ShaderOps[0].get();
      }
      else {
        pShaderOp = ShaderOpSet->GetShaderOp(pName);
      }
      if (pShaderOp == nullptr) {
        std::string msg = "Unable to find shader op ";
        msg += pName;
        msg += "; available ops";
        const char sep = ':';
        for (auto &pAvailOp : ShaderOpSet->ShaderOps) {
          msg += sep;
          msg += pAvailOp->Name ? pAvailOp->Name : "[n/a]";
        }
        CA2W msgWide(msg.c_str());
        pOutputStrFn(pStrCtx, msgWide);
        return E_FAIL;
      }

      std::shared_ptr<st::ShaderOpTest> test = std::make_shared<st::ShaderOpTest>();
      test->SetupRenderTarget(pShaderOp, pDevice, pCommandQueue, pRenderTarget);
      test->SetDxcSupport(&m_support);
      test->RunShaderOp(pShaderOp);
      test->PresentRenderTarget(pShaderOp, pCommandQueue, pRenderTarget);

      pOutputStrFn(pStrCtx, L"Rendering complete.\r\n");

      if (!pShaderOp->IsCompute()) {
        D3D12_QUERY_DATA_PIPELINE_STATISTICS stats;
        test->GetPipelineStats(&stats);
        wchar_t statsText[400];
        StringCchPrintfW(statsText, _countof(statsText),
          L"Vertices/primitives read by input assembler: %I64u/%I64u\r\n"
          L"Vertex shader invocations: %I64u\r\n"
          L"Geometry shader invocations/output primitive: %I64u/%I64u\r\n"
          L"Primitives sent to rasterizer/rendered: %I64u/%I64u\r\n"
          L"PS/HS/DS/CS invocations: %I64u/%I64u/%I64u/%I64u\r\n",
          stats.IAVertices, stats.IAPrimitives, stats.VSInvocations,
          stats.GSInvocations, stats.GSPrimitives, stats.CInvocations,
          stats.CPrimitives, stats.PSInvocations, stats.HSInvocations,
          stats.DSInvocations, stats.CSInvocations);
        pOutputStrFn(pStrCtx, statsText);
      }

      if (pReadBackDump) {
        WriteReadBackDump(pShaderOp, test.get(), &pDump);
      }

      hr = S_OK;
    }
    catch (const CAtlException &E)
    {
      hr = E.m_hr;
    }
    catch (const std::bad_alloc &)
    {
      hr = E_OUTOFMEMORY;
    }
    catch (const std::exception &)
    {
      hr = E_FAIL;
    }

    // Drain the device message queue if available.
    if (pInfoQueue != nullptr) {
      wchar_t buf[200];
      StringCchPrintfW(buf, _countof(buf),
        L"NumStoredMessages=%u limit/discarded by limit=%u/%u "
        L"allowed/denied by storage filter=%u/%u "
        L"NumStoredMessagesAllowedByRetrievalFilter=%u\r\n",
        (unsigned)pInfoQueue->GetNumStoredMessages(),
        (unsigned)pInfoQueue->GetMessageCountLimit(),
        (unsigned)pInfoQueue->GetNumMessagesDiscardedByMessageCountLimit(),
        (unsigned)pInfoQueue->GetNumMessagesAllowedByStorageFilter(),
        (unsigned)pInfoQueue->GetNumMessagesDeniedByStorageFilter(),
        (unsigned)pInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter());
      pOutputStrFn(pStrCtx, buf);

      WriteInfoQueueMessages(pStrCtx, pOutputStrFn, pInfoQueue);

      pInfoQueue->ClearStoredMessages();
      pInfoQueue->PopRetrievalFilter();
      pInfoQueue->PopStorageFilter();
      if (FilterCreation) {
        pInfoQueue->PopStorageFilter();
      }
    }

    if (pReadBackDump) *pReadBackDump = pDump.Detach();

    return hr;
  }
}
#endif
// MARKER: ExecutionTest/DxilConf Shared Implementation End
// Do not remove the line above - it is used by TranslateExecutionTest.py
