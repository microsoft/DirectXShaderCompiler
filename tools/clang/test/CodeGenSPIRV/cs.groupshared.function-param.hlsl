// Run: %dxc -T cs_6_0 -E main

// CHECK: %A = OpVariable %_ptr_Uniform_type_RWStructuredBuffer_int Uniform
RWStructuredBuffer<int> A;

// CHECK: %B = OpVariable %_ptr_Private_int Private
static int B;

// CHECK: %C = OpVariable %_ptr_Workgroup_int Workgroup
groupshared int C;

// CHECK: %D = OpVariable %_ptr_Workgroup__arr_int_uint_10 Workgroup
groupshared int D[10];

int foo(int x, int y, int z, int w[10], int v) {
  return x | y | z | w[0] | v;
}

[numthreads(1,1,1)]
void main() {
// CHECK: %E = OpVariable %_ptr_Function_int Function
  int E;

// CHECK:      %param_var_x = OpVariable %_ptr_Function_int Function
// CHECK-NEXT: %param_var_y = OpVariable %_ptr_Function_int Function
// CHECK-NEXT: %param_var_z = OpVariable %_ptr_Function_int Function
// CHECK-NEXT: %param_var_w = OpVariable %_ptr_Function__arr_int_uint_10 Function
// CHECK-NEXT: %param_var_v = OpVariable %_ptr_Function_int Function


// CHECK:      [[A:%\d+]] = OpLoad %int {{%\d+}}
// CHECK-NEXT:              OpStore %param_var_x [[A]]
// CHECK-NEXT: [[B:%\d+]] = OpLoad %int %B
// CHECK-NEXT:              OpStore %param_var_y [[B]]
// CHECK-NEXT: [[C:%\d+]] = OpLoad %int %C
// CHECK-NEXT:              OpStore %param_var_z [[C]]
// CHECK-NEXT: [[D:%\d+]] = OpLoad %_arr_int_uint_10 %D
// CHECK-NEXT:              OpStore %param_var_w [[D]]
// CHECK-NEXT: [[E:%\d+]] = OpLoad %int %E
// CHECK-NEXT:              OpStore %param_var_v [[E]]
// CHECK-NEXT:   {{%\d+}} = OpFunctionCall %int %foo %param_var_x %param_var_y %param_var_z %param_var_w %param_var_v
  A[0] = foo(A[0], B, C, D, E);
}
