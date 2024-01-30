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
// MinShaderTarget: (Mesh(13) << 16) + (SM 6.5 ((6 << 4) + 5)) = 0xD0065 = 852069
// RDAT18: MinShaderTarget: 852069
// Old: 6.0
// RDAT17: MinShaderTarget: 852064

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
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Amplification(14) << 16) + (SM 6.5 ((6 << 4) + 5)) = 0xE0065 = 917605
// RDAT: MinShaderTarget: 917605

groupshared Vertex pld;

[shader("amplification")]
[numthreads(8, 8, 1)]
void amplification(uint3 DTid : SV_DispatchThreadID) {
  DispatchMesh(1, 1, 1, pld);
}
