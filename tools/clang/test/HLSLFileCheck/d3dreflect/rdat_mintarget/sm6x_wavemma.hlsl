// RUN: %dxc -T lib_6_x %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT

// Ensure min shader target incorporates optional features used

// SM 6.9+

///////////////////////////////////////////////////////////////////////////////
// ShaderFeatureInfo_WaveMMA (0x8000000) = 134217728

RWByteAddressBuffer BAB : register(u1, space0);

// RDAT-LABEL: UnmangledName: "use_wavematrix"
// ShaderFeatureInfo_WaveMMA (0x8000000) = 134217728
// + WaveOps (0x4000) = 0x8004000 = 134234112
// RDAT:   FeatureInfo1: 134234112
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Library(6) << 16) + (SM 6.9 ((6 << 4) + 9)) = 0x60069 = 393321
// RDAT: MinShaderTarget: 393321

[noinline] export
void use_wavematrix() {
  // Use WaveMatrix in a minimal way that survives dead code elimination.
  WaveMatrixLeft<float, 16, 16> wml;
  wml.Fill(0);
  wml.Store(BAB, 0, 1024, false);
}

// RDAT-LABEL: UnmangledName: "call_use_wavematrix"
// ShaderFeatureInfo_WaveMMA (0x8000000) = 134217728
// + WaveOps (0x4000) = 0x8004000 = 134234112
// RDAT:   FeatureInfo1: 134234112
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Library(6) << 16) + (SM 6.9 ((6 << 4) + 9)) = 0x60069 = 393321
// RDAT: MinShaderTarget: 393321

[noinline] export
void call_use_wavematrix() {
  use_wavematrix();
}

// RDAT-LABEL: UnmangledName: "wavematrix_compute"
// ShaderFeatureInfo_WaveMMA (0x8000000) = 134217728
// + WaveOps (0x4000) = 0x8004000 = 134234112
// RDAT:   FeatureInfo1: 134234112
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Compute(5) << 16) + (SM 6.9 ((6 << 4) + 9)) = 0x50069 = 327785
// RDAT: MinShaderTarget: 327785

[shader("compute")]
[numthreads(1,1,1)]
void wavematrix_compute(uint tidx : SV_GroupIndex) {
  call_use_wavematrix();
}
