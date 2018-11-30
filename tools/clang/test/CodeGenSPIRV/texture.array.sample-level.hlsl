// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s5);

// Note: The front end forbids sampling from non-floating-point texture formats.

Texture1DArray   <float4> t1 : register(t1);
Texture2DArray   <float4> t2 : register(t2);
TextureCubeArray <float4> t3 : register(t3);
Texture2DArray   <float>  t4 : register(t4);
TextureCubeArray <float3> t5 : register(t5);

// CHECK: OpCapability ImageGatherExtended
// CHECK: OpCapability SparseResidency

// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_1 %float_1
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_1 %float_2 %float_1
// CHECK: [[v4fc:%\d+]] = OpConstantComposite %v4float %float_1 %float_2 %float_3 %float_1

// CHECK: %type_sampled_image = OpTypeSampledImage %type_1d_image_array
// CHECK: %type_sampled_image_0 = OpTypeSampledImage %type_2d_image_array
// CHECK: %type_sampled_image_1 = OpTypeSampledImage %type_cube_image_array
// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4float

float4 main(int2 offset : A) : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image_array %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v2fc]] Lod|ConstOffset %float_10 %int_1
    float4 val1 = t1.SampleLevel(gSampler, float2(1, 1), 10, 1);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v3fc]] Lod|Offset %float_20 [[offset]]
    float4 val2 = t2.SampleLevel(gSampler, float3(1, 2, 1), 20, offset);

// CHECK:              [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleExplicitLod %v4float [[sampledImg]] [[v4fc]] Lod %float_30
    float4 val3 = t3.SampleLevel(gSampler, float4(1, 2, 3, 1), 30);

    uint status;
// CHECK:                [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:       [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleExplicitLod %SparseResidencyStruct [[sampledImg]] [[v3fc]] Lod|Offset %float_20 [[offset]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val4 [[result]]
    float4 val4 = t2.SampleLevel(gSampler, float3(1, 2, 1), 20, offset, status);

// CHECK:                [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleExplicitLod %SparseResidencyStruct [[sampledImg]] [[v4fc]] Lod %float_30
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val5 [[result]]
    float4 val5 = t3.SampleLevel(gSampler, float4(1, 2, 3, 1), 30, status);

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Make sure OpImageSampleExplicitLod returns a vec4.
// Make sure OpImageSparseSampleExplicitLod returns a struct, in which the second member is a vec4.
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// CHECK: [[v4result:%\d+]] = OpImageSampleExplicitLod %v4float {{%\d+}} {{%\d+}} Lod|Offset %float_20 {{%\d+}}
// CHECK:          {{%\d+}} = OpCompositeExtract %float [[v4result]] 0
    float val6 = t4.SampleLevel(gSampler, float3(1, 2, 1), 20, offset);

// CHECK: [[structResult:%\d+]] = OpImageSparseSampleExplicitLod %SparseResidencyStruct {{%\d+}} {{%\d+}} Lod %float_30
// CHECK:     [[v4result:%\d+]] = OpCompositeExtract %v4float [[structResult]] 1
// CHECK:              {{%\d+}} = OpVectorShuffle %v3float [[v4result]] [[v4result]] 0 1 2
    float3 val7 = t5.SampleLevel(gSampler, float4(1, 2, 3, 1), 30, status);

    return 1.0;
}
