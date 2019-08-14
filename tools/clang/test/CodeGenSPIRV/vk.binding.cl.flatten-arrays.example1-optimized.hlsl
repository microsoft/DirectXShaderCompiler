// Run: %dxc -T ps_6_0 -E main -fspv-flatten-resource-arrays -O3

// CHECK: OpDecorate %AnotherTexture Binding 5
// CHECK: OpDecorate %NextTexture Binding 6
// CHECK: OpDecorate [[MyTextures0:%\d+]] Binding 0
// CHECK: OpDecorate [[MyTextures1:%\d+]] Binding 1
// CHECK: OpDecorate [[MyTextures2:%\d+]] Binding 2
// CHECK: OpDecorate [[MyTextures3:%\d+]] Binding 3
// CHECK: OpDecorate [[MyTextures4:%\d+]] Binding 4
// CHECK: OpDecorate [[MySamplers0:%\d+]] Binding 7
// CHECK: OpDecorate [[MySamplers1:%\d+]] Binding 8

// CHECK: [[MyTextures0]] = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: [[MyTextures1]] = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: [[MyTextures2]] = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: [[MyTextures3]] = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: [[MyTextures4]] = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: [[MySamplers0]] = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK: [[MySamplers1]] = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
Texture2D    MyTextures[5] : register(t0);
Texture2D    NextTexture;  // This is suppose to be t6.
Texture2D    AnotherTexture : register(t5);
SamplerState MySamplers[2];

float4 main(float2 TexCoord : TexCoord) : SV_Target0
{
  float4 result =
    MyTextures[0].Sample(MySamplers[0], TexCoord) +
    MyTextures[1].Sample(MySamplers[0], TexCoord) +
    MyTextures[2].Sample(MySamplers[0], TexCoord) +
    MyTextures[3].Sample(MySamplers[1], TexCoord) +
    MyTextures[4].Sample(MySamplers[1], TexCoord) +
    AnotherTexture.Sample(MySamplers[1], TexCoord) +
    NextTexture.Sample(MySamplers[1], TexCoord);
  return result;
}
