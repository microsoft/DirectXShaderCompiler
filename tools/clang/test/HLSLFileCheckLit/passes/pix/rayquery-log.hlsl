// RUN: %dxc -T cs_6_5 -E main %s | %dxopt -S -hlsl-dxil-pix-rayquery-log,maxNumEntriesInLog=24,traceSiteBase=7,subCallIndex=5,roiMinX=0,roiMinY=0,roiMinZ=0,roiMaxX=16,roiMaxY=16,roiMaxZ=1,sampleRate=2,logCandidates=1 | FileCheck %s
// RUN: %dxc -T cs_6_5 -E main %s | not %dxopt -S -hlsl-dxil-pix-rayquery-log,maxNumEntriesInLog=24,traceSiteBase=65536 2>&1 | FileCheck %s --check-prefix=OVERFLOW

// CHECK: PIX_RAYQUERY_LOG_V1
// CHECK: site id=7 local=0
// CHECK: END_PIX_RAYQUERY_LOG_V1
// CHECK-DAG: PIX_RayQuery_CountUAV_Handle
// CHECK-DAG: PIX_RayQuery_LogUAV_Handle
// CHECK-DAG: PIX_RayQuery_CandidateCountUAV_Handle
// CHECK-DAG: PIX_RayQuery_CandidateLogUAV_Handle
// CHECK-NOT: IrtRayQueryHandle
// CHECK-DAG: xor i32 826366246
// CHECK-DAG: 747796405
// CHECK-DAG: 277803737
// CHECK-DAG: atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_RayQuery_CountUAV_Handle, i32 0, i32 0
// CHECK-DAG: atomicBinOp.i32(i32 78, %dx.types.Handle {{.*}}, i32 7, i32 0
// CHECK-DAG: atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_RayQuery_CountUAV_Handle, i32 0, i32 4
// CHECK-DAG: atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_RayQuery_CandidateCountUAV_Handle, i32 6, i32 4
// CHECK-DAG: IrtShouldStore{{[0-9]*}} = icmp ult i32 %IrtSlot{{.*}}, 24
// CHECK-DAG: IrtRecordOffset{{[0-9]*}} = mul i32 %IrtSlot{{.*}}, 48
// CHECK-DAG: IrtRecordOffset{{[0-9]*}} = mul i32 %IrtSlot{{.*}}, 64
// CHECK-DAG: store i32 513, i32* %IrtEffectiveRayFlags
// CHECK-DAG: IrtCandidateType = call i32 @dx.op.rayQuery_StateScalar.i32
// CHECK-DAG: IrtEncodedCandidateType = add i32 %IrtCandidateType, 1
// CHECK: IrtCandidateIsTriangle = icmp eq i32 %IrtCandidateType, 0
// CHECK: br i1 %IrtCandidateIsTriangle
// CHECK: IrtCandidateT = call float @dx.op.rayQuery_StateScalar.f32
// CHECK: IrtCandidateBaryX = call float @dx.op.rayQuery_StateVector.f32
// CHECK: IrtCandidateBaryY = call float @dx.op.rayQuery_StateVector.f32
// CHECK: IrtCandidateIsProcedural = icmp eq i32 %IrtCandidateType, 1
// CHECK: br i1 %IrtCandidateIsProcedural
// CHECK: IrtCandidateProceduralNonOpaque = call i1 @dx.op.rayQuery_StateScalar.i1
// CHECK: or i32 %IrtEncodedCandidateType, 256
// CHECK: br i1 %IrtShouldStore{{.*}}, label %[[STORE:.*]], label %
// CHECK: ; <label>:[[STORE]]
// CHECK-NEXT: call void @dx.op.rawBufferStore.i32
// CHECK-DAG: IrtLoadedTraceInvocation
// CHECK-DAG: IrtCommittedStatus = call i32 @dx.op.rayQuery_StateScalar.i32
// CHECK-DAG: IrtCommittedT = call float @dx.op.rayQuery_StateScalar.f32
// CHECK-DAG: IrtHasCommittedHit = icmp ne i32 %IrtCommittedStatus, 0
// CHECK-DAG: br i1 %IrtHasCommittedHit
// CHECK-DAG: IrtInstanceIndex{{[0-9]*}} = call i32 @dx.op.rayQuery_StateScalar.i32
// OVERFLOW: Operation failed

RaytracingAccelerationStructure RTAS : register(t0);

[numthreads(1, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
  RayQuery<RAY_FLAG_FORCE_OPAQUE> rayQuery;
  RayDesc rayDescription;
  rayDescription.Origin = float3(0.0, 0.0, 0.0);
  rayDescription.Direction = float3(0.0, 0.0, 1.0);
  rayDescription.TMin = 0.0;
  rayDescription.TMax = 100.0;
  rayQuery.TraceRayInline(RTAS, RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES, 0xff, rayDescription);
  if (rayQuery.Proceed())
  {
  }
  rayQuery.TraceRayInline(RTAS, 0, 0xff, rayDescription);
  rayQuery.Abort();
}
