// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s5);

Texture2DArray<float4>  t2f4       : register(t1);
Texture2DArray<int3>    t2i3       : register(t2);
TextureCubeArray<uint4> tCubeArray : register(t3);
// .GatherBlue() does not support Texture1DArray.

// CHECK: OpCapability SparseResidency

// CHECK:      [[c12:%\d+]] = OpConstantComposite %v2int %int_1 %int_2
// CHECK:      [[c34:%\d+]] = OpConstantComposite %v2int %int_3 %int_4
// CHECK:      [[c56:%\d+]] = OpConstantComposite %v2int %int_5 %int_6
// CHECK:      [[c78:%\d+]] = OpConstantComposite %v2int %int_7 %int_8
// CHECK:    [[c1to8:%\d+]] = OpConstantComposite %_arr_v2int_uint_4 [[c12]] [[c34]] [[c56]] [[c78]]

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4int

// CHECK: [[cv4f_1_5:%\d+]] = OpConstantComposite %v4float %float_1_5 %float_1_5 %float_1_5 %float_1_5

// CHECK: %SparseResidencyStruct_0 = OpTypeStruct %uint %v4uint

float4 main(float3 location: A) : SV_Target {
// CHECK:            [[t2f4:%\d+]] = OpLoad %type_2d_image_array %t2f4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:        [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t2f4]] [[gSampler]]
// CHECK-NEXT:        [[res:%\d+]] = OpImageGather %v4float [[sampledImg]] [[loc]] %int_2 ConstOffset [[c12]]
// CHECK-NEXT:                       OpStore %a [[res]]
    float4 a = t2f4.GatherBlue(gSampler, location, int2(1, 2));
// CHECK:            [[t2f4:%\d+]] = OpLoad %type_2d_image_array %t2f4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:        [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t2f4]] [[gSampler]]
// CHECK-NEXT:        [[res:%\d+]] = OpImageGather %v4float [[sampledImg]] [[loc]] %int_2 ConstOffsets [[c1to8]]
// CHECK-NEXT:                       OpStore %b [[res]]
    float4 b = t2f4.GatherBlue(gSampler, location, int2(1, 2), int2(3, 4), int2(5, 6), int2(7, 8));

// CHECK:            [[t2i3:%\d+]] = OpLoad %type_2d_image_array_0 %t2i3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:        [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2i3]] [[gSampler]]
// CHECK-NEXT:        [[res:%\d+]] = OpImageGather %v4int [[sampledImg]] [[loc]] %int_2 ConstOffset [[c12]]
// CHECK-NEXT:                       OpStore %c [[res]]
    int4 c = t2i3.GatherBlue(gSampler, location, int2(1, 2));
// CHECK:            [[t2i3:%\d+]] = OpLoad %type_2d_image_array_0 %t2i3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:        [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2i3]] [[gSampler]]
// CHECK-NEXT:        [[res:%\d+]] = OpImageGather %v4int [[sampledImg]] [[loc]] %int_2 ConstOffsets [[c1to8]]
// CHECK-NEXT:                       OpStore %d [[res]]
    int4 d = t2i3.GatherBlue(gSampler, location, int2(1, 2), int2(3, 4), int2(5, 6), int2(7, 8));

    uint status;
// CHECK:              [[t2i3:%\d+]] = OpLoad %type_2d_image_array_0 %t2i3
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:          [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2i3]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseGather %SparseResidencyStruct [[sampledImg]] [[loc]] %int_2 ConstOffset [[c12]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4int [[structResult]] 1
// CHECK-NEXT:                         OpStore %e [[result]]
    int4 e = t2i3.GatherBlue(gSampler, location, int2(1, 2), status);

// CHECK:              [[t2i3:%\d+]] = OpLoad %type_2d_image_array_0 %t2i3
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:          [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2i3]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseGather %SparseResidencyStruct [[sampledImg]] [[loc]] %int_2 ConstOffsets [[c1to8]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4int [[structResult]] 1
// CHECK-NEXT:                         OpStore %f [[result]]
    int4 f = t2i3.GatherBlue(gSampler, location, int2(1, 2), int2(3, 4), int2(5, 6), int2(7, 8), status);

// CHECK:        [[tCubeArray:%\d+]] = OpLoad %type_cube_image_array %tCubeArray
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[tCubeArray]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseGather %SparseResidencyStruct_0 [[sampledImg]] [[cv4f_1_5]] %int_2 None
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4uint [[structResult]] 1
// CHECK-NEXT:                         OpStore %g [[result]]
    uint4 g = tCubeArray.GatherBlue(gSampler, /*location*/ float4(1.5, 1.5, 1.5, 1.5), status);

    return 1.0;
}
