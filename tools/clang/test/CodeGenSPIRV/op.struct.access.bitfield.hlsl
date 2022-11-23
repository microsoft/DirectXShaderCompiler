// RUN: %dxc -T ps_6_6 -E main -HV 2021

struct S1 {
  uint f1 : 1;
  uint f2 : 8;
};

struct S2 {
  int f1 : 2;
  int f2 : 9;
};

struct S3 {
  int f1;
  int f2 : 1;
  int f3;
};

// CHECK: OpMemberName %S1 0 "f1"
// CHECK-NOT: OpMemberName %S1 1 "f2"
// CHECK: OpMemberName %S2 0 "f1"
// CHECK-NOT: OpMemberName %S2 1 "f2"

// CHECK: %S1 = OpTypeStruct %uint
// CHECK: %S2 = OpTypeStruct %int

void main() {
  // CHECK: [[s1_var:%\w+]] = OpVariable %_ptr_Function_S1 Function
  // CHECK: [[s2_var:%\w+]] = OpVariable %_ptr_Function_S2 Function
  // CHECK: [[s3_var:%\w+]] = OpVariable %_ptr_Function_S3 Function
  S1 s1;
  S2 s2;
  S3 s3;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint [[s1_var]] %int_0
  // CHECK: [[load:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[insert:%\d+]] = OpBitFieldInsert %uint [[load]] %uint_1 %int_0 %int_1
  // CHECK: OpStore [[ptr]] [[insert]]
  s1.f1 = 1;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint [[s1_var]] %int_0
  // CHECK: [[load:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[insert:%\d+]] = OpBitFieldInsert %uint [[load]] %uint_2 %int_1 %int_8
  // CHECK: OpStore [[ptr]] [[insert]]
  s1.f2 = 2;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_int [[s2_var]] %int_0
  // CHECK: [[load:%\d+]] = OpLoad %int [[ptr]]
  // CHECK: [[insert:%\d+]] = OpBitFieldInsert %int [[load]] %int_3 %int_0 %int_2
  // CHECK: OpStore [[ptr]] [[insert]]
  s2.f1 = 3;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_int [[s2_var]] %int_0
  // CHECK: [[load:%\d+]] = OpLoad %int [[ptr]]
  // CHECK: [[insert:%\d+]] = OpBitFieldInsert %int [[load]] %int_4 %int_2 %int_9
  // CHECK: OpStore [[ptr]] [[insert]]
  s2.f2 = 4;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint [[s1_var]] %int_0
  // CHECK: [[load:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[extract:%\d+]] = OpBitFieldUExtract %uint [[load]] %int_0 %int_1
  // CHECK: OpStore %t1 [[extract]]
  uint t1 = s1.f1;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_uint [[s1_var]] %int_0
  // CHECK: [[load:%\d+]] = OpLoad %uint [[ptr]]
  // CHECK: [[extract:%\d+]] = OpBitFieldUExtract %uint [[load]] %int_1 %int_8
  // CHECK: OpStore %t2 [[extract]]
  uint t2 = s1.f2;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_int [[s2_var]] %int_0
  // CHECK: [[load:%\d+]] = OpLoad %int [[ptr]]
  // CHECK: [[extract:%\d+]] = OpBitFieldSExtract %int [[load]] %int_0 %int_2
  // CHECK: OpStore %t3 [[extract]]
  int t3 = s2.f1;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_int [[s2_var]] %int_0
  // CHECK: [[load:%\d+]] = OpLoad %int [[ptr]]
  // CHECK: [[extract:%\d+]] = OpBitFieldSExtract %int [[load]] %int_2 %int_9
  // CHECK: OpStore %t4 [[extract]]
  int t4 = s2.f2;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_int [[s2_var]] %int_0
  // CHECK: [[load:%\d+]] = OpLoad %int [[ptr]]
  // CHECK: [[extract:%\d+]] = OpBitFieldSExtract %int [[load]] %int_2 %int_9
  // CHECK: [[cast:%\d+]] = OpBitcast %uint [[extract]]
  // CHECK: OpStore %t5 [[cast]]
  uint t5 = s2.f2;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_int [[s3_var]] %int_0
  // CHECK: OpStore [[ptr]] %int_3
  s3.f1 = 3;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_int [[s3_var]] %int_1
  // CHECK: [[load:%\d+]] = OpLoad %int [[ptr]]
  // CHECK: [[insert:%\d+]] = OpBitFieldInsert %int [[load]] %int_4 %int_0 %int_1
  // CHECK: OpStore [[ptr]] [[insert]]
  s3.f2 = 4;

  // CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Function_int [[s3_var]] %int_2
  // CHECK: OpStore [[ptr]] %int_4
  s3.f3 = 4;
}

