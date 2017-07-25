// Run: %dxc -T ps_6_0 -E main

// TODO
// 16bit & 64bit integer & floats (require additional capability)
// float: denormalized numbers, Inf, NaN

void main() {
  // Boolean constants
// CHECK-DAG: %true = OpConstantTrue %bool
  bool c_bool_t = true;
// CHECK-DAG: %false = OpConstantFalse %bool
  bool c_bool_f = false;

  // Signed integer constants
// CHECK-DAG: %int_0 = OpConstant %int 0
  int c_int_0 = 0;
// CHECK-DAG: %int_1 = OpConstant %int 1
  int c_int_1 = 1;
// CHECK-DAG: %int_n1 = OpConstant %int -1
  int c_int_n1 = -1;
// CHECK-DAG: %int_42 = OpConstant %int 42
  int c_int_42 = 42;
// CHECK-DAG: %int_n42 = OpConstant %int -42
  int c_int_n42 = -42;
// CHECK-DAG: %int_2147483647 = OpConstant %int 2147483647
  int c_int_max = 2147483647;
// CHECK-DAG: %int_n2147483648 = OpConstant %int -2147483648
  int c_int_min = -2147483648;

  // Unsigned integer constants
// CHECK-DAG: %uint_0 = OpConstant %uint 0
  uint c_uint_0 = 0;
// CHECK-DAG: %uint_1 = OpConstant %uint 1
  uint c_uint_1 = 1;
// CHECK-DAG: %uint_38 = OpConstant %uint 38
  uint c_uint_38 = 38;
// CHECK-DAG: %uint_4294967295 = OpConstant %uint 4294967295
  uint c_uint_max = 4294967295;

  // Float constants
// CHECK-DAG: %float_0 = OpConstant %float 0
  float c_float_0 = 0.;
// CHECK-DAG: %float_n0 = OpConstant %float -0
  float c_float_n0 = -0.;
// CHECK-DAG: %float_4_2 = OpConstant %float 4.2
  float c_float_4_2 = 4.2;
// CHECK-DAG: %float_n4_2 = OpConstant %float -4.2
  float c_float_n4_2 = -4.2;
}
