// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'any' function can only operate on int, bool, float,
// vector of these scalars, and matrix of these scalars.

// CHECK:      [[v4int_0:%\d+]] = OpConstantComposite %v4int %int_0 %int_0 %int_0 %int_0
// CHECK-NEXT: [[v4uint_0:%\d+]] = OpConstantComposite %v4uint %uint_0 %uint_0 %uint_0 %uint_0
// CHECK-NEXT: [[v4float_0:%\d+]] = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0

void main() {
    bool result;

    // CHECK:      [[a:%\d+]] = OpLoad %int %a
    // CHECK-NEXT: [[any_int:%\d+]] = OpINotEqual %bool [[a]] %int_0
    // CHECK-NEXT: OpStore %result [[any_int]]
    int a;
    result = any(a);

    // CHECK-NEXT: [[b:%\d+]] = OpLoad %uint %b
    // CHECK-NEXT: [[any_uint:%\d+]] = OpINotEqual %bool [[b]] %uint_0
    // CHECK-NEXT: OpStore %result [[any_uint]]
    uint b;
    result = any(b);

    // CHECK-NEXT: [[c:%\d+]] = OpLoad %bool %c
    // CHECK-NEXT: OpStore %result [[c]]
    bool c;
    result = any(c);

    // CHECK-NEXT: [[d:%\d+]] = OpLoad %float %d
    // CHECK-NEXT: [[any_float:%\d+]] = OpFOrdNotEqual %bool [[d]] %float_0
    // CHECK-NEXT: OpStore %result [[any_float]]
    float d;
    result = any(d);

    // CHECK-NEXT: [[e:%\d+]] = OpLoad %int %e
    // CHECK-NEXT: [[any_int1:%\d+]] = OpINotEqual %bool [[e]] %int_0
    // CHECK-NEXT: OpStore %result [[any_int1]]
    int1 e;
    result = any(e);

    // CHECK-NEXT: [[f:%\d+]] = OpLoad %uint %f
    // CHECK-NEXT: [[any_uint1:%\d+]] = OpINotEqual %bool [[f]] %uint_0
    // CHECK-NEXT: OpStore %result [[any_uint1]]
    uint1 f;
    result = any(f);

    // CHECK-NEXT: [[g:%\d+]] = OpLoad %bool %g
    // CHECK-NEXT: OpStore %result [[g]]
    bool1 g;
    result = any(g);

    // CHECK-NEXT: [[h:%\d+]] = OpLoad %float %h
    // CHECK-NEXT: [[any_float1:%\d+]] = OpFOrdNotEqual %bool [[h]] %float_0
    // CHECK-NEXT: OpStore %result [[any_float1]]
    float1 h;
    result = any(h);

    // CHECK-NEXT: [[i:%\d+]] = OpLoad %v4int %i
    // CHECK-NEXT: [[v4int_to_bool:%\d+]] = OpINotEqual %bool [[i]] [[v4int_0]]
    // CHECK-NEXT: [[any_int4:%\d+]] = OpAny %bool [[v4int_to_bool]]
    // CHECK-NEXT: OpStore %result [[any_int4]]
    int4 i;
    result = any(i);

    // CHECK-NEXT: [[j:%\d+]] = OpLoad %v4uint %j
    // CHECK-NEXT: [[v4uint_to_bool:%\d+]] = OpINotEqual %bool [[j]] [[v4uint_0]]
    // CHECK-NEXT: [[any_uint4:%\d+]] = OpAny %bool [[v4uint_to_bool]]
    // CHECK-NEXT: OpStore %result [[any_uint4]]
    uint4 j;
    result = any(j);

    // CHECK-NEXT: [[k:%\d+]] = OpLoad %v4bool %k
    // CHECK-NEXT: [[any_bool4:%\d+]] = OpAny %bool [[k]]
    // CHECK-NEXT: OpStore %result [[any_bool4]]
    bool4 k;
    result = any(k);

    // CHECK-NEXT: [[l:%\d+]] = OpLoad %v4float %l
    // CHECK-NEXT: [[v4float_to_bool:%\d+]] = OpFOrdNotEqual %bool [[l]] [[v4float_0]]
    // CHECK-NEXT: [[any_float4:%\d+]] = OpAny %bool [[v4float_to_bool]]
    // CHECK-NEXT: OpStore %result [[any_float4]]
    float4 l;
    result = any(l);
}

