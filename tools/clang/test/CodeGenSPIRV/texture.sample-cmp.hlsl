// Run: %dxc -T ps_6_0 -E main

SamplerComparisonState gSampler : register(s5);

Texture1D   <float4> t1 : register(t1);
Texture2D   <float2> t2 : register(t2);
TextureCube <float>  t4 : register(t4);
// No .SampleCmp() for Texture3D.

// CHECK: OpCapability MinLod
// CHECK: OpCapability SparseResidency

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %float

// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_1 %float_2
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_1 %float_2 %float_3

float4 main(int2 offset: A, float comparator: B) : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] %float_1 [[comparator]] ConstOffset %int_5
    float val1 = t1.SampleCmp(gSampler, 1, comparator, 5);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v2fc]] [[comparator]] Offset [[offset]]
    float val2 = t2.SampleCmp(gSampler, float2(1, 2), comparator, offset);

// CHECK:              [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t4]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v3fc]] [[comparator]]
    float val4 = t4.SampleCmp(gSampler, float3(1, 2, 3), comparator);

    float clamp;
// CHECK:           [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:         [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v2fc]] [[comparator]] Offset|MinLod [[offset]] [[clamp]]
    float val5 = t2.SampleCmp(gSampler, float2(1, 2), comparator, offset, clamp);

// CHECK:              [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t4]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v3fc]] [[comparator]] MinLod %float_2_5
    float val6 = t4.SampleCmp(gSampler, float3(1, 2, 3), comparator, /*clamp*/2.5);

    uint status;
// CHECK:             [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:           [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:       [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleDrefImplicitLod %SparseResidencyStruct [[sampledImg]] [[v2fc]] [[comparator]] Offset|MinLod [[offset]] [[clamp]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val7 [[result]]
    float val7 = t2.SampleCmp(gSampler, float2(1, 2), comparator, offset, clamp, status);

// CHECK:                [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t4]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleDrefImplicitLod %SparseResidencyStruct [[sampledImg]] [[v3fc]] [[comparator]] MinLod %float_2_5
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val8 [[result]]
    float val8 = t4.SampleCmp(gSampler, float3(1, 2, 3), comparator, /*clamp*/2.5, status);

    return 1.0;
}
