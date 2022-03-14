// RUN: %dxc -T hs_6_0 -E main -pack-optimized

struct HSPatchConstData {
  float tessFactor[3] : SV_TessFactor;
  float insideTessFactor[1] : SV_InsideTessFactor;

// CHECK: OpDecorate %out_var_A Location 0
  float a : A;

// CHECK: OpDecorate %out_var_B Location 0
// CHECK: OpDecorate %out_var_B Component 2
  double b : B;

// CHECK: OpDecorate %out_var_C_0 Location 1
// CHECK: OpDecorate %out_var_C_1 Location 1
// CHECK: OpDecorate %out_var_C_1 Component 2
// CHECK: OpDecorate %out_var_C_2 Location 2
  float2 c[3] : C;

// CHECK: OpDecorate %out_var_D_0 Location 2
// CHECK: OpDecorate %out_var_D_0 Component 2
// CHECK: OpDecorate %out_var_D_1 Location 3
  float2x2 d : D;

// CHECK: OpDecorate %out_var_E Location 3
// CHECK: OpDecorate %out_var_E Component 2
  int e : E;

// CHECK: OpDecorate %out_var_F Location 4
  float2 f : F;

// CHECK: OpDecorate %out_var_G Location 3
// CHECK: OpDecorate %out_var_G Component 3
  float g : G;
};

struct HSCtrlPt {
// CHECK: OpDecorate %out_var_H Location 5
  float h : H;

// CHECK: OpDecorate %out_var_I Location 5
// CHECK: OpDecorate %out_var_I Component 1
  float2 i : I;

// CHECK: OpDecorate %out_var_J_0 Location 4
// CHECK: OpDecorate %out_var_J_0 Component 2
// CHECK: OpDecorate %out_var_J_1 Location 4
// CHECK: OpDecorate %out_var_J_1 Component 3
// CHECK: OpDecorate %out_var_J_2 Location 6
// CHECK: OpDecorate %out_var_J_3 Location 6
// CHECK: OpDecorate %out_var_J_3 Component 1
// CHECK: OpDecorate %out_var_J_4 Location 6
// CHECK: OpDecorate %out_var_J_4 Component 2
  float j[5] : J;

// CHECK: OpDecorate %out_var_K_0 Location 7
// CHECK: OpDecorate %out_var_K_1 Location 8
// CHECK: OpDecorate %out_var_K_2 Location 9
// CHECK: OpDecorate %out_var_K_3 Location 10
  float4x3 k : K;
};

HSPatchConstData HSPatchConstantFunc(const OutputPatch<HSCtrlPt, 3> input) {
  HSPatchConstData data;
  data.tessFactor[0] = 3.0;
  data.tessFactor[1] = 3.0;
  data.tessFactor[2] = 3.0;
  data.insideTessFactor[0] = 3.0;
  return data;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HSPatchConstantFunc")]
[maxtessfactor(15)]
HSCtrlPt main(InputPatch<HSCtrlPt, 3> input, uint CtrlPtID : SV_OutputControlPointID) {
  HSCtrlPt data;
  data.k = input[CtrlPtID].k;
  return data;
}

// CHECK:     [[id:%\w+]] = OpLoad %uint %gl_InvocationID
// CHECK: [[result:%\w+]] = OpFunctionCall %HSCtrlPt %src_main %param_var_input %param_var_CtrlPtID

// CHECK:   [[val:%\w+]] = OpCompositeExtract %_arr_float_uint_5 [[result]] 2
// CHECK: [[ptr_j:%\w+]] = OpAccessChain %_ptr_Private__arr_float_uint_5 %out_var_J [[id]]
// CHECK:                  OpStore [[ptr_j]] [[val]]

// CHECK:  [[j04:%\w+]] = OpAccessChain %_ptr_Private_float %out_var_J %uint_0 %uint_4
// CHECK:  [[j04:%\w+]] = OpLoad %float [[j04]]
// CHECK: [[j_40:%\w+]] = OpAccessChain %_ptr_Output_float %out_var_J_4 %uint_0
// CHECK:                 OpStore [[j_40]] [[j04]]
// CHECK:                 OpAccessChain %_ptr_Private_float %out_var_J %uint_1 %uint_4
// CHECK:                 OpLoad %float
// CHECK:                 OpAccessChain %_ptr_Output_float %out_var_J_4 %uint_1
// CHECK:                 OpStore
// CHECK:                 OpAccessChain %_ptr_Private_float %out_var_J %uint_2 %uint_4
// CHECK:                 OpLoad %float
// CHECK:                 OpAccessChain %_ptr_Output_float %out_var_J_4 %uint_2
// CHECK:                 OpStore

// CHECK:   [[val:%\w+]] = OpCompositeExtract %mat4v3float [[result]] 3
// CHECK: [[ptr_k:%\w+]] = OpAccessChain %_ptr_Private_mat4v3float %out_var_K [[id]]
// CHECK:                  OpStore [[ptr_k]] [[val]]

// CHECK:  [[k03:%\w+]] = OpAccessChain %_ptr_Private_v3float %out_var_K %uint_0 %uint_3
// CHECK:  [[k03:%\w+]] = OpLoad %v3float [[k03]]
// CHECK: [[k_30:%\w+]] = OpAccessChain %_ptr_Output_v3float %out_var_K_3 %uint_0
// CHECK:                 OpStore [[k_30]] [[k03]]
// CHECK:                 OpAccessChain %_ptr_Private_v3float %out_var_K %uint_1 %uint_3
// CHECK:                 OpLoad %v3float
// CHECK:                 OpAccessChain %_ptr_Output_v3float %out_var_K_3 %uint_1
// CHECK:                 OpStore
// CHECK:                 OpAccessChain %_ptr_Private_v3float %out_var_K %uint_2 %uint_3
// CHECK:                 OpLoad %v3float
// CHECK:                 OpAccessChain %_ptr_Output_v3float %out_var_K_3 %uint_2
// CHECK:                 OpStore

// CHECK: OpControlBarrier
