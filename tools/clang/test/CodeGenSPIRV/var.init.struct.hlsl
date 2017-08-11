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

void main() {
// CHECK-LABEL: %bb_entry = OpLabel

    // Flat initializer list
// CHECK:      [[a:%\d+]] = OpCompositeConstruct %v3int %int_1 %int_2 %int_3
// CHECK-NEXT: [[c0:%\d+]] = OpCompositeConstruct %v2float %float_1 %float_2
// CHECK-NEXT: [[c1:%\d+]] = OpCompositeConstruct %v2float %float_3 %float_4
// CHECK-NEXT: [[c:%\d+]] = OpCompositeConstruct %mat2v2float [[c0]] [[c1]]
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %S [[a]] %uint_42 [[c]]
    S s1 = {1, 2, 3, 42, 1., 2., 3., 4.};

    // Random parentheses
// CHECK:      [[a:%\d+]] = OpCompositeConstruct %v3int %int_1 %int_2 %int_3
// CHECK-NEXT: [[c0:%\d+]] = OpCompositeConstruct %v2float %float_1 %float_2
// CHECK-NEXT: [[c1:%\d+]] = OpCompositeConstruct %v2float %float_3 %float_4
// CHECK-NEXT: [[c:%\d+]] = OpCompositeConstruct %mat2v2float [[c0]] [[c1]]
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %S [[a]] %uint_42 [[c]]
    S s2 = {{1, 2}, 3, {{42}, {{1.}}}, {2., {3., 4.}}};

    // Flat initalizer list for nested structs
// CHECK:      [[y:%\d+]] = OpCompositeConstruct %O %int_1
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %P [[y]] %float_2
    P p = {1, 2.};

    // Mixed case: use struct as a whole, decomposing struct, type casting
// CHECK:      [[s1a:%\d+]] = OpAccessChain %_ptr_Function_v3int %s1 %int_0
// CHECK-NEXT: [[h:%\d+]] = OpLoad %v3int [[s1a]]

// CHECK-NEXT: [[s1b:%\d+]] = OpAccessChain %_ptr_Function_uint %s1 %int_1
// CHECK-NEXT: [[i:%\d+]] = OpLoad %uint [[s1b]]

// CHECK-NEXT: [[s1c:%\d+]] = OpAccessChain %_ptr_Function_mat2v2float %s1 %int_2
// CHECK-NEXT: [[j:%\d+]] = OpLoad %mat2v2float [[s1c]]

// CHECK-NEXT: [[k:%\d+]] = OpCompositeConstruct %v2bool %true %false

// CHECK-NEXT: [[l:%\d+]] = OpLoad %S %s2

// CHECK-NEXT: [[s2a:%\d+]] = OpAccessChain %_ptr_Function_v3int %s2 %int_0
// CHECK-NEXT: [[s2av:%\d+]] = OpLoad %v3int [[s2a]]
// CHECK-NEXT: [[m:%\d+]] = OpConvertSToF %v3float [[s2av]]

// CHECK-NEXT: [[s2b:%\d+]] = OpAccessChain %_ptr_Function_uint %s2 %int_1
// CHECK-NEXT: [[s2bv:%\d+]] = OpLoad %uint [[s2b]]
// CHECK-NEXT: [[n:%\d+]] = OpBitcast %int [[s2bv]]

// CHECK-NEXT: [[s2c:%\d+]] = OpAccessChain %_ptr_Function_mat2v2float %s2 %int_2
// CHECK-NEXT: [[o:%\d+]] = OpLoad %mat2v2float [[s2c]]

// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %T [[h]] [[i]] [[j]] [[k]] [[l]] [[m]] [[n]] [[o]]
    T t = {s1,          // Decomposing struct
           true, false, // constructing field from scalar
           s2,          // Embedded struct
           s2           // Decomposing struct + type casting
          };
}
