// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT,RDAT18
// RUN: %dxilver 1.7 | %dxc -T lib_6_7 -validator-version 1.7 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT17

// Ensure min shader target incorporates optional features used

// RDAT: FunctionTable[{{.*}}] = {

// SM 6.6+

///////////////////////////////////////////////////////////////////////////////
// ShaderFeatureInfo_ResourceDescriptorHeapIndexing (0x2000000) = 33554432

// RDAT-LABEL: UnmangledName: "res_heap_index"
// ShaderFeatureInfo_ResourceDescriptorHeapIndexing (0x2000000) = 33554432
// RDAT: FeatureInfo1: 33554432
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Library(6) << 16) + (SM 6.6 ((6 << 4) + 6)) = 0x60066 = 393318
// RDAT: MinShaderTarget: 393318

[noinline] export
void res_heap_index(int i) {
  RWStructuredBuffer<int> SB = ResourceDescriptorHeap[0];
  SB[0] = i;
}

// RDAT-LABEL: UnmangledName: "res_heap_index_in_compute"
// ShaderFeatureInfo_ResourceDescriptorHeapIndexing (0x2000000) = 33554432
// RDAT18: FeatureInfo1: 33554432
// Old: missed called function
// RDAT17: FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Compute(5) << 16) + (SM 6.6 ((6 << 4) + 6)) = 0x50066 = 327782
// RDAT18: MinShaderTarget: 327782
// Old: 6.0
// RDAT17: MinShaderTarget: 327776

[shader("compute")]
[numthreads(8, 8, 1)]
void res_heap_index_in_compute(uint3 DTid : SV_DispatchThreadID) {
  res_heap_index(DTid.x);
}

// RDAT-LABEL: UnmangledName: "res_heap_index_in_raygen"
// ShaderFeatureInfo_ResourceDescriptorHeapIndexing (0x2000000) = 33554432
// RDAT18: FeatureInfo1: 33554432
// Old: missed called function
// RDAT17: FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (RayGeneration(7) << 16) + (SM 6.6 ((6 << 4) + 6)) = 0x70066 = 458854
// RDAT18: MinShaderTarget: 458854
// Old: 6.0
// RDAT17: MinShaderTarget: 458848

[shader("raygeneration")]
void res_heap_index_in_raygen() {
  res_heap_index(1);
}

///////////////////////////////////////////////////////////////////////////////
// ShaderFeatureInfo_SamplerDescriptorHeapIndexing (0x4000000) = 67108864

// RDAT-LABEL: UnmangledName: "samp_heap_index"
// ShaderFeatureInfo_SamplerDescriptorHeapIndexing (0x4000000) = 67108864
// RDAT: FeatureInfo1: 67108864
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Library(6) << 16) + (SM 6.6 ((6 << 4) + 6)) = 0x60066 = 393318
// RDAT: MinShaderTarget: 393318

RWByteAddressBuffer BAB : register(u1, space0);
Texture2D<float4> T2D : register(t0, space0);

[noinline] export void samp_heap_index(int i) {
  SamplerState S = SamplerDescriptorHeap[i];
  BAB.Store(0, T2D.SampleLevel(S, float2(0.5, 0.5), 0.0));
}

// RDAT-LABEL: UnmangledName: "samp_heap_index_in_compute"
// ShaderFeatureInfo_SamplerDescriptorHeapIndexing (0x4000000) = 67108864
// RDAT18: FeatureInfo1: 67108864
// Old: missed called function
// RDAT17: FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Compute(5) << 16) + (SM 6.6 ((6 << 4) + 6)) = 0x50066 = 327782
// RDAT18: MinShaderTarget: 327782
// Old: 6.0
// RDAT17: MinShaderTarget: 327776

[shader("compute")]
[numthreads(8, 8, 1)]
void samp_heap_index_in_compute(uint3 DTid : SV_DispatchThreadID) {
  samp_heap_index(DTid.x);
}

// RDAT-LABEL: UnmangledName: "samp_heap_index_in_raygen"
// ShaderFeatureInfo_SamplerDescriptorHeapIndexing (0x4000000) = 67108864
// RDAT18: FeatureInfo1: 67108864
// Old: missed called function
// RDAT17: FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (RayGeneration(7) << 16) + (SM 6.6 ((6 << 4) + 6)) = 0x70066 = 458854
// RDAT18: MinShaderTarget: 458854
// Old: 6.0
// RDAT17: MinShaderTarget: 458848

[shader("raygeneration")]
void samp_heap_index_in_raygen() {
  samp_heap_index(1);
}
