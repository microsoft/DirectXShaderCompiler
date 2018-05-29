// RUN: %dxc -ECSMain -Tcs_6_2 %s | %opt -S -hlsl-dxil-pix-shader-access-instrumentation,config=S0:1:1i1;U0:2:10i0;.. | %FileCheck %s

// Check we added the UAV:
// CHECK:  %PIX_CountUAV_Handle = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 1, i32 0, i1 false)

// check for correct out-of-bounds calculation
// CHECK: CompareWithSlotLimit = icmp uge i32
// CHECK: CompareWithSlotLimitAsUint = zext i1 %CompareWithSlotLimit to i32
// CHECK: IsInBounds = sub i32 1, %CompareWithSlotLimitAsUint
// CHECK: SlotOffset = add i32
// CHECK: slotIndex = mul i32

// Check for udpate of UAV:
// CHECK: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %bufferArray_UAV_rawbuf, i32 0, i32
// CHECKL %UAVOrResult1 = call i32 @dx.op.atomicBinOp.i32(i32 78, %dx.types.Handle %PIX_CountUAV_Handle, i32 2

ByteAddressBuffer inBuffer : register(t0);
RWByteAddressBuffer bufferArray[] : register(u0);

[numthreads(1, 1, 1)]
void CSMain()
{
  // Simple read
  uint dynamicBufferIndex = inBuffer.Load(0);

  // Dynamically indexed write
  bufferArray[dynamicBufferIndex].Store(0, 1);
}