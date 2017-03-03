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
#include "CompilationResult.h"
#include "HLSLTestData.h"
#include <Shlwapi.h>
#include <atlcoll.h>

#undef _read
#include "WexTestClass.h"
#include "HlslTestUtils.h"
#include "DxcTestUtils.h"
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
#include <D3dx12.h>
#include <DirectXMath.h>
#include <strsafe.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include "ShaderOpTest.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dxguid.lib")

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

class ExecutionTest {
public:
  // By default, ignore these tests, which require a recent build to run properly.
  BEGIN_TEST_CLASS(ExecutionTest)
    TEST_CLASS_PROPERTY(L"Ignore", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()
  TEST_CLASS_SETUP(ExecutionTestClassSetup)

  TEST_METHOD(BasicComputeTest);
  TEST_METHOD(BasicTriangleTest);
  TEST_METHOD(BasicTriangleOpTest);
  TEST_METHOD(MinMaxTest);
  TEST_METHOD(OutOfBoundsTest);
  TEST_METHOD(SaturateTest);
  TEST_METHOD(SignTest);
  TEST_METHOD(Int64Test);
  TEST_METHOD(WaveIntrinsicsTest);
  TEST_METHOD(WaveIntrinsicsInPSTest);
  TEST_METHOD(DoShaderOpArithTest);

  dxc::DxcDllSupport m_support;
  bool m_ExperimentalModeEnabled = false;
  static const float ClearColor[4];

  bool UseDxbc() {
    return GetTestParamBool(L"DXBC");
  }

  bool UseDebugIfaces() {
    return true;
  }

  bool SaveImages() {
    return GetTestParamBool(L"SaveImages");
  }

  void CompileFromText(LPCSTR pText, LPCWSTR pEntryPoint, LPCWSTR pTargetProfile, ID3DBlob **ppBlob) {
    VERIFY_SUCCEEDED(m_support.Initialize());
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcLibrary> pLibrary;
    CComPtr<IDxcBlobEncoding> pTextBlob;
    CComPtr<IDxcOperationResult> pResult;
    HRESULT resultCode;
    VERIFY_SUCCEEDED(m_support.CreateInstance(CLSID_DxcCompiler, &pCompiler));
    VERIFY_SUCCEEDED(m_support.CreateInstance(CLSID_DxcLibrary, &pLibrary));
    VERIFY_SUCCEEDED(pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)pText, strlen(pText), CP_UTF8, &pTextBlob));
    VERIFY_SUCCEEDED(pCompiler->Compile(pTextBlob, L"hlsl.hlsl", pEntryPoint, pTargetProfile, nullptr, 0, nullptr, 0, nullptr, &pResult));
    VERIFY_SUCCEEDED(pResult->GetStatus(&resultCode));
    if (FAILED(resultCode)) {
      CComPtr<IDxcBlobEncoding> errors;
      VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&errors));
      LogCommentFmt(L"Failed to compile shader: %s", BlobToUtf16(errors).data());
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
      DXBCFromText(pShader, L"main", L"cs_6_0", &pComputeShader);
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

  bool CreateDevice(_COM_Outptr_ ID3D12Device **ppDevice) {
    const D3D_FEATURE_LEVEL FeatureLevelRequired = D3D_FEATURE_LEVEL_11_0;
    CComPtr<IDXGIFactory4> factory;
    CComPtr<ID3D12Device> pDevice;

    *ppDevice = nullptr;

    VERIFY_SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
    if (GetTestParamUseWARP(true)) {
      CComPtr<IDXGIAdapter> warpAdapter;
      VERIFY_SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
      HRESULT createHR = D3D12CreateDevice(warpAdapter, FeatureLevelRequired,
                                           IID_PPV_ARGS(&pDevice));
      if (FAILED(createHR)) {
        LogCommentFmt(L"The available version of WARP does not support d3d12.");
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Blocked);
        return false;
      }
    } else {
      CComPtr<IDXGIAdapter1> hardwareAdapter;
      WEX::Common::String AdapterValue;
      IFT(WEX::TestExecution::RuntimeParameters::TryGetValue(L"Adapter",
                                                             AdapterValue));
      GetHardwareAdapter(factory, AdapterValue, &hardwareAdapter);
      if (hardwareAdapter == nullptr) {
        WEX::Logging::Log::Error(
            L"Unable to find hardware adapter with D3D12 support.");
        return false;
      }
      VERIFY_SUCCEEDED(D3D12CreateDevice(hardwareAdapter, FeatureLevelRequired,
                                         IID_PPV_ARGS(&pDevice)));
    }
    if (pDevice == nullptr)
      return false;

    if (!UseDxbc()) {
      // Check for DXIL support.
      // This is defined in d3d.h for Windows 10 Anniversary Edition SDK, but we only
      // require the Windows 10 SDK.
      typedef enum D3D_SHADER_MODEL {
        D3D_SHADER_MODEL_5_1 = 0x51,
        D3D_SHADER_MODEL_6_0 = 0x60
      } D3D_SHADER_MODEL;
      typedef struct D3D12_FEATURE_DATA_SHADER_MODEL {
        _Inout_ D3D_SHADER_MODEL HighestShaderModel;
      } D3D12_FEATURE_DATA_SHADER_MODEL;
      const UINT D3D12_FEATURE_SHADER_MODEL = 7;
      D3D12_FEATURE_DATA_SHADER_MODEL SMData;
      SMData.HighestShaderModel = D3D_SHADER_MODEL_6_0;
      VERIFY_SUCCEEDED(pDevice->CheckFeatureSupport(
        (D3D12_FEATURE)D3D12_FEATURE_SHADER_MODEL, &SMData, sizeof(SMData)));
      if (SMData.HighestShaderModel != D3D_SHADER_MODEL_6_0) {
        LogCommentFmt(L"The selected device does not support "
                      L"shader model 6 (required for DXIL).");
        WEX::Logging::Log::Result(WEX::Logging::TestResults::Blocked);
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
      DXBCFromText(pShaders, L"VSMain", L"vs_6_0", &vertexShader);
      DXBCFromText(pShaders, L"PSMain", L"ps_6_0", &pixelShader);
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
    pVertexBufferView->SizeInBytes = vertexBufferSize;

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

  bool DoesDeviceSupportWaveOps(ID3D12Device *pDevice) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 O;
    if (FAILED(pDevice->CheckFeatureSupport((D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &O, sizeof(O))))
      return false;
    return O.WaveOps != FALSE;
  }

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

  HRESULT EnableExperimentalMode() {
    if (m_ExperimentalModeEnabled) {
      return S_OK;
    }
    if (!GetTestParamBool(L"ExperimentalShaders")) {
      return S_OK;
    }
    HRESULT hr = EnableExperimentalShaderModels();
    if (SUCCEEDED(hr)) {
      m_ExperimentalModeEnabled = true;
    }
    return hr;
  }

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
    viewport.Height = rtDesc.Height;
    viewport.Width = rtDesc.Width;
    viewport.MaxDepth = 1.0f;
    memset(&scissorRect, 0, sizeof(scissorRect));
    scissorRect.right = rtDesc.Width;
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
    UINT rowPitch = rtDesc.Width * 4;
    if (rowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
      rowPitch += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - (rowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint;
    Footprint.Offset = 0;
    Footprint.Footprint = CD3DX12_SUBRESOURCE_FOOTPRINT(DXGI_FORMAT_R8G8B8A8_UNORM, rtDesc.Width, rtDesc.Height, 1, rowPitch);
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

const float ExecutionTest::ClearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };

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


static void SetupComputeValuePattern(std::vector<uint32_t> &values, size_t count) {
  values.resize(count); // one element per dispatch group, in bytes
  for (size_t i = 0; i < count; ++i) {
    values[i] = i;
  }
}

bool ExecutionTest::ExecutionTestClassSetup() {
  HRESULT hr = EnableExperimentalMode();
  if (FAILED(hr)) {
    LogCommentFmt(L"Unable to enable shader experimental mode - 0x%08x.", hr);
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

  const size_t valueSizeInBytes = values.size() * sizeof(uint32_t);
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
    uavDesc.Buffer.NumElements = values.size();
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
    memcpy(values.data(), pData, valueSizeInBytes);
  }
  WaitForSignal(pCommandQueue, FO);
}

TEST_F(ExecutionTest, BasicComputeTest) {
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
  static const int NumtheadsX = 8;
  static const int NumtheadsY = 8;
  static const int NumtheadsZ = 1;
  static const int ThreadsPerGroup = NumtheadsX * NumtheadsY * NumtheadsZ;
  static const int DispatchGroupCount = 1;

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;

  std::vector<uint32_t> values;
  SetupComputeValuePattern(values, ThreadsPerGroup * DispatchGroupCount);
  VERIFY_ARE_EQUAL(values[0], 0);
  RunRWByteBufferComputeTest(pDevice, pShader, values);
  VERIFY_ARE_EQUAL(values[0], 1);
}

TEST_F(ExecutionTest, BasicTriangleTest) {
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
  static const int NumtheadsX = 8;
  static const int NumtheadsY = 8;
  static const int NumtheadsZ = 1;
  static const int ThreadsPerGroup = NumtheadsX * NumtheadsY * NumtheadsZ;
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
  VERIFY_ARE_EQUAL(values[0], 0);
  RunRWByteBufferComputeTest(pDevice, pShader, values);
  VERIFY_ARE_EQUAL(values[0], 0);
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
  static const int NumtheadsX = 8;
  static const int NumtheadsY = 1;
  static const int NumtheadsZ = 1;
  static const int ThreadsPerGroup = NumtheadsX * NumtheadsY * NumtheadsZ;
  static const int DispatchGroupCount = 1;

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;

  std::vector<uint32_t> values = { (uint32_t)-3, (uint32_t)-2, (uint32_t)-1, 0, 1, 2, 3, 4};
  RunRWByteBufferComputeTest(pDevice, pShader, values);
  VERIFY_ARE_EQUAL(values[0], -1);
  VERIFY_ARE_EQUAL(values[1], -1);
  VERIFY_ARE_EQUAL(values[2], -1);
  VERIFY_ARE_EQUAL(values[3], 0);
  VERIFY_ARE_EQUAL(values[4], 1);
  VERIFY_ARE_EQUAL(values[5], 1);
  VERIFY_ARE_EQUAL(values[6], 1);
  VERIFY_ARE_EQUAL(values[7], 1);
}

TEST_F(ExecutionTest, WaveIntrinsicsTest) {
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
}

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
    "typedef uint uint32_t;\r\n"
    "uint pos_to_id(float4 pos) { return pos.x * 128 + pos.y; }\r\n"
    "struct PerPixelData {\r\n"
    " float4 position;\r\n"
    " uint32_t id, flags, laneIndex, laneCount, firstLaneId, sum1;\r\n"
    " uint32_t id0, id1, id2, id3;\r\n"
    " uint32_t acrossX, acrossY, acrossDiag, quadActiveCount;\r\n"
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
  UINT valueSizeInBytes = values.size() * sizeof(PerPixelData);
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
    uavDesc.Buffer.NumElements = values.size();
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
    LogCommentFmt(L"%u elements in append buffer");
  }

  {
    MappedData mappedData(pUavReadBuffer, values.size());
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

    // Multiple threads may have tried to shade the same pixel.
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
            uint32_t quadId = (*i).second->id0;
            auto match = quads.find(quadId);
            if (match == quads.end()) {
              QuadData qdata;
              qdata.count = 1;
              qdata.data[0] = (*i).second;
              quads.insert(std::make_pair(quadId, qdata));
            }
            else {
              VERIFY_IS_TRUE((*match).second.count < 4);
              (*match).second.data[(*match).second.count++] = (*i).second;
            }
          }
          for (auto quadPair : quads) {
            unsigned count = quadPair.second.count;
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

static float g_SinCosFloats[] = {
  -(INFINITY),
  -1.0f,
  -(FLT_MIN/2),
  -0.0f,
  0.0f,
  FLT_MIN / 2,
  1.0f,
  INFINITY,
  NAN
};

std::shared_ptr<ShaderOpTestResult>
RunShaderOpTest(ID3D12Device *pDevice, dxc::DxcDllSupport &support,
                IStream *pStream, LPCSTR pName,
                st::ShaderOpTest::TInitCallbackFn pInitCallback) {
  DXASSERT_NOMSG(pStream != nullptr);
  std::shared_ptr<st::ShaderOpSet> ShaderOpSet =
      std::make_shared<st::ShaderOpSet>();
  st::ParseShaderOpSetFromStream(pStream, ShaderOpSet.get());
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

  std::shared_ptr<st::ShaderOpTest> test = std::make_shared<st::ShaderOpTest>();
  test->SetDxcSupport(&support);
  test->SetInitCallback(pInitCallback);
  test->RunShaderOp(pShaderOp);

  std::shared_ptr<ShaderOpTestResult> result =
      std::make_shared<ShaderOpTestResult>();
  result->ShaderOpSet = ShaderOpSet;
  result->Test = test;
  result->ShaderOp = pShaderOp;
  return result;
}

static bool isdenorm(float f) {
  return FP_SUBNORMAL == fpclassify(f);
}
static bool isdenorm(double d) {
  return FP_SUBNORMAL == fpclassify(d);
}

TEST_F(ExecutionTest, DoShaderOpArithTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;

  // Single operation test at the moment.
  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(pDevice, m_support, pStream, "SinCos",
    [](LPCSTR Name, std::vector<BYTE> &Data) {
    // Initialize the SPrimitives buffer.
    VERIFY_IS_TRUE(0 == _stricmp(Name, "SPrimitives"));
    size_t count = 8 * 8;
    size_t size = sizeof(SPrimitives) * count;
    Data.resize(size);
    SPrimitives *pPrimitives = (SPrimitives *)Data.data();
    for (size_t i = 0; i < count; ++i) {
      SPrimitives *p = &pPrimitives[i];
      p->f_float = g_SinCosFloats[i % _countof(g_SinCosFloats)];
      p->f_float2 = p->f_float;
    }
  });
  MappedData data;
  test->Test->GetReadBackData("SPrimitives", &data);
  // data.dump(); // Uncomment to dump raw bytes from buffer.

  unsigned count = 8 * 8;
  SPrimitives *pPrimitives = (SPrimitives *)data.data();
  WEX::TestExecution::DisableVerifyExceptions dve;
  static const float Error = 0.0008f;
  for (unsigned i = 0; i < count; ++i) {
    SPrimitives *p = &pPrimitives[i];
    float input = p->f_float;
    float sin_o = p->f_float_o;
    float cos_o = p->f_float2_o;
    LogCommentFmt(L"Element #%u, input %f, sin=%f, cos=%f", i, input, sin_o, cos_o);
    if (isinf(input)) {
      VERIFY_IS_TRUE(isnan(sin_o));
      VERIFY_IS_TRUE(isnan(cos_o));
    }
    else if (isnan(input)) {
      VERIFY_IS_TRUE(isnan(sin_o));
      VERIFY_IS_TRUE(isnan(cos_o));
    }
    else if (isdenorm(input)) {
      VERIFY_IS_TRUE(1.0f == cos_o);
      if (signbit(input)) {
        VERIFY_IS_TRUE(-0.0f == sin_o);
      }
      else {
        VERIFY_IS_TRUE(0.0f == sin_o);
      }
    }
    else if (input == 0.0f) {
      VERIFY_IS_TRUE(0.0f == sin_o);
      VERIFY_IS_TRUE(1.0f == cos_o);
    }
    else if (input == -0.0f) {
      VERIFY_IS_TRUE(-0.0f == sin_o);
      VERIFY_IS_TRUE(1.0f == cos_o);
    }
    else {
      float f_sin = sin(input);
      float f_cos = cos(input);
      VERIFY_IS_TRUE((f_sin - Error) <= sin_o && sin_o <= (f_sin + Error));
      VERIFY_IS_TRUE((f_cos - Error) <= cos_o && cos_o <= (f_cos + Error));
    }
  }
}

static float ifdenorm_flushf(float a) {
  return isdenorm(a) ? copysign(0.0f, a) : a;
}

static bool ifdenorm_flushf_eq(float a, float b) {
  return ifdenorm_flushf(a) == ifdenorm_flushf(b);
}

static bool ifdenorm_flushf_eq_or_nans(float a, float b) {
  if (isnan(a) && isnan(b)) return true;
  return ifdenorm_flushf(a) == ifdenorm_flushf(b);
}

TEST_F(ExecutionTest, MinMaxTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  struct SMinMaxElem {
    float f_fa;
    float f_fb;
    float f_fmin_o;
    float f_fmax_o;
  };
  float TestValues[] = {
    -(INFINITY),
    -1.0f,
    -(FLT_MIN/2),
    -0.0f,
    0.0f,
    FLT_MIN / 2,
    1.0f,
    INFINITY,
    NAN
  };

  // Single operation test at the moment.
  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(pDevice, m_support, pStream, "MinMax",
    [&TestValues](LPCSTR Name, std::vector<BYTE> &Data) {
    // Initialize the SPrimitives buffer.
    VERIFY_IS_TRUE(0 == _stricmp(Name, "SPrimitives"));
    size_t count = 10 * 10;
    size_t size = sizeof(SMinMaxElem) * count;
    Data.resize(size);
    SMinMaxElem *pElems = (SMinMaxElem *)Data.data();
    for (size_t a = 0; a < 10; ++a) {
      float fa = TestValues[a % _countof(TestValues)];
      for (size_t b = 0; b < 10; ++b) {
        SMinMaxElem *p = &pElems[a * 10 + b];
        ZeroMemory(p, sizeof(*p));
        p->f_fa = fa;
        p->f_fb = TestValues[b % _countof(TestValues)];
      }
    }
  });
  MappedData data;
  test->Test->GetReadBackData("SPrimitives", &data);
  // data.dump(); // Uncomment to dump raw bytes from buffer.

  unsigned count = 10 * 10;
  SMinMaxElem *pPrimitives = (SMinMaxElem *)data.data();
  WEX::TestExecution::DisableVerifyExceptions dve;
  static const float Error = 0.0008f;
  for (unsigned i = 0; i < count; ++i) {
    SMinMaxElem *p = &pPrimitives[i];
    float fa = p->f_fa;
    float fb = p->f_fb;
    float fmin = p->f_fmin_o;
    float fmax = p->f_fmax_o;
    LogCommentFmt(L"Element #%u, a %f, b %f, min=%f, max=%f", i, fa, fb, fmin, fmax);
    if (isnan(fa)) {
      VERIFY_IS_TRUE(ifdenorm_flushf_eq_or_nans(fmin, fb));
      VERIFY_IS_TRUE(ifdenorm_flushf_eq_or_nans(fmax, fb));
    }
    else if (isnan(fb)) {
      VERIFY_IS_TRUE(ifdenorm_flushf_eq_or_nans(fmin, fa));
      VERIFY_IS_TRUE(ifdenorm_flushf_eq_or_nans(fmax, fa));
    }
    else {
      // Flushing is allowed - check both cases.
      float fmax_0 = fa >= fb ? fa : fb;
      float fmax_1 = ifdenorm_flushf(fmax_0);
      VERIFY_IS_TRUE(fmax == fmax_0 || fmax == fmax_1);
      float fmin_0 = fa < fb ? fa : fb;
      float fmin_1 = ifdenorm_flushf(fmin_0);
      VERIFY_IS_TRUE(fmin == fmin_0 || fmin == fmin_1);
    }
  }
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
    VERIFY_ARE_EQUAL(*pValues, ExpectedCases[i]);
    ++pValues;
  }
}

TEST_F(ExecutionTest, BasicTriangleOpTest) {
  WEX::TestExecution::SetVerifyOutput verifySettings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
  CComPtr<IStream> pStream;
  ReadHlslDataIntoNewStream(L"ShaderOpArith.xml", &pStream);

  // Single operation test at the moment.
  CComPtr<ID3D12Device> pDevice;
  if (!CreateDevice(&pDevice))
    return;

  std::shared_ptr<ShaderOpTestResult> test = RunShaderOpTest(pDevice, m_support, pStream, "Triangle", nullptr);
  MappedData data;
  D3D12_RESOURCE_DESC &D = test->ShaderOp->GetResourceByName("RTarget")->Desc;
  UINT width = (UINT64)D.Width;
  UINT height = (UINT64)D.Height;
  test->Test->GetReadBackData("RTarget", &data);
  const uint32_t *pPixels = (uint32_t *)data.data();
  if (SaveImages()) {
    SavePixelsToFile(pPixels, DXGI_FORMAT_R8G8B8A8_UNORM, 320, 200, L"basic.bmp");
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
}

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
