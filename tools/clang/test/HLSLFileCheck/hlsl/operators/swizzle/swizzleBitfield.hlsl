// RUN: %dxc -T ps_6_6 -HV 2021 %s | FileCheck %s

// This test is a regression test that tests the implementation of L-Value
// conversions work on bitfields, for both constant and structured buffers


struct MyStruct
{
    uint v0: 5;
    uint v1: 15;
    uint v2: 12;
};

ConstantBuffer<MyStruct> myConstBuff;
StructuredBuffer<MyStruct> myStructBuff;

// CHECK: extractvalue %dx.types.ResRet.i32
// CHECK: extractvalue %dx.types.CBufRet.i32


[RootSignature("RootFlags(0), DescriptorTable(CBV(b0), SRV(t0))")]
float4 main(uint addr: TEXCOORD): SV_Target
{
    float4 temp1 = myStructBuff[0].v2.xxxx;
    float4 temp2 = myConstBuff.v2.xxxx;
    return  temp1 + temp2;
}
