// RUN: %dxc -T ps_6_0 -E main %s -spirv | FileCheck %s

// CHECK: [[type_2d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image]]
// CHECK: [[ptr_type:%[a-zA-Z0-9_]+]] = OpTypePointer UniformConstant [[type_sampled_image]]

// CHECK: [[tex:%[a-zA-Z0-9_]+]] = OpVariable [[ptr_type]] UniformConstant
// CHECK: [[in_var_TEXCOORD:%[a-zA-Z0-9_]+]] = OpVariable %_ptr_Input_v2float Input
// CHECK: [[out_var_SV_Target:%[a-zA-Z0-9_]+]] = OpVariable %_ptr_Output_v4float Output

vk::SampledTexture2D tex : register(t0);

float4 main(float2 uv : TEXCOORD) : SV_Target {
// CHECK: [[tex_coord:%[a-zA-Z0-9_]+]] = OpLoad %v2float [[in_var_TEXCOORD]]
// CHECK: [[tex_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image]] [[tex]]
// CHECK: [[sampled_result:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float [[tex_load]] [[tex_coord]] None
// CHECK: OpStore [[out_var_SV_Target]] [[sampled_result]]
    return tex.Sample(uv);
}
