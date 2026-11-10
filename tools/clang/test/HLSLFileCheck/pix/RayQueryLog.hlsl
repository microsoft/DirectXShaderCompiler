// RUN: %dxc -T cs_6_5 -E main %s | %opt -S -hlsl-dxil-pix-rayquery-log,maxNumEntriesInLog=24,traceSiteBase=7,subCallIndex=5,roiMinX=0,roiMinY=0,roiMinZ=0,roiMaxX=16,roiMaxY=16,roiMaxZ=1,sampleRate=2,logCandidates=1 | %FileCheck %s

// CHECK: PIX_RAYQUERY_LOG_V1
// CHECK: site id=7 local=0
// CHECK: END_PIX_RAYQUERY_LOG_V1
// CHECK-DAG: PIX_RayQuery_CountUAV_Handle
// CHECK-DAG: PIX_RayQuery_LogUAV_Handle
// CHECK-DAG: PIX_RayQuery_CandidateCountUAV_Handle
// CHECK-DAG: PIX_RayQuery_CandidateLogUAV_Handle
// CHECK-DAG: dx.op.atomicBinOp.i32
// CHECK-DAG: dx.op.rawBufferStore.i32
// CHECK-DAG: xor i32 826366246
// CHECK-DAG: 747796405
// CHECK-DAG: 277803737
// CHECK-DAG: icmp ult i32 %IrtSlot{{.*}}, 24
// CHECK-DAG: IrtSelectedSlot = select i1 %IrtShouldStore{{.*}}, i32 %IrtSlot{{.*}}, i32 24
// CHECK-DAG: mul i32 %IrtSelectedSlot{{.*}}, 48
// CHECK-DAG: mul i32 %IrtSelectedSlot{{.*}}, 64
// CHECK-DAG: IrtCandidateType = call i32 @dx.op.rayQuery_StateScalar.i32
// CHECK-DAG: IrtEncodedCandidateType = add i32 %IrtCandidateType, 1
// CHECK-DAG: IrtCandidateIsTriangle = icmp eq i32 %IrtCandidateType, 0
// CHECK-DAG: IrtCandidateBaryX = call float @dx.op.rayQuery_StateVector.f32
// CHECK-DAG: IrtCandidateIsProcedural = icmp eq i32 %IrtCandidateType, 1
// CHECK-DAG: or i32 %IrtEncodedCandidateType, 256
// CHECK: br i1 %IrtShouldStore{{.*}}, label %[[STORE:.*]], label %
// CHECK: ; <label>:[[STORE]]
// CHECK-NEXT: call void @dx.op.rawBufferStore.i32
// CHECK-DAG: IrtLoadedTraceInvocation
// CHECK-DAG: store i32 8, i32* %IrtTraceSiteId

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
  rayQuery.TraceRayInline(RTAS, 0, 0xff, rayDescription);
  if (rayQuery.Proceed())
  {
  }
  rayQuery.TraceRayInline(RTAS, 0, 0xff, rayDescription);
  rayQuery.Abort();
}
