// RUN: %dxc -T lib_6_9 %s | FileCheck %s 

RaytracingAccelerationStructure RTAS;
// DXR entry point to ensure RDAT flags match during validation.
[shader("vertex")]
void main(RayDesc rayDesc : RAYDESC) {

  // CHECK: call i32 @dx.op.allocateRayQuery2(i32 258, i32 1024, i32 1)  ; AllocateRayQuery2(constRayFlags,constRayQueryFlags)
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> rayQuery1;

  rayQuery1.TraceRayInline(RTAS, RAY_FLAG_FORCE_OMM_2_STATE, 2, rayDesc);
}
