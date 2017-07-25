// Run: %dxc -T ps_6_0 -E main

// TODO: assignment for composite types

void main() {
    int a, b, c;

// CHECK: [[b0:%\d+]] = OpLoad %int %b
// CHECK-NEXT: OpStore %a [[b0]]
    a = b;
// CHECK-NEXT: [[c0:%\d+]] = OpLoad %int %c
// CHECK-NEXT: OpStore %b [[c0]]
// CHECK-NEXT: OpStore %a [[c0]]
    a = b = c;

// CHECK-NEXT: [[a0:%\d+]] = OpLoad %int %a
// CHECK-NEXT: OpStore %a [[a0]]
    a = a;
// CHECK-NEXT: [[a1:%\d+]] = OpLoad %int %a
// CHECK-NEXT: OpStore %a [[a1]]
// CHECK-NEXT: OpStore %a [[a1]]
    a = a = a;
}
