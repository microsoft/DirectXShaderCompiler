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
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s1 %int_0
// CHECK:     [[load:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:     [[insert:%\d+]] = OpBitFieldInsert %uint [[load]] %uint_1 %uint_0 %uint_1
// CHECK:     OpStore [[ptr]] [[insert]]
  s1.f1 = 1;

  S2 s2;
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s2 %int_0
// CHECK:     [[load:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:     [[insert:%\d+]] = OpBitFieldInsert %uint [[load]] %uint_1 %uint_0 %uint_1
// CHECK:     OpStore [[ptr]] [[insert]]
  s2.f1 = 1;
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s2 %int_0
// CHECK:     [[load:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:     [[insert:%\d+]] = OpBitFieldInsert %uint [[load]] %uint_5 %uint_1 %uint_3
// CHECK:     OpStore [[ptr]] [[insert]]
  s2.f2 = 5;
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s2 %int_0
// CHECK:     [[load:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:     [[insert:%\d+]] = OpBitFieldInsert %uint [[load]] %uint_2 %uint_4 %uint_8
// CHECK:     OpStore [[ptr]] [[insert]]
  s2.f3 = 2;

// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s2 %int_0
// CHECK:     [[load:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:     [[insert:%\d+]] = OpBitFieldInsert %uint [[load]] %uint_1 %uint_12 %uint_1
// CHECK:     OpStore [[ptr]] [[insert]]
  s2.f4 = 1;

  S3 s3;
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s3 %int_0
// CHECK:     [[load:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:     [[insert:%\d+]] = OpBitFieldInsert %uint [[load]] %uint_1 %uint_0 %uint_1
// CHECK:     OpStore [[ptr]] [[insert]]
  s3.f1 = 1;
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Function_int %s3 %int_1
// CHECK:     [[load:%\d+]] = OpLoad %int [[ptr]]
// CHECK:     [[insert:%\d+]] = OpBitFieldInsert %int [[load]] %int_0 %uint_0 %uint_1
// CHECK:     OpStore [[ptr]] [[insert]]
  s3.f2 = 0;
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s3 %int_2
// CHECK:     [[load:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:     [[insert:%\d+]] = OpBitFieldInsert %uint [[load]] %uint_1 %uint_0 %uint_1
// CHECK:     OpStore [[ptr]] [[insert]]
  s3.f3 = 1;

// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint %s2 %int_0
// CHECK:     [[load:%\d+]] = OpLoad %uint [[ptr]]
// CHECK:     [[s2f4_extract:%\d+]] = OpBitFieldUExtract %uint [[load]] %uint_12 %uint_1
// CHECK:     [[s2f4_sext:%\d+]] = OpBitcast %int [[s2f4_extract]]
// CHECK:     [[ptr:%\d+]] = OpAccessChain %_ptr_Function_int %s3 %int_1
// CHECK:     [[load:%\d+]] = OpLoad %int [[ptr]]
// CHECK:     [[insert:%\d+]] = OpBitFieldInsert %int [[load]] [[s2f4_sext]] %uint_0 %uint_1
// CHECK:     OpStore [[ptr]] [[insert]]
  s3.f2 = s2.f4;
}
