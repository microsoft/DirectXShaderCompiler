// Run: %dxc -T vs_6_0 -E main

// According to HLSL reference:
// The 'asfloat' function can only operate on int, uint, float,
// vector of these scalars, and matrix of these scalars.

void main() {
    float result;
    float4 result4;
    float1x1 result1x1;
    float1x3 result1x3;
    float2x1 result2x1;
    float2x3 result2x3; 


    // CHECK:      [[a:%\d+]] = OpLoad %int %a
    // CHECK-NEXT: [[a_as_float:%\d+]] = OpBitcast %float [[a]]
    // CHECK-NEXT: OpStore %result [[a_as_float]]
    int a;
    result = asfloat(a);

    // CHECK-NEXT: [[b:%\d+]] = OpLoad %uint %b
    // CHECK-NEXT: [[b_as_float:%\d+]] = OpBitcast %float [[b]]
    // CHECK-NEXT: OpStore %result [[b_as_float]]
    uint b;
    result = asfloat(b);

    // CHECK-NEXT: [[c:%\d+]] = OpLoad %float %c
    // CHECK-NEXT: OpStore %result [[c]]
    float c;
    result = asfloat(c);

    // CHECK-NEXT: [[d:%\d+]] = OpLoad %int %d
    // CHECK-NEXT: [[d_as_float:%\d+]] = OpBitcast %float [[d]]
    // CHECK-NEXT: OpStore %result [[d_as_float]]
    int1 d;
    result = asfloat(d);

    // CHECK-NEXT: [[e:%\d+]] = OpLoad %uint %e
    // CHECK-NEXT: [[e_as_float:%\d+]] = OpBitcast %float [[e]]
    // CHECK-NEXT: OpStore %result [[e_as_float]]
    uint1 e;
    result = asfloat(e);

    // CHECK-NEXT: [[f:%\d+]] = OpLoad %float %f
    // CHECK-NEXT: OpStore %result [[f]]
    float1 f;
    result = asfloat(f);

    // CHECK-NEXT: [[g:%\d+]] = OpLoad %v4int %g
    // CHECK-NEXT: [[g_as_float:%\d+]] = OpBitcast %v4float [[g]]
    // CHECK-NEXT: OpStore %result4 [[g_as_float]]
    int4 g;
    result4 = asfloat(g);

    // CHECK-NEXT: [[h:%\d+]] = OpLoad %v4uint %h
    // CHECK-NEXT: [[h_as_float:%\d+]] = OpBitcast %v4float [[h]]
    // CHECK-NEXT: OpStore %result4 [[h_as_float]]
    uint4 h;
    result4 = asfloat(h);

    // CHECK-NEXT: [[i:%\d+]] = OpLoad %v4float %i
    // CHECK-NEXT: OpStore %result4 [[i]]
    float4 i;
    result4 = asfloat(i);
    
    // CHECK-NEXT: [[j:%\d+]] = OpLoad %float %j
    // CHECK-NEXT: OpStore %result1x1 [[j]]
    float1x1 j;
    result1x1 = asfloat(j);
    
    // CHECK-NEXT: [[k:%\d+]] = OpLoad %v3float %k
    // CHECK-NEXT: OpStore %result1x3 [[k]]    
    float1x3 k;
    result1x3 = asfloat(k);
    
    // CHECK-NEXT: [[l:%\d+]] = OpLoad %v2float %l
    // CHECK-NEXT: OpStore %result2x1 [[l]]
    float2x1 l;
    result2x1 = asfloat(l);
    
    // CHECK-NEXT: [[m:%\d+]] = OpLoad %mat2v3float %m
    // CHECK-NEXT: OpStore %result2x3 [[m]]
    float2x3 m;
    result2x3 = asfloat(m);
}
