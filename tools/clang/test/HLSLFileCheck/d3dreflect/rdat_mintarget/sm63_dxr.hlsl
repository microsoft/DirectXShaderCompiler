// RUN: %dxilver 1.8 | %dxc -T lib_6_8 %s | %D3DReflect %s | %FileCheck %s -check-prefixes=RDAT,RDAT18
// RUN: %dxilver 1.7 | %dxc -T lib_6_7 -validator-version 1.7 %s | %D3DReflect %s | FileCheck %s -check-prefixes=RDAT,RDAT17

// Ensure min shader target incorporates shader stage of entry function
// These must be minimal shaders since intrinsic usage associated with the
// shader stage will cause the min target to be set that way.

// This covers raytracing entry points, which should always be SM 6.3+

// RDAT: FunctionTable[{{.*}}] = {

RWByteAddressBuffer BAB : register(u1, space0);

// RDAT-LABEL: UnmangledName: "raygen"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (RayGeneration(7) << 16) + (SM 6.3 ((6 << 4) + 3)) = 0x70063 = 458851
// RDAT18: MinShaderTarget: 458851
// Old: 6.0
// RDAT17: MinShaderTarget: 458848

[shader("raygeneration")]
void raygen() {
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "intersection"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Intersection(8) << 16) + (SM 6.3 ((6 << 4) + 3)) = 0x80063 = 524387
// RDAT18: MinShaderTarget: 524387
// Old: 6.0
// RDAT17: MinShaderTarget: 524384

[shader("intersection")]
void intersection() {
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "anyhit"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (AnyHit(9) << 16) + (SM 6.3 ((6 << 4) + 3)) = 0x90063 = 589923
// RDAT18: MinShaderTarget: 589923
// Old: 6.0
// RDAT17: MinShaderTarget: 589920

struct [raypayload] MyPayload {
  float2 loc : write(caller) : read(caller);
};

[shader("anyhit")]
void anyhit(inout MyPayload payload : SV_RayPayload,
    in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes ) {
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "closesthit"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (ClosestHit(10) << 16) + (SM 6.3 ((6 << 4) + 3)) = 0xA0063 = 655459
// RDAT18: MinShaderTarget: 655459
// Old: 6.0
// RDAT17: MinShaderTarget: 655456

[shader("closesthit")]
void closesthit(inout MyPayload payload : SV_RayPayload,
    in BuiltInTriangleIntersectionAttributes attr : SV_IntersectionAttributes ) {
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "miss"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Miss(11) << 16) + (SM 6.3 ((6 << 4) + 3)) = 0xB0063 = 720995
// RDAT18: MinShaderTarget: 720995
// Old: 6.0
// RDAT17: MinShaderTarget: 720992

[shader("miss")]
void miss(inout MyPayload payload : SV_RayPayload) {
  BAB.Store(0, 0);
}

// RDAT-LABEL: UnmangledName: "callable"
// RDAT:   FeatureInfo1: 0
// RDAT:   FeatureInfo2: 0
// MinShaderTarget: (Callable(12) << 16) + (SM 6.3 ((6 << 4) + 3)) = 0xC0063 = 786531
// RDAT18: MinShaderTarget: 786531
// Old: 6.0
// RDAT17: MinShaderTarget: 786528

[shader("callable")]
void callable(inout MyPayload param) {
  BAB.Store(0, 0);
}
