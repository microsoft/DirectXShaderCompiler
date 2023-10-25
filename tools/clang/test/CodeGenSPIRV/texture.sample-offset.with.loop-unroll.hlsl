// RUN: %dxc -T ps_6_0 -E main -Oconfig=--legalize-hlsl

SamplerState      gSampler  : register(s5);
Texture2D<float4> t         : register(t1);

// CHECK: [[vi1:%\d+]] = OpConstantComposite %v2int %int_1 %int_1
// CHECK: [[vi2:%\d+]] = OpConstantComposite %v2int %int_2 %int_2

float4 main(int2 offset: A) : SV_Target {
    uint status = offset.x;
    float clamp = offset.x;
    float4 ret = 0;

// CHECK: OpImageSampleExplicitLod %v4float {{%\d+}} {{%\d+}} Lod %float_0
// CHECK: OpImageSampleExplicitLod %v4float {{%\d+}} {{%\d+}} Lod|ConstOffset %float_0 [[vi1]]
// CHECK: OpImageSampleExplicitLod %v4float {{%\d+}} {{%\d+}} Lod|ConstOffset %float_0 [[vi2]]

    [unroll] for(int i = 0; i < 3; ++i)
        ret += t.SampleLevel(gSampler, float2(0.1, 0.2), 0.0, /*offset*/ i);

    return ret;
}
