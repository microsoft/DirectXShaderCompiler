// RUN: %dxc -T cs_6_5 -E main %s | %dxopt -S -hlsl-dxil-pix-rayquery-log,maxNumEntriesInLog=8,roiMinX=2,roiMaxX=5,sampleRate=2 | FileCheck %s

// Events filtered out by ROI, sampling, or an inactive emit must branch around
// the counter atomics and record stores instead of executing them with a zero
// increment.

// CHECK-NOT: @dx.op.atomicBinOp
// CHECK: urem i32 %{{[0-9]+}}, 2
// CHECK: %IrtActiveWrite = and i1
// CHECK-NOT: @dx.op.atomicBinOp
// CHECK: br i1 %IrtActiveWrite, label %[[WRITE:[0-9]+]], label %[[SKIP:[0-9]+]]
// CHECK: ; <label>:[[WRITE]]
// CHECK-NOT: ; <label>
// CHECK: %IrtDemand = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_RayQuery_CountUAV_Handle, i32 0, i32 0, i32 undef, i32 undef, i32 1)
// CHECK-NOT: ; <label>
// CHECK: %IrtSlot = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_RayQuery_CountUAV_Handle, i32 0, i32 4, i32 undef, i32 undef, i32 1)
// CHECK-NOT: ; <label>
// CHECK: br i1 %IrtShouldStore, label %[[STORE:[0-9]+]], label %[[STORE_JOIN:[0-9]+]]
// CHECK: ; <label>:[[STORE]]
// CHECK-NOT: ; <label>
// CHECK: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %PIX_RayQuery_LogUAV_Handle
// CHECK: ; <label>:[[STORE_JOIN]]
// CHECK-NEXT: br label %[[SKIP]]
// CHECK: ; <label>:[[SKIP]]

RaytracingAccelerationStructure Scene : register(t0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    RayDesc ray;
    ray.Origin = float3((float)dispatchThreadId.x, 0, 0);
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.0f;
    ray.TMax = 100.0f;

    RayQuery<RAY_FLAG_NONE> query;
    query.TraceRayInline(Scene, RAY_FLAG_NONE, 0xFF, ray);
    query.Proceed();
}
