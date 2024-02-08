// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT,RDAT18
// RUN: %dxilver 1.7 | %dxc -T lib_6_7 -validator-version 1.7 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT17

// Ensure min shader target incorporates optional features used

// RDAT: FunctionTable[{{.*}}] = {

// SM 6.7+

///////////////////////////////////////////////////////////////////////////////
// ShaderFeatureInfo_AdvancedTextureOps (0x20000000) = 536870912

// RDAT-LABEL: UnmangledName: "sample_offset"
// ShaderFeatureInfo_AdvancedTextureOps (0x20000000) = 536870912
// RDAT: FeatureInfo1: 536870912
// OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
// RDAT18:   FeatureInfo2: 256
// Old: deriv use not tracked
// RDAT17:   FeatureInfo2: 0
// MinShaderTarget: (Library(6) << 16) + (SM 6.7 ((6 << 4) + 7)) = 0x60067 = 393319
// RDAT18: MinShaderTarget: 393319
// Old: 6.0
// RDAT17: MinShaderTarget: 393312

Texture2D<float4> T2D : register(t0, space0);
SamplerState Samp : register(s0, space0);
RWByteAddressBuffer BAB : register(u1, space0);

[noinline] export
void sample_offset(float2 uv, int2 offsets) {
  BAB.Store(0, T2D.Sample(Samp, uv, offsets));
}

// RDAT-LABEL: UnmangledName: "sample_offset_pixel"
// ShaderFeatureInfo_AdvancedTextureOps (0x20000000) = 536870912
// RDAT18: FeatureInfo1: 536870912
// Old: missed called function
// RDAT17: FeatureInfo1: 0
// OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
// RDAT18:   FeatureInfo2: 256
// Old: deriv use not tracked
// RDAT17:   FeatureInfo2: 0
// MinShaderTarget: (Pixel(0) << 16) + (SM 6.7 ((6 << 4) + 7)) = 0x67 = 103
// RDAT18: MinShaderTarget: 103
// Old: 6.0
// RDAT17: MinShaderTarget: 96

[shader("pixel")]
void sample_offset_pixel(float4 color : COLOR) {
  sample_offset(color.xy, (int)color.zw);
}
