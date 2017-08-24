// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'asint' function can only operate on uint, float,
// vector of these scalars, and matrix of these scalars.

void main() {
    int result;
    int4 result4;

    // CHECK:      [[b:%\d+]] = OpLoad %uint %b
    // CHECK-NEXT: [[b_as_int:%\d+]] = OpBitcast %int [[b]]
    // CHECK-NEXT: OpStore %result [[b_as_int]]
    uint b;
    result = asint(b);

    // CHECK-NEXT: [[c:%\d+]] = OpLoad %float %c
    // CHECK-NEXT: [[c_as_int:%\d+]] = OpBitcast %int [[c]]
    // CHECK-NEXT: OpStore %result [[c_as_int]]
    float c;
    result = asint(c);

    // CHECK-NEXT: [[e:%\d+]] = OpLoad %uint %e
    // CHECK-NEXT: [[e_as_int:%\d+]] = OpBitcast %int [[e]]
    // CHECK-NEXT: OpStore %result [[e_as_int]]
    uint1 e;
    result = asint(e);

    // CHECK-NEXT: [[f:%\d+]] = OpLoad %float %f
    // CHECK-NEXT: [[f_as_int:%\d+]] = OpBitcast %int [[f]]
    // CHECK-NEXT: OpStore %result [[f_as_int]]
    float1 f;
    result = asint(f);

    // CHECK-NEXT: [[h:%\d+]] = OpLoad %v4uint %h
    // CHECK-NEXT: [[h_as_int:%\d+]] = OpBitcast %v4int [[h]]
    // CHECK-NEXT: OpStore %result4 [[h_as_int]]
    uint4 h;
    result4 = asint(h);

    // CHECK-NEXT: [[i:%\d+]] = OpLoad %v4float %i
    // CHECK-NEXT: [[i_as_int:%\d+]] = OpBitcast %v4int [[i]]
    // CHECK-NEXT: OpStore %result4 [[i_as_int]]
    float4 i;
    result4 = asint(i);
}
