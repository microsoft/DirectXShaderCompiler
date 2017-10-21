// Run: %dxc -T ps_6_0 -E main

SamplerComparisonState gSampler : register(s5);

Texture2D<float4> t1 : register(t1);
Texture2D<float2> t2 : register(t2);
Texture2D<float>  t3 : register(t3);
// .GatherCmp() does not support Texture1D and Texture3D.
// .GatherCmp() for TextureCube requires the status parameter.

// CHECK: [[v2ic:%\d+]] = OpConstantComposite %v2int %int_1 %int_2
// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_0_1 %float_0_2

float4 main(float2 location: A, float comparator: B, int2 offset: C) : SV_Target {
// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:        [[loc:%\d+]] = OpLoad %v2float %location
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageDrefGather %v4float [[sampledImg]] [[loc]] [[comparator]] ConstOffset [[v2ic]]
    float4 val1 = t1.GatherCmp(gSampler, location, comparator, int2(1, 2));

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageDrefGather %v4float [[sampledImg]] [[v2fc]] [[comparator]]
    float4 val2 = t2.GatherCmp(gSampler, float2(0.1, 0.2), comparator);

// CHECK:              [[t3:%\d+]] = OpLoad %type_2d_image %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:        [[loc:%\d+]] = OpLoad %v2float %location
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageDrefGather %v4float [[sampledImg]] [[loc]] [[comparator]] Offset [[offset]]
    float4 val3 = t3.GatherCmp(gSampler, location, comparator, offset);

    return 1.0;
}
