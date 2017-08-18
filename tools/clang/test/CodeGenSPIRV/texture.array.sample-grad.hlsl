// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s1);

// Note: The front end forbids sampling from non-floating-point texture formats.

Texture1DArray   <float4> t1 : register(t1);
Texture2DArray   <float4> t2 : register(t2);
TextureCubeArray <float4> t3 : register(t3);

// CHECK: %type_sampled_image = OpTypeSampledImage %type_1d_image
// CHECK: %type_sampled_image_0 = OpTypeSampledImage %type_2d_image
// CHECK: %type_sampled_image_1 = OpTypeSampledImage %type_cube_image

// CHECK: [[v2f_0_1:%\d+]] = OpConstantComposite %v2float %float_0_1 %float_0_1
// CHECK: [[v3f_0_1:%\d+]] = OpConstantComposite %v3float %float_0_1 %float_0_1 %float_0_1
// CHECK: [[v2f_0_2:%\d+]] = OpConstantComposite %v2float %float_0_2 %float_0_2
// CHECK: [[v2f_0_3:%\d+]] = OpConstantComposite %v2float %float_0_3 %float_0_3
// CHECK: [[v4f_0_1:%\d+]] = OpConstantComposite %v4float %float_0_1 %float_0_1 %float_0_1 %float_0_1
// CHECK: [[v3f_0_2:%\d+]] = OpConstantComposite %v3float %float_0_2 %float_0_2 %float_0_2
// CHECK: [[v3f_0_3:%\d+]] = OpConstantComposite %v3float %float_0_3 %float_0_3 %float_0_3

float4 main() : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image_array %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v2f_0_1]] Grad|ConstOffset %float_0_2 %float_0_3 %int_1
    float4 val1 = t1.SampleGrad(gSampler, float2(0.1, 0.1), 0.2, 0.3, 1);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v3f_0_1]] Grad [[v2f_0_2]] [[v2f_0_3]]
    float4 val2 = t2.SampleGrad(gSampler, float3(0.1, 0.1, 0.1), float2(0.2, 0.2), float2(0.3, 0.3));

// CHECK:              [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v4f_0_1]] Grad [[v3f_0_2]] [[v3f_0_3]]
    float4 val3 = t3.SampleGrad(gSampler, float4(0.1, 0.1, 0.1, 0.1), float3(0.2, 0.2, 0.2), float3(0.3, 0.3, 0.3));

    return 1.0;
}
