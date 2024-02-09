// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT,RDAT18
// RUN: %dxilver 1.7 | %dxc -T lib_6_7 -validator-version 1.7 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT17


// Ensure min shader target incorporates optional features used

// RDAT: FunctionTable[{{.*}}] = {

// SM 6.7+

///////////////////////////////////////////////////////////////////////////////
// ShaderFeatureInfo_WriteableMSAATextures (0x40000000) = 1073741824

// RDAT-LABEL: UnmangledName: "rwmsaa"
// ShaderFeatureInfo_WriteableMSAATextures (0x40000000) = 1073741824
// RDAT:   FeatureInfo1: 1073741824
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Library(6) << 16) + (SM 6.7 ((6 << 4) + 7)) = 0x60067 = 393319
// RDAT18: MinShaderTarget: 393319
// Old: 6.0
// RDAT17: MinShaderTarget: 393312

RWByteAddressBuffer BAB : register(u1, space0);
RWTexture2DMS<float, 1> T2DMS : register(u2, space0);

[noinline] export
void rwmsaa() {
  BAB.Store(0, T2DMS.sample[1][uint2(1, 2)]);
}

// RDAT-LABEL: UnmangledName: "no_rwmsaa"
// RDAT18: FeatureInfo1: 0
// 1.7 Incorrectly reports feature use for function
// RDAT17: FeatureInfo1: 1073741824
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Library(6) << 16) + (SM 6.0 ((6 << 4) + 0)) = 0x60060 = 393312
// RDAT: MinShaderTarget: 393312

export void no_rwmsaa() {
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "rwmsaa_in_raygen"
// ShaderFeatureInfo_WriteableMSAATextures (0x40000000) = 1073741824
// Old: set because of global
// RDAT: FeatureInfo1: 1073741824
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (RayGeneration(7) << 16) + (SM 6.7 ((6 << 4) + 7)) = 0x70067 = 458855
// RDAT18: MinShaderTarget: 458855
// Old: 6.0
// RDAT17: MinShaderTarget: 458848

[shader("raygeneration")]
void rwmsaa_in_raygen() {
  rwmsaa();
}

// RDAT-LABEL: UnmangledName: "rwmsaa_not_used_in_raygen"
// ShaderFeatureInfo_WriteableMSAATextures (0x40000000) = 1073741824
// RDAT18: FeatureInfo1: 0
// Old: set because of global
// RDAT17: FeatureInfo1: 1073741824
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (RayGeneration(7) << 16) + (SM 6.3 ((6 << 4) + 3)) = 0x70063 = 458851
// RDAT18: MinShaderTarget: 458851
// Old: 6.0
// RDAT17: MinShaderTarget: 458848

[shader("raygeneration")]
void rwmsaa_not_used_in_raygen() {
  no_rwmsaa();
}
