// Run: %dxc -T ps_6_0 -E main

SamplerComparisonState gSampler : register(s5);

Texture1D   <float4> t1 : register(t1);
Texture2D   <float2> t2 : register(t2);
TextureCube <float>  t4 : register(t4);
// No .SampleCmp() for Texture3D.

// CHECK: OpCapability SparseResidency

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %float

// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_1 %float_2
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_1 %float_2 %float_3

float4 main(int2 offset: A, float comparator: B) : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefExplicitLod %float [[sampledImg]] %float_1 [[comparator]] Lod|ConstOffset %float_0 %int_5
    float val1 = t1.SampleCmpLevelZero(gSampler, 1, comparator, 5);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefExplicitLod %float [[sampledImg]] [[v2fc]] [[comparator]] Lod|Offset %float_0 [[offset]]
    float val2 = t2.SampleCmpLevelZero(gSampler, float2(1, 2), comparator, offset);

// CHECK:              [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t4]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefExplicitLod %float [[sampledImg]] [[v3fc]] [[comparator]] Lod %float_0
    float val4 = t4.SampleCmpLevelZero(gSampler, float3(1, 2, 3), comparator);

    uint status;
// CHECK:                [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:       [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleDrefExplicitLod %SparseResidencyStruct [[sampledImg]] [[v2fc]] [[comparator]] Lod|Offset %float_0 [[offset]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val5 [[result]]
    float val5 = t2.SampleCmpLevelZero(gSampler, float2(1, 2), comparator, offset, status);

// CHECK:                [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t4]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseSampleDrefExplicitLod %SparseResidencyStruct [[sampledImg]] [[v3fc]] [[comparator]] Lod %float_0
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %float [[structResult]] 1
// CHECK-NEXT:                         OpStore %val6 [[result]]
    float val6 = t4.SampleCmpLevelZero(gSampler, float3(1, 2, 3), comparator, status);

    return 1.0;
}
