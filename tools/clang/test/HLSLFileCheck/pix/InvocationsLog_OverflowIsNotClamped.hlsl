// RUN: %dxc -Tlib_6_6 %s | %opt -S -hlsl-dxil-pix-dxr-invocations-log,maxNumEntriesInLog=100 | %FileCheck %s

// Each invocation claims one log slot.
// The counter keeps counting past the log capacity.
// The stores execute only when the claimed slot is in range.

// CHECK: [[ENTRYINDEX:%EntryIndexResult[0-9]*]] = call i32 @dx.op.atomicBinOp.i32(i32 78,
// CHECK: [[INRANGE:%EntryIndexIsInRange[0-9]*]] = icmp ult i32 [[ENTRYINDEX]], 100
// CHECK: br i1 [[INRANGE]]
// CHECK: mul i32 [[ENTRYINDEX]], 52
// CHECK: call void @dx.op.bufferStore.i32
// CHECK: call void @dx.op.bufferStore.f32
// CHECK: call void @dx.op.bufferStore.f32
// CHECK: call void @dx.op.bufferStore.i32

// UMin is not part of this shader.
// CHECK-NOT: @dx.op.binary.i32(i32 40
// CHECK-NOT: declare i32 @dx.op.binary.i32

struct Payload
{
    float4 color;
};

struct Attribs
{
    float2 barycentrics;
};

RaytracingAccelerationStructure scene : register(t0);
RWTexture2D<float4> output : register(u0);

[shader("raygeneration")]
void RayGen()
{
    RayDesc ray;
    ray.Origin = float3(0, 0, 0);
    ray.Direction = float3(0, 0, 1);
    ray.TMin = 0.001f;
    ray.TMax = 1000.f;
    Payload payload;
    payload.color = float4(0, 0, 0, 0);
    TraceRay(scene, RAY_FLAG_NONE, ~0, 0, 1, 0, ray, payload);
    output[DispatchRaysIndex().xy] = payload.color;
}

[shader("closesthit")]
void ClosestHit(inout Payload payload, in Attribs attribs)
{
    payload.color = float4(attribs.barycentrics, 0, 1);
}

[shader("miss")]
void Miss(inout Payload payload)
{
    payload.color = float4(1, 0, 0, 1);
}
