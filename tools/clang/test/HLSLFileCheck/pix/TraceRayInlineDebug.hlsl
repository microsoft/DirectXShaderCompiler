// RUN: %dxc -T vs_6_5 -Od -E main %s | %opt -S -dxil-annotate-with-virtual-regs -hlsl-dxil-debug-instrumentation | FileCheck %s

// CHECK: call void @dx.op.rayQuery_TraceRayInline
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle

RaytracingAccelerationStructure RTAS;
RWStructuredBuffer<int> UAV : register(u0);

float main(RayDesc rayDesc
           : RAYDESC) : OUT {
  RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rayQuery0;
  RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rayQuery1;
  rayQuery0.TraceRayInline(RTAS, 1, 2, rayDesc);
  rayQuery1.TraceRayInline(RTAS, 1, 2, rayDesc);
  RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> usedQuery = rayQuery0;
  if (UAV[0] == 1)
    usedQuery = rayQuery1;
  UAV[1] = usedQuery.CandidatePrimitiveIndex();
  rayQuery0.CommitNonOpaqueTriangleHit();
  if (rayQuery0.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
    UAV[4] = 42;
  }

  return 0;
}
