// Run: %dxc -T cs_6_0 -E main

struct struct_with_bool {
  bool2 elem_v2bool;
  int2 elem_v2int;
  float2 elem_v2float;
  bool elem_bool;
  int elem_int;
  float elem_float;
};

ConsumeStructuredBuffer<bool2> consume_v2bool;
ConsumeStructuredBuffer<float2> consume_v2float;
ConsumeStructuredBuffer<int2> consume_v2int;
ConsumeStructuredBuffer<struct_with_bool> consume_struct_with_bool;

ConsumeStructuredBuffer<bool> consume_bool;
ConsumeStructuredBuffer<float> consume_float;
ConsumeStructuredBuffer<int> consume_int;

AppendStructuredBuffer<bool2> append_v2bool;
AppendStructuredBuffer<float2> append_v2float;
AppendStructuredBuffer<int2> append_v2int;
AppendStructuredBuffer<struct_with_bool> append_struct_with_bool;

AppendStructuredBuffer<bool> append_bool;
AppendStructuredBuffer<float> append_float;
AppendStructuredBuffer<int> append_int;

RWStructuredBuffer<bool> rw_bool;
RWStructuredBuffer<bool2> rw_v2bool;

void main() {
// CHECK:       [[p_0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %append_bool %uint_0 {{%\d+}}

// CHECK:       [[p_1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %consume_bool %uint_0 {{%\d+}}
// CHECK-NEXT:  [[i_0:%\d+]] = OpLoad %uint [[p_1]]
// CHECK-NEXT:  [[b_0:%\d+]] = OpINotEqual %bool [[i_0]] %uint_0
// CHECK-NEXT: [[bi_0:%\d+]] = OpSelect %uint [[b_0]] %uint_1 %uint_0
// CHECK-NEXT:                 OpStore [[p_0]] [[bi_0]]
  append_bool.Append(consume_bool.Consume());

// CHECK:       [[p_2:%\d+]] = OpAccessChain %_ptr_Uniform_int %consume_int %uint_0 {{%\d+}}
// CHECK-NEXT:  [[i_1:%\d+]] = OpLoad %int [[p_2]]
// CHECK-NEXT:  [[b_1:%\d+]] = OpINotEqual %bool [[i_1]] %int_0
// CHECK-NEXT: [[bi_1:%\d+]] = OpSelect %uint [[b_1]] %uint_1 %uint_0
// CHECK-NEXT:                 OpStore {{%\d+}} [[bi_1]]
  append_bool.Append(consume_int.Consume());

// CHECK:       [[p_3:%\d+]] = OpAccessChain %_ptr_Uniform_float %consume_float %uint_0 {{%\d+}}
// CHECK-NEXT:  [[f_0:%\d+]] = OpLoad %float [[p_3]]
// CHECK-NEXT:  [[b_2:%\d+]] = OpFOrdNotEqual %bool [[f_0]] %float_0
// CHECK-NEXT: [[bi_2:%\d+]] = OpSelect %uint [[b_2]] %uint_1 %uint_0
// CHECK-NEXT:                 OpStore {{%\d+}} [[bi_2]]
  append_bool.Append(consume_float.Consume());

// CHECK:       [[p_4:%\d+]] = OpAccessChain %_ptr_Uniform_uint %append_bool %uint_0 {{%\d+}}

// CHECK:       [[p_5:%\d+]] = OpAccessChain %_ptr_Uniform_struct_with_bool %consume_struct_with_bool %uint_0 {{%\d+}}
// CHECK-NEXT:  [[s_0:%\d+]] = OpLoad %struct_with_bool [[p_5]]
// CHECK-NEXT: [[vu_0:%\d+]] = OpCompositeExtract %v2uint [[s_0]] 0
// CHECK-NEXT: [[vb_0:%\d+]] = OpINotEqual %v2bool [[vu_0]] {{%\d+}}
// CHECK-NEXT: [[vi_0:%\d+]] = OpCompositeExtract %v2int [[s_0]] 1
// CHECK-NEXT: [[vf_0:%\d+]] = OpCompositeExtract %v2float [[s_0]] 2
// CHECK-NEXT:  [[i_2:%\d+]] = OpCompositeExtract %uint [[s_0]] 3
// CHECK-NEXT:  [[b_3:%\d+]] = OpINotEqual %bool [[i_2]] %uint_0
// CHECK-NEXT:  [[i_3:%\d+]] = OpCompositeExtract %int [[s_0]] 4
// CHECK-NEXT:  [[f_1:%\d+]] = OpCompositeExtract %float [[s_0]] 5
// CHECK-NEXT:  [[s_1:%\d+]] = OpCompositeConstruct %struct_with_bool_0 [[vb_0]] [[vi_0]] [[vf_0]] [[b_3]] [[i_3]] [[f_1]]
// CHECK-NEXT:                 OpStore [[temp_0:%\w+]] [[s_1]]
// CHECK-NEXT:  [[p_6:%\d+]] = OpAccessChain %_ptr_Function_bool [[temp_0]] %int_3
// CHECK-NEXT:  [[b_4:%\d+]] = OpLoad %bool [[p_6]]
// CHECK-NEXT: [[bi_3:%\d+]] = OpSelect %uint [[b_4]] %uint_1 %uint_0
// CHECK-NEXT:                 OpStore [[p_4]] [[bi_3]]
  append_bool.Append(consume_struct_with_bool.Consume().elem_bool);

  //
  // TODO(jaebaek): Uncomment this and all other commented lines after fixing type cast bug
  // https://github.com/Microsoft/DirectXShaderCompiler/issues/2031
  //
  // append_bool.Append(consume_struct_with_bool.Consume().elem_int);
  // append_bool.Append(consume_struct_with_bool.Consume().elem_float);

// CHECK:       [[p_7:%\d+]] = OpAccessChain %_ptr_Uniform_uint %consume_bool %uint_0 {{%\d+}}
// CHECK-NEXT:  [[i_4:%\d+]] = OpLoad %uint [[p_7]]
// CHECK-NEXT:  [[b_5:%\d+]] = OpINotEqual %bool [[i_4]] %uint_0
// CHECK-NEXT: [[bi_4:%\d+]] = OpSelect %int [[b_5]] %int_1 %int_0
// CHECK-NEXT:                 OpStore {{%\d+}} [[bi_4]]
  append_int.Append(consume_bool.Consume());

// CHECK:       [[p_8:%\d+]] = OpAccessChain %_ptr_Uniform_int %consume_int %uint_0 {{%\d+}}
// CHECK-NEXT:  [[i_5:%\d+]] = OpLoad %int [[p_8]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[i_5]]
  append_int.Append(consume_int.Consume());

// CHECK:       [[p_9:%\d+]] = OpAccessChain %_ptr_Uniform_float %consume_float %uint_0 {{%\d+}}
// CHECK-NEXT:  [[f_2:%\d+]] = OpLoad %float [[p_9]]
// CHECK-NEXT:  [[i_6:%\d+]] = OpConvertFToS %int [[f_2]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[i_6]]
  append_int.Append(consume_float.Consume());

  // append_int.Append(consume_struct_with_bool.Consume().elem_bool);

// CHECK:      [[p_10:%\d+]] = OpAccessChain %_ptr_Function_int {{%\w+}} %int_4
// CHECK-NEXT:  [[i_7:%\d+]] = OpLoad %int [[p_10]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[i_7]]
  append_int.Append(consume_struct_with_bool.Consume().elem_int);

  // append_int.Append(consume_struct_with_bool.Consume().elem_float);

// CHECK:      [[p_11:%\d+]] = OpAccessChain %_ptr_Uniform_uint %consume_bool %uint_0 {{%\d+}}
// CHECK-NEXT:  [[i_7:%\d+]] = OpLoad %uint [[p_11]]
// CHECK-NEXT:  [[b_6:%\d+]] = OpINotEqual %bool [[i_7]] %uint_0
// CHECK-NEXT: [[bf_0:%\d+]] = OpSelect %float [[b_6]] %float_1 %float_0
// CHECK-NEXT:                 OpStore {{%\d+}} [[bf_0]]
  append_float.Append(consume_bool.Consume());

// CHECK:      [[p_12:%\d+]] = OpAccessChain %_ptr_Uniform_int %consume_int %uint_0 {{%\d+}}
// CHECK-NEXT:  [[i_8:%\d+]] = OpLoad %int [[p_12]]
// CHECK-NEXT:  [[f_3:%\d+]] = OpConvertSToF %float [[i_8]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[f_3]]
  append_float.Append(consume_int.Consume());

// CHECK:      [[p_13:%\d+]] = OpAccessChain %_ptr_Uniform_float %consume_float %uint_0 {{%\d+}}
// CHECK-NEXT:  [[f_4:%\d+]] = OpLoad %float [[p_13]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[f_4]]
  append_float.Append(consume_float.Consume());

  // append_float.Append(consume_struct_with_bool.Consume().elem_bool);
  // append_float.Append(consume_struct_with_bool.Consume().elem_int);

// CHECK:      [[p_14:%\d+]] = OpAccessChain %_ptr_Function_float {{%\w+}} %int_5
// CHECK-NEXT:  [[f_5:%\d+]] = OpLoad %float [[p_14]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[f_5]]
  append_float.Append(consume_struct_with_bool.Consume().elem_float);

// CHECK:      [[p_15:%\d+]] = OpAccessChain %_ptr_Uniform_v2uint %consume_v2bool %uint_0 {{%\d+}}
// CHECK-NEXT: [[vu_1:%\d+]] = OpLoad %v2uint [[p_15]]
// CHECK-NEXT: [[vb_1:%\d+]] = OpINotEqual %v2bool [[vu_1]] {{%\d+}}
// CHECK-NEXT: [[vu_2:%\d+]] = OpSelect %v2uint [[vb_1]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                 OpStore {{%\d+}} [[vu_2]]
  append_v2bool.Append(consume_v2bool.Consume());

// CHECK:      [[p_16:%\d+]] = OpAccessChain %_ptr_Uniform_v2int %consume_v2int %uint_0 {{%\d+}}
// CHECK-NEXT: [[vi_1:%\d+]] = OpLoad %v2int [[p_16]]
// CHECK-NEXT: [[vb_2:%\d+]] = OpINotEqual %v2bool [[vi_1]] {{%\d+}}
// CHECK-NEXT: [[vu_3:%\d+]] = OpSelect %v2uint [[vb_2]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                 OpStore {{%\d+}} [[vu_3]]
  append_v2bool.Append(consume_v2int.Consume());

// CHECK:      [[p_17:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %consume_v2float %uint_0 {{%\d+}}
// CHECK-NEXT: [[vf_1:%\d+]] = OpLoad %v2float [[p_17]]
// CHECK-NEXT: [[vb_3:%\d+]] = OpFOrdNotEqual %v2bool [[vf_1]] {{%\d+}}
// CHECK-NEXT: [[vu_4:%\d+]] = OpSelect %v2uint [[vb_3]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                 OpStore {{%\d+}} [[vu_4]]
  append_v2bool.Append(consume_v2float.Consume());

// CHECK:      [[p_18:%\d+]] = OpAccessChain %_ptr_Function_v2bool {{%\w+}} %int_0
// CHECK-NEXT: [[vb_4:%\d+]] = OpLoad %v2bool [[p_18]]
// CHECK-NEXT: [[vu_5:%\d+]] = OpSelect %v2uint [[vb_4]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                 OpStore {{%\d+}} [[vu_5]]
  append_v2bool.Append(consume_struct_with_bool.Consume().elem_v2bool);

// CHECK:      [[p_19:%\d+]] = OpAccessChain %_ptr_Function_v2int {{%\w+}} %int_1
// CHECK-NEXT: [[vi_2:%\d+]] = OpLoad %v2int [[p_19]]
// CHECK-NEXT: [[vb_5:%\d+]] = OpINotEqual %v2bool [[vi_2]] {{%\d+}}
// CHECK-NEXT: [[vu_6:%\d+]] = OpSelect %v2uint [[vb_5]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                 OpStore {{%\d+}} [[vu_6]]
  append_v2bool.Append(consume_struct_with_bool.Consume().elem_v2int);

// CHECK:      [[p_20:%\d+]] = OpAccessChain %_ptr_Function_v2float {{%\w+}} %int_2
// CHECK-NEXT: [[vf_2:%\d+]] = OpLoad %v2float [[p_20]]
// CHECK-NEXT: [[vb_6:%\d+]] = OpFOrdNotEqual %v2bool [[vf_2]] {{%\d+}}
// CHECK-NEXT: [[vu_7:%\d+]] = OpSelect %v2uint [[vb_6]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                 OpStore {{%\d+}} [[vu_7]]
  append_v2bool.Append(consume_struct_with_bool.Consume().elem_v2float);

// CHECK:      [[p_21:%\d+]] = OpAccessChain %_ptr_Uniform_v2uint %consume_v2bool %uint_0 {{%\d+}}
// CHECK-NEXT: [[vu_8:%\d+]] = OpLoad %v2uint [[p_21]]
// CHECK-NEXT: [[vb_7:%\d+]] = OpINotEqual %v2bool [[vu_8]] {{%\d+}}
// CHECK-NEXT: [[vu_9:%\d+]] = OpSelect %v2int [[vb_7]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                 OpStore {{%\d+}} [[vu_9]]
  append_v2int.Append(consume_v2bool.Consume());

// CHECK:      [[p_22:%\d+]] = OpAccessChain %_ptr_Uniform_v2int %consume_v2int %uint_0 {{%\d+}}
// CHECK-NEXT: [[vi_3:%\d+]] = OpLoad %v2int [[p_22]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[vi_3]]
  append_v2int.Append(consume_v2int.Consume());

// CHECK:      [[p_23:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %consume_v2float %uint_0 {{%\d+}}
// CHECK-NEXT: [[vf_3:%\d+]] = OpLoad %v2float [[p_23]]
// CHECK-NEXT: [[vi_4:%\d+]] = OpConvertFToS %v2int [[vf_3]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[vi_4]]
  append_v2int.Append(consume_v2float.Consume());

// CHECK:       [[p_24:%\d+]] = OpAccessChain %_ptr_Function_v2bool {{%\w+}} %int_0
// CHECK-NEXT:  [[vb_8:%\d+]] = OpLoad %v2bool [[p_24]]
// CHECK-NEXT: [[vu_10:%\d+]] = OpSelect %v2int [[vb_8]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                  OpStore {{%\d+}} [[vu_10]]
  append_v2int.Append(consume_struct_with_bool.Consume().elem_v2bool);

// CHECK:      [[p_25:%\d+]] = OpAccessChain %_ptr_Function_v2int {{%\w+}} %int_1
// CHECK-NEXT: [[vi_5:%\d+]] = OpLoad %v2int [[p_25]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[vi_5]]
  append_v2int.Append(consume_struct_with_bool.Consume().elem_v2int);

// CHECK:      [[p_26:%\d+]] = OpAccessChain %_ptr_Function_v2float {{%\w+}} %int_2
// CHECK-NEXT: [[vf_4:%\d+]] = OpLoad %v2float [[p_26]]
// CHECK-NEXT: [[vi_6:%\d+]] = OpConvertFToS %v2int [[vf_4]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[vi_6]]
  append_v2int.Append(consume_struct_with_bool.Consume().elem_v2float);

// CHECK:       [[p_27:%\d+]] = OpAccessChain %_ptr_Uniform_v2uint %consume_v2bool %uint_0 {{%\d+}}
// CHECK-NEXT: [[vu_11:%\d+]] = OpLoad %v2uint [[p_27]]
// CHECK-NEXT:  [[vb_9:%\d+]] = OpINotEqual %v2bool [[vu_11]] {{%\d+}}
// CHECK-NEXT:  [[vf_5:%\d+]] = OpSelect %v2float [[vb_9]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                  OpStore {{%\d+}} [[vf_5]]
  append_v2float.Append(consume_v2bool.Consume());

// CHECK:      [[p_28:%\d+]] = OpAccessChain %_ptr_Uniform_v2int %consume_v2int %uint_0 {{%\d+}}
// CHECK-NEXT: [[vi_7:%\d+]] = OpLoad %v2int [[p_28]]
// CHECK-NEXT: [[vf_6:%\d+]] = OpConvertSToF %v2float [[vi_7]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[vf_6]]
  append_v2float.Append(consume_v2int.Consume());

// CHECK:      [[p_29:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %consume_v2float %uint_0 {{%\d+}}
// CHECK-NEXT: [[vf_7:%\d+]] = OpLoad %v2float [[p_29]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[vf_7]]
  append_v2float.Append(consume_v2float.Consume());

// CHECK:       [[p_30:%\d+]] = OpAccessChain %_ptr_Function_v2bool {{%\w+}} %int_0
// CHECK-NEXT: [[vb_10:%\d+]] = OpLoad %v2bool [[p_30]]
// CHECK-NEXT:  [[vf_8:%\d+]] = OpSelect %v2float [[vb_10]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                  OpStore {{%\d+}} [[vf_8]]
  append_v2float.Append(consume_struct_with_bool.Consume().elem_v2bool);

// CHECK:      [[p_31:%\d+]] = OpAccessChain %_ptr_Function_v2int {{%\w+}} %int_1
// CHECK-NEXT: [[vi_8:%\d+]] = OpLoad %v2int [[p_31]]
// CHECK-NEXT: [[vf_9:%\d+]] = OpConvertSToF %v2float [[vi_8]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[vf_9]]
  append_v2float.Append(consume_struct_with_bool.Consume().elem_v2int);

// CHECK:       [[p_32:%\d+]] = OpAccessChain %_ptr_Function_v2float {{%\w+}} %int_2
// CHECK-NEXT: [[vf_10:%\d+]] = OpLoad %v2float [[p_32]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[vf_10]]
  append_v2float.Append(consume_struct_with_bool.Consume().elem_v2float);

// CHECK:       [[p_33:%\d+]] = OpAccessChain %_ptr_Uniform_v2uint %consume_v2bool %uint_0 {{%\d+}}
// CHECK-NEXT: [[vu_12:%\d+]] = OpLoad %v2uint [[p_33]]
// CHECK-NEXT: [[vb_11:%\d+]] = OpINotEqual %v2bool [[vu_12]] {{%\d+}}
// CHECK-NEXT:   [[b_7:%\d+]] = OpCompositeExtract %bool [[vb_11]] 0
// CHECK-NEXT:   [[i_9:%\d+]] = OpSelect %uint [[b_7]] %uint_1 %uint_0
// CHECK-NEXT:                  OpStore {{%\d+}} [[i_9]]
  append_bool.Append(consume_v2bool.Consume().x);

// CHECK:       [[p_34:%\d+]] = OpAccessChain %_ptr_Uniform_v2int %consume_v2int %uint_0 {{%\d+}}
// CHECK-NEXT: [[vu_13:%\d+]] = OpLoad %v2int [[p_34]]
// CHECK-NEXT:  [[i_10:%\d+]] = OpCompositeExtract %int [[vu_13]] 0
// CHECK-NEXT:   [[b_8:%\d+]] = OpINotEqual %bool [[i_10]] %int_0
// CHECK-NEXT:  [[i_10:%\d+]] = OpSelect %uint [[b_8]] %uint_1 %uint_0
// CHECK-NEXT:                  OpStore {{%\d+}} [[i_10]]
  append_bool.Append(consume_v2int.Consume().x);

// CHECK:       [[p_35:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %consume_v2float %uint_0 {{%\d+}}
// CHECK-NEXT: [[vf_11:%\d+]] = OpLoad %v2float [[p_35]]
// CHECK-NEXT:   [[f_6:%\d+]] = OpCompositeExtract %float [[vf_11]] 0
// CHECK-NEXT:   [[b_9:%\d+]] = OpFOrdNotEqual %bool [[f_6]] %float_0
// CHECK-NEXT:  [[i_11:%\d+]] = OpSelect %uint [[b_9]] %uint_1 %uint_0
// CHECK-NEXT:                  OpStore {{%\d+}} [[i_11]]
  append_bool.Append(consume_v2float.Consume().x);

// CHECK:       [[p_36:%\d+]] = OpAccessChain %_ptr_Function_v2bool {{%\w+}} %int_0
// CHECK-NEXT: [[vb_12:%\d+]] = OpLoad %v2bool [[p_36]]
// CHECK-NEXT:  [[b_10:%\d+]] = OpCompositeExtract %bool [[vb_12]] 0
// CHECK-NEXT:  [[i_12:%\d+]] = OpSelect %uint [[b_10]] %uint_1 %uint_0
// CHECK-NEXT:                  OpStore {{%\d+}} [[i_12]]
  append_bool.Append(consume_struct_with_bool.Consume().elem_v2bool.x);

  // append_bool.Append(consume_struct_with_bool.Consume().elem_v2int.x);
  // append_bool.Append(consume_struct_with_bool.Consume().elem_v2float.x);

// CHECK:       [[p_37:%\d+]] = OpAccessChain %_ptr_Uniform_v2uint %consume_v2bool %uint_0 {{%\d+}}
// CHECK-NEXT: [[vu_14:%\d+]] = OpLoad %v2uint [[p_37]]
// CHECK-NEXT: [[vb_13:%\d+]] = OpINotEqual %v2bool [[vu_14]] {{%\d+}}
// CHECK-NEXT:  [[b_11:%\d+]] = OpCompositeExtract %bool [[vb_13]] 0
// CHECK-NEXT:  [[i_13:%\d+]] = OpSelect %int [[b_11]] %int_1 %int_0
// CHECK-NEXT:                  OpStore {{%\d+}} [[i_13]]
  append_int.Append(consume_v2bool.Consume().x);

// CHECK:       [[p_38:%\d+]] = OpAccessChain %_ptr_Uniform_v2int %consume_v2int %uint_0 {{%\d+}}
// CHECK-NEXT:  [[vi_9:%\d+]] = OpLoad %v2int [[p_38]]
// CHECK-NEXT:  [[i_14:%\d+]] = OpCompositeExtract %int [[vi_9]] 0
// CHECK-NEXT:                  OpStore {{%\d+}} [[i_14]]
  append_int.Append(consume_v2int.Consume().x);

// CHECK:       [[p_39:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %consume_v2float %uint_0 {{%\d+}}
// CHECK-NEXT: [[vf_12:%\d+]] = OpLoad %v2float [[p_39]]
// CHECK-NEXT:   [[f_7:%\d+]] = OpCompositeExtract %float [[vf_12]] 0
// CHECK-NEXT:  [[i_15:%\d+]] = OpConvertFToS %int [[f_7]]
// CHECK-NEXT:                  OpStore {{%\d+}} [[i_15]]
  append_int.Append(consume_v2float.Consume().x);

  // append_int.Append(consume_struct_with_bool.Consume().elem_v2bool.x);

// CHECK:       [[p_40:%\d+]] = OpAccessChain %_ptr_Function_v2int {{%\w+}} %int_1
// CHECK-NEXT: [[vi_10:%\d+]] = OpLoad %v2int [[p_40]]
// CHECK-NEXT:  [[i_16:%\d+]] = OpCompositeExtract %int [[vi_10]] 0
// CHECK-NEXT:                  OpStore {{%\d+}} [[i_16]]
  append_int.Append(consume_struct_with_bool.Consume().elem_v2int.x);

  // append_int.Append(consume_struct_with_bool.Consume().elem_v2float.x);

// CHECK:       [[p_41:%\d+]] = OpAccessChain %_ptr_Uniform_v2uint %consume_v2bool %uint_0 {{%\d+}}
// CHECK-NEXT: [[vu_15:%\d+]] = OpLoad %v2uint [[p_41]]
// CHECK-NEXT: [[vb_14:%\d+]] = OpINotEqual %v2bool [[vu_15]] {{%\d+}}
// CHECK-NEXT:  [[b_12:%\d+]] = OpCompositeExtract %bool [[vb_14]] 0
// CHECK-NEXT:   [[f_8:%\d+]] = OpSelect %float [[b_12]] %float_1 %float_0
// CHECK-NEXT:                  OpStore {{%\d+}} [[f_8]]
  append_float.Append(consume_v2bool.Consume().x);

// CHECK:       [[p_42:%\d+]] = OpAccessChain %_ptr_Uniform_v2int %consume_v2int %uint_0 {{%\d+}}
// CHECK-NEXT: [[vi_11:%\d+]] = OpLoad %v2int [[p_42]]
// CHECK-NEXT:  [[i_17:%\d+]] = OpCompositeExtract %int [[vi_11]] 0
// CHECK-NEXT:   [[f_9:%\d+]] = OpConvertSToF %float [[i_17]]
// CHECK-NEXT:                  OpStore {{%\d+}} [[f_9]]
  append_float.Append(consume_v2int.Consume().x);

// CHECK:       [[p_43:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %consume_v2float %uint_0 {{%\d+}}
// CHECK-NEXT: [[vf_13:%\d+]] = OpLoad %v2float [[p_43]]
// CHECK-NEXT:  [[f_10:%\d+]] = OpCompositeExtract %float [[vf_13]] 0
// CHECK-NEXT:                  OpStore {{%\d+}} [[f_10]]
  append_float.Append(consume_v2float.Consume().x);

  // append_float.Append(consume_struct_with_bool.Consume().elem_v2bool.x);
  // append_float.Append(consume_struct_with_bool.Consume().elem_v2int.x);

// CHECK:       [[p_44:%\d+]] = OpAccessChain %_ptr_Function_v2float {{%\w+}} %int_2
// CHECK-NEXT: [[vf_14:%\d+]] = OpLoad %v2float [[p_44]]
// CHECK-NEXT:  [[f_11:%\d+]] = OpCompositeExtract %float [[vf_14]] 0
// CHECK-NEXT:                  OpStore {{%\d+}} [[f_11]]
  append_float.Append(consume_struct_with_bool.Consume().elem_v2float.x);

// CHECK:      [[p_45:%\d+]] = OpAccessChain %_ptr_Uniform_uint %append_bool %uint_0 {{%\d+}}
// CHECK:      [[p_46:%\d+]] = OpAccessChain %_ptr_Uniform_uint %rw_bool %int_0 %uint_0
// CHECK-NEXT: [[i_18:%\d+]] = OpLoad %uint [[p_46]]
// CHECK-NEXT: [[b_13:%\d+]] = OpINotEqual %bool [[i_18]] %uint_0
// CHECK-NEXT: [[i_19:%\d+]] = OpSelect %uint [[b_13]] %uint_1 %uint_0
// CHECK-NEXT:                 OpStore [[p_45]] [[i_19]]
  append_bool.Append(rw_bool[0]);

// CHECK:      [[i_19:%\d+]] = OpLoad %uint {{%\d+}}
// CHECK-NEXT: [[b_14:%\d+]] = OpINotEqual %bool [[i_19]] %uint_0
// CHECK-NEXT: [[i_20:%\d+]] = OpSelect %uint [[b_14]] %uint_1 %uint_0
// CHECK-NEXT:                 OpStore {{%\d+}} [[i_20]]
  append_bool.Append(rw_v2bool[0].x);

// CHECK:      [[p_47:%\d+]] = OpAccessChain %_ptr_Uniform_int %append_int %uint_0 {{%\d+}}
// CHECK:      [[p_48:%\d+]] = OpAccessChain %_ptr_Uniform_uint %rw_bool %int_0 %uint_0
// CHECK-NEXT: [[i_21:%\d+]] = OpLoad %uint [[p_48]]
// CHECK-NEXT: [[b_15:%\d+]] = OpINotEqual %bool [[i_21]] %uint_0
// CHECK-NEXT: [[i_22:%\d+]] = OpSelect %int [[b_15]] %int_1 %int_0
// CHECK-NEXT:                 OpStore [[p_47]] [[i_22]]
  append_int.Append(rw_bool[0]);

// CHECK:      [[i_23:%\d+]] = OpLoad %uint {{%\d+}}
// CHECK-NEXT: [[b_16:%\d+]] = OpINotEqual %bool [[i_23]] %uint_0
// CHECK-NEXT: [[i_24:%\d+]] = OpSelect %int [[b_16]] %int_1 %int_0
// CHECK-NEXT:                 OpStore {{%\d+}} [[i_24]]
  append_int.Append(rw_v2bool[0].x);

// CHECK:      [[p_49:%\d+]] = OpAccessChain %_ptr_Uniform_float %append_float %uint_0 {{%\d+}}
// CHECK:      [[p_50:%\d+]] = OpAccessChain %_ptr_Uniform_uint %rw_bool %int_0 %uint_0
// CHECK-NEXT: [[i_25:%\d+]] = OpLoad %uint [[p_50]]
// CHECK-NEXT: [[b_17:%\d+]] = OpINotEqual %bool [[i_25]] %uint_0
// CHECK-NEXT: [[f_12:%\d+]] = OpSelect %float [[b_17]] %float_1 %float_0
// CHECK-NEXT:                 OpStore [[p_49]] [[f_12]]
  append_float.Append(rw_bool[0]);

// CHECK:      [[i_26:%\d+]] = OpLoad %uint {{%\d+}}
// CHECK-NEXT: [[b_18:%\d+]] = OpINotEqual %bool [[i_26]] %uint_0
// CHECK-NEXT: [[i_27:%\d+]] = OpSelect %float [[b_18]] %float_1 %float_0
// CHECK-NEXT:                 OpStore {{%\d+}} [[i_27]]
  append_float.Append(rw_v2bool[0].x);
}
