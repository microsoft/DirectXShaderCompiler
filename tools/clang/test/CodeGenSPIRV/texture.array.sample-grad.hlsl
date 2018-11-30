// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s5);

// Note: The front end forbids sampling from non-floating-point texture formats.

Texture1DArray   <float4> t1 : register(t1);
Texture2DArray   <float4> t2 : register(t2);
TextureCubeArray <float4> t3 : register(t3);
Texture2DArray   <float>  t4 : register(t4);
TextureCubeArray <float2> t5 : register(t5);


// CHECK: OpCapability ImageGatherExtended
// CHECK: OpCapability MinLod
// CHECK: OpCapability SparseResidency

// CHECK: [[v2f_1:%\d+]] = OpConstantComposite %v2float %float_1 %float_1
// CHECK: [[v3f_1:%\d+]] = OpConstantComposite %v3float %float_1 %float_1 %float_1
// CHECK: [[v2f_2:%\d+]] = OpConstantComposite %v2float %float_2 %float_2
// CHECK: [[v2f_3:%\d+]] = OpConstantComposite %v2float %float_3 %float_3
// CHECK: [[v4f_1:%\d+]] = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
// CHECK: [[v3f_2:%\d+]] = OpConstantComposite %v3float %float_2 %float_2 %float_2
// CHECK: [[v3f_3:%\d+]] = OpConstantComposite %v3float %float_3 %float_3 %float_3

// CHECK: %type_sampled_image = OpTypeSampledImage %type_1d_image_array
// CHECK: %type_sampled_image_0 = OpTypeSampledImage %type_2d_image_array
// CHECK: %type_sampled_image_1 = OpTypeSampledImage %type_cube_image_array
// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4float

float4 main(int2 offset : A) : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image_array %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v2f_1]] Grad|ConstOffset %float_2 %float_3 %int_1
    float4 val1 = t1.SampleGrad(gSampler, float2(1, 1), 2, 3, 1);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v3f_1]] Grad|Offset [[v2f_2]] [[v2f_3]] [[offset]]
    float4 val2 = t2.SampleGrad(gSampler, float3(1, 1, 1), float2(2, 2), float2(3, 3), offset);

// CHECK:              [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v4f_1]] Grad [[v3f_2]] [[v3f_3]]
    float4 val3 = t3.SampleGrad(gSampler, float4(1, 1, 1, 1), float3(2, 2, 2), float3(3, 3, 3));

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v3f_1]] Grad|Offset|MinLod [[v2f_2]] [[v2f_3]] [[offset]] %float_2_5
    float4 val4 = t2.SampleGrad(gSampler, float3(1, 1, 1), float2(2, 2), float2(3, 3), offset, /*clamp*/2.5);

    float clamp;
// CHECK:           [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:         [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v4f_1]] Grad|MinLod [[v3f_2]] [[v3f_3]] [[clamp]]
    float4 val5 = t3.SampleGrad(gSampler, float4(1, 1, 1, 1), float3(2, 2, 2), float3(3, 3, 3), clamp);

    uint status;
// CHECK:                [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:       [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleExplicitLod %SparseResidencyStruct [[sampledImg]] [[v3f_1]] Grad|Offset|MinLod [[v2f_2]] [[v2f_3]] [[offset]] %float_2_5
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val6 [[result]]
    float4 val6 = t2.SampleGrad(gSampler, float3(1, 1, 1), float2(2, 2), float2(3, 3), offset, /*clamp*/2.5, status);

// CHECK:             [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:           [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleExplicitLod %SparseResidencyStruct [[sampledImg]] [[v4f_1]] Grad|MinLod [[v3f_2]] [[v3f_3]] [[clamp]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val7 [[result]]
    float4 val7 = t3.SampleGrad(gSampler, float4(1, 1, 1, 1), float3(2, 2, 2), float3(3, 3, 3), clamp, status);

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Make sure OpImageSampleExplicitLod returns a vec4.
// Make sure OpImageSparseSampleExplicitLod returns a struct, in which the second member is a vec4.
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK: [[v4result:%\d+]] = OpImageSampleExplicitLod %v4float {{%\d+}} {{%\d+}} Grad|Offset {{%\d+}} {{%\d+}} {{%\d+}}
// CHECK:          {{%\d+}} = OpCompositeExtract %float [[v4result]] 0
	float  val8 = t4.SampleGrad(gSampler, float3(1, 1, 1), float2(2, 2), float2(3, 3), offset);

// CHECK: [[structResult:%\d+]] = OpImageSparseSampleExplicitLod %SparseResidencyStruct {{%\d+}} {{%\d+}} Grad|MinLod {{%\d+}} {{%\d+}} {{%\d+}}
// CHECK:     [[v4result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK:              {{%\d+}} = OpVectorShuffle %v2float [[v4result]] [[v4result]] 0 1
    float2 val9 = t5.SampleGrad(gSampler, float4(1, 1, 1, 1), float3(2, 2, 2), float3(3, 3, 3), clamp, status);

    return 1.0;
}
