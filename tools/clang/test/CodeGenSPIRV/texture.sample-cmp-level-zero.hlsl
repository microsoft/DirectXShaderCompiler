// Run: %dxc -T ps_6_0 -E main

SamplerComparisonState gSampler : register(s5);

Texture1D   <float4> t1 : register(t1);
Texture2D   <float2> t2 : register(t2);
TextureCube <float>  t4 : register(t4);
// No .SampleCmp() for Texture3D.

// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_0_1 %float_0_2
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_0_1 %float_0_2 %float_0_3

float4 main(int2 offset: A, float comparator: B) : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefExplicitLod %float [[sampledImg]] %float_0_1 [[comparator]] Lod|ConstOffset %float_0 %int_5
    float val1 = t1.SampleCmpLevelZero(gSampler, 0.1, comparator, 5);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefExplicitLod %float [[sampledImg]] [[v2fc]] [[comparator]] Lod|Offset %float_0 [[offset]]
    float val2 = t2.SampleCmpLevelZero(gSampler, float2(0.1, 0.2), comparator, offset);

// CHECK:              [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t4]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefExplicitLod %float [[sampledImg]] [[v3fc]] [[comparator]] Lod %float_0
    float val4 = t4.SampleCmpLevelZero(gSampler, float3(0.1, 0.2, 0.3), comparator);

    return 1.0;
}

