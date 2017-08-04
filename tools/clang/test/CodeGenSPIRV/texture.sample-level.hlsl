// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s1);

// Note: The front end forbids sampling from non-floating-point texture formats.

Texture1D   <float4> t1 : register(t1);
Texture2D   <float4> t2 : register(t2);
Texture3D   <float4> t3 : register(t3);
TextureCube <float4> t4 : register(t4);

// CHECK: OpCapability ImageGatherExtended

// CHECK: %type_sampled_image = OpTypeSampledImage %type_1d_image
// CHECK: %type_sampled_image_0 = OpTypeSampledImage %type_2d_image
// CHECK: %type_sampled_image_1 = OpTypeSampledImage %type_3d_image
// CHECK: %type_sampled_image_2 = OpTypeSampledImage %type_cube_image

// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_0_1 %float_0_2
// CHECK: [[v2ic:%\d+]] = OpConstantComposite %v2int %int_2 %int_2
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_0_1 %float_0_2 %float_0_3

float4 main(int3 offset: A) : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] %float_0_1 Lod %float_10
    float4 val1 = t1.SampleLevel(gSampler, 0.1, 10);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v2fc]] Lod|ConstOffset %float_10 [[v2ic]]
    float4 val2 = t2.SampleLevel(gSampler, float2(0.1, 0.2), 10, 2);

// CHECK:              [[t3:%\d+]] = OpLoad %type_3d_image %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v3int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v3fc]] Lod|Offset %float_10 [[offset]]
    float4 val3 = t3.SampleLevel(gSampler, float3(0.1, 0.2, 0.3), 10, offset);

// CHECK:              [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_2 [[t4]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v3fc]] Lod %float_10
    float4 val4 = t4.SampleLevel(gSampler, float3(0.1, 0.2, 0.3), 10);

    return 1.0;
}
