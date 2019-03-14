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

  append_bool.Append(consume_int.Consume());

// CHECK:       [[p_2:%\d+]] = OpAccessChain %_ptr_Uniform_float %consume_float %uint_0 {{%\d+}}
// CHECK-NEXT:  [[f_0:%\d+]] = OpLoad %float [[p_2]]
// CHECK-NEXT:  [[b_1:%\d+]] = OpFOrdNotEqual %bool [[f_0]] %float_0
// CHECK-NEXT: [[bi_1:%\d+]] = OpSelect %uint [[b_1]] %uint_1 %uint_0
// CHECK-NEXT:                 OpStore {{%\d+}} [[bi_1]]
  append_bool.Append(consume_float.Consume());

  append_bool.Append(consume_struct_with_bool.Consume().elem_bool);
  append_bool.Append(consume_struct_with_bool.Consume().elem_int);
  append_bool.Append(consume_struct_with_bool.Consume().elem_float);

  append_int.Append(consume_bool.Consume());

// CHECK:       [[p_3:%\d+]] = OpAccessChain %_ptr_Uniform_int %consume_int %uint_0 {{%\d+}}
// CHECK-NEXT:  [[i_1:%\d+]] = OpLoad %int [[p_3]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[i_1]]
  append_int.Append(consume_int.Consume());

// CHECK:           {{%\d+}} = OpConvertFToS %int {{%\d+}}
  append_int.Append(consume_float.Consume());

  append_int.Append(consume_struct_with_bool.Consume().elem_bool);
  append_int.Append(consume_struct_with_bool.Consume().elem_int);
  append_int.Append(consume_struct_with_bool.Consume().elem_float);

  append_float.Append(consume_bool.Consume());
  append_float.Append(consume_int.Consume());
  append_float.Append(consume_float.Consume());
  append_float.Append(consume_struct_with_bool.Consume().elem_bool);
  append_float.Append(consume_struct_with_bool.Consume().elem_int);
  append_float.Append(consume_struct_with_bool.Consume().elem_float);

  append_v2bool.Append(consume_v2bool.Consume());
  append_v2bool.Append(consume_v2int.Consume());
  append_v2bool.Append(consume_v2float.Consume());
  append_v2bool.Append(consume_struct_with_bool.Consume().elem_v2bool);
  append_v2bool.Append(consume_struct_with_bool.Consume().elem_v2int);
  append_v2bool.Append(consume_struct_with_bool.Consume().elem_v2float);

  append_v2int.Append(consume_v2bool.Consume());
  append_v2int.Append(consume_v2int.Consume());
  append_v2int.Append(consume_v2float.Consume());
  append_v2int.Append(consume_struct_with_bool.Consume().elem_v2bool);
  append_v2int.Append(consume_struct_with_bool.Consume().elem_v2int);
  append_v2int.Append(consume_struct_with_bool.Consume().elem_v2float);

  append_v2float.Append(consume_v2bool.Consume());
  append_v2float.Append(consume_v2int.Consume());
  append_v2float.Append(consume_v2float.Consume());
  append_v2float.Append(consume_struct_with_bool.Consume().elem_v2bool);
  append_v2float.Append(consume_struct_with_bool.Consume().elem_v2int);
  append_v2float.Append(consume_struct_with_bool.Consume().elem_v2float);

  append_bool.Append(consume_v2bool.Consume().x);
  append_bool.Append(consume_v2int.Consume().x);
  append_bool.Append(consume_v2float.Consume().x);
  append_bool.Append(consume_struct_with_bool.Consume().elem_v2bool.x);
  append_bool.Append(consume_struct_with_bool.Consume().elem_v2int.x);
  append_bool.Append(consume_struct_with_bool.Consume().elem_v2float.x);

  append_int.Append(consume_v2bool.Consume().x);
  append_int.Append(consume_v2int.Consume().x);
  append_int.Append(consume_v2float.Consume().x);
  append_int.Append(consume_struct_with_bool.Consume().elem_v2bool.x);
  append_int.Append(consume_struct_with_bool.Consume().elem_v2int.x);
  append_int.Append(consume_struct_with_bool.Consume().elem_v2float.x);

  append_float.Append(consume_v2bool.Consume().x);
  append_float.Append(consume_v2int.Consume().x);
  append_float.Append(consume_v2float.Consume().x);
  append_float.Append(consume_struct_with_bool.Consume().elem_v2bool.x);
  append_float.Append(consume_struct_with_bool.Consume().elem_v2int.x);
  append_float.Append(consume_struct_with_bool.Consume().elem_v2float.x);

  append_bool.Append(rw_bool[0]);
  append_bool.Append(rw_v2bool[0].x);
  append_int.Append(rw_bool[0]);
  append_int.Append(rw_v2bool[0].x);
  append_float.Append(rw_bool[0]);
  append_float.Append(rw_v2bool[0].x);
}
