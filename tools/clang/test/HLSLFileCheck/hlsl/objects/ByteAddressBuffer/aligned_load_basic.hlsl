// RUN: %dxc -T cs_6_0 -E main %s | FileCheck %s

// Test basic AlignedLoad functionality with various types and alignment values

ByteAddressBuffer buf : register(t0);
RWByteAddressBuffer rwbuf : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
    uint offset = tid.x * 16;
    
    // CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 1, i32 4)
    uint data1 = buf.AlignedLoad<uint>(offset, 4);
    
    // CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 15, i32 16)
    uint4 data2 = buf.AlignedLoad<uint4>(offset, 16);
    
    // CHECK: call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 7, i32 16)
    float3 data3 = buf.AlignedLoad<float3>(offset, 16);
    
    // CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 15, i32 32)
    uint4 data4 = rwbuf.AlignedLoad<uint4>(offset, 32);
    
    // Test with larger offset
    uint offset2 = tid.x * 64;
    
    // CHECK: call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %{{[0-9]+}}, i32 %{{[0-9]+}}, i32 undef, i8 3, i32 8)
    uint2 data5 = buf.AlignedLoad<uint2>(offset2, 8);
    
    // Use the values to prevent DCE
    rwbuf.Store<uint>(offset, data1 + data2.x + (uint)(data3.x) + data4.x + data5.x);
}
