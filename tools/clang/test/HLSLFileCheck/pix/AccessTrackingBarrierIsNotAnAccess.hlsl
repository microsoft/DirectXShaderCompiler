// RUN: %dxc -T cs_6_8 -E main -Od %s | %opt -S -hlsl-dxil-pix-shader-access-instrumentation,config=U0:0:2i0;.0;256;512. | %FileCheck %s

// Barrier() on a resource handle orders accesses to that resource. It is
// not itself an access.
//
// The config puts the UAVs of space 0 at slot 0 onwards, so g_out is slot
// 0 and g_rw is slot 1. A slot is three dwords, so g_out's write dword is
// at byte 4 and g_rw's write dword is at byte 16.

// g_rw is only barriered, never accessed, so nothing is recorded against
// it.
// CHECK-NOT: bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 16,

// The store to g_out is a genuine write and is recorded.
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 4,

// CHECK-NOT: bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 16,

RWByteAddressBuffer g_out : register(u0);
RWTexture2D<float4> g_rw : register(u1);

[numthreads(1, 1, 1)]
void main(uint index : SV_GroupIndex)
{
    Barrier(g_rw, DEVICE_SCOPE);
    g_out.Store(0, 1);
}
