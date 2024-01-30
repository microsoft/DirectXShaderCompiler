// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT,RDAT18
// RUN: %dxilver 1.7 | %dxc -T lib_6_7 -validator-version 1.7 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT17

// Ensure min shader target incorporates optional features used

// RDAT: FunctionTable[{{.*}}] = {

// SM 6.7+

///////////////////////////////////////////////////////////////////////////////
// ShaderFeatureInfo_WriteableMSAATextures (0x40000000) = 1073741824

// RDAT-LABEL: UnmangledName: "rwmsaa"
// ShaderFeatureInfo_WriteableMSAATextures (0x40000000) = 1073741824
// ResourceDescriptorHeapIndexing (0x2000000) = 33554432
// 0x40000000 + 0x2000000 = 0x42000000 = 1107296256
// RDAT18:   FeatureInfo1: 1107296256
// Old: missed use of WriteableMSAATextures
// RDAT17:   FeatureInfo1: 33554432
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Library(6) << 16) + (SM 6.7 ((6 << 4) + 7)) = 0x60067 = 393319
// RDAT18: MinShaderTarget: 393319
// Old: 6.6
// RDAT17: MinShaderTarget: 393318

RWByteAddressBuffer BAB : register(u1, space0);

[noinline] export
void rwmsaa() {
  // Use dynamic resource to avoid MSAA flag on all functions issue in 1.7
  RWTexture2DMS<float, 1> T2DMS = ResourceDescriptorHeap[0];
  uint3 whs;
  T2DMS.GetDimensions(whs.x, whs.y, whs.z);
  BAB.Store(0, whs);
}

// RDAT-LABEL: UnmangledName: "rwmsaa_in_raygen"
// ShaderFeatureInfo_WriteableMSAATextures (0x40000000) = 1073741824
// + ResourceDescriptorHeapIndexing (0x2000000) = 0x42000000 = 1107296256
// RDAT18: FeatureInfo1: 1107296256
// Old: missed called function
// RDAT17: FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (RayGeneration(7) << 16) + (SM 6.7 ((6 << 4) + 7)) = 0x70067 = 458855
// RDAT18: MinShaderTarget: 458855
// Old: 6.0
// RDAT17: MinShaderTarget: 458848

[shader("raygeneration")]
void rwmsaa_in_raygen() {
  rwmsaa();
}
