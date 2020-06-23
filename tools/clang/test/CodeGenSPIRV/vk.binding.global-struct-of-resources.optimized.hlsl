// Run: %dxc -T ps_6_0 -E main -fspv-flatten-resource-arrays -O3

// Check the names
//
// CHECK: OpName %secondGlobal_t_0_ "secondGlobal.t[0]"
// CHECK: OpName %secondGlobal_t_1_ "secondGlobal.t[1]"
// CHECK: OpName %firstGlobal_0__0__t_0_ "firstGlobal[0][0].t[0]"
// CHECK: OpName %firstGlobal_0__0__t_1_ "firstGlobal[0][0].t[1]"
// CHECK: OpName %firstGlobal_0__1__t_0_ "firstGlobal[0][1].t[0]"
// CHECK: OpName %firstGlobal_0__1__t_1_ "firstGlobal[0][1].t[1]"
// CHECK: OpName %firstGlobal_1__0__t_0_ "firstGlobal[1][0].t[0]"
// CHECK: OpName %firstGlobal_1__0__t_1_ "firstGlobal[1][0].t[1]"
// CHECK: OpName %firstGlobal_1__1__t_0_ "firstGlobal[1][1].t[0]"
// CHECK: OpName %firstGlobal_1__1__t_1_ "firstGlobal[1][1].t[1]"
// CHECK: OpName %secondGlobal_tt_0__s_1_ "secondGlobal.tt[0].s[1]"
// CHECK: OpName %secondGlobal_tt_1__s_2_ "secondGlobal.tt[1].s[2]"
// CHECK: OpName %firstGlobal_0__0__tt_0__s_1_ "firstGlobal[0][0].tt[0].s[1]"
// CHECK: OpName %firstGlobal_0__0__tt_1__s_2_ "firstGlobal[0][0].tt[1].s[2]"
// CHECK: OpName %firstGlobal_0__1__tt_0__s_1_ "firstGlobal[0][1].tt[0].s[1]"
// CHECK: OpName %firstGlobal_0__1__tt_1__s_2_ "firstGlobal[0][1].tt[1].s[2]"
// CHECK: OpName %firstGlobal_1__0__tt_0__s_1_ "firstGlobal[1][0].tt[0].s[1]"
// CHECK: OpName %firstGlobal_1__0__tt_1__s_2_ "firstGlobal[1][0].tt[1].s[2]"
// CHECK: OpName %firstGlobal_1__1__tt_0__s_1_ "firstGlobal[1][1].tt[0].s[1]"
// CHECK: OpName %firstGlobal_1__1__tt_1__s_2_ "firstGlobal[1][1].tt[1].s[2]"

// Check flattening of bindings
// Explanation: Only the resources that are used will have a binding assignment
// Unused resources will result in gaps in the bindings. This is consistent with
// the behavior of FXC.
//
// In this example:
//
// firstGlobal[0][0].t[0]        binding: 0  (used)
// firstGlobal[0][0].t[1]        binding: 1  (used)
// firstGlobal[0][0].tt[0].s[0]  binding: 2
// firstGlobal[0][0].tt[0].s[1]  binding: 3  (used)
// firstGlobal[0][0].tt[0].s[2]  binding: 4
// firstGlobal[0][0].tt[1].s[0]  binding: 5
// firstGlobal[0][0].tt[1].s[1]  binding: 6
// firstGlobal[0][0].tt[1].s[2]  binding: 7  (used)
// firstGlobal[0][1].t[0]        binding: 8  (used)
// firstGlobal[0][1].t[1]        binding: 9  (used)
// firstGlobal[0][1].tt[0].s[0]  binding: 10
// firstGlobal[0][1].tt[0].s[1]  binding: 11 (used)
// firstGlobal[0][1].tt[0].s[2]  binding: 12
// firstGlobal[0][1].tt[1].s[0]  binding: 13
// firstGlobal[0][1].tt[1].s[1]  binding: 14
// firstGlobal[0][1].tt[1].s[2]  binding: 15 (used)
// firstGlobal[1][0].t[0]        binding: 16 (used)
// firstGlobal[1][0].t[1]        binding: 17 (used)
// firstGlobal[1][0].tt[0].s[0]  binding: 18
// firstGlobal[1][0].tt[0].s[1]  binding: 19 (used)
// firstGlobal[1][0].tt[0].s[2]  binding: 20
// firstGlobal[1][0].tt[1].s[0]  binding: 21
// firstGlobal[1][0].tt[1].s[1]  binding: 22
// firstGlobal[1][0].tt[1].s[2]  binding: 23 (used)
// firstGlobal[1][1].t[0]        binding: 24 (used)
// firstGlobal[1][1].t[1]        binding: 25 (used)
// firstGlobal[1][1].tt[0].s[0]  binding: 26
// firstGlobal[1][1].tt[0].s[1]  binding: 27 (used)
// firstGlobal[1][1].tt[0].s[2]  binding: 28
// firstGlobal[1][1].tt[1].s[0]  binding: 29
// firstGlobal[1][1].tt[1].s[1]  binding: 30
// firstGlobal[1][1].tt[1].s[2]  binding: 31 (used)
// secondGlobal.t[0]             binding: 32 (used)
// secondGlobal.t[1]             binding: 33 (used)
// secondGlobal.tt[0].s[0]       binding: 34
// secondGlobal.tt[0].s[1]       binding: 35 (used)
// secondGlobal.tt[0].s[2]       binding: 36
// secondGlobal.tt[1].s[0]       binding: 37
// secondGlobal.tt[1].s[1]       binding: 38
// secondGlobal.tt[1].s[2]       binding: 39 (used)
//
// CHECK: OpDecorate %secondGlobal_t_0_ Binding 32
// CHECK: OpDecorate %secondGlobal_t_1_ Binding 33
// CHECK: OpDecorate %firstGlobal_0__0__t_0_ Binding 0
// CHECK: OpDecorate %firstGlobal_0__0__t_1_ Binding 1
// CHECK: OpDecorate %firstGlobal_0__1__t_0_ Binding 8
// CHECK: OpDecorate %firstGlobal_0__1__t_1_ Binding 9
// CHECK: OpDecorate %firstGlobal_1__0__t_0_ Binding 16
// CHECK: OpDecorate %firstGlobal_1__0__t_1_ Binding 17
// CHECK: OpDecorate %firstGlobal_1__1__t_0_ Binding 24
// CHECK: OpDecorate %firstGlobal_1__1__t_1_ Binding 25
// CHECK: OpDecorate %secondGlobal_tt_0__s_1_ Binding 35
// CHECK: OpDecorate %secondGlobal_tt_1__s_2_ Binding 39
// CHECK: OpDecorate %firstGlobal_0__0__tt_0__s_1_ Binding 3
// CHECK: OpDecorate %firstGlobal_0__0__tt_1__s_2_ Binding 7
// CHECK: OpDecorate %firstGlobal_0__1__tt_0__s_1_ Binding 11
// CHECK: OpDecorate %firstGlobal_0__1__tt_1__s_2_ Binding 15
// CHECK: OpDecorate %firstGlobal_1__0__tt_0__s_1_ Binding 19
// CHECK: OpDecorate %firstGlobal_1__0__tt_1__s_2_ Binding 23
// CHECK: OpDecorate %firstGlobal_1__1__tt_0__s_1_ Binding 27
// CHECK: OpDecorate %firstGlobal_1__1__tt_1__s_2_ Binding 31

// Check existence of replacement variables
//
// CHECK: %secondGlobal_t_0_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: %secondGlobal_t_1_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: %firstGlobal_0__0__t_0_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: %firstGlobal_0__0__t_1_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: %firstGlobal_0__1__t_0_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: %firstGlobal_0__1__t_1_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: %firstGlobal_1__0__t_0_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: %firstGlobal_1__0__t_1_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: %firstGlobal_1__1__t_0_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: %firstGlobal_1__1__t_1_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: %secondGlobal_tt_0__s_1_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK: %secondGlobal_tt_1__s_2_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK: %firstGlobal_0__0__tt_0__s_1_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK: %firstGlobal_0__0__tt_1__s_2_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK: %firstGlobal_0__1__tt_0__s_1_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK: %firstGlobal_0__1__tt_1__s_2_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK: %firstGlobal_1__0__tt_0__s_1_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK: %firstGlobal_1__0__tt_1__s_2_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK: %firstGlobal_1__1__tt_0__s_1_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
// CHECK: %firstGlobal_1__1__tt_1__s_2_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant

struct T {
  SamplerState s[3];
};

struct S {
  Texture2D t[2];
  T tt[2];
};

float4 tex2D(S x, float2 v) {
  return x.t[0].Sample(x.tt[0].s[1], v) + x.t[1].Sample(x.tt[1].s[2], v);
}

S firstGlobal[2][2];
S secondGlobal;

float4 main() : SV_Target {
  return 
// CHECK:      [[fg_0_0_t_0:%\d+]] = OpLoad %type_2d_image %firstGlobal_0__0__t_0_
// CHECK:      [[fg_0_0_t_1:%\d+]] = OpLoad %type_2d_image %firstGlobal_0__0__t_1_
// CHECK: [[fg_0_0_tt_0_s_1:%\d+]] = OpLoad %type_sampler %firstGlobal_0__0__tt_0__s_1_
// CHECK: [[fg_0_0_tt_1_s_2:%\d+]] = OpLoad %type_sampler %firstGlobal_0__0__tt_1__s_2_
// CHECK:   [[sampled_img_1:%\d+]] = OpSampledImage %type_sampled_image [[fg_0_0_t_0]] [[fg_0_0_tt_0_s_1]]
// CHECK:                 {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampled_img_1]]
// CHECK:   [[sampled_img_2:%\d+]] = OpSampledImage %type_sampled_image [[fg_0_0_t_1]] [[fg_0_0_tt_1_s_2]]
// CHECK:                 {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampled_img_2]]
// CHECK:                            OpFAdd
    tex2D(firstGlobal[0][0], float2(0,0)) +
// CHECK:      [[fg_0_1_t_0:%\d+]] = OpLoad %type_2d_image %firstGlobal_0__1__t_0_
// CHECK:      [[fg_0_1_t_1:%\d+]] = OpLoad %type_2d_image %firstGlobal_0__1__t_1_
// CHECK: [[fg_0_1_tt_0_s_1:%\d+]] = OpLoad %type_sampler %firstGlobal_0__1__tt_0__s_1_
// CHECK: [[fg_0_1_tt_1_s_2:%\d+]] = OpLoad %type_sampler %firstGlobal_0__1__tt_1__s_2_
// CHECK:   [[sampled_img_3:%\d+]] = OpSampledImage %type_sampled_image [[fg_0_1_t_0]] [[fg_0_1_tt_0_s_1]]
// CHECK:                 {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampled_img_3]]
// CHECK:   [[sampled_img_4:%\d+]] = OpSampledImage %type_sampled_image [[fg_0_1_t_1]] [[fg_0_1_tt_1_s_2]]
// CHECK:                 {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampled_img_4]]
// CHECK:                            OpFAdd
    tex2D(firstGlobal[0][1], float2(0,0)) +
// CHECK:      [[fg_1_0_t_0:%\d+]] = OpLoad %type_2d_image %firstGlobal_1__0__t_0_
// CHECK:      [[fg_1_0_t_1:%\d+]] = OpLoad %type_2d_image %firstGlobal_1__0__t_1_
// CHECK: [[fg_1_0_tt_0_s_1:%\d+]] = OpLoad %type_sampler %firstGlobal_1__0__tt_0__s_1_
// CHECK: [[fg_1_0_tt_1_s_2:%\d+]] = OpLoad %type_sampler %firstGlobal_1__0__tt_1__s_2_
// CHECK:   [[sampled_img_5:%\d+]] = OpSampledImage %type_sampled_image [[fg_1_0_t_0]] [[fg_1_0_tt_0_s_1]]
// CHECK:                 {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampled_img_5]]
// CHECK:   [[sampled_img_6:%\d+]] = OpSampledImage %type_sampled_image [[fg_1_0_t_1]] [[fg_1_0_tt_1_s_2]]
// CHECK:                 {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampled_img_6]]
// CHECK:                            OpFAdd
    tex2D(firstGlobal[1][0], float2(0,0)) +
// CHECK:      [[fg_1_1_t_0:%\d+]] = OpLoad %type_2d_image %firstGlobal_1__1__t_0_
// CHECK:      [[fg_1_1_t_1:%\d+]] = OpLoad %type_2d_image %firstGlobal_1__1__t_1_
// CHECK: [[fg_1_1_tt_0_s_1:%\d+]] = OpLoad %type_sampler %firstGlobal_1__1__tt_0__s_1_
// CHECK: [[fg_1_1_tt_1_s_2:%\d+]] = OpLoad %type_sampler %firstGlobal_1__1__tt_1__s_2_
// CHECK:   [[sampled_img_7:%\d+]] = OpSampledImage %type_sampled_image [[fg_1_1_t_0]] [[fg_1_1_tt_0_s_1]]
// CHECK:                 {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampled_img_7]]
// CHECK:   [[sampled_img_8:%\d+]] = OpSampledImage %type_sampled_image [[fg_1_1_t_1]] [[fg_1_1_tt_1_s_2]]
// CHECK:                 {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampled_img_8]]
// CHECK:                            OpFAdd
    tex2D(firstGlobal[1][1], float2(0,0)) +
// CHECK:          [[sg_t_0:%\d+]] = OpLoad %type_2d_image %secondGlobal_t_0_
// CHECK:     [[sg_tt_0_s_1:%\d+]] = OpLoad %type_sampler %secondGlobal_tt_0__s_1_
// CHECK:   [[sampled_img_9:%\d+]] = OpSampledImage %type_sampled_image [[sg_t_0]] [[sg_tt_0_s_1]]
// CHECK:                 {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampled_img_9]]
// CHECK:                            OpFAdd
    secondGlobal.t[0].Sample(secondGlobal.tt[0].s[1], float2(0,0)) +
// CHECK:          [[sg_t_1:%\d+]] = OpLoad %type_2d_image %secondGlobal_t_1_
// CHECK:     [[sg_tt_1_s_2:%\d+]] = OpLoad %type_sampler %secondGlobal_tt_1__s_2_
// CHECK:  [[sampled_img_10:%\d+]] = OpSampledImage %type_sampled_image [[sg_t_1]] [[sg_tt_1_s_2]]
// CHECK:                 {{%\d+}} = OpImageSampleImplicitLod %v4float [[sampled_img_10]]
    secondGlobal.t[1].Sample(secondGlobal.tt[1].s[2], float2(0,0));
}

