// RUN: %dxilver 1.8 | %dxc -T lib_6_8 -Vd %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT

// Verifies that a Barrier requiring a visible group in a noinline function
// called by a vertex shader is correctly marked as requiring a group in RDAT.
// Validation is disabled to allow this to produce the RDAT blob for checking
// the flags, and for generating .ll tests.

// RDAT: FunctionTable[{{.*}}] = {

RWBuffer<uint> Buf : register(u0);

// RDAT-LABEL:   UnmangledName: "write_value"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (Pixel | Vertex | Geometry | Hull | Domain | Compute | Library | RayGeneration | Intersection | AnyHit | ClosestHit | Miss | Callable | Mesh | Amplification | Node)
// RDAT:   MinShaderTarget: 0x60060

[noinline] export
void write_value(uint value) {
    Buf[value] = value;
}

// RDAT-LABEL:   UnmangledName: "barrier_group"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: (Opt_RequiresGroup)
// RDAT:   ShaderStageFlag: (Compute | Library | Mesh | Amplification | Node)
// RDAT:   MinShaderTarget: 0x60060

[noinline] export
void barrier_group() {
    write_value(1);
    Barrier(GROUP_SHARED_MEMORY, GROUP_SCOPE);
    write_value(2);
}

// RDAT-LABEL:   UnmangledName: "intermediate"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: (Opt_RequiresGroup)
// RDAT:   ShaderStageFlag: (Compute | Library | Mesh | Amplification | Node)
// RDAT:   MinShaderTarget: 0x60060

[noinline] export
void intermediate() {
    write_value(3);
    barrier_group();
    write_value(4);
}

// RDAT-LABEL:   UnmangledName: "main"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: (Opt_RequiresGroup)
// ShaderStageFlag indicates no compatible entry type after masking for vertex.
// RDAT:   ShaderStageFlag: 0
// MinShaderTarget still indicates vertex shader.
// RDAT:   MinShaderTarget: 0x10060

[shader("vertex")]
void main() {
    intermediate();
}
