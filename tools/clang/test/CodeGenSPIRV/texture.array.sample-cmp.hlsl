// Run: %dxc -T ps_6_0 -E main

SamplerComparisonState gSampler : register(s5);

Texture1DArray   <float4> t1 : register(t1);
Texture2DArray   <float3> t2 : register(t2);
TextureCubeArray <float>  t3 : register(t3);

// CHECK: OpCapability MinLod
// CHECK: OpCapability SparseResidency

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %float

// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_1 %float_1
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_1 %float_2 %float_1
// CHECK: [[v4fc:%\d+]] = OpConstantComposite %v4float %float_1 %float_2 %float_3 %float_1

float4 main(int2 offset: A, float comparator: B) : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image_array %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v2fc]] [[comparator]] ConstOffset %int_1
    float val1 = t1.SampleCmp(gSampler, float2(1, 1), comparator, 1);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v3fc]] [[comparator]] Offset [[offset]]
    float val2 = t2.SampleCmp(gSampler, float3(1, 2, 1), comparator, offset);

// CHECK:              [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v4fc]] [[comparator]]
    float val3 = t3.SampleCmp(gSampler, float4(1, 2, 3, 1), comparator);

    float clamp;
// CHECK:           [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:         [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v3fc]] [[comparator]] Offset|MinLod [[offset]] [[clamp]]
    float val4 = t2.SampleCmp(gSampler, float3(1, 2, 1), comparator, offset, clamp);

// CHECK:              [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v4fc]] [[comparator]] MinLod %float_1_5
    float val5 = t3.SampleCmp(gSampler, float4(1, 2, 3, 1), comparator, /*clamp*/ 1.5);

    uint status;
// CHECK:             [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:           [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:       [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleDrefImplicitLod %SparseResidencyStruct [[sampledImg]] [[v3fc]] [[comparator]] Offset|MinLod [[offset]] [[clamp]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val6 [[result]]
    float val6 = t2.SampleCmp(gSampler, float3(1, 2, 1), comparator, offset, clamp, status);

// CHECK:                [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleDrefImplicitLod %SparseResidencyStruct [[sampledImg]] [[v4fc]] [[comparator]] MinLod %float_1_5
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val7 [[result]]
    float val7 = t3.SampleCmp(gSampler, float4(1, 2, 3, 1), comparator, /*clamp*/ 1.5, status);

    return 1.0;
}
