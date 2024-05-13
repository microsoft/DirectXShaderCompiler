// RUN: %dxc -T lib_6_x %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT

// Ensure min shader target incorporates optional features used

// SM 6.9+

///////////////////////////////////////////////////////////////////////////////
// ShaderFeatureInfo_WaveMMA (0x8000000) = 134217728

RWByteAddressBuffer BAB : register(u1, space0);

// RDAT-LABEL: UnmangledName: "use_wavematrix"
// RDAT:   FeatureInfo1: (WaveOps | WaveMMA)
// RDAT:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (Compute | Library)
// RDAT:   MinShaderTarget: 0x60069

[noinline] export
void use_wavematrix() {
  // Use WaveMatrix in a minimal way that survives dead code elimination.
  WaveMatrixLeft<float, 16, 16> wml;
  wml.Fill(0);
  wml.Store(BAB, 0, 1024, false);
}

// RDAT-LABEL: UnmangledName: "call_use_wavematrix"
// RDAT:   FeatureInfo1: (WaveOps | WaveMMA)
// RDAT:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (Compute | Library)
// RDAT:   MinShaderTarget: 0x60069

[noinline] export
void call_use_wavematrix() {
  use_wavematrix();
}

// RDAT-LABEL: UnmangledName: "wavematrix_compute"
// RDAT:   FeatureInfo1: (WaveOps | WaveMMA)
// RDAT:   FeatureInfo2: 0
// RDAT:   ShaderStageFlag: (Compute)
// RDAT:   MinShaderTarget: 0x50069

[shader("compute")]
[numthreads(1,1,1)]
void wavematrix_compute(uint tidx : SV_GroupIndex) {
  call_use_wavematrix();
}
