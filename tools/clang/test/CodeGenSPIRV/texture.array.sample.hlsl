// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s5);

// Note: The front end forbids sampling from non-floating-point texture formats.

Texture1DArray   <float4> t1 : register(t1);
Texture2DArray   <float4> t2 : register(t2);
TextureCubeArray <float4> t3 : register(t3);
Texture1DArray   <float>  t4 : register(t4);
TextureCubeArray <float3> t5 : register(t5);

// CHECK: OpCapability MinLod
// CHECK: OpCapability SparseResidency

// CHECK: %type_sampled_image = OpTypeSampledImage %type_1d_image_array
// CHECK: %type_sampled_image_0 = OpTypeSampledImage %type_2d_image_array
// CHECK: %type_sampled_image_1 = OpTypeSampledImage %type_cube_image_array
// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4float

// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_0_5 %float_1
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_0_5 %float_0_25 %float_1
// CHECK: [[v4fc:%\d+]] = OpConstantComposite %v4float %float_0_5 %float_0_25 %float_0_125 %float_1

float4 main() : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image_array %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v2fc]] ConstOffset %int_1
    float4 val1 = t1.Sample(gSampler, float2(0.5, 1), 1);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v3fc]]
    float4 val2 = t2.Sample(gSampler, float3(0.5, 0.25, 1));

// CHECK:              [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v4fc]]
    float4 val3 = t3.Sample(gSampler, float4(0.5, 0.25, 0.125, 1));

    float clamp;
// CHECK:           [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:         [[t1:%\d+]] = OpLoad %type_1d_image_array %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v2fc]] ConstOffset|MinLod %int_1 [[clamp]]
    float4 val4 = t1.Sample(gSampler, float2(0.5, 1), 1, clamp);

// CHECK:              [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampledImg]] [[v4fc]] MinLod %float_1_5
    float4 val5 = t3.Sample(gSampler, float4(0.5, 0.25, 0.125, 1), /*clamp*/ 1.5);

    uint status;
// CHECK:             [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:           [[t1:%\d+]] = OpLoad %type_1d_image_array %t1
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleImplicitLod %SparseResidencyStruct [[sampledImg]] [[v2fc]] ConstOffset|MinLod %int_1 [[clamp]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val6 [[result]]
    float4 val6 = t1.Sample(gSampler, float2(0.5, 1), 1, clamp, status);

// CHECK:                [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleImplicitLod %SparseResidencyStruct [[sampledImg]] [[v4fc]] MinLod %float_1_5
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val7 [[result]]
    float4 val7 = t3.Sample(gSampler, float4(0.5, 0.25, 0.125, 1), /*clamp*/ 1.5, status);

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Make sure OpImageSampleImplicitLod returns a vec4.
// Make sure OpImageSparseSampleImplicitLod returns a struct, in which the second member is a vec4.
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK: [[v4result:%\d+]] = OpImageSampleImplicitLod %v4float {{%\d+}} {{%\d+}} ConstOffset %int_1
// CHECK:          {{%\d+}} = OpCompositeExtract %float [[v4result]] 0
    float val8 = t4.Sample(gSampler, float2(0.5, 1), 1);

// CHECK: [[structResult:%\d+]] = OpImageSparseSampleImplicitLod %SparseResidencyStruct {{%\d+}} {{%\d+}} MinLod %float_1_5
// CHECK:     [[v4result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK:              {{%\d+}} = OpVectorShuffle %v3float [[v4result]] [[v4result]] 0 1 2
    float3 val9 = t5.Sample(gSampler, float4(0.5, 0.25, 0.125, 1), /*clamp*/ 1.5, status);

    return 1.0;
}
