// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT,RDAT18
// RUN: %dxilver 1.7 | %dxc -T lib_6_7 -validator-version 1.7 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT17

// Ensure min shader target incorporates optional features used

// RDAT: FunctionTable[{{.*}}] = {

// SM 6.1+

///////////////////////////////////////////////////////////////////////////////
// ShaderFeatureInfo_ViewID (0x10000) = 65536

// ViewID is loaded using an intrinsic, so prior validator already adjusted SM
// for it.

// RDAT-LABEL: UnmangledName: "viewid"
// ShaderFeatureInfo_ViewID (0x10000) = 65536
// RDAT:   FeatureInfo1: 65536
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Pixel(0) << 16) + (SM 6.1 ((6 << 4) + 1)) = 0x61 = 97
// RDAT: MinShaderTarget: 97

[shader("pixel")]
void viewid(uint vid : SV_ViewID, out float4 target : SV_Target) {
  target = vid;
}
