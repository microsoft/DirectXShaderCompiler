// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT,RDAT18
// RUN: %dxilver 1.7 | %dxc -T lib_6_7 -validator-version 1.7 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT17

// Ensure min shader target incorporates optional features used

// RDAT: FunctionTable[{{.*}}] = {

// SM 6.1+

///////////////////////////////////////////////////////////////////////////////
// ShaderFeatureInfo_Barycentrics (0x20000) = 131072

// RDAT-LABEL: UnmangledName: "bary1"
// RDAT:   FeatureInfo1: 131072
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Pixel(0) << 16) + (SM 6.1 ((6 << 4) + 1)) = 0x61 = 97
// RDAT: MinShaderTarget: 97

[shader("pixel")]
void bary1(float3 barycentrics : SV_Barycentrics, out float4 target : SV_Target) {
  target = float4(barycentrics, 1);
}

// RDAT-LABEL: UnmangledName: "bary2"
// RDAT:   FeatureInfo1: 131072
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Pixel(0) << 16) + (SM 6.1 ((6 << 4) + 1)) = 0x61 = 97
// RDAT: MinShaderTarget: 97

[shader("pixel")]
void bary2(nointerpolation float4 color : COLOR, out float4 target : SV_Target) {
  target = GetAttributeAtVertex(color, 1);
}
