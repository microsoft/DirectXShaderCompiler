// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT,RDAT18
// RUN: %dxilver 1.7 | %dxc -T lib_6_7 -validator-version 1.7 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT17

// Ensure min shader target incorporates shader stage of entry function
// These must be minimal shaders since intrinsic usage associated with the
// shader stage will cause the min target to be set that way.

// This covers mesh and amplification shaders, which should always be SM 6.5+

// RDAT: FunctionTable[{{.*}}] = {

RWByteAddressBuffer BAB : register(u1, space0);

////////////////////////////////////////
// Mesh shader
// Currently, mesh shader is not requiring output vertices or indices, so this
// works.  If that requirement were to be enforced, we would have to declare
// these outputs.  However, if we do, there should also be a requirement that
// mesh shader vertex output has SV_Position (not enforced currently either).
// If that were to be enforced, and added to the struct, then the validator
// will fail unless you write to all components of SV_Position.  This should
// probably only be the case if OutputCounts are set to anything other than 0.
// In any case, all this means that if some rules start to be enforced, we
// will be forced to use things which will produce intrinsic calls in the mesh
// shader which will cause the min target to be set to SM 6.5+ even for
// validator version 1.7 and below.

// RDAT-LABEL: UnmangledName: "mesh"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (Mesh)
// RDAT18: MinShaderTarget: 0xd0065
// Old: 6.0
// RDAT17: MinShaderTarget: 0xd0060

struct Vertex {
  float4 val : UNUSED;
};

[shader("mesh")]
[numthreads(1, 1, 1)]
[outputtopology("triangle")]
void mesh(//out vertices Vertex verts[1],
          //out indices uint3 tris[1]
    ) {
  BAB.Store(0, 0);
}

////////////////////////////////////////
// Amplification shader
// It turns out that amplification shaders require exactly one DispatchMesh
// call, which causes the entry to get the correct min target without basing
// it on the shader type.

// RDAT-LABEL: UnmangledName: "amplification"
// RDAT:   FeatureInfo1: 0
// RDAT18:   FeatureInfo2: (Opt_RequiresGroup)
// Old: no Opt_RequiresGroup flag.
// RDAT17:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (Amplification)
// RDAT:   MinShaderTarget: 0xe0065

groupshared Vertex pld;

[shader("amplification")]
[numthreads(8, 8, 1)]
void amplification(uint3 DTid : SV_DispatchThreadID) {
  DispatchMesh(1, 1, 1, pld);
}

// RDAT: ID3D12LibraryReflection1:
// RDAT:   D3D12_LIBRARY_DESC:
// RDAT:     Creator: <nullptr>
// RDAT:     Flags: 0
// RDAT:     FunctionCount: 2
// RDAT:   ID3D12FunctionReflection:
// RDAT:     D3D12_FUNCTION_DESC: Name: amplification
// RDAT18:     Shader Version: Amplification 6.8
// RDAT17:     Shader Version: Amplification 6.7
// RDAT:       Creator: <nullptr>
// RDAT:       Flags: 0
// RDAT18:     RequiredFeatureFlags: 0x20000000000
// RDAT17:     RequiredFeatureFlags: 0
// RDAT:       ConstantBuffers: 0
// RDAT:       BoundResources: 0
// RDAT:       FunctionParameterCount: 0
// RDAT:       HasReturn: FALSE
// RDAT:   ID3D12FunctionReflection1:
// RDAT:     D3D12_FUNCTION_DESC1:
// RDAT:       RootSignatureSize: 0
// RDAT:       PayloadSize: 16
// RDAT:       NumThreads: 8, 8, 1
// RDAT:   ID3D12FunctionReflection:
// RDAT:     D3D12_FUNCTION_DESC: Name: mesh
// RDAT18:     Shader Version: Mesh 6.8
// RDAT17:     Shader Version: Mesh 6.7
// RDAT:       Creator: <nullptr>
// RDAT:       Flags: 0
// RDAT:       RequiredFeatureFlags: 0
// RDAT:       ConstantBuffers: 0
// RDAT:       BoundResources: 1
// RDAT:       FunctionParameterCount: 0
// RDAT:       HasReturn: FALSE
// RDAT:     Bound Resources:
// RDAT:       D3D12_SHADER_INPUT_BIND_DESC: Name: BAB
// RDAT:         Type: D3D_SIT_UAV_RWBYTEADDRESS
// RDAT:         uID: 0
// RDAT:         BindCount: 1
// RDAT:         BindPoint: 1
// RDAT:         Space: 0
// RDAT:         ReturnType: D3D_RETURN_TYPE_MIXED
// RDAT:         Dimension: D3D_SRV_DIMENSION_BUFFER
// RDAT:         NumSamples (or stride): 0
// RDAT:         uFlags: 0
// RDAT:   ID3D12FunctionReflection1:
// RDAT:     D3D12_FUNCTION_DESC1:
// RDAT:       RootSignatureSize: 0
// RDAT:       D3D12_MESH_SHADER_DESC:
// RDAT:         PayloadSize: 0
// RDAT:         MaxVertexCount: 0
// RDAT:         MaxPrimitiveCount: 0
// RDAT:         OutputTopology: Triangle
// RDAT:         NumThreads: 1, 1, 1
