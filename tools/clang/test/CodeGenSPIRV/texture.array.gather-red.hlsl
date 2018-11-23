// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s5);

Texture2DArray<float4> t2f4 : register(t1);
Texture2DArray<uint>   t2u1 : register(t2);
TextureCubeArray<int4> tCubeArray : register(t3);
// .GatherRed() does not support Texture1DArray.

// CHECK: OpCapability SparseResidency

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4uint
// CHECK: %SparseResidencyStruct_0 = OpTypeStruct %uint %v4int

// CHECK:      [[c12:%\d+]] = OpConstantComposite %v2int %int_1 %int_2
// CHECK:      [[c34:%\d+]] = OpConstantComposite %v2int %int_3 %int_4
// CHECK:      [[c56:%\d+]] = OpConstantComposite %v2int %int_5 %int_6
// CHECK:      [[c78:%\d+]] = OpConstantComposite %v2int %int_7 %int_8
// CHECK:    [[c1to8:%\d+]] = OpConstantComposite %_arr_v2int_uint_4 [[c12]] [[c34]] [[c56]] [[c78]]
// CHECK: [[cv4f_1_5:%\d+]] = OpConstantComposite %v4float %float_1_5 %float_1_5 %float_1_5 %float_1_5

float4 main(float3 location: A, int2 offset : B) : SV_Target {
// CHECK:            [[t2f4:%\d+]] = OpLoad %type_2d_image_array %t2f4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:        [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t2f4]] [[gSampler]]
// CHECK-NEXT:        [[res:%\d+]] = OpImageGather %v4float [[sampledImg]] [[loc]] %int_0 ConstOffset [[c12]]
// CHECK-NEXT:                       OpStore %a [[res]]
    float4 a = t2f4.GatherRed(gSampler, location, int2(1, 2));
// CHECK:            [[t2f4:%\d+]] = OpLoad %type_2d_image_array %t2f4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:        [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t2f4]] [[gSampler]]
// CHECK-NEXT:        [[res:%\d+]] = OpImageGather %v4float [[sampledImg]] [[loc]] %int_0 ConstOffsets [[c1to8]]
// CHECK-NEXT:                       OpStore %b [[res]]
    float4 b = t2f4.GatherRed(gSampler, location, int2(1, 2), int2(3, 4), int2(5, 6), int2(7, 8));

// CHECK:            [[t2u1:%\d+]] = OpLoad %type_2d_image_array_0 %t2u1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:        [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2u1]] [[gSampler]]
// CHECK-NEXT:        [[res:%\d+]] = OpImageGather %v4uint [[sampledImg]] [[loc]] %int_0 ConstOffset [[c12]]
// CHECK-NEXT:                       OpStore %c [[res]]
    uint4 c = t2u1.GatherRed(gSampler, location, int2(1, 2));
// CHECK:            [[t2u1:%\d+]] = OpLoad %type_2d_image_array_0 %t2u1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:        [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2u1]] [[gSampler]]
// CHECK-NEXT:        [[res:%\d+]] = OpImageGather %v4uint [[sampledImg]] [[loc]] %int_0 ConstOffsets [[c1to8]]
// CHECK-NEXT:                       OpStore %d [[res]]
    uint4 d = t2u1.GatherRed(gSampler, location, int2(1, 2), int2(3, 4), int2(5, 6), int2(7, 8));

    uint status;
// CHECK:              [[t2u1:%\d+]] = OpLoad %type_2d_image_array_0 %t2u1
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:          [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2u1]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseGather %SparseResidencyStruct [[sampledImg]] [[loc]] %int_0 ConstOffset [[c12]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4uint [[structResult]] 1
// CHECK-NEXT:                         OpStore %e [[result]]
    uint4 e = t2u1.GatherRed(gSampler, location, int2(1, 2), status);

// CHECK:              [[t2u1:%\d+]] = OpLoad %type_2d_image_array_0 %t2u1
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:          [[loc:%\d+]] = OpLoad %v3float %location
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2u1]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseGather %SparseResidencyStruct [[sampledImg]] [[loc]] %int_0 ConstOffsets [[c1to8]]
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4uint [[structResult]] 1
// CHECK-NEXT:                         OpStore %f [[result]]
    uint4 f = t2u1.GatherRed(gSampler, location, int2(1, 2), int2(3, 4), int2(5, 6), int2(7, 8), status);

// CHECK:        [[tCubeArray:%\d+]] = OpLoad %type_cube_image_array %tCubeArray
// CHECK-NEXT:     [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:   [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[tCubeArray]] [[gSampler]]
// CHECK-NEXT: [[structResult:%\d+]] = OpImageSparseGather %SparseResidencyStruct_0 [[sampledImg]] [[cv4f_1_5]] %int_0 None
// CHECK-NEXT:       [[status:%\d+]] = OpCompositeExtract %uint [[structResult]] 0
// CHECK-NEXT:                         OpStore %status [[status]]
// CHECK-NEXT:       [[result:%\d+]] = OpCompositeExtract %v4int [[structResult]] 1
// CHECK-NEXT:                         OpStore %g [[result]]
    int4 g = tCubeArray.GatherRed(gSampler, /*location*/ float4(1.5, 1.5, 1.5, 1.5), status);

// CHECK: [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK: [[gather:%\d+]] = OpImageGather %v4float {{%\d+}} {{%\d+}} %int_0 Offset [[offset]]
// CHECK: [[texel0:%\d+]] = OpCompositeExtract %float [[gather]] 0
// CHECK: [[gather:%\d+]] = OpImageGather %v4float {{%\d+}} {{%\d+}} %int_0 Offset [[c34]]
// CHECK: [[texel1:%\d+]] = OpCompositeExtract %float [[gather]] 1
// CHECK: [[gather:%\d+]] = OpImageGather %v4float {{%\d+}} {{%\d+}} %int_0 Offset [[c56]]
// CHECK: [[texel2:%\d+]] = OpCompositeExtract %float [[gather]] 2
// CHECK: [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK: [[gather:%\d+]] = OpImageGather %v4float {{%\d+}} {{%\d+}} %int_0 Offset [[offset]]
// CHECK: [[texel3:%\d+]] = OpCompositeExtract %float [[gather]] 3
// CHECK:                   OpCompositeConstruct %v4float [[texel0]] [[texel1]] [[texel2]] [[texel3]]
    float4 h = t2f4.GatherRed(gSampler, location, offset, int2(3, 4), int2(5, 6), offset);

    return 1.0;
}
