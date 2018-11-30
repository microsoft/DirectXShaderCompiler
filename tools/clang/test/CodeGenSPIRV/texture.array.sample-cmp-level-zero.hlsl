// Run: %dxc -T ps_6_0 -E main

SamplerComparisonState gSampler : register(s5);

Texture1DArray   <float4> t1 : register(t1);
Texture2DArray   <float3> t2 : register(t2);
TextureCubeArray <float>  t3 : register(t3);

// CHECK: OpCapability SparseResidency

// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_1 %float_1
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_1 %float_2 %float_1
// CHECK: [[v4fc:%\d+]] = OpConstantComposite %v4float %float_1 %float_2 %float_3 %float_1

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %float

float4 main(int2 offset: A, float comparator: B) : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image_array %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefExplicitLod %float [[sampledImg]] [[v2fc]] [[comparator]] Lod|ConstOffset %float_0 %int_1
    float val1 = t1.SampleCmpLevelZero(gSampler, float2(1, 1), comparator, 1);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefExplicitLod %float [[sampledImg]] [[v3fc]] [[comparator]] Lod|Offset %float_0 [[offset]]
    float val2 = t2.SampleCmpLevelZero(gSampler, float3(1, 2, 1), comparator, offset);

// CHECK:              [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefExplicitLod %float [[sampledImg]] [[v4fc]] [[comparator]] Lod %float_0
    float val3 = t3.SampleCmpLevelZero(gSampler, float4(1, 2, 3, 1), comparator);

    uint status;
// CHECK:                [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:       [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleDrefExplicitLod %SparseResidencyStruct [[sampledImg]] [[v3fc]] [[comparator]] Lod|Offset %float_0 [[offset]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val4 [[result]]
    float val4 = t2.SampleCmpLevelZero(gSampler, float3(1, 2, 1), comparator, offset, status);

// CHECK:                [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleDrefExplicitLod %SparseResidencyStruct [[sampledImg]] [[v4fc]] [[comparator]] Lod %float_0
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val5 [[result]]
    float val5 = t3.SampleCmpLevelZero(gSampler, float4(1, 2, 3, 1), comparator, status);

    return 1.0;
}
