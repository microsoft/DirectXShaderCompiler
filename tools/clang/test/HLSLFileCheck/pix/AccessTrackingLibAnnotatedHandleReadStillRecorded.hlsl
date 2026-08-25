// RUN: %dxc -T lib_6_6 -Od %s | %opt -S -hlsl-dxil-pix-shader-access-instrumentation,config=S0:1:1i0;U0:2:1i0;.256;512;1024. | %FileCheck %s

// annotateHandle is not an access, but a genuine access that uses an
// annotated handle still records the resource class from that annotation.
//
// Offsets with this config (SRV space 0 at slot 1, UAV space 0 at slot 2,
// three dwords per slot, descriptor-heap records at byte 256):
//   g_input read      slot 1, read dword    -> 12
//   g_input write     slot 1, write dword   -> 16   (must not appear)
//   g_output write    slot 2, write dword   -> 28
//   heapTexture read  descriptor 3          -> 292
//
// A descriptor-heap record encodes shader kind in its top four bits and
// ResourceAccessStyle in the next four. RayGeneration is 7 and SRVRead is
// 5, so 0x75000000 == 1962934272.

// CHECK-NOT: bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 16,
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 12,
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 292, i32 undef, i32 1962934272,
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 28,
// CHECK-NOT: bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 16,

ByteAddressBuffer g_input : register(t0);
RWByteAddressBuffer g_output : register(u0);

[shader("raygeneration")]
void RayGen()
{
    Texture2D<float4> heapTexture = ResourceDescriptorHeap[3];
    uint value = g_input.Load(0);
    value += asuint(heapTexture.Load(int3(0, 0, 0)).x);
    g_output.Store(0, value);
}
