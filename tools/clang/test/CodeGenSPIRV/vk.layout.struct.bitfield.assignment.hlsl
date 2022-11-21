// RUN: %dxc -T vs_6_0 -E main -HV 2021

// Sanity check.
struct S1 {
    uint f1 : 1;
};

struct S2 {
    uint f1 : 1;
    uint f2 : 3;
    uint f3 : 8;
    uint f4 : 1;
};

struct S3 {
    uint f1 : 1;
     int f2 : 1;
    uint f3 : 1;
};

void main() : A {
  S1 s1;
// CHECK:     [[s1f1_ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s1 %int_0
// CHECK:     [[s1f1_load:%\d+]] = OpLoad %uint [[s1f1_ptr]]
// CHECK:     [[s1f1_insert:%\d+]] = OpBitFieldInsert %uint [[s1f1_load]] %uint_1 %int_0 %int_1
// CHECK:     OpStore [[s1f1_ptr]] [[s1f1_insert]]
  s1.f1 = 1;

  S2 s2;
// CHECK:     [[s2f1_ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s2 %int_0
// CHECK:     [[s2f1_load:%\d+]] = OpLoad %uint [[s2f1_ptr]]
// CHECK:     [[s2f1_insert:%\d+]] = OpBitFieldInsert %uint [[s2f1_load]] %uint_1 %int_0 %int_1
// CHECK:     OpStore [[s2f1_ptr]] [[s2f1_insert]]
  s2.f1 = 1;
// CHECK:     [[s2f2_ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s2 %int_0
// CHECK:     [[s2f2_load:%\d+]] = OpLoad %uint [[s2f2_ptr]]
// CHECK:     [[s2f2_insert:%\d+]] = OpBitFieldInsert %uint [[s2f2_load]] %uint_5 %int_1 %int_3
// CHECK:     OpStore [[s2f2_ptr]] [[s2f2_insert]]
  s2.f2 = 5;
// CHECK:     [[s2f3_ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s2 %int_0
// CHECK:     [[s2f3_load:%\d+]] = OpLoad %uint [[s2f3_ptr]]
// CHECK:     [[s2f3_insert:%\d+]] = OpBitFieldInsert %uint [[s2f3_load]] %uint_2 %int_4 %int_8
// CHECK:     OpStore [[s2f3_ptr]] [[s2f3_insert]]
  s2.f3 = 2;

// CHECK:     [[s2f4_ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s2 %int_0
// CHECK:     [[s2f4_load:%\d+]] = OpLoad %uint [[s2f4_ptr]]
// CHECK:     [[s2f4_insert:%\d+]] = OpBitFieldInsert %uint [[s2f4_load]] %uint_1 %int_12 %int_1
// CHECK:     OpStore [[s2f4_ptr]] [[s2f4_insert]]
  s2.f4 = 1;

  S3 s3;
// CHECK:     [[s3f1_ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s3 %int_0
// CHECK:     [[s3f1_load:%\d+]] = OpLoad %uint [[s3f1_ptr]]
// CHECK:     [[s3f1_insert:%\d+]] = OpBitFieldInsert %uint [[s3f1_load]] %uint_1 %int_0 %int_1
// CHECK:     OpStore [[s3f1_ptr]] [[s3f1_insert]]
  s3.f1 = 1;
// CHECK:     [[s3f2_ptr:%\d+]] = OpAccessChain %_ptr_Function_int %s3 %int_1
// CHECK:     [[s3f2_load:%\d+]] = OpLoad %int [[s3f2_ptr]]
// CHECK:     [[s3f2_insert:%\d+]] = OpBitFieldInsert %int [[s3f2_load]] %int_0 %int_0 %int_1
// CHECK:     OpStore [[s3f2_ptr]] [[s3f2_insert]]
  s3.f2 = 0;
// CHECK:     [[s3f3_ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s3 %int_2
// CHECK:     [[s3f3_load:%\d+]] = OpLoad %uint [[s3f3_ptr]]
// CHECK:     [[s3f3_insert:%\d+]] = OpBitFieldInsert %uint [[s3f3_load]] %uint_1 %int_0 %int_1
// CHECK:     OpStore [[s3f3_ptr]] [[s3f3_insert]]
  s3.f3 = 1;

// CHECK:     [[s2f4_2_ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s2 %int_0
// CHECK:     [[s2f4_2_load:%\d+]] = OpLoad %uint [[s2f4_2_ptr]]
// CHECK:     [[s2f4_extract:%\d+]] = OpBitFieldUExtract %uint [[s2f4_2_load]] %int_12 %int_1
// CHECK:     [[s2f4_sext:%\d+]] = OpBitcast %int [[s2f4_extract]]
// CHECK:     [[s3f2_2_ptr:%\d+]] = OpAccessChain %_ptr_Function_int %s3 %int_1
// CHECK:     [[s3f2_2_load:%\d+]] = OpLoad %int [[s3f2_2_ptr]]
// CHECK:     [[s3f2_2_insert:%\d+]] = OpBitFieldInsert %int [[s3f2_2_load]] [[s2f4_sext]] %int_0 %int_1
// CHECK:     OpStore [[s3f2_2_ptr]] [[s3f2_2_insert]]
  s3.f2 = s2.f4;
}
