// RUN: %dxc -T cs_6_0 -E main %s | FileCheck %s

// Test basic AlignedStore functionality with various types and alignment values

RWByteAddressBuffer rwbuf : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    uint offset = tid.x * 16;
    
    // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i32 %{{[^,]+}}, i32 undef, i32 undef, i32 undef, i8 1, i32 4)
    uint value1 = 42;
    rwbuf.AlignedStore<uint>(offset, 4, value1);
    
    // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i32 %{{[^,]+}}, i32 %{{[^,]+}}, i32 %{{[^,]+}}, i32 %{{[^,]+}}, i8 15, i32 16)
    uint4 value2 = uint4(1, 2, 3, 4);
    rwbuf.AlignedStore<uint4>(offset, 16, value2);
    
    // CHECK: call void @dx.op.rawBufferStore.f32(i32 140, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, float %{{[^,]+}}, float %{{[^,]+}}, float %{{[^,]+}}, float undef, i8 7, i32 16)
    float3 value3 = float3(1.0, 2.0, 3.0);
    rwbuf.AlignedStore<float3>(offset, 16, value3);
    
    // Test with larger offset
    uint offset2 = tid.x * 64;
    
    // CHECK: call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i32 %{{[^,]+}}, i32 %{{[^,]+}}, i32 undef, i32 undef, i8 3, i32 32)
    uint2 value4 = uint2(5, 6);
    rwbuf.AlignedStore<uint2>(offset2, 32, value4);
}

