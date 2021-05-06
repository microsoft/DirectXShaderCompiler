// Run: %dxc -T cs_6_0 -E main -fvk-use-dx-layout

struct TestStruct0
{
    double Double;
    float3 Float3;
};

struct TestStruct1
{
    double Double;
    float3 Float3;
};

RWTexture2D<float4> Result;

// CHECK: OpMemberDecorate %TestStruct0 0 Offset 0
// CHECK: OpMemberDecorate %TestStruct0 1 Offset 8
// CHECK: OpDecorate %_runtimearr_TestStruct0 ArrayStride 24
StructuredBuffer<TestStruct0> testSB0;

// CHECK: OpMemberDecorate %TestStruct1 0 Offset 0
// CHECK: OpMemberDecorate %TestStruct1 1 Offset 8
// CHECK: OpDecorate %_runtimearr_TestStruct1 ArrayStride 24
StructuredBuffer<TestStruct1> testSB1;

[numthreads(8,8,2)]
void main (uint3 id : SV_DispatchThreadID)
{
    TestStruct0 temp0 = testSB0[id.z];
    Result[id.xy] = float(temp0.Double).xxxx;

    TestStruct1 temp1 = testSB1[id.x];
    Result[id.zy] = float(temp1.Double).xxxx;
}

