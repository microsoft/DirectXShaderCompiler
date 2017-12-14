// Run: %dxc -T vs_6_0 -E main

bool main(int a : A, int b : B) : C {
// CHECK:       [[a:%\d+]] = OpLoad %int %a
// CHECK-NEXT:  [[b:%\d+]] = OpLoad %int %b
// CHECK-NEXT: [[eq:%\d+]] = OpIEqual %bool [[a]] [[b]]
// CHECK-NEXT:    {{%\d+}} = OpSelect %int [[eq]] %int_1 %int_0
    return (a == b) != 0;
}
