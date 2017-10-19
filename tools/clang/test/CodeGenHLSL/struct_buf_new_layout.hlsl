// RUN: %dxc -E main -T ps_6_2 -no-min-precision %s  | FileCheck %s

// CHECK: call void @dx.op.bufferStore.f16(i32 69, %dx.types.Handle %g_sb1_UAV_structbuf, i32 0, i32 0, half 0xH3C00, half 0xH3C00, half 0xH3C00, half undef, i8 7)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %g_sb1_UAV_structbuf, i32 0, i32 8, i32 2, i32 2, i32 2, i32 2, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
// CHECK: call void @dx.op.bufferStore.f16(i32 69, %dx.types.Handle %g_sb1_UAV_structbuf, i32 0, i32 24, half 0xH4200, half 0xH4200, half 0xH4200, half undef, i8 7)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
// CHECK: call void @dx.op.bufferStore.f16(i32 69, %dx.types.Handle %g_sb1_UAV_structbuf, i32 0, i32 30, half 0xH4400, half 0xH4400, half 0xH4400, half 0xH4400, i8 15)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %g_sb1_UAV_structbuf, i32 0, i32 40, i32 %1, i32 %2, i32 undef, i32 undef, i8 3)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
// CHECK: call void @dx.op.bufferStore.f16(i32 69, %dx.types.Handle %g_sb1_UAV_structbuf, i32 0, i32 48, half 0xH4600, half undef, half undef, half undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
// CHECK: call void @dx.op.bufferStore.f16(i32 69, %dx.types.Handle %g_sb1_UAV_structbuf, i32 0, i32 50, half 0xH4700, half undef, half undef, half undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
// CHECK: call void @dx.op.bufferStore.f16(i32 69, %dx.types.Handle %g_sb1_UAV_structbuf, i32 0, i32 52, half 0xH4800, half undef, half undef, half undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %g_sb1_UAV_structbuf, i32 0, i32 56, i32 9, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %g_sb2_UAV_structbuf, i32 0, i32 0, i32 %4, i32 %5, i32 undef, i32 undef, i8 3)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
// CHECK: call void @dx.op.bufferStore.f16(i32 69, %dx.types.Handle %g_sb2_UAV_structbuf, i32 0, i32 8, half 0xH4000, half 0xH4000, half 0xH4000, half undef, i8 7)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)
// CHECK: call void @dx.op.bufferStore.i32(i32 69, %dx.types.Handle %g_sb2_UAV_structbuf, i32 0, i32 16, i32 3, i32 undef, i32 undef, i32 undef, i8 1)  ; BufferStore(uav,coord0,coord1,value0,value1,value2,value3,mask)

struct MyStruct1
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

struct MyStruct2
{
    double m_1;
    half3  m_2;
    int    m_3;
};

RWStructuredBuffer<MyStruct1> g_sb1: register(u0);
RWStructuredBuffer<MyStruct2> g_sb2: register(u1);

float4 main() : SV_Target {
    MyStruct1 myStruct;
    myStruct.m_1 = 1;
    myStruct.m_2 = 2;
    myStruct.m_3 = 3;
    myStruct.m_4 = 4;
    myStruct.m_5 = 5;
    myStruct.m_6 = 6;
    myStruct.m_7 = 7;
    myStruct.m_8 = 8;
    myStruct.m_9 = 9;
    g_sb1[0] = myStruct;

    MyStruct2 myStruct2;
    myStruct2.m_1 = 1;
    myStruct2.m_2 = 2;
    myStruct2.m_3 = 3;
    g_sb2[0] = myStruct2;

    return 1;
}