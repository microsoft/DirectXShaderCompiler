// RUN: %dxc -T lib_6_6 -Od %s | %opt -S -hlsl-dxil-pix-shader-access-instrumentation,config=S0:1:1i0;U0:2:1i0;.0;0;0. | %FileCheck %s

// annotateHandle attaches type information to a handle. It is not a
// memory operation. g_untouched is only passed to GetDimensions, which
// this pass skips, so the annotation is that resource's only handle use.
// Nothing is recorded against it.
//
// The config puts the SRV of space 0 at slot 1 and the UAV of space 0 at
// slot 2. A slot is three dwords, so g_untouched's read dword is at byte
// 12 and its write dword at byte 16. g_output's write dword is at byte 28.

// CHECK-NOT: bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 12,
// CHECK-NOT: bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 16,

// The store to g_output is a genuine access and is recorded:
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 28,

// CHECK-NOT: bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 12,
// CHECK-NOT: bufferStore.i32(i32 69, %dx.types.Handle {{.*}}, i32 16,

Texture2D<float4> g_untouched : register(t0);
RWByteAddressBuffer g_output : register(u0);

[shader("raygeneration")]
void RayGen()
{
    uint width, height;
    g_untouched.GetDimensions(width, height);
    g_output.Store(0, width + height);
}
