// RUN: %dxc -T cs_6_2 -enable-16bit-types -E main %s | FileCheck %s

// Test AlignedLoad with different types including 16-bit and 64-bit

ByteAddressBuffer buf : register(t0);
RWByteAddressBuffer rwbuf : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    uint offset = tid.x * 64;
    
    // 16-bit types
    // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 1, i32 2)
    uint16_t data1 = buf.AlignedLoad<uint16_t>(offset, 2);
    rwbuf.AlignedStore<uint16_t>(offset, 2, data1);
    
    // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 15, i32 8)
    half4 data2 = buf.AlignedLoad<half4>(offset, 8);
    rwbuf.AlignedStore<half4> (offset, 8, data2);
    
    // 32-bit types
    // CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 1, i32 4)
    int data3 = buf.AlignedLoad<int>(offset, 4);
    rwbuf.AlignedStore<int>(offset, 4, data3);
    
    // CHECK: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 15, i32 16)
    float4 data4 = buf.AlignedLoad<float4>(offset, 16);
    rwbuf.AlignedStore<float4>(offset, 16, data4);
    
    // 64-bit types
    // CHECK: call %dx.types.ResRet.i64 @dx.op.rawBufferLoad.i64(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 1, i32 8)
    uint64_t data5 = buf.AlignedLoad<uint64_t>(offset, 8);
    rwbuf.AlignedStore<uint64_t>(offset, 8, data5);
    
    // CHECK: call %dx.types.ResRet.f64 @dx.op.rawBufferLoad.f64(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 1, i32 8)
    double data6 = buf.AlignedLoad<double>(offset, 8);
    rwbuf.AlignedStore<double>(offset, 8, data6);
    
    // Mixed vector sizes
    // CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 3, i32 8)
    uint2 data7 = buf.AlignedLoad<uint2>(offset, 8);
    rwbuf.AlignedStore<uint2> (offset, 8, data7);
    
    // CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 7, i32 16)
    uint3 data8 = buf.AlignedLoad<uint3>(offset, 16);
    rwbuf.AlignedStore<uint3>(offset, 16, data8);
}

