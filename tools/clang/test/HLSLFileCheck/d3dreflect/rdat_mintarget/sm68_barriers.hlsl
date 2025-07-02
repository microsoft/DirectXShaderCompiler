// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT

// Check that stage flags are set correctly for different barrier modes with
// new the SM 6.8 barrier intrinsics.

// RDAT: FunctionTable[{{.*}}] = {

RWByteAddressBuffer BAB : register(u1, space0);

// RDAT-LABEL: UnmangledName: "node_barrier"
// RDAT: FeatureInfo1: 0
// RDAT: FeatureInfo2: (Opt_RequiresGroup)
// RDAT: ShaderStageFlag: (Node)
// RDAT: MinShaderTarget: 0xf0068

[shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1,1,1)]
void node_barrier() {
  GroupMemoryBarrierWithGroupSync();
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "fn_barrier_device1"
// RDAT: FeatureInfo1: 0
// RDAT: FeatureInfo2: 0
// RDAT: ShaderStageFlag: (Pixel | Vertex | Geometry | Hull | Domain | Compute | Library | RayGeneration | Intersection | AnyHit | ClosestHit | Miss | Callable | Mesh | Amplification | Node)
// RDAT: MinShaderTarget: 0x60060

[noinline] export
void fn_barrier_device1() {
  Barrier(UAV_MEMORY, DEVICE_SCOPE);
}

// RDAT-LABEL: UnmangledName: "fn_barrier_device2"
// RDAT: FeatureInfo1: 0
// RDAT: FeatureInfo2: 0
// RDAT: ShaderStageFlag: (Pixel | Vertex | Geometry | Hull | Domain | Compute | Library | RayGeneration | Intersection | AnyHit | ClosestHit | Miss | Callable | Mesh | Amplification | Node)
// RDAT: MinShaderTarget: 0x60068

[noinline] export
void fn_barrier_device2() {
  Barrier(BAB, DEVICE_SCOPE);
}

// RDAT-LABEL: UnmangledName: "fn_barrier_group1"
// RDAT: FeatureInfo1: 0
// RDAT: FeatureInfo2: (Opt_RequiresGroup)
// RDAT: ShaderStageFlag: (Compute | Library | Mesh | Amplification | Node)
// RDAT: MinShaderTarget: 0x60060

[noinline] export
void fn_barrier_group1() {
  Barrier(GROUP_SHARED_MEMORY, GROUP_SYNC | GROUP_SCOPE);
}

// RDAT-LABEL: UnmangledName: "fn_barrier_group2"
// RDAT: FeatureInfo1: 0
// RDAT: FeatureInfo2: (Opt_RequiresGroup)
// RDAT: ShaderStageFlag: (Compute | Library | Mesh | Amplification | Node)
// RDAT: MinShaderTarget: 0x60068

[noinline] export
void fn_barrier_group2() {
  Barrier(BAB, GROUP_SYNC | GROUP_SCOPE);
}

// RDAT-LABEL: UnmangledName: "fn_barrier_node1"
// RDAT: FeatureInfo1: 0
// RDAT: FeatureInfo2: 0
// RDAT: ShaderStageFlag: (Library | Node)
// RDAT: MinShaderTarget: 0x60068

[noinline] export
void fn_barrier_node1() {
  Barrier(NODE_INPUT_MEMORY, DEVICE_SCOPE);
}

// RDAT-LABEL: UnmangledName: "fn_barrier_node_group1"
// RDAT: FeatureInfo1: 0
// RDAT: FeatureInfo2: (Opt_RequiresGroup)
// RDAT: ShaderStageFlag: (Library | Node)
// RDAT: MinShaderTarget: 0x60068

[noinline] export
void fn_barrier_node_group1() {
  Barrier(NODE_INPUT_MEMORY, GROUP_SYNC | GROUP_SCOPE);
}

// RDAT-LABEL: UnmangledName: "node_barrier_device_in_call"
// RDAT: FeatureInfo1: 0
// RDAT: FeatureInfo2: 0
// RDAT: ShaderStageFlag: (Node)
// RDAT: MinShaderTarget: 0xf0068

[shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1,1,1)]
void node_barrier_device_in_call() {
  fn_barrier_device1();
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "node_barrier_node_in_call"
// RDAT: FeatureInfo1: 0
// RDAT: FeatureInfo2: 0
// RDAT: ShaderStageFlag: (Node)
// RDAT: MinShaderTarget: 0xf0068

[shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1,1,1)]
void node_barrier_node_in_call() {
  fn_barrier_node1();
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "node_barrier_node_group_in_call"
// RDAT: FeatureInfo1: 0
// RDAT: FeatureInfo2: (Opt_RequiresGroup)
// RDAT: ShaderStageFlag: (Node)
// RDAT: MinShaderTarget: 0xf0068

[shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 1)]
[NumThreads(1,1,1)]
void node_barrier_node_group_in_call() {
  fn_barrier_node_group1();
  BAB.Store(0, 0);
}

//RDAT:ID3D12LibraryReflection1:
//RDAT:  D3D12_LIBRARY_DESC:
//RDAT:    Creator: <nullptr>
//RDAT:    Flags: 0
//RDAT:    FunctionCount: 10
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: \01?fn_barrier_device1@@YAXXZ
//RDAT:      Shader Version: Library 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0
//RDAT:      ConstantBuffers: 0
//RDAT:      BoundResources: 0
//RDAT:      FunctionParameterCount: 0
//RDAT:      HasReturn: FALSE
//RDAT:  ID3D12FunctionReflection1:
//RDAT:    D3D12_FUNCTION_DESC1:
//RDAT:      RootSignatureSize: 0
//RDAT:      EarlyDepthStencil: FALSE
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: \01?fn_barrier_device2@@YAXXZ
//RDAT:      Shader Version: Library 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0
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
//RDAT:      EarlyDepthStencil: FALSE
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: \01?fn_barrier_group1@@YAXXZ
//RDAT:      Shader Version: Library 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0x20000000000
//RDAT:      ConstantBuffers: 0
//RDAT:      BoundResources: 0
//RDAT:      FunctionParameterCount: 0
//RDAT:      HasReturn: FALSE
//RDAT:  ID3D12FunctionReflection1:
//RDAT:    D3D12_FUNCTION_DESC1:
//RDAT:      RootSignatureSize: 0
//RDAT:      EarlyDepthStencil: FALSE
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: \01?fn_barrier_group2@@YAXXZ
//RDAT:      Shader Version: Library 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0x20000000000
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
//RDAT:      EarlyDepthStencil: FALSE
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: \01?fn_barrier_node1@@YAXXZ
//RDAT:      Shader Version: Library 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0
//RDAT:      ConstantBuffers: 0
//RDAT:      BoundResources: 0
//RDAT:      FunctionParameterCount: 0
//RDAT:      HasReturn: FALSE
//RDAT:  ID3D12FunctionReflection1:
//RDAT:    D3D12_FUNCTION_DESC1:
//RDAT:      RootSignatureSize: 0
//RDAT:      EarlyDepthStencil: FALSE
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: \01?fn_barrier_node_group1@@YAXXZ
//RDAT:      Shader Version: Library 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0x20000000000
//RDAT:      ConstantBuffers: 0
//RDAT:      BoundResources: 0
//RDAT:      FunctionParameterCount: 0
//RDAT:      HasReturn: FALSE
//RDAT:  ID3D12FunctionReflection1:
//RDAT:    D3D12_FUNCTION_DESC1:
//RDAT:      RootSignatureSize: 0
//RDAT:      EarlyDepthStencil: FALSE
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: node_barrier
//RDAT:      Shader Version: Node 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0x20000000000
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
//RDAT:          Name: node_barrier
//RDAT:          ID: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderSharedInput)
//RDAT:          Name: node_barrier
//RDAT:          ID: 0
//RDAT:        InputNodes: 0
//RDAT:        OutputNodes: 0
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: node_barrier_device_in_call
//RDAT:      Shader Version: Node 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0
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
//RDAT:          Name: node_barrier_device_in_call
//RDAT:          ID: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderSharedInput)
//RDAT:          Name: node_barrier_device_in_call
//RDAT:          ID: 0
//RDAT:        InputNodes: 0
//RDAT:        OutputNodes: 0
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: node_barrier_node_group_in_call
//RDAT:      Shader Version: Node 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0x20000000000
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
//RDAT:          Name: node_barrier_node_group_in_call
//RDAT:          ID: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderSharedInput)
//RDAT:          Name: node_barrier_node_group_in_call
//RDAT:          ID: 0
//RDAT:        InputNodes: 0
//RDAT:        OutputNodes: 0
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: node_barrier_node_in_call
//RDAT:      Shader Version: Node 6.8
//RDAT:      Creator: <nullptr>
//RDAT:      Flags: 0
//RDAT:      RequiredFeatureFlags: 0
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
//RDAT:          Name: node_barrier_node_in_call
//RDAT:          ID: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderSharedInput)
//RDAT:          Name: node_barrier_node_in_call
//RDAT:          ID: 0
//RDAT:        InputNodes: 0
//RDAT:        OutputNodes: 0