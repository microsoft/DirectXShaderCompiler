// Run: %dxc -T vs_6_0 -E main

struct S1 {
    float2 a;
};

struct S2 {
    float2 b[2];
};

struct T1 {
    S2 c;  // Need to split to match T2.f1 & T2.f2
    S2 d;  // Match T2.f3 exactly
};

struct T2 {
    S1 e;
    S1 f;
    S2 g;
};

// Flattend T2: need to split all fields in T2
struct T3 {
    float2 h;
    float2 i;
    float2 j;
    float2 k;
};

void main() {
    T1 val1[2];

// val2[0]: Construct T2.e from T1.c.b[0]
// CHECK:       [[val1_0:%\d+]] = OpAccessChain %_ptr_Function_T1 %val1 %uint_0
// CHECK-NEXT:  [[T1_c_b:%\d+]] = OpAccessChain %_ptr_Function__arr_v2float_uint_2 [[val1_0]] %int_0 %int_0
// CHECK-NEXT:     [[b_0:%\d+]] = OpAccessChain %_ptr_Function_v2float [[T1_c_b]] %uint_0
// CHECK-NEXT: [[b_0_val:%\d+]] = OpLoad %v2float [[b_0]]
// CHECK-NEXT:   [[e_val:%\d+]] = OpCompositeConstruct %S1 [[b_0_val]]

// val2[0]: Construct T2.f from T1.c.b[1]
// CHECK-NEXT:  [[val1_0:%\d+]] = OpAccessChain %_ptr_Function_T1 %val1 %uint_0
// CHECK-NEXT:  [[T1_c_b:%\d+]] = OpAccessChain %_ptr_Function__arr_v2float_uint_2 [[val1_0]] %int_0 %int_0
// CHECK-NEXT:     [[b_1:%\d+]] = OpAccessChain %_ptr_Function_v2float [[T1_c_b]] %uint_1
// CHECK-NEXT: [[b_1_val:%\d+]] = OpLoad %v2float [[b_1]]
// CHECK-NEXT:   [[f_val:%\d+]] = OpCompositeConstruct %S1 [[b_1_val]]

// val2[0]: Read T1.d as T2.g
// CHECK-NEXT:  [[val1_0:%\d+]] = OpAccessChain %_ptr_Function_T1 %val1 %uint_0
// CHECK-NEXT:    [[T1_d:%\d+]] = OpAccessChain %_ptr_Function_S2 [[val1_0]] %int_1
// CHECK-NEXT:   [[d_val:%\d+]] = OpLoad %S2 [[T1_d]]

// CHECK-NEXT:  [[val2_0:%\d+]] = OpCompositeConstruct %T2 [[e_val]] [[f_val]] [[d_val]]

// val2[1]: Construct T2.e from T1.c.b[0]
// CHECK-NEXT:  [[val1_1:%\d+]] = OpAccessChain %_ptr_Function_T1 %val1 %uint_1
// CHECK-NEXT:  [[T1_c_b:%\d+]] = OpAccessChain %_ptr_Function__arr_v2float_uint_2 [[val1_1]] %int_0 %int_0
// CHECK-NEXT:     [[b_0:%\d+]] = OpAccessChain %_ptr_Function_v2float [[T1_c_b]] %uint_0
// CHECK-NEXT: [[b_0_val:%\d+]] = OpLoad %v2float [[b_0]]
// CHECK-NEXT:   [[e_val:%\d+]] = OpCompositeConstruct %S1 [[b_0_val]]

// val2[1]: Construct T2.f from T1.c.b[1]
// CHECK-NEXT:  [[val1_1:%\d+]] = OpAccessChain %_ptr_Function_T1 %val1 %uint_1
// CHECK-NEXT:  [[T1_c_b:%\d+]] = OpAccessChain %_ptr_Function__arr_v2float_uint_2 [[val1_1]] %int_0 %int_0
// CHECK-NEXT:     [[b_1:%\d+]] = OpAccessChain %_ptr_Function_v2float [[T1_c_b]] %uint_1
// CHECK-NEXT: [[b_1_val:%\d+]] = OpLoad %v2float [[b_1]]
// CHECK-NEXT:   [[f_val:%\d+]] = OpCompositeConstruct %S1 [[b_1_val]]

// val2[1]: Read T1.d as T2.g
// CHECK-NEXT:  [[val1_1:%\d+]] = OpAccessChain %_ptr_Function_T1 %val1 %uint_1
// CHECK-NEXT:    [[T1_d:%\d+]] = OpAccessChain %_ptr_Function_S2 [[val1_1]] %int_1
// CHECK-NEXT:   [[d_val:%\d+]] = OpLoad %S2 [[T1_d]]

// CHECK-NEXT:  [[val2_1:%\d+]] = OpCompositeConstruct %T2 [[e_val]] [[f_val]] [[d_val]]

// CHECK-NEXT:    [[val2:%\d+]] = OpCompositeConstruct %_arr_T2_uint_2 [[val2_0]] [[val2_1]]
// CHECK-NEXT:                    OpStore %val2 [[val2]]
    T2 val2[2] = {val1};

// val3[0]: Construct T3.h from T1.c.b[0]
// CHECK:       [[val1_0:%\d+]] = OpAccessChain %_ptr_Function_T1 %val1 %int_0
// CHECK-NEXT:  [[T1_c_b:%\d+]] = OpAccessChain %_ptr_Function__arr_v2float_uint_2 [[val1_0]] %int_0 %int_0
// CHECK-NEXT:     [[b_0:%\d+]] = OpAccessChain %_ptr_Function_v2float [[T1_c_b]] %uint_0
// CHECK-NEXT:   [[h_val:%\d+]] = OpLoad %v2float [[b_0]]

// val3[0]: Construct T3.i from T1.c.b[1]
// CHECK-NEXT:  [[val1_0:%\d+]] = OpAccessChain %_ptr_Function_T1 %val1 %int_0
// CHECK-NEXT:  [[T1_c_b:%\d+]] = OpAccessChain %_ptr_Function__arr_v2float_uint_2 [[val1_0]] %int_0 %int_0
// CHECK-NEXT:     [[b_1:%\d+]] = OpAccessChain %_ptr_Function_v2float [[T1_c_b]] %uint_1
// CHECK-NEXT:   [[i_val:%\d+]] = OpLoad %v2float [[b_1]]

// val3[0]: Construct T3.j from T1.d.b[0]
// CHECK-NEXT:  [[val1_0:%\d+]] = OpAccessChain %_ptr_Function_T1 %val1 %int_0
// CHECK-NEXT:  [[T1_d_b:%\d+]] = OpAccessChain %_ptr_Function__arr_v2float_uint_2 [[val1_0]] %int_1 %int_0
// CHECK-NEXT:     [[b_0:%\d+]] = OpAccessChain %_ptr_Function_v2float [[T1_d_b]] %uint_0
// CHECK-NEXT:   [[j_val:%\d+]] = OpLoad %v2float [[b_0]]

// val3[0]: Construct T3.k from T1.d.b[1]
// CHECK-NEXT:  [[val1_0:%\d+]] = OpAccessChain %_ptr_Function_T1 %val1 %int_0
// CHECK-NEXT:  [[T1_d_b:%\d+]] = OpAccessChain %_ptr_Function__arr_v2float_uint_2 [[val1_0]] %int_1 %int_0
// CHECK-NEXT:     [[b_1:%\d+]] = OpAccessChain %_ptr_Function_v2float [[T1_d_b]] %uint_1
// CHECK-NEXT:   [[k_val:%\d+]] = OpLoad %v2float [[b_1]]

// CHECK-NEXT:  [[val3_0:%\d+]] = OpCompositeConstruct %T3 [[h_val]] [[i_val]] [[j_val]] [[k_val]]

// val3[1]
// CHECK-NEXT:  [[t3_val:%\d+]] = OpLoad %T3 %t3

// val3[2]: Construct T3.h from S1.a
// CHECK-NEXT:    [[s1_a:%\d+]] = OpAccessChain %_ptr_Function_v2float %s1 %int_0
// CHECK-NEXT:   [[h_val:%\d+]] = OpLoad %v2float [[s1_a]]

// val3[2]: Construct T3.i from S2.b[0]
// CHECK-NEXT:    [[s2_b:%\d+]] = OpAccessChain %_ptr_Function__arr_v2float_uint_2 %s2 %int_0
// CHECK-NEXT:  [[s2_b_0:%\d+]] = OpAccessChain %_ptr_Function_v2float [[s2_b]] %uint_0
// CHECK-NEXT:   [[i_val:%\d+]] = OpLoad %v2float [[s2_b_0]]

// val3[2]: Construct T3.j from S2.b[1]
// CHECK-NEXT:    [[s2_b:%\d+]] = OpAccessChain %_ptr_Function__arr_v2float_uint_2 %s2 %int_0
// CHECK-NEXT:  [[s2_b_1:%\d+]] = OpAccessChain %_ptr_Function_v2float [[s2_b]] %uint_1
// CHECK-NEXT:   [[j_val:%\d+]] = OpLoad %v2float [[s2_b_1]]

// val3[2]: Construct T3.k from S1.a
// CHECK-NEXT:    [[s1_a:%\d+]] = OpAccessChain %_ptr_Function_v2float %s1 %int_0
// CHECK-NEXT:   [[k_val:%\d+]] = OpLoad %v2float [[s1_a]]

// CHECK-NEXT:  [[val3_2:%\d+]] = OpCompositeConstruct %T3 [[h_val]] [[i_val]] [[j_val]] [[k_val]]

// CHECK-NEXT:    [[val3:%\d+]] = OpCompositeConstruct %_arr_T3_uint_3 [[val3_0]] [[t3_val]]
// CHECK-NEXT:                    OpStore %val3 [[val3]]
    S1 s1;
    S2 s2;
    T3 t3;
    T3 val3[3] = {val1[0],
                  t3,
                  s1, s2, s1};
}
