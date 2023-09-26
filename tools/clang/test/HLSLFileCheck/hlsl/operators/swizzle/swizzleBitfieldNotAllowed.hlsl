// RUN: %dxc -T ps_6_6 -HV 2021 %s | FileCheck %s

// This test ensures that swizzled bitfields 
// cannot be used as an LValue.


struct MyStruct
{
    uint v0: 5;
    uint v1: 15;
    uint v2: 12;
};

ConstantBuffer<MyStruct> myConstBuff;
StructuredBuffer<MyStruct> myStructBuff;


[RootSignature("RootFlags(0), DescriptorTable(CBV(b0), SRV(t0))")]
float4 main(uint addr: TEXCOORD): SV_Target
{  
    // CHECK: error: expression is not assignable
    myConstBuff.v2.x = 4;
    // CHECK: error: expression is not assignable
    myStructBuff[0].v2.x = 4;
    // CHECK: error: vector swizzle 'xy' is out of bounds
    uint z = myConstBuff.v2.xy;
}
