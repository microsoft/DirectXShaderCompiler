// Run: %dxc -T cs_6_0 -E main

ConsumeStructuredBuffer<bool> foo;
AppendStructuredBuffer<bool> bar;

void main() {
// CHECK:       [[p_0:%\d+]] = OpAccessChain %_ptr_Uniform_uint %bar %uint_0 {{%\d+}}

// CHECK:       [[p_1:%\d+]] = OpAccessChain %_ptr_Uniform_uint %foo %uint_0 {{%\d+}}
// CHECK-NEXT:    [[i:%\d+]] = OpLoad %uint [[p_1]]
// CHECK-NEXT:    [[b:%\d+]] = OpINotEqual %bool [[i]] %uint_0
// CHECK-NEXT:   [[bi:%\d+]] = OpSelect %uint [[b]] %uint_1 %uint_0
// CHECK-NEXT:                 OpStore [[p_0]] [[bi]]
  bar.Append(foo.Consume());
}
