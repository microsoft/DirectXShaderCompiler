// Run: %dxc -T ps_6_0 -E main

struct S {
    float x;
};

struct T {
    float y;
    S z;
};

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

    T p, q;

// CHECK-NEXT: [[q:%\d+]] = OpLoad %T %q
// CHECK-NEXT: OpStore %p [[q]]
    p = q;     // assign as a whole
// CHECK-NEXT: [[q1ptr:%\d+]] = OpAccessChain %_ptr_Function_S %q %int_1
// CHECK-NEXT: [[q1val:%\d+]] = OpLoad %S [[q1ptr]]
// CHECK-NEXT: [[p1ptr:%\d+]] = OpAccessChain %_ptr_Function_S %p %int_1
// CHECK-NEXT: OpStore [[p1ptr]] [[q1val]]
    p.z = q.z; // assign nested struct
}
