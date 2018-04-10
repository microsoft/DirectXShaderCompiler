// Run: %dxc -T ps_6_0 -E main

// CHECK:        %type_2d_image = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK:   %type_sampled_image = OpTypeSampledImage %type_2d_image
// CHECK:      %type_2d_image_0 = OpTypeImage %float 2D 1 0 0 1 Unknown
// CHECK: %type_sampled_image_0 = OpTypeSampledImage %type_2d_image_0

// CHECK: %gTexture = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
Texture2D              gTexture;
SamplerState           gSampler;
SamplerComparisonState gCmpSampler;

float4 main(float3 input : A) : SV_Target {
  // Use OpSampledImage with Depth=0 OpTypeImage for normal sampling
// CHECK: [[tex:%\d+]] = OpLoad %type_2d_image %gTexture
// CHECK:  [[si:%\d+]] = OpSampledImage %type_sampled_image [[tex]]
// CHECK:                OpImageSampleImplicitLod %v4float [[si]]

  // Use OpSampledImage with Depth=1 OpTypeImage for depth-comparison sampling
    float4 a = gTexture.Sample(gSampler, input.xy);
// CHECK: [[tex:%\d+]] = OpLoad %type_2d_image %gTexture
// CHECK:  [[si:%\d+]] = OpSampledImage %type_sampled_image_0 [[tex]]
// CHECK:                OpImageSampleDrefImplicitLod %float [[si]]
    float4 b = gTexture.SampleCmp(gCmpSampler, input.xy, input.z);

  // Use OpSampledImage with Depth=1 OpTypeImage for depth-comparison sampling
// CHECK: [[tex:%\d+]] = OpLoad %type_2d_image %gTexture
// CHECK:  [[si:%\d+]] = OpSampledImage %type_sampled_image_0 [[tex]]
// CHECK:                OpImageSampleDrefExplicitLod %float [[si]]
    float4 c = gTexture.SampleCmpLevelZero(gCmpSampler, input.xy, input.z);

    return a + b + c;
}
