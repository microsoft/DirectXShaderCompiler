// RUN: %dxilver 1.8 | %dxc -T lib_6_8 -enable-16bit-types %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT

// Ensure alignment field set correctly in RDAT for various node records

RWByteAddressBuffer BAB : register(u1, space0);

#define GLUE2(x, y) x##y
#define GLUE(x, y) GLUE2(x, y)

#define TEST_TYPE(EntryName, CompType) \
  struct GLUE(EntryName, _record) { \
    vector<CompType, 3> x; \
  }; \
  [shader("node")] \
  [NodeLaunch("broadcasting")] \
  [NodeDispatchGrid(1, 1, 1)] \
  [NumThreads(1,1,1)] \
  void EntryName(DispatchNodeInputRecord<GLUE(EntryName, _record)> Input) { \
    BAB.Store(0, Input.Get().x.x); \
  }

// RDAT: FunctionTable[{{.*}}] = {

// RDAT-LABEL: UnmangledName: "node_half"
// RDAT: RecordAlignmentInBytes: 2
TEST_TYPE(node_half, half)

// RDAT-LABEL: UnmangledName: "node_float16"
// RDAT: RecordAlignmentInBytes: 2
TEST_TYPE(node_float16, float16_t)

// RDAT-LABEL: UnmangledName: "node_int16"
// RDAT: RecordAlignmentInBytes: 2
TEST_TYPE(node_int16, int16_t)

// min16 types are converted to native 16-bit types.

// RDAT-LABEL: UnmangledName: "node_min16float"
// RDAT: RecordAlignmentInBytes: 2
TEST_TYPE(node_min16float, min16float)

// RDAT-LABEL: UnmangledName: "node_min16int"
// RDAT: RecordAlignmentInBytes: 2
TEST_TYPE(node_min16int, min16int)

//RDAT:ID3D12LibraryReflection1:
//RDAT:  D3D12_LIBRARY_DESC:
//RDAT:    Creator: <nullptr>
//RDAT:    Flags: 0
//RDAT:    FunctionCount: 5
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: node_float16
//RDAT:      Shader Version: Node 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0x40000
//RDAT:      ConstantBuffers: 0
//RDAT:      BoundResources: 1
//RDAT:      FunctionParameterCount: 0
//RDAT:      HasReturn: FALSE
//RDAT:    Bound Resources:
//RDAT:      D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
//RDAT:        Type: D3D_SIT_UAV_RWBYTEADDRESS
//RDAT:        uID: 0
//RDAT:        BindCount: 1
//RDAT:        BindPoint: 1
//RDAT:        Space: 0
//RDAT:        ReturnType: D3D_RETURN_TYPE_MIXED
//RDAT:        Dimension: D3D_SRV_DIMENSION_BUFFER
//RDAT:        NumSamples (or stride): 0
//RDAT:        uFlags: 0
//RDAT:  ID3D12FunctionReflection1:
//RDAT:    D3D12_FUNCTION_DESC1:
//RDAT:      RootSignatureSize: 0
//RDAT:      D3D12_NODE_SHADER_DESC:
//RDAT:        D3D12_COMPUTE_SHADER_DESC:
//RDAT:          NumThreads: 1, 1, 1
//RDAT:        LaunchType: D3D12_NODE_LAUNCH_TYPE_BROADCASTING_LAUNCH
//RDAT:        IsProgramEntry: FALSE
//RDAT:        LocalRootArgumentsTableIndex: -1
//RDAT:        DispatchGrid[0]: 1
//RDAT:        DispatchGrid[1]: 1
//RDAT:        DispatchGrid[2]: 1
//RDAT:        MaxDispatchGrid[0]: 0
//RDAT:        MaxDispatchGrid[1]: 0
//RDAT:        MaxDispatchGrid[2]: 0
//RDAT:        MaxRecursionDepth: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderId)
//RDAT:          Name: node_float16
//RDAT:          ID: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderSharedInput)
//RDAT:          Name: node_float16
//RDAT:          ID: 0
//RDAT:        InputNodes: 1
//RDAT:        OutputNodes: 0
//RDAT:    Input Nodes:
//RDAT:      D3D12_NODE_DESC:
//RDAT:        Flags: 0x61
//RDAT:        Type:
//RDAT:          Size: 6
//RDAT:          Alignment: 2
//RDAT:          DispatchGrid:
//RDAT:            ByteOffset: 0
//RDAT:            ComponentType: <unknown: 0>
//RDAT:            NumComponents: 0
//RDAT:        MaxRecords: 0
//RDAT:        MaxRecordsSharedWith: -1
//RDAT:        OutputArraySize: 0
//RDAT:        AllowSparseNodes: FALSE
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: node_half
//RDAT:      Shader Version: Node 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0x40000
//RDAT:      ConstantBuffers: 0
//RDAT:      BoundResources: 1
//RDAT:      FunctionParameterCount: 0
//RDAT:      HasReturn: FALSE
//RDAT:    Bound Resources:
//RDAT:      D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
//RDAT:        Type: D3D_SIT_UAV_RWBYTEADDRESS
//RDAT:        uID: 0
//RDAT:        BindCount: 1
//RDAT:        BindPoint: 1
//RDAT:        Space: 0
//RDAT:        ReturnType: D3D_RETURN_TYPE_MIXED
//RDAT:        Dimension: D3D_SRV_DIMENSION_BUFFER
//RDAT:        NumSamples (or stride): 0
//RDAT:        uFlags: 0
//RDAT:  ID3D12FunctionReflection1:
//RDAT:    D3D12_FUNCTION_DESC1:
//RDAT:      RootSignatureSize: 0
//RDAT:      D3D12_NODE_SHADER_DESC:
//RDAT:        D3D12_COMPUTE_SHADER_DESC:
//RDAT:          NumThreads: 1, 1, 1
//RDAT:        LaunchType: D3D12_NODE_LAUNCH_TYPE_BROADCASTING_LAUNCH
//RDAT:        IsProgramEntry: FALSE
//RDAT:        LocalRootArgumentsTableIndex: -1
//RDAT:        DispatchGrid[0]: 1
//RDAT:        DispatchGrid[1]: 1
//RDAT:        DispatchGrid[2]: 1
//RDAT:        MaxDispatchGrid[0]: 0
//RDAT:        MaxDispatchGrid[1]: 0
//RDAT:        MaxDispatchGrid[2]: 0
//RDAT:        MaxRecursionDepth: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderId)
//RDAT:          Name: node_half
//RDAT:          ID: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderSharedInput)
//RDAT:          Name: node_half
//RDAT:          ID: 0
//RDAT:        InputNodes: 1
//RDAT:        OutputNodes: 0
//RDAT:    Input Nodes:
//RDAT:      D3D12_NODE_DESC:
//RDAT:        Flags: 0x61
//RDAT:        Type:
//RDAT:          Size: 6
//RDAT:          Alignment: 2
//RDAT:          DispatchGrid:
//RDAT:            ByteOffset: 0
//RDAT:            ComponentType: <unknown: 0>
//RDAT:            NumComponents: 0
//RDAT:        MaxRecords: 0
//RDAT:        MaxRecordsSharedWith: -1
//RDAT:        OutputArraySize: 0
//RDAT:        AllowSparseNodes: FALSE
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: node_int16
//RDAT:      Shader Version: Node 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0x40000
//RDAT:      ConstantBuffers: 0
//RDAT:      BoundResources: 1
//RDAT:      FunctionParameterCount: 0
//RDAT:      HasReturn: FALSE
//RDAT:    Bound Resources:
//RDAT:      D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
//RDAT:        Type: D3D_SIT_UAV_RWBYTEADDRESS
//RDAT:        uID: 0
//RDAT:        BindCount: 1
//RDAT:        BindPoint: 1
//RDAT:        Space: 0
//RDAT:        ReturnType: D3D_RETURN_TYPE_MIXED
//RDAT:        Dimension: D3D_SRV_DIMENSION_BUFFER
//RDAT:        NumSamples (or stride): 0
//RDAT:        uFlags: 0
//RDAT:  ID3D12FunctionReflection1:
//RDAT:    D3D12_FUNCTION_DESC1:
//RDAT:      RootSignatureSize: 0
//RDAT:      D3D12_NODE_SHADER_DESC:
//RDAT:        D3D12_COMPUTE_SHADER_DESC:
//RDAT:          NumThreads: 1, 1, 1
//RDAT:        LaunchType: D3D12_NODE_LAUNCH_TYPE_BROADCASTING_LAUNCH
//RDAT:        IsProgramEntry: FALSE
//RDAT:        LocalRootArgumentsTableIndex: -1
//RDAT:        DispatchGrid[0]: 1
//RDAT:        DispatchGrid[1]: 1
//RDAT:        DispatchGrid[2]: 1
//RDAT:        MaxDispatchGrid[0]: 0
//RDAT:        MaxDispatchGrid[1]: 0
//RDAT:        MaxDispatchGrid[2]: 0
//RDAT:        MaxRecursionDepth: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderId)
//RDAT:          Name: node_int16
//RDAT:          ID: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderSharedInput)
//RDAT:          Name: node_int16
//RDAT:          ID: 0
//RDAT:        InputNodes: 1
//RDAT:        OutputNodes: 0
//RDAT:    Input Nodes:
//RDAT:      D3D12_NODE_DESC:
//RDAT:        Flags: 0x61
//RDAT:        Type:
//RDAT:          Size: 6
//RDAT:          Alignment: 2
//RDAT:          DispatchGrid:
//RDAT:            ByteOffset: 0
//RDAT:            ComponentType: <unknown: 0>
//RDAT:            NumComponents: 0
//RDAT:        MaxRecords: 0
//RDAT:        MaxRecordsSharedWith: -1
//RDAT:        OutputArraySize: 0
//RDAT:        AllowSparseNodes: FALSE
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: node_min16float
//RDAT:      Shader Version: Node 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0x40000
//RDAT:      ConstantBuffers: 0
//RDAT:      BoundResources: 1
//RDAT:      FunctionParameterCount: 0
//RDAT:      HasReturn: FALSE
//RDAT:    Bound Resources:
//RDAT:      D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
//RDAT:        Type: D3D_SIT_UAV_RWBYTEADDRESS
//RDAT:        uID: 0
//RDAT:        BindCount: 1
//RDAT:        BindPoint: 1
//RDAT:        Space: 0
//RDAT:        ReturnType: D3D_RETURN_TYPE_MIXED
//RDAT:        Dimension: D3D_SRV_DIMENSION_BUFFER
//RDAT:        NumSamples (or stride): 0
//RDAT:        uFlags: 0
//RDAT:  ID3D12FunctionReflection1:
//RDAT:    D3D12_FUNCTION_DESC1:
//RDAT:      RootSignatureSize: 0
//RDAT:      D3D12_NODE_SHADER_DESC:
//RDAT:        D3D12_COMPUTE_SHADER_DESC:
//RDAT:          NumThreads: 1, 1, 1
//RDAT:        LaunchType: D3D12_NODE_LAUNCH_TYPE_BROADCASTING_LAUNCH
//RDAT:        IsProgramEntry: FALSE
//RDAT:        LocalRootArgumentsTableIndex: -1
//RDAT:        DispatchGrid[0]: 1
//RDAT:        DispatchGrid[1]: 1
//RDAT:        DispatchGrid[2]: 1
//RDAT:        MaxDispatchGrid[0]: 0
//RDAT:        MaxDispatchGrid[1]: 0
//RDAT:        MaxDispatchGrid[2]: 0
//RDAT:        MaxRecursionDepth: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderId)
//RDAT:          Name: node_min16float
//RDAT:          ID: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderSharedInput)
//RDAT:          Name: node_min16float
//RDAT:          ID: 0
//RDAT:        InputNodes: 1
//RDAT:        OutputNodes: 0
//RDAT:    Input Nodes:
//RDAT:      D3D12_NODE_DESC:
//RDAT:        Flags: 0x61
//RDAT:        Type:
//RDAT:          Size: 6
//RDAT:          Alignment: 2
//RDAT:          DispatchGrid:
//RDAT:            ByteOffset: 0
//RDAT:            ComponentType: <unknown: 0>
//RDAT:            NumComponents: 0
//RDAT:        MaxRecords: 0
//RDAT:        MaxRecordsSharedWith: -1
//RDAT:        OutputArraySize: 0
//RDAT:        AllowSparseNodes: FALSE
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: node_min16int
//RDAT:      Shader Version: Node 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0x40000
//RDAT:      ConstantBuffers: 0
//RDAT:      BoundResources: 1
//RDAT:      FunctionParameterCount: 0
//RDAT:      HasReturn: FALSE
//RDAT:    Bound Resources:
//RDAT:      D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
//RDAT:        Type: D3D_SIT_UAV_RWBYTEADDRESS
//RDAT:        uID: 0
//RDAT:        BindCount: 1
//RDAT:        BindPoint: 1
//RDAT:        Space: 0
//RDAT:        ReturnType: D3D_RETURN_TYPE_MIXED
//RDAT:        Dimension: D3D_SRV_DIMENSION_BUFFER
//RDAT:        NumSamples (or stride): 0
//RDAT:        uFlags: 0
//RDAT:  ID3D12FunctionReflection1:
//RDAT:    D3D12_FUNCTION_DESC1:
//RDAT:      RootSignatureSize: 0
//RDAT:      D3D12_NODE_SHADER_DESC:
//RDAT:        D3D12_COMPUTE_SHADER_DESC:
//RDAT:          NumThreads: 1, 1, 1
//RDAT:        LaunchType: D3D12_NODE_LAUNCH_TYPE_BROADCASTING_LAUNCH
//RDAT:        IsProgramEntry: FALSE
//RDAT:        LocalRootArgumentsTableIndex: -1
//RDAT:        DispatchGrid[0]: 1
//RDAT:        DispatchGrid[1]: 1
//RDAT:        DispatchGrid[2]: 1
//RDAT:        MaxDispatchGrid[0]: 0
//RDAT:        MaxDispatchGrid[1]: 0
//RDAT:        MaxDispatchGrid[2]: 0
//RDAT:        MaxRecursionDepth: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderId)
//RDAT:          Name: node_min16int
//RDAT:          ID: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderSharedInput)
//RDAT:          Name: node_min16int
//RDAT:          ID: 0
//RDAT:        InputNodes: 1
//RDAT:        OutputNodes: 0
//RDAT:    Input Nodes:
//RDAT:      D3D12_NODE_DESC:
//RDAT:        Flags: 0x61
//RDAT:        Type:
//RDAT:          Size: 6
//RDAT:          Alignment: 2
//RDAT:          DispatchGrid:
//RDAT:            ByteOffset: 0
//RDAT:            ComponentType: <unknown: 0>
//RDAT:            NumComponents: 0
//RDAT:        MaxRecords: 0
//RDAT:        MaxRecordsSharedWith: -1
//RDAT:        OutputArraySize: 0
//RDAT:        AllowSparseNodes: FALSE
