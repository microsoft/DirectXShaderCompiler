// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s5);

Texture2DArray<float4> t2f4 : register(t1);
Texture2DArray<int3>   t2i3 : register(t2);
// .GatherBlue() does not support Texture1DArray.
// .GatherBlue() for TextureCubeArray only has one signature that takes the status parameter.

// CHECK: [[c12:%\d+]] = OpConstantComposite %v2int %int_1 %int_2
// CHECK: [[c34:%\d+]] = OpConstantComposite %v2int %int_3 %int_4
// CHECK: [[c56:%\d+]] = OpConstantComposite %v2int %int_5 %int_6
// CHECK: [[c78:%\d+]] = OpConstantComposite %v2int %int_7 %int_8
// CHECK: [[c1to8:%\d+]] = OpConstantComposite %_arr_v2int_uint_4 [[c12]] [[c34]] [[c56]] [[c78]]

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

    return 1.0;
}
