// RUN: %dxc -T lib_6_8 -DNODE %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT1
// RUN: %dxc -T lib_6_8 -DNODE -DRANGE=,64 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT2
// RUN: %dxc -T lib_6_8 -DNODE -DRANGE=,64,32 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT3


// RDAT has no min/max wave count until SM 6.8
// RDAT-LABEL: <0:RuntimeDataFunctionInfo{{.}}> = {
// RDAT: Name: "main"
// RDAT: MinimumExpectedWaveLaneCount: 16
// RDAT1: MaximumExpectedWaveLaneCount: 16
// RDAT2: MaximumExpectedWaveLaneCount: 64
// RDAT3: MaximumExpectedWaveLaneCount: 64

// RDAT-LABEL: <1:RuntimeDataFunctionInfo{{.}}> = {
// RDAT: Name: "node"
// RDAT: MinimumExpectedWaveLaneCount: 16
// RDAT1: MaximumExpectedWaveLaneCount: 16
// RDAT2: MaximumExpectedWaveLaneCount: 64
// RDAT3: MaximumExpectedWaveLaneCount: 64

#ifndef RANGE
#define RANGE
#endif

[shader("compute")]
[wavesize(16 RANGE)]
[numthreads(1,1,8)]
void main() {
}

#ifdef NODE
[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,8)]
[NodeDispatchGrid(1,1,1)]
[WaveSize(16 RANGE)]
void node() { }
#endif

//RDAT:ID3D12LibraryReflection1:
//RDAT:  D3D12_LIBRARY_DESC:
//RDAT:    Creator: <nullptr>
//RDAT:    Flags: 0
//RDAT:    FunctionCount: 2
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: main
//RDAT:      Shader Version: Compute 6.8
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
//RDAT:      D3D12_COMPUTE_SHADER_DESC:
//RDAT1:       WaveSize: min: 16, max: 0, preferred: 0
//RDAT2:       WaveSize: min: 16, max: 64, preferred: 0
//RDAT3:       WaveSize: min: 16, max: 64, preferred: 32
//RDAT:        NumThreads: 1, 1, 8
//RDAT:  ID3D12FunctionReflection:
//RDAT:    D3D12_FUNCTION_DESC: Name: node
//RDAT:      Shader Version: Node 6.8
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
//RDAT:      D3D12_NODE_SHADER_DESC:
//RDAT:        D3D12_COMPUTE_SHADER_DESC:
//RDAT1:         WaveSize: min: 16, max: 0, preferred: 0
//RDAT2:         WaveSize: min: 16, max: 64, preferred: 0
//RDAT3:         WaveSize: min: 16, max: 64, preferred: 32
//RDAT:          NumThreads: 1, 1, 8
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
//RDAT:          Name: node
//RDAT:          ID: 0
//RDAT:        D3D12_NODE_ID_DESC: (ShaderSharedInput)
//RDAT:          Name: node
//RDAT:          ID: 0
//RDAT:        InputNodes: 0
//RDAT:        OutputNodes: 0