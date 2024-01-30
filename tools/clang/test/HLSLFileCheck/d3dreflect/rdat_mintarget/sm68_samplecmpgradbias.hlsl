// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT

// Ensure min shader target incorporates optional features used

// RDAT: FunctionTable[{{.*}}] = {

// SM 6.8+

///////////////////////////////////////////////////////////////////////////////
// ShaderFeatureInfo_SampleCmpGradientOrBias (0x80000000) = 2147483648

Texture2D<float4> T2D : register(t0, space0);
SamplerComparisonState SampCmp : register(s0, space0);
RWByteAddressBuffer BAB : register(u1, space0);

// RDAT-LABEL: UnmangledName: "samplecmpgrad"
// ShaderFeatureInfo_SampleCmpGradientOrBias (0x80000000) = 2147483648
// RDAT:   FeatureInfo1: 2147483648
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Library(6) << 16) + (SM 6.8 ((6 << 4) + 8)) = 0x60068 = 393320
// RDAT: MinShaderTarget: 393320

[noinline] export
void samplecmpgrad() {
  // Use SampleCmpGrad in a minimal way that survives dead code elimination.
  float4 tex = T2D.SampleCmpGrad(
      SampCmp, float2(0.5, 0.5), 0.5,
      float2(0.005, 0.005), float2(0.005, 0.005));
  BAB.Store(0, tex.x);
}

// RDAT-LABEL: UnmangledName: "samplecmpgrad_compute"
// ShaderFeatureInfo_SampleCmpGradientOrBias (0x80000000) = 2147483648
// RDAT:   FeatureInfo1: 2147483648
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Compute(5) << 16) + (SM 6.8 ((6 << 4) + 8)) = 0x50068 = 327784
// RDAT: MinShaderTarget: 327784

[shader("compute")]
[numthreads(1,1,1)]
void samplecmpgrad_compute(uint tidx : SV_GroupIndex) {
  samplecmpgrad();
}

// RDAT-LABEL: UnmangledName: "samplecmpbias"
// ShaderFeatureInfo_SampleCmpGradientOrBias (0x80000000) = 2147483648
// RDAT:   FeatureInfo1: 2147483648
// OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
// RDAT18:   FeatureInfo2: 256
// Old: deriv use not tracked
// RDAT17:   FeatureInfo2: 0
// MinShaderTarget: (Library(6) << 16) + (SM 6.8 ((6 << 4) + 8)) = 0x60068 = 393320
// RDAT: MinShaderTarget: 393320

[noinline] export
void samplecmpbias() {
  // Use SampleCmpGrad in a minimal way that survives dead code elimination.
  float4 tex = T2D.SampleCmpBias(SampCmp, float2(0.5, 0.5), 0.5, 0.5);
  BAB.Store(0, tex.x);
}

// RDAT-LABEL: UnmangledName: "samplecmpbias_compute"
// ShaderFeatureInfo_SampleCmpGradientOrBias (0x80000000) = 2147483648
// RDAT:   FeatureInfo1: 2147483648
// OptFeatureInfo_UsesDerivatives (0x0000010000000000) = FeatureInfo2: 256
// RDAT18:   FeatureInfo2: 256
// Old: deriv use not tracked
// RDAT17:   FeatureInfo2: 0
// MinShaderTarget: (Compute(5) << 16) + (SM 6.8 ((6 << 4) + 8)) = 0x50068 = 327784
// RDAT: MinShaderTarget: 327784

[shader("compute")]
[numthreads(1,1,1)]
void samplecmpbias_compute(uint tidx : SV_GroupIndex) {
  samplecmpbias();
}
