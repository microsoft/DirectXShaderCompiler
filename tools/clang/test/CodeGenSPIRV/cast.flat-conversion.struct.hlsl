// Run: %dxc -T ps_6_0 -E main

// Compositing a struct by casting from its single member

struct S {
    float4 val;
};

struct T {
    S val;
};

float4 main(float4 a: A) : SV_Target {
// CHECK:      [[a:%\d+]] = OpLoad %v4float %a
// CHECK-NEXT: [[s:%\d+]] = OpCompositeConstruct %S [[a]]
// CHECK-NEXT:              OpStore %s [[s]]
    S s = (S)a;

// CHECK:      [[valptr:%\d+]] = OpAccessChain %_ptr_Function_v4float %s %int_0
// CHECK-NEXT:    [[val:%\d+]] = OpLoad %v4float [[valptr]]
// CHECK-NEXT:      [[s:%\d+]] = OpCompositeConstruct %S [[val]]
// CHECK-NEXT:      [[t:%\d+]] = OpCompositeConstruct %T [[s]]
// CHECK-NEXT:                   OpStore %t [[t]]
    T t = (T)s;

    return s.val + t.val.val;
}
