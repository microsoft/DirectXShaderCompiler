// RUN: %dxc -E main -T ps_6_0 %s

// CHECK: call void @dx.op.bufferStore.f16(i32 69, %dx.types.Handle %g_sb_UAV_structbuf, i32 0, i32 0, half 0xH3C00, half 0xH3C00, half 0xH3C00, half undef, i8 7)
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %g_sb_UAV_structbuf, i32 0, i32 8, i32 2, i32 2, i32 2, i32 2, i8 15)
// CHECK: call void @dx.op.bufferStore.f16(i32 69, %dx.types.Handle %g_sb_UAV_structbuf, i32 0, i32 24, half 0xH4200, half 0xH4200, half 0xH4200, half undef, i8 7)
// CHECK: call void @dx.op.bufferStore.f16(i32 69, %dx.types.Handle %g_sb_UAV_structbuf, i32 0, i32 30, half 0xH4400, half 0xH4400, half 0xH4400, half 0xH4400, i8 15)
// CHECK: %0 = call %dx.types.splitdouble @dx.op.splitDouble.f64(i32 102, double 5.000000e+00)  ; SplitDouble(value)
// CHECK: %1 = extractvalue %dx.types.splitdouble %0, 0
// CHECK: %2 = extractvalue %dx.types.splitdouble %0, 1
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %g_sb_UAV_structbuf, i32 0, i32 40, i32 %1, i32 %2, i32 undef, i32 undef, i8 3)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
 
struct MyStruct 
{
    half3  m_1;
    int4   m_2;
    half3  m_3;
    half4  m_4;
    double m_5;
    half   m_6;
    half   m_7;
    half   m_8;
    int    m_9;
};

RWStructuredBuffer<MyStruct> g_sb: register(u0);

float4 main() : SV_Target {
    MyStruct myStruct;
    myStruct.m_1 = 1;
    myStruct.m_2 = 2;
    myStruct.m_3 = 3;
    myStruct.m_4 = 4;
    myStruct.m_5 = 5;
    myStruct.m_6 = 6;
    myStruct.m_7 = 7;
    myStruct.m_8 = 8;
    myStruct.m_9 = 9;
    g_sb[0] = myStruct;

    return 1;
}