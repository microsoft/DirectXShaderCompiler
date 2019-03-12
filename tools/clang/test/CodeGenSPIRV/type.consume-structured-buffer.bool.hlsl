// Run: %dxc -T cs_6_0 -E main

// CHECK: %type_ConsumeStructuredBuffer_bool = OpTypeStruct %_runtimearr_uint
// CHECK: %_ptr_Uniform_type_ConsumeStructuredBuffer_bool = OpTypePointer Uniform %type_ConsumeStructuredBuffer_bool
// CHECK: %foo = OpVariable %_ptr_Uniform_type_ConsumeStructuredBuffer_bool Uniform
ConsumeStructuredBuffer<bool> foo;

void main() {
// CHECK:       [[p_0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %foo %uint_0 {{%\d+}}
// CHECK-NEXT:  [[i_0:%\d+]] = OpLoad %uint [[p_0]]
// CHECK-NEXT:  [[b_0:%\d+]] = OpINotEqual %bool [[i_0]] %uint_0
// CHECK-NEXT: [[bi_0:%\d+]] = OpSelect %int [[b_0]] %int_1 %int_0
// CHECK-NEXT:                 OpStore %bar [[bi_0]]
  int bar = foo.Consume();

// CHECK:      [[p_1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %foo %uint_0 {{%\d+}}
// CHECK-NEXT: [[i_1:%\d+]] = OpLoad %uint [[p_1]]
// CHECK-NEXT: [[b_1:%\d+]] = OpINotEqual %bool [[i_1]] %uint_0
// CHECK-NEXT:                OpStore %baz [[b_1]]
  bool baz = foo.Consume();
}
