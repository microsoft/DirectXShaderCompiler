// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s5);

// Note: The front end forbids sampling from non-floating-point texture formats.

Texture1D   <float4> t1 : register(t1);
Texture2D   <float4> t2 : register(t2);
Texture3D   <float4> t3 : register(t3);
TextureCube <float4> t4 : register(t4);
Texture1D   <float>  t5 : register(t5);
Texture3D   <float2> t6 : register(t6);

// CHECK: OpCapability ImageGatherExtended
// CHECK: OpCapability MinLod
// CHECK: OpCapability SparseResidency

// CHECK: %type_sampled_image = OpTypeSampledImage %type_1d_image
// CHECK: %type_sampled_image_0 = OpTypeSampledImage %type_2d_image
// CHECK: %type_sampled_image_1 = OpTypeSampledImage %type_3d_image
// CHECK: %type_sampled_image_2 = OpTypeSampledImage %type_cube_image
// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4float

// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_1 %float_2
// CHECK: [[v2ic:%\d+]] = OpConstantComposite %v2int %int_2 %int_2
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_1 %float_2 %float_3

float4 main(int3 offset: A) : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] %float_1 Bias %float_0_5
    float4 val1 = t1.SampleBias(gSampler, 1, 0.5);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v2fc]] Bias|ConstOffset %float_0_5 [[v2ic]]
    float4 val2 = t2.SampleBias(gSampler, float2(1, 2), 0.5, 2);

// CHECK:              [[t3:%\d+]] = OpLoad %type_3d_image %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v3int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v3fc]] Bias|Offset %float_0_5 [[offset]]
    float4 val3 = t3.SampleBias(gSampler, float3(1, 2, 3), 0.5, offset);

// CHECK:              [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_2 [[t4]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v3fc]] Bias %float_0_5
    float4 val4 = t4.SampleBias(gSampler, float3(1, 2, 3), 0.5);

    float clamp;
// CHECK:           [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:         [[t3:%\d+]] = OpLoad %type_3d_image %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v3int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v3fc]] Bias|Offset|MinLod %float_0_5 [[offset]] [[clamp]]
    float4 val5 = t3.SampleBias(gSampler, float3(1, 2, 3), 0.5, offset, clamp);

// CHECK:              [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_2 [[t4]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v3fc]] Bias|MinLod %float_0_5 %float_2_5
    float4 val6 = t4.SampleBias(gSampler, float3(1, 2, 3), 0.5, /*clamp*/ 2.5);

    uint status;
// CHECK:             [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:           [[t3:%\d+]] = OpLoad %type_3d_image %t3
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:       [[offset:%\d+]] = OpLoad %v3int %offset
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleImplicitLod %SparseResidencyStruct [[sampledImg]] [[v3fc]] Bias|Offset|MinLod %float_0_5 [[offset]] [[clamp]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val7 [[result]]
    float4 val7 = t3.SampleBias(gSampler, float3(1, 2, 3), 0.5, offset, clamp, status);

// CHECK:                [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_2 [[t4]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleImplicitLod %SparseResidencyStruct [[sampledImg]] [[v3fc]] Bias|MinLod %float_0_5 %float_2_5
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val8 [[result]]
    float4 val8 = t4.SampleBias(gSampler, float3(1, 2, 3), 0.5, /*clamp*/ 2.5, status);

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Make sure OpImageSampleImplicitLod returns a vec4.
// Make sure OpImageSparseSampleImplicitLod returns a struct, in which the second member is a vec4.
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK:  [[v4result:%\d+]] = OpImageSampleImplicitLod %v4float {{%\d+}} %float_1 Bias %float_0_5
// CHECK:           {{%\d+}} = OpCompositeExtract %float [[v4result]] 0
    float val9 = t5.SampleBias(gSampler, 1, 0.5);

// CHECK: [[structResult:%\d+]] = OpImageSparseSampleImplicitLod %SparseResidencyStruct {{%\d+}} {{%\d+}} Bias|Offset|MinLod %float_0_5 {{%\d+}} {{%\d+}}
// CHECK:     [[v4result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK:              {{%\d+}} = OpVectorShuffle %v2float [[v4result]] [[v4result]] 0 1
    float2 val10 = t6.SampleBias(gSampler, float3(1, 2, 3), 0.5, offset, clamp, status);

    return 1.0;
}
