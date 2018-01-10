// Run: %dxc -T ps_6_0 -E main

// CHECK: [[v3i0:%\d+]] = OpConstantComposite %v3int %int_0 %int_0 %int_0

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    bool b0;
    int m, n, o;
    // Plain assign (scalar)
// CHECK:      [[b0:%\d+]] = OpLoad %bool %b0
// CHECK-NEXT: [[m0:%\d+]] = OpLoad %int %m
// CHECK-NEXT: [[n0:%\d+]] = OpLoad %int %n
// CHECK-NEXT: [[s0:%\d+]] = OpSelect %int [[b0]] [[m0]] [[n0]]
// CHECK-NEXT: OpStore %o [[s0]]
    o = b0 ? m : n;


    bool1 b1;
    bool3 b3;
    uint1 p, q, r;
    float3 x, y, z;
    // Plain assign (vector)
// CHECK-NEXT: [[b1:%\d+]] = OpLoad %bool %b1
// CHECK-NEXT: [[p0:%\d+]] = OpLoad %uint %p
// CHECK-NEXT: [[q0:%\d+]] = OpLoad %uint %q
// CHECK-NEXT: [[s1:%\d+]] = OpSelect %uint [[b1]] [[p0]] [[q0]]
// CHECK-NEXT: OpStore %r [[s1]]
    r = b1 ? p : q;
// CHECK-NEXT: [[b3:%\d+]] = OpLoad %v3bool %b3
// CHECK-NEXT: [[x0:%\d+]] = OpLoad %v3float %x
// CHECK-NEXT: [[y0:%\d+]] = OpLoad %v3float %y
// CHECK-NEXT: [[s2:%\d+]] = OpSelect %v3float [[b3]] [[x0]] [[y0]]
// CHECK-NEXT: OpStore %z [[s2]]
    z = b3 ? x : y;

    // Try condition with various type.
    // Note: the SPIR-V OpSelect selection argument must be the same size as the return type.
    int3 u, v, w;
    bool  cond;
    bool3 cond3;
    float floatCond;
    int3 int3Cond;
 
// CHECK:      [[cond3:%\d+]] = OpLoad %v3bool %cond3
// CHECK-NEXT:     [[u:%\d+]] = OpLoad %v3int %u
// CHECK-NEXT:     [[v:%\d+]] = OpLoad %v3int %v
// CHECK-NEXT:       {{%\d+}} = OpSelect %v3int [[cond3]] [[u]] [[v]]
    w = cond3 ? u : v;

// CHECK:       [[cond:%\d+]] = OpLoad %bool %cond
// CHECK-NEXT:     [[u:%\d+]] = OpLoad %v3int %u
// CHECK-NEXT:     [[v:%\d+]] = OpLoad %v3int %v
// CHECK-NEXT: [[splat:%\d+]] = OpCompositeConstruct %v3bool [[cond]] [[cond]] [[cond]]
// CHECK-NEXT:       {{%\d+}} = OpSelect %v3int [[splat]] [[u]] [[v]]
    w = cond  ? u : v;

// CHECK:      [[floatCond:%\d+]] = OpLoad %float %floatCond
// CHECK-NEXT:  [[boolCond:%\d+]] = OpFOrdNotEqual %bool [[floatCond]] %float_0
// CHECK-NEXT: [[bool3Cond:%\d+]] = OpCompositeConstruct %v3bool [[boolCond]] [[boolCond]] [[boolCond]]
// CHECK-NEXT:         [[u:%\d+]] = OpLoad %v3int %u
// CHECK-NEXT:         [[v:%\d+]] = OpLoad %v3int %v
// CHECK-NEXT:           {{%\d+}} = OpSelect %v3int [[bool3Cond]] [[u]] [[v]]
    w = floatCond ? u : v;

// CHECK:       [[int3Cond:%\d+]] = OpLoad %v3int %int3Cond
// CHECK-NEXT: [[bool3Cond:%\d+]] = OpINotEqual %v3bool [[int3Cond]] [[v3i0]]
// CHECK-NEXT:         [[u:%\d+]] = OpLoad %v3int %u
// CHECK-NEXT:         [[v:%\d+]] = OpLoad %v3int %v
// CHECK-NEXT:           {{%\d+}} = OpSelect %v3int [[bool3Cond]] [[u]] [[v]]
    w = int3Cond ? u : v;
}
