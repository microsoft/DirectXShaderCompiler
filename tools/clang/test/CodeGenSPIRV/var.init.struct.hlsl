// Run: %dxc -T vs_6_0 -E main

struct S {
    int3 a;
    uint b;
    float2x2 c;
};

struct T {
    // Same fields as S
    int3 h;
    uint i;
    float2x2 j;

    // Additional field
    bool2 k;

    // Embedded S
    S l;

    // Similar to S but need some casts
    float3 m;
    int n;
    float2x2 o;
};

struct O {
    int x;
};

struct P {
    O y;
    float z;
};

struct W {
  float4 color;
};

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    // Flat initializer list
// CHECK:      [[a:%\d+]] = OpCompositeConstruct %v3int %int_1 %int_2 %int_3
// CHECK-NEXT: [[c0:%\d+]] = OpCompositeConstruct %v2float %float_1 %float_2
// CHECK-NEXT: [[c1:%\d+]] = OpCompositeConstruct %v2float %float_3 %float_4
// CHECK-NEXT: [[c:%\d+]] = OpCompositeConstruct %mat2v2float [[c0]] [[c1]]
// CHECK-NEXT: [[s1:%\d+]] = OpCompositeConstruct %S [[a]] %uint_42 [[c]]
// CHECK-NEXT: OpStore %s1 [[s1]]
    S s1 = {1, 2, 3, 42, 1., 2., 3., 4.};

    // Random parentheses
// CHECK:      [[a:%\d+]] = OpCompositeConstruct %v3int %int_1 %int_2 %int_3
// CHECK-NEXT: [[c0:%\d+]] = OpCompositeConstruct %v2float %float_1 %float_2
// CHECK-NEXT: [[c1:%\d+]] = OpCompositeConstruct %v2float %float_3 %float_4
// CHECK-NEXT: [[c:%\d+]] = OpCompositeConstruct %mat2v2float [[c0]] [[c1]]
// CHECK-NEXT: [[s2:%\d+]] = OpCompositeConstruct %S [[a]] %uint_42 [[c]]
// CHECK-NEXT: OpStore %s2 [[s2]]
    S s2 = {{1, 2}, 3, {{42}, {{1.}}}, {2., {3., 4.}}};

    // Flat initalizer list for nested structs
// CHECK:      [[y:%\d+]] = OpCompositeConstruct %O %int_1
// CHECK-NEXT: [[p:%\d+]] = OpCompositeConstruct %P [[y]] %float_2
// CHECK-NEXT: OpStore %p [[p]]
    P p = {1, 2.};

    // Mixed case: use struct as a whole, decomposing struct, type casting

// CHECK-NEXT: [[s1_val:%\d+]] = OpLoad %S %s1
// CHECK-NEXT: [[l:%\d+]] = OpLoad %S %s2
// CHECK-NEXT: [[s2_val:%\d+]] = OpLoad %S %s2
// CHECK-NEXT: [[h:%\d+]] = OpCompositeExtract %v3int [[s1_val]] 0
// CHECK-NEXT: [[i:%\d+]] = OpCompositeExtract %uint [[s1_val]] 1
// CHECK-NEXT: [[j:%\d+]] = OpCompositeExtract %mat2v2float [[s1_val]] 2

// CHECK-NEXT: [[k:%\d+]] = OpCompositeConstruct %v2bool %true %false

// CHECK-NEXT: [[s2av:%\d+]] = OpCompositeExtract %v3int [[s2_val]] 0
// CHECK-NEXT: [[s2bv:%\d+]] = OpCompositeExtract %uint [[s2_val]] 1
// CHECK-NEXT: [[o:%\d+]] = OpCompositeExtract %mat2v2float [[s2_val]] 2
// CHECK-NEXT: [[m:%\d+]] = OpConvertSToF %v3float [[s2av]]
// CHECK-NEXT: [[n:%\d+]] = OpBitcast %int [[s2bv]]
// CHECK-NEXT: [[t:%\d+]] = OpCompositeConstruct %T [[h]] [[i]] [[j]] [[k]] [[l]] [[m]] [[n]] [[o]]
// CHECK-NEXT: OpStore %t [[t]]
    T t = {s1,          // Decomposing struct
           true, false, // constructing field from scalar
           s2,          // Embedded struct
           s2           // Decomposing struct + type casting
          };

    // Using InitListExpr
// CHECK:        [[int4_zero:%\d+]] = OpCompositeConstruct %v4int %int_0 %int_0 %int_0 %int_0
// CHECK-NEXT: [[float4_zero:%\d+]] = OpConvertSToF %v4float [[int4_zero]]
// CHECK-NEXT:             {{%\d+}} = OpCompositeConstruct %W [[float4_zero]]
    W w = { (0).xxxx };
}
