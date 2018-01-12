#include "Common.h"
#include "SimpleTextFile.h"
#include "FenceWrapper.h"
#include "MappedData.h"

void SetDescriptorHeap(ID3D12GraphicsCommandList *pCommandList, ID3D12DescriptorHeap *pHeap)
{
    ID3D12DescriptorHeap *const pHeaps[1] = { pHeap };
    pCommandList->SetDescriptorHeaps(ARRAYSIZE(pHeaps), pHeaps);
}

void ExecuteCommandList(ID3D12CommandQueue *pQueue, ID3D12CommandList *pList)
{
    ID3D12CommandList *ppCommandLists[] = { pList };
    pQueue->ExecuteCommandLists(ARRAYSIZE(ppCommandLists), ppCommandLists);
}

void CreateComputeCommandQueue(ID3D12Device *pDevice, LPCWSTR pName, ID3D12CommandQueue **ppCommandQueue)
{
    HRESULT hr;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    IFT(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(ppCommandQueue)));
    IFT((*ppCommandQueue)->SetName(pName));
}

void CreateRootSignatureFromDesc(ID3D12Device *pDevice, const D3D12_ROOT_SIGNATURE_DESC *pDesc, ID3D12RootSignature **pRootSig)
{
    HRESULT hr;

    ComPtr<ID3DBlob> spSignature;
    ComPtr<ID3DBlob> spError;
    IFT(D3D12SerializeRootSignature(pDesc, D3D_ROOT_SIGNATURE_VERSION_1, &spSignature, &spError));
    IFT(pDevice->CreateRootSignature(0, spSignature->GetBufferPointer(), spSignature->GetBufferSize(), IID_PPV_ARGS(pRootSig)));
}

void CompileFromText(LPCSTR pText, LPCWSTR pEntryPoint, LPCWSTR pTargetProfile, ID3DBlob **ppBlob)
{
    HRESULT hr;

    dxc::DxcDllSupport support;
    IFT(support.Initialize());

    ComPtr<IDxcCompiler2> spCompiler;
    IFT(support.CreateInstance(CLSID_DxcCompiler, spCompiler.ReleaseAndGetAddressOf()));

    ComPtr<IDxcLibrary> spLibrary;
    IFT(support.CreateInstance(CLSID_DxcLibrary, spLibrary.ReleaseAndGetAddressOf()));

    ComPtr<IDxcBlobEncoding> spTextBlob;
    IFT(spLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)pText, (UINT32)strlen(pText), CP_UTF8, &spTextBlob));

    const wchar_t* pszArgs = L"/Zi";

    ComPtr<IDxcOperationResult> spResult;
    IFT(spCompiler->Compile(
        spTextBlob.Get(),
        L"hlsl.hlsl",
        pEntryPoint,
        pTargetProfile,
        &pszArgs,
        1,
        nullptr,
        0,
        nullptr,
        &spResult
    ));

    HRESULT resultCode = S_OK;
    IFT(spResult->GetStatus(&resultCode));

    if (FAILED(resultCode))
    {
        ComPtr<IDxcBlobEncoding> spErrors;
        IFT(spResult->GetErrorBuffer(&spErrors));
    }

    IFT(resultCode);
    IFT(spResult->GetResult(reinterpret_cast<IDxcBlob **>(ppBlob)));
}

void CreateComputePSO(ID3D12Device *pDevice, ID3D12RootSignature *pRootSignature, LPCSTR pszShader, ID3D12PipelineState **pspComputeState)
{
    HRESULT hr;

    // Load and compile shaders.
    ComPtr<ID3DBlob> spComputeShader;
    CompileFromText(pszShader, L"main", L"cs_6_0", spComputeShader.ReleaseAndGetAddressOf());

    // Describe and create the compute pipeline state object (PSO).
    D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
    computePsoDesc.pRootSignature = pRootSignature;
    computePsoDesc.CS = CD3DX12_SHADER_BYTECODE(spComputeShader.Get());
    computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG;

    IFT(pDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(pspComputeState)));
}

void RecordTransitionBarrier(ID3D12GraphicsCommandList *pCommandList,
    ID3D12Resource *pResource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
{
    CD3DX12_RESOURCE_BARRIER barrier(CD3DX12_RESOURCE_BARRIER::Transition(pResource, before, after));

    pCommandList->ResourceBarrier(1, &barrier);
}

void CreateTestUavs(ID3D12Device *pDevice,
    ID3D12GraphicsCommandList *pCommandList, 
    LPCVOID values,
    UINT valueSizeInBytes, 
    ID3D12Resource **ppUavResource,
    ID3D12Resource **ppReadBuffer,
    ID3D12Resource **ppUploadResource)
{
    HRESULT hr;

    ComPtr<ID3D12Resource> spUavResource;
    D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(valueSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    IFT(pDevice->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&spUavResource)));

    ComPtr<ID3D12Resource> spUploadResource;
    D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(valueSizeInBytes);
    IFT(pDevice->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&spUploadResource)));

    ComPtr<ID3D12Resource> spReadBuffer;
    CD3DX12_HEAP_PROPERTIES readHeap(D3D12_HEAP_TYPE_READBACK);
    CD3DX12_RESOURCE_DESC readDesc(CD3DX12_RESOURCE_DESC::Buffer(valueSizeInBytes));
    IFT(pDevice->CreateCommittedResource(
        &readHeap, 
        D3D12_HEAP_FLAG_NONE, 
        &readDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, 
        nullptr, 
        IID_PPV_ARGS(&spReadBuffer)));

    D3D12_SUBRESOURCE_DATA transferData;
    transferData.pData = values;
    transferData.RowPitch = valueSizeInBytes;
    transferData.SlicePitch = transferData.RowPitch;

    UpdateSubresources<1>(pCommandList, spUavResource.Get(), spUploadResource.Get(), 0, 0, 1, &transferData);
    RecordTransitionBarrier(pCommandList, spUavResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    *ppUavResource = spUavResource.Detach();
    *ppReadBuffer = spReadBuffer.Detach();
    *ppUploadResource = spUploadResource.Detach();
}

void JsonArrayToVector(IJsonArray* pArray, std::vector<uint32_t> &vec)
{
    HRESULT hr;

    ComPtr<IVector<IJsonValue *>> spInputListVector;
    IFT(pArray->QueryInterface(IID_PPV_ARGS(&spInputListVector)));

    unsigned int inputSize = 0;
    IFT(spInputListVector->get_Size(&inputSize));

    vec.resize(inputSize);

    for (unsigned int inputIndex = 0; inputIndex < inputSize; inputIndex++)
    {
        double value = 0.0;
        IFT(pArray->GetNumberAt(inputIndex, &value));
        vec[inputIndex] = static_cast<uint32_t>(value);
    }
}

int __cdecl main(void)
{
    HRESULT hr;

    try
    {
        // Initialize the Windows Runtime.
        RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);

        ComPtr<IJsonObjectStatics> spJsonStaticsFactory;
        IFT(GetActivationFactory(HStringReference(RuntimeClass_Windows_Data_Json_JsonObject).Get(), &spJsonStaticsFactory));

        // Read the test metadata file
        SimpleTextFile jsonFile(L"..\\Conformance\\FirstTests.json");
        std::vector<wchar_t> jsonContents;

        // Convert to wide when reading
        jsonFile.ReadFile(jsonContents);

        // Json object wants a Platform::String
        ComPtr<IJsonObject> spTestListJson;
        IFT(spJsonStaticsFactory->Parse(HStringReference(jsonContents.data()).Get(), &spTestListJson));

        ComPtr<IJsonArray> spTestList;
        IFT(spTestListJson->GetNamedArray(HStringReference(L"TestList").Get(), &spTestList));

        dxc::DxcDllSupport support;
        IFT(support.Initialize());

        // Take the debug layer if we can get it
        ComPtr<ID3D12Debug> spDebugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&spDebugController))))
        {
            spDebugController->EnableDebugLayer();
        }

        ComPtr<IDXGIFactory4> spFactory;
        IFT(CreateDXGIFactory1(IID_PPV_ARGS(&spFactory)));

        ComPtr<IDXGIAdapter> spAdapter;
        IFT(spFactory->EnumWarpAdapter(IID_PPV_ARGS(&spAdapter)));

        ComPtr<ID3D12Device> spDevice;
        IFT(D3D12CreateDevice(spAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&spDevice)));

        ComPtr<IDXGraphicsAnalysis> spGA;
        DXGIGetDebugInterface1(0, IID_PPV_ARGS(&spGA));

        if (spGA != nullptr)
        {
            spGA->BeginCapture();
        }

        static const int DispatchGroupX = 1;
        static const int DispatchGroupY = 1;
        static const int DispatchGroupZ = 1;

        ComPtr<ID3D12CommandQueue> spCommandQueue;
        CreateComputeCommandQueue(spDevice.Get(), L"RunRWByteBufferComputeTest Command Queue", &spCommandQueue);

        // Describe and create a UAV descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = 1;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        ComPtr<ID3D12DescriptorHeap> spUavHeap;
        IFT(spDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&spUavHeap)));

        // Create root signature.
        ComPtr<ID3D12RootSignature> spRootSignature;
        {
            CD3DX12_DESCRIPTOR_RANGE ranges[1];
            ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 0);

            CD3DX12_ROOT_PARAMETER rootParameters[1];
            rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);

            CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

            CreateRootSignatureFromDesc(spDevice.Get(), &rootSignatureDesc, &spRootSignature);
        }

        ComPtr<IJsonObject> spFirstEntry;
        IFT(spTestList->GetObjectAt(0, &spFirstEntry));

        HString testName;
        IFT(spFirstEntry->GetNamedString(HStringReference(L"name").Get(), testName.GetAddressOf()));

        // Load up the input data to the shader
        ComPtr<IJsonArray> spInputList;        
        IFT(spFirstEntry->GetNamedArray(HStringReference(L"input").Get(), &spInputList));

        std::vector<uint32_t> InputData;
        JsonArrayToVector(spInputList.Get(), InputData);

        const UINT valueSizeInBytes = static_cast<UINT>(InputData.size() * sizeof(uint32_t));

        // Load up the expected output
        ComPtr<IJsonArray> spExpectedList;
        IFT(spFirstEntry->GetNamedArray(HStringReference(L"output").Get(), &spExpectedList));

        std::vector<uint32_t> ExpectedData;
        JsonArrayToVector(spExpectedList.Get(), ExpectedData);

        HString hlslSource;
        IFT(spFirstEntry->GetNamedString(HStringReference(L"source").Get(), hlslSource.GetAddressOf()));

        HString hlslSourceWithPath;
        IFT(::WindowsConcatString(HStringReference(L"..\\Conformance\\").Get(), hlslSource.Get(), hlslSourceWithPath.GetAddressOf()));

        SimpleTextFile hlslFile(::WindowsGetStringRawBuffer(hlslSourceWithPath.Get(), nullptr));

        std::vector<char> shaderSource;
        hlslFile.ReadFile(shaderSource);

        // Create pipeline state object.
        ComPtr<ID3D12PipelineState> spComputeState;
        CreateComputePSO(spDevice.Get(), spRootSignature.Get(), shaderSource.data(), &spComputeState);

        // Create a command allocator and list for compute.
        ComPtr<ID3D12CommandAllocator> spCommandAllocator;
        IFT(spDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&spCommandAllocator)));

        ComPtr<ID3D12GraphicsCommandList> spCommandList;
        IFT(spDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, spCommandAllocator.Get(), spComputeState.Get(), IID_PPV_ARGS(&spCommandList)));
        spCommandList->SetName(L"ExecutionTest::RunRWByteButterComputeTest Command List");

        // Set up UAV resource.
        ComPtr<ID3D12Resource> spUavResource;
        ComPtr<ID3D12Resource> spReadBuffer;
        ComPtr<ID3D12Resource> spUploadResource;
        CreateTestUavs(spDevice.Get(), spCommandList.Get(), InputData.data(), valueSizeInBytes, &spUavResource, &spReadBuffer, &spUploadResource);
        IFT(spUavResource->SetName(L"RunRWByteBufferComputeText UAV"));
        IFT(spReadBuffer->SetName(L"RunRWByteBufferComputeText UAV Read Buffer"));
        IFT(spUploadResource->SetName(L"RunRWByteBufferComputeText UAV Upload Buffer"));

        // Close the command list and execute it to perform the GPU setup.
        spCommandList->Close();
        ExecuteCommandList(spCommandQueue.Get(), spCommandList.Get());

        // Wait for the command list to be done
        FenceWrapper FO(spDevice.Get());
        FO.WaitForSignal(spCommandQueue.Get());

        IFT(spCommandAllocator->Reset());
        IFT(spCommandList->Reset(spCommandAllocator.Get(), spComputeState.Get()));

        // Run the compute shader and copy the results back to readable memory.
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = (UINT)InputData.size();
        uavDesc.Buffer.StructureByteStride = 0;
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(spUavHeap->GetCPUDescriptorHandleForHeapStart());
        spDevice->CreateUnorderedAccessView(spUavResource.Get(), nullptr, &uavDesc, uavHandle);

        SetDescriptorHeap(spCommandList.Get(), spUavHeap.Get());
        spCommandList->SetComputeRootSignature(spRootSignature.Get());

        CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandleGpu(spUavHeap->GetGPUDescriptorHandleForHeapStart());
        spCommandList->SetComputeRootDescriptorTable(0, uavHandleGpu);

        spCommandList->Dispatch(DispatchGroupX, DispatchGroupY, DispatchGroupZ);
        RecordTransitionBarrier(spCommandList.Get(), spUavResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        spCommandList->CopyResource(spReadBuffer.Get(), spUavResource.Get());
        spCommandList->Close();
        ExecuteCommandList(spCommandQueue.Get(), spCommandList.Get());
        FO.WaitForSignal(spCommandQueue.Get());

        // Readback shader result
        std::vector<uint32_t> OutputData;
        OutputData.resize(InputData.size());

        {
            MappedData mappedData(spReadBuffer.Get(), valueSizeInBytes);
            uint32_t *pData = static_cast<uint32_t *>(mappedData.GetData());
            memcpy(OutputData.data(), pData, valueSizeInBytes);
        }

        FO.WaitForSignal(spCommandQueue.Get());

        if (spGA != nullptr) 
        {
            spGA->EndCapture();
        }

        PCWSTR pszTestName = ::WindowsGetStringRawBuffer(testName.Get(), nullptr);

        // Check that output matches expected
        size_t check;
        for (check = 0; check < ExpectedData.size(); check++)
        {
            if (ExpectedData[check] != OutputData[check])
            {
                ::wprintf(L"Test %s Failed\n", pszTestName);
                break;
            }
        }

        if (check == OutputData.size())
        {
            ::wprintf(L"Test %s Passed\n", pszTestName);
        }
    }
    catch (HRESULT hrFail)
    {
        ::wprintf(L"Error %x\n", hrFail);
    }

    return 0;
}
