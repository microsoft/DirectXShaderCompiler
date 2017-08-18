// Run: %dxc -T ps_6_0 -E main

SamplerState gSampler : register(s1);

Texture2D   <float4> t2 : register(t2);
TextureCube <float4> t4 : register(t4);
// .Gather() does not support Texture1D and Texture3D.

// CHECK: [[v2ic:%\d+]] = OpConstantComposite %v2int %int_1 %int_2
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_0_1 %float_0_2 %float_0_3

float4 main(float2 location: A) : SV_Target {

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT:        [[loc:%\d+]] = OpLoad %v2float %location
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageGather %v4float [[sampledImg]] [[loc]] %int_0 ConstOffset [[v2ic]]
    float4 val2 = t2.Gather(gSampler, location, int2(1, 2));

// CHECK:              [[t4:%\d+]] = OpLoad %type_cube_image %t4
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t4]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageGather %v4float [[sampledImg]] [[v3fc]] %int_0
    float4 val4 = t4.Gather(gSampler, float3(0.1, 0.2, 0.3));

    return 1.0;
}
