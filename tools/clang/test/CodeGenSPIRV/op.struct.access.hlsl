// Run: %dxc -T vs_6_0 -E main

struct S {
    bool a;
    uint2 b;
    float2x3 c;
};

struct T {
    int h; // Nested struct
    S i;
};

void main() {
    T t;

// CHECK:      [[h:%\d+]] = OpAccessChain %_ptr_Function_int %t %int_0
// CHECK-NEXT: {{%\d+}} = OpLoad %int [[h]]
    int v1 = t.h;
// CHECK:      [[a:%\d+]] = OpAccessChain %_ptr_Function_bool %t %int_1 %int_0
// CHECK-NEXT: {{%\d+}} = OpLoad %bool [[a]]
    bool v2 = t.i.a;

// CHECK:      [[b:%\d+]] = OpAccessChain %_ptr_Function_v2uint %t %int_1 %int_1
// CHECK-NEXT: [[b0:%\d+]] = OpAccessChain %_ptr_Function_uint [[b]] %uint_0
// CHECK-NEXT: {{%\d+}} = OpLoad %uint [[b0]]
    uint v3 = t.i.b[0];
// CHECK:      [[b:%\d+]] = OpAccessChain %_ptr_Function_v2uint %t %int_1 %int_1
// CHECK-NEXT: {{%\d+}} = OpLoad %v2uint [[b]]
    uint2 v4 = t.i.b.rg;

// CHECK:      [[c:%\d+]] = OpAccessChain %_ptr_Function_mat2v3float %t %int_1 %int_2
// CHECK-NEXT: [[c00p:%\d+]] = OpAccessChain %_ptr_Function_float [[c]] %int_0 %int_0
// CHECK-NEXT: [[c00v:%\d+]] = OpLoad %float [[c00p]]
// CHECK-NEXT: [[c11p:%\d+]] = OpAccessChain %_ptr_Function_float [[c]] %int_1 %int_1
// CHECK-NEXT: [[c11v:%\d+]] = OpLoad %float [[c11p]]
// CHECK-NEXT: {{%\d+}} = OpCompositeConstruct %v2float [[c00v]] [[c11v]]
    float2 v5 = t.i.c._11_22;
// CHECK:      [[c:%\d+]] = OpAccessChain %_ptr_Function_mat2v3float %t %int_1 %int_2
// CHECK-NEXT: [[c1:%\d+]] = OpAccessChain %_ptr_Function_v3float [[c]] %uint_1
// CHECK-NEXT: {{%\d+}} = OpLoad %v3float [[c1]]
    float3 v6 = t.i.c[1];

// CHECK:      [[h:%\d+]] = OpAccessChain %_ptr_Function_int %t %int_0
// CHECK-NEXT: OpStore [[h]] {{%\d+}}
    t.h = v1;
// CHECK:      [[a:%\d+]] = OpAccessChain %_ptr_Function_bool %t %int_1 %int_0
// CHECK-NEXT: OpStore [[a]] {{%\d+}}
    t.i.a = v2;

// CHECK:      [[b:%\d+]] = OpAccessChain %_ptr_Function_v2uint %t %int_1 %int_1
// CHECK-NEXT: [[b1:%\d+]] = OpAccessChain %_ptr_Function_uint [[b]] %uint_1
// CHECK-NEXT: OpStore [[b1]] {{%\d+}}
    t.i.b[1] = v3;
// CHECK:      [[v4:%\d+]] = OpLoad %v2uint %v4
// CHECK-NEXT: [[b:%\d+]] = OpAccessChain %_ptr_Function_v2uint %t %int_1 %int_1
// CHECK-NEXT: [[bv:%\d+]] = OpLoad %v2uint [[b]]
// CHECK-NEXT: [[gr:%\d+]] = OpVectorShuffle %v2uint [[bv]] [[v4]] 3 2
// CHECK-NEXT: OpStore [[b]] [[gr]]
    t.i.b.gr = v4;

// CHECK:      [[v5:%\d+]] = OpLoad %v2float %v5
// CHECK-NEXT: [[c:%\d+]] = OpAccessChain %_ptr_Function_mat2v3float %t %int_1 %int_2
// CHECK-NEXT: [[v50:%\d+]] = OpCompositeExtract %float [[v5]] 0
// CHECK-NEXT: [[c11:%\d+]] = OpAccessChain %_ptr_Function_float [[c]] %int_1 %int_1
// CHECK-NEXT: OpStore [[c11]] [[v50]]
// CHECK-NEXT: [[v51:%\d+]] = OpCompositeExtract %float [[v5]] 1
// CHECK-NEXT: [[c00:%\d+]] = OpAccessChain %_ptr_Function_float [[c]] %int_0 %int_0
// CHECK-NEXT: OpStore [[c00]] [[v51]]
    t.i.c._22_11 = v5;
// CHECK:      [[c:%\d+]] = OpAccessChain %_ptr_Function_mat2v3float %t %int_1 %int_2
// CHECK-NEXT: [[c0:%\d+]] = OpAccessChain %_ptr_Function_v3float [[c]] %uint_0
// CHECK-NEXT: OpStore [[c0]] {{%\d+}}
    t.i.c[0] = v6;
}
