// RUN: %dxc -T vs_6_5 -E main %s | FileCheck %s

// CHECK: %[[RTAS:[^ ]+]] = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 0, i32 0, i32 0, i1 false)
// CHECK: %[[RQ:[^ ]+]] = call i32 @dx.op.allocateRayQuery(i32 168, i32 1)
// CHECK: call void @dx.op.traceRayInline(i32 169, i32 %[[RQ]], %dx.types.Handle %[[RTAS]], i32 0, i32 1,
// CHECK: call void @dx.op.traceRayInline(i32 169, i32 %[[RQ]], %dx.types.Handle %[[RTAS]], i32 1, i32 2,

RaytracingAccelerationStructure RTAS;

void DoTrace(RayQuery<RAY_FLAG_FORCE_OPAQUE> rayQuery, RayDesc rayDesc) {
  rayQuery.TraceRayInline(RTAS, 0, 1, rayDesc);
}

float main(RayDesc rayDesc : RAYDESC) : OUT {
  RayQuery<RAY_FLAG_FORCE_OPAQUE> rayQuery;
  DoTrace(rayQuery, rayDesc);
  rayQuery.TraceRayInline(RTAS, 1, 2, rayDesc);
  return 0;
}
