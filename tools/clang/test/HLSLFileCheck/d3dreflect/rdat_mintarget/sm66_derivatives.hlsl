// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT,RDAT18
// RUN: %dxilver 1.7 | %dxc -T lib_6_7 -validator-version 1.7 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT17

// Ensure min shader target incorporates optional features used

// RDAT: FunctionTable[{{.*}}] = {

// SM 6.6+

///////////////////////////////////////////////////////////////////////////////
// ShaderFeatureInfo_DerivativesInMeshAndAmpShaders (0x1000000) = 16777216
// OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256

// OptFeatureInfo_UsesDerivatives Flag used to indicate derivative use in
// functions, then fixed up for entry functions.
// Val. ver. 1.8 required to recursively check called functions.

// RDAT-LABEL: UnmangledName: "deriv_in_func"
// RDAT: FeatureInfo1: 0
// OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
// RDAT18:   FeatureInfo2: 256
// Old: deriv use not tracked
// RDAT17:   FeatureInfo2: 0
// Pixel(0), Compute(5), Library(6), Mesh(13), Amplification(14), Node(15) = 0xE061 = 57441
// RDAT18: ShaderStageFlag: 57441
// Old would not report Compute, Mesh, Amplification, or Node compatibility.
// Pixel(0), Library(6) = 0x41 = 65
// RDAT17: ShaderStageFlag: 65
// MinShaderTarget: (Library(6) << 16) + (SM 6.0 ((6 << 4) + 0)) = 0x60060 = 393312
// RDAT: MinShaderTarget: 393312

RWByteAddressBuffer BAB : register(u1, space0);

[noinline] export
void deriv_in_func(float2 uv) {
  BAB.Store(0, ddx(uv));
}

// RDAT-LABEL: UnmangledName: "deriv_in_mesh"
// ShaderFeatureInfo_DerivativesInMeshAndAmpShaders (0x1000000) = 16777216
// RDAT18: FeatureInfo1: 16777216
// Old: missed called function
// RDAT17: FeatureInfo1: 0
// OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
// RDAT18:   FeatureInfo2: 256
// Old: deriv use not tracked
// RDAT17:   FeatureInfo2: 0
// Mesh(13) = 0x2000 = 8192
// RDAT: ShaderStageFlag: 8192
// MinShaderTarget: (Mesh(13) << 16) + (SM 6.6 ((6 << 4) + 6)) = 0xD0066 = 852070
// RDAT18: MinShaderTarget: 852070
// Old: 6.0
// RDAT17: MinShaderTarget: 852064

[shader("mesh")]
[numthreads(8, 8, 1)]
[outputtopology("triangle")]
void deriv_in_mesh(uint3 DTid : SV_DispatchThreadID) {
  float2 uv = DTid.xy/float2(8, 8);
  deriv_in_func(uv);
}

// RDAT-LABEL: UnmangledName: "deriv_in_compute"
// RDAT:   FeatureInfo1: 0
// OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
// RDAT18:   FeatureInfo2: 256
// Old: deriv use not tracked
// RDAT17:   FeatureInfo2: 0
// MinShaderTarget: (Compute(5) << 16) + (SM 6.6 ((6 << 4) + 6)) = 0x50066 = 327782
// Compute(5) = 0x20 = 32
// RDAT: ShaderStageFlag: 32
// RDAT18: MinShaderTarget: 327782
// Old: 6.0
// RDAT17: MinShaderTarget: 327776

[shader("compute")]
[numthreads(8, 8, 1)]
void deriv_in_compute(uint3 DTid : SV_DispatchThreadID) {
  float2 uv = DTid.xy/float2(8, 8);
  deriv_in_func(uv);
}

// RDAT-LABEL: UnmangledName: "deriv_in_pixel"
// RDAT:   FeatureInfo1: 0
// OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
// RDAT18:   FeatureInfo2: 256
// Old: deriv use not tracked
// RDAT17:   FeatureInfo2: 0
// Pixel(0) = 0x1 = 1
// RDAT: ShaderStageFlag: 1
// MinShaderTarget: (Pixel(0) << 16) + (SM 6.0 ((6 << 4) + 0)) = 0x60 = 96
// RDAT: MinShaderTarget: 96

[shader("pixel")]
void deriv_in_pixel(float2 uv : TEXCOORD) {
  deriv_in_func(uv);
}

// Make sure function-level derivative flag isn't in RequiredFeatureFlags,
// and make sure mesh shader sets required flag.

// RDAT-LABEL: ID3D12LibraryReflection:

// RDAT-LABEL: D3D12_FUNCTION_DESC: Name:
// RDAT-SAME: deriv_in_func
// RDAT: RequiredFeatureFlags: 0

// RDAT-LABEL: D3D12_FUNCTION_DESC: Name: deriv_in_compute
// RDAT: RequiredFeatureFlags: 0

// RDAT-LABEL: D3D12_FUNCTION_DESC: Name: deriv_in_mesh
// ShaderFeatureInfo_DerivativesInMeshAndAmpShaders (0x1000000) = 16777216
// RDAT18: RequiredFeatureFlags: 0x1000000
// Old: missed called function
// RDAT17: RequiredFeatureFlags: 0

// RDAT-LABEL: D3D12_FUNCTION_DESC: Name: deriv_in_pixel
// RDAT: RequiredFeatureFlags: 0
