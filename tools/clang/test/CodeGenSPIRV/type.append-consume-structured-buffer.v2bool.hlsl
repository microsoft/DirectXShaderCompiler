// Run: %dxc -T cs_6_0 -E main

ConsumeStructuredBuffer<bool2> foo;
AppendStructuredBuffer<bool2> bar;

void main() {
// CHECK:       [[p_0:%\d+]] = OpAccessChain %_ptr_Uniform_v2uint %bar %uint_0 {{%\d+}}

// CHECK:       [[p_1:%\d+]] = OpAccessChain %_ptr_Uniform_v2uint %foo %uint_0 {{%\d+}}
// CHECK-NEXT:    [[i:%\d+]] = OpLoad %v2uint [[p_1]]
// CHECK-NEXT:    [[b:%\d+]] = OpINotEqual %v2bool [[i]] {{%\d+}}
// CHECK-NEXT:   [[bi:%\d+]] = OpSelect %v2uint [[b]] {{%\d+}} {{%\d+}}
// CHECK-NEXT:                 OpStore [[p_0]] [[bi]]
  bar.Append(foo.Consume());
}
