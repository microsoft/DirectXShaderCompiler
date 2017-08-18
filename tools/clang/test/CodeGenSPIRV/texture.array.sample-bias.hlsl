// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s1);

// Note: The front end forbids sampling from non-floating-point texture formats.

Texture1DArray   <float4> t1 : register(t1);
Texture2DArray   <float4> t2 : register(t2);
TextureCubeArray <float4> t3 : register(t3);

// CHECK: %type_sampled_image = OpTypeSampledImage %type_1d_image_array
// CHECK: %type_sampled_image_0 = OpTypeSampledImage %type_2d_image_array
// CHECK: %type_sampled_image_1 = OpTypeSampledImage %type_cube_image_array

// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_0_1 %float_1
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_0_1 %float_0_2 %float_1
// CHECK: [[v4fc:%\d+]] = OpConstantComposite %v4float %float_0_1 %float_0_2 %float_0_3 %float_1

float4 main() : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image_array %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v2fc]] Bias|ConstOffset %float_0_5 %int_1
    float4 val1 = t1.SampleBias(gSampler, float2(0.1, 1), 0.5, 1);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v3fc]] Bias %float_0_5
    float4 val2 = t2.SampleBias(gSampler, float3(0.1, 0.2, 1), 0.5);

// CHECK:              [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v4fc]] Bias %float_0_5
    float4 val3 = t3.SampleBias(gSampler, float4(0.1, 0.2, 0.3, 1), 0.5);

    return 1.0;
}
