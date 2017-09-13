// Run: %dxc -T ps_6_0 -E main

void main() {
  int a = 1, b = 2;
  int c;

// CHECK:                          OpStore %a %int_2
// CHECK-NEXT:                     OpStore %b %int_3
// CHECK-NEXT:                     OpStore %c %int_4
// CHECK-NEXT:        [[a:%\d+]] = OpLoad %int %a
// CHECK-NEXT:        [[b:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[a_plus_b:%\d+]] = OpIAdd %int [[a]] [[b]]
// CHECK-NEXT:                     OpStore %c [[a_plus_b]]
  c = (a=2, b=3, c=4, a+b);
}
