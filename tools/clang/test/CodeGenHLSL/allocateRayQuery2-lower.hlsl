// RUN: %dxc -T lib_6_9 -fcgl %s | FileCheck %s 

// CHECK: %"class.RayQuery<1024, 1>" = type { i32 }
// CHECK: %"class.RayQuery<1, 0>" = type { i32 }

RaytracingAccelerationStructure RTAS;
[shader("vertex")]
void main(RayDesc rayDesc : RAYDESC) {

  // CHECK: alloca %"class.RayQuery<1024, 1>", align 4
  // CHECK: alloca %"class.RayQuery<1, 0>", align 4

  // CHECK: call i32 @"dx.hl.op..i32 (i32, i32, i32)"(i32 [[ARQ:[0-9]*]], i32 1024, i32 1)
  RayQuery<RAY_FLAG_FORCE_OMM_2_STATE, RAYQUERY_FLAG_ALLOW_OPACITY_MICROMAPS> rayQuery1;

  rayQuery1.TraceRayInline(RTAS, RAY_FLAG_FORCE_OMM_2_STATE, 2, rayDesc);

  // CHECK: call i32 @"dx.hl.op..i32 (i32, i32, i32)"(i32 [[ARQ]], i32 1, i32 0)
  RayQuery<RAY_FLAG_FORCE_OPAQUE> rayQuery2;
  rayQuery2.TraceRayInline(RTAS, 0, 2, rayDesc);
}
