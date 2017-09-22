// Run: %dxc -T ps_6_0 -E main

void foo() { int x = 1; }

void main() {
  bool cond;
  int a = 1, b = 2;
  int c = 0;

// CHECK:                          OpStore %a %int_2
// CHECK-NEXT:                     OpStore %b %int_3
// CHECK-NEXT:          {{%\d+}} = OpFunctionCall %void %foo
// CHECK-NEXT:     [[cond:%\d+]] = OpLoad %bool %cond
// CHECK-NEXT:        [[a:%\d+]] = OpLoad %int %a
// CHECK-NEXT:        [[c:%\d+]] = OpLoad %int %c
// CHECK-NEXT:      [[sel:%\d+]] = OpSelect %int [[cond]] [[a]] [[c]]
// CHECK-NEXT:                     OpStore %b [[sel]]
// CHECK-NEXT:       [[a1:%\d+]] = OpLoad %int %a
// CHECK-NEXT:        [[b:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[a_plus_b:%\d+]] = OpIAdd %int [[a1]] [[b]]
// CHECK-NEXT:                     OpStore %c [[a_plus_b]]
  c = (a=2, b=3, foo(), b = cond ? a : c , a+b);
}
