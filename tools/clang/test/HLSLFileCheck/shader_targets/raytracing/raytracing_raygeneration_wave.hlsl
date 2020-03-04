// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK:   %[[_2_:[0-9]+]] = load %struct.RWByteAddressBuffer, %struct.RWByteAddressBuffer* @"\01?Log@@3URWByteAddressBuffer@@A", align 4
// CHECK:   %[[DispatchRaysIndex:DispatchRaysIndex|[0-9]+]] = call i32 @dx.op.dispatchRaysIndex.i32(i32 145, i8 0)
// CHECK:   %[[DispatchRaysIndex16:DispatchRaysIndex16|[0-9]+]] = call i32 @dx.op.dispatchRaysIndex.i32(i32 145, i8 1)
// CHECK:   %[[_4_:[0-9]+]] = call %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32 160, %struct.RWByteAddressBuffer %[[_2_]])
// CHECK:   %[[AtomicAdd:AtomicAdd|[0-9]+]] = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %[[_4_]], i32 0, i32 0, i32 undef, i32 undef, i32 8)
// CHECK:   %[[WaveActiveOp:WaveActiveOp|[0-9]+]] = call i64 @dx.op.waveActiveOp.i64(i32 119, i64 8, i8 0, i8 0)
// CHECK:   %[[conv:conv|[0-9]+]] = trunc i64 %[[WaveActiveOp]] to i32
// CHECK:   %[[add:add|[0-9]+]] = add i32 %[[conv]], %[[AtomicAdd]]
// CHECK:   %[[_5_:[0-9]+]] = call %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32 160, %struct.RWByteAddressBuffer %[[_2_]])
// CHECK:   call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %[[_5_]], i32 %[[add]], i32 undef, i32 %[[DispatchRaysIndex]], i32 undef, i32 undef, i32 undef, i8 1, i32 4)
// CHECK:   %[[add5:add5|[0-9]+]] = add i32 %[[add]], 4
// CHECK:   %[[_6_:[0-9]+]] = call %dx.types.Handle @dx.op.createHandleForLib.struct.RWByteAddressBuffer(i32 160, %struct.RWByteAddressBuffer %[[_2_]])
// CHECK:   call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %[[_6_]], i32 %[[add5]], i32 undef, i32 %[[DispatchRaysIndex16]], i32 undef, i32 undef, i32 undef, i8 1, i32 4)

struct MyPayload {
  float4 color;
  uint2 pos;
};

RaytracingAccelerationStructure RTAS : register(t5);

RWByteAddressBuffer Log;

[shader("raygeneration")]
void raygen1()
{
  MyPayload p = (MyPayload)0;
  p.pos = DispatchRaysIndex().xy;

  uint offset = 0;
  Log.InterlockedAdd(0, 8, offset);
  offset += WaveActiveSum(8);
  Log.Store(offset, p.pos.x);
  Log.Store(offset + 4, p.pos.y);

  float3 origin = {0, 0, 0};
  float3 dir = normalize(float3(p.pos / (float)DispatchRaysDimensions(), 1));
  RayDesc ray = { origin, 0.125, dir, 128.0};
  TraceRay(RTAS, RAY_FLAG_NONE, 0, 0, 1, 0, ray, p);
}
