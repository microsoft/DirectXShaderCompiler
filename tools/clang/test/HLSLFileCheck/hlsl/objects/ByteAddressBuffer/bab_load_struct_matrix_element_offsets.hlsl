// Reading individual matrix elements through the struct member of a
// ByteAddressBuffer.Load<struct{float4x4}> must address each element at its own
// byte offset. For raw buffers the byte offset rides in the load index operand
// and the element-offset operand stays undef, so the four first-column reads
// below must land at indices 0, 4, 8 and 12 -- they must not collapse to the
// matrix base (index 0), which is what happened before the matrix-element
// subscript path was taught the raw-buffer addressing convention.

// RUN: %dxc -T cs_6_6 -E cs %s | FileCheck %s

struct Box { float4x4 m; };

ByteAddressBuffer   src : register(t0);
RWByteAddressBuffer dst : register(u0);

[numthreads(1, 1, 1)]
void cs()
{
    Box b = src.Load<Box>(0);

    // Column-major: _m00/_m10/_m20/_m30 live at bytes 0/4/8/12.
    dst.Store(0,  asuint(b.m._m00));
    dst.Store(4,  asuint(b.m._m10));
    dst.Store(8,  asuint(b.m._m20));
    dst.Store(12, asuint(b.m._m30));
}

// CHECK: rawBufferLoad.f32(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 undef, i8 1, i32 4)
// CHECK: rawBufferLoad.f32(i32 139, %dx.types.Handle %{{.*}}, i32 4, i32 undef, i8 1, i32 4)
// CHECK: rawBufferLoad.f32(i32 139, %dx.types.Handle %{{.*}}, i32 8, i32 undef, i8 1, i32 4)
// CHECK: rawBufferLoad.f32(i32 139, %dx.types.Handle %{{.*}}, i32 12, i32 undef, i8 1, i32 4)
