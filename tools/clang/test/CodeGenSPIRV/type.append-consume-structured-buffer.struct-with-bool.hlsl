// Run: %dxc -T cs_6_0 -E main

struct S {
  float4 a;
  bool   b;
};

ConsumeStructuredBuffer<S> foo;
AppendStructuredBuffer<bool> bar;
AppendStructuredBuffer<S> zoo;

void main() {
// CHECK:       [[p_0:%\d+]] = OpAccessChain %_ptr_Uniform_S %foo %uint_0 {{%\d+}}
// CHECK-NEXT:  [[p_1:%\d+]] = OpAccessChain %_ptr_Uniform_uint [[p_0]] %int_1
// CHECK-NEXT:    [[i:%\d+]] = OpLoad %uint [[p_1]]
// CHECK-NEXT:    [[b:%\d+]] = OpINotEqual %bool [[i]] %uint_0
// CHECK-NEXT:   [[bi:%\d+]] = OpSelect %uint [[b]] %uint_1 %uint_0
// CHECK-NEXT:                 OpStore {{%\d+}} [[bi]]
  bar.Append(foo.Consume().b);

// CHECK:       [[p_2:%\d+]] = OpAccessChain %_ptr_Uniform_S %foo %uint_0 {{%\d+}}
// CHECK-NEXT:    [[s:%\d+]] = OpLoad %S [[p_2]]
// CHECK-NEXT:                 OpStore {{%\d+}} [[s]]
  zoo.Append(foo.Consume());
}
