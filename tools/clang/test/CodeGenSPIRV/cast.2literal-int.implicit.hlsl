// Run: %dxc -T ps_6_0 -E main

void main() {
  int a, b;

// CHECK:          [[a:%\d+]] = OpLoad %int %a
// CHECK-NEXT:     [[b:%\d+]] = OpLoad %int %b
// CHECK-NEXT:    [[eq:%\d+]] = OpIEqual %bool [[a]] [[b]]
// CHECK-NEXT: [[c_int:%\d+]] = OpSelect %int [[eq]] %int_1 %int_0
// CHECK-NEXT:       {{%\d+}} = OpINotEqual %bool [[c_int]] %int_1
  bool c = (a == b) != 1;

// CHECK:            [[a:%\d+]] = OpLoad %int %a
// CHECK-NEXT:       [[b:%\d+]] = OpLoad %int %b
// CHECK-NEXT:      [[eq:%\d+]] = OpIEqual %bool [[a]] [[b]]
// CHECK-NEXT: [[d_float:%\d+]] = OpSelect %float [[eq]] %float_1 %float_0
// CHECK-NEXT:         {{%\d+}} = OpFOrdNotEqual %bool [[d_float]] %float_1
  bool d = (a == b) != 1.0;
}
