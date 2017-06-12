// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'all' function can only operate on int, bool, float,
// vector of these scalars, and matrix of these scalars.

// CHECK:      [[v4int_0:%\d+]] = OpConstantComposite %v4int %int_0 %int_0 %int_0 %int_0
// CHECK-NEXT: [[v4uint_0:%\d+]] = OpConstantComposite %v4uint %uint_0 %uint_0 %uint_0 %uint_0
// CHECK-NEXT: [[v4float_0:%\d+]] = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0

void main() {
    bool result;

    // CHECK:      [[a:%\d+]] = OpLoad %int %a
    // CHECK-NEXT: [[all_int:%\d+]] = OpINotEqual %bool [[a]] %int_0
    // CHECK-NEXT: OpStore %result [[all_int]]
    int a;
    result = all(a);

    // CHECK-NEXT: [[b:%\d+]] = OpLoad %uint %b
    // CHECK-NEXT: [[all_uint:%\d+]] = OpINotEqual %bool [[b]] %uint_0
    // CHECK-NEXT: OpStore %result [[all_uint]]
    uint b;
    result = all(b);

    // CHECK-NEXT: [[c:%\d+]] = OpLoad %bool %c
    // CHECK-NEXT: OpStore %result [[c]]
    bool c;
    result = all(c);

    // CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
    // CHECK-NEXT: [[all_float:%\d+]] = OpFOrdNotEqual %bool [[d]] %float_0
    // CHECK-NEXT: OpStore %result [[all_float]]
    float d;
    result = all(d);

    // CHECK-NEXT: [[e:%\d+]] = OpLoad %int %e
    // CHECK-NEXT: [[all_int1:%\d+]] = OpINotEqual %bool [[e]] %int_0
    // CHECK-NEXT: OpStore %result [[all_int1]]
    int1 e;
    result = all(e);

    // CHECK-NEXT: [[f:%\d+]] = OpLoad %uint %f
    // CHECK-NEXT: [[all_uint1:%\d+]] = OpINotEqual %bool [[f]] %uint_0
    // CHECK-NEXT: OpStore %result [[all_uint1]]
    uint1 f;
    result = all(f);

    // CHECK-NEXT: [[g:%\d+]] = OpLoad %bool %g
    // CHECK-NEXT: OpStore %result [[g]]
    bool1 g;
    result = all(g);

    // CHECK-NEXT: [[h:%\d+]] = OpLoad %float %h
    // CHECK-NEXT: [[all_float1:%\d+]] = OpFOrdNotEqual %bool [[h]] %float_0
    // CHECK-NEXT: OpStore %result [[all_float1]]
    float1 h;
    result = all(h);

    // CHECK-NEXT: [[i:%\d+]] = OpLoad %v4int %i
    // CHECK-NEXT: [[v4int_to_bool:%\d+]] = OpINotEqual %bool [[i]] [[v4int_0]]
    // CHECK-NEXT: [[all_int4:%\d+]] = OpAll %bool [[v4int_to_bool]]
    // CHECK-NEXT: OpStore %result [[all_int4]]
    int4 i;
    result = all(i);

    // CHECK-NEXT: [[j:%\d+]] = OpLoad %v4uint %j
    // CHECK-NEXT: [[v4uint_to_bool:%\d+]] = OpINotEqual %bool [[j]] [[v4uint_0]]
    // CHECK-NEXT: [[all_uint4:%\d+]] = OpAll %bool [[v4uint_to_bool]]
    // CHECK-NEXT: OpStore %result [[all_uint4]]
    uint4 j;
    result = all(j);

    // CHECK-NEXT: [[k:%\d+]] = OpLoad %v4bool %k
    // CHECK-NEXT: [[all_bool4:%\d+]] = OpAll %bool [[k]]
    // CHECK-NEXT: OpStore %result [[all_bool4]]
    bool4 k;
    result = all(k);

    // CHECK-NEXT: [[l:%\d+]] = OpLoad %v4float %l
    // CHECK-NEXT: [[v4float_to_bool:%\d+]] = OpFOrdNotEqual %bool [[l]] [[v4float_0]]
    // CHECK-NEXT: [[all_float4:%\d+]] = OpAll %bool [[v4float_to_bool]]
    // CHECK-NEXT: OpStore %result [[all_float4]]
    float4 l;
    result = all(l);
}

