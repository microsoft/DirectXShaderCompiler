// Run: %dxc -T cs_6_0 -E main

struct S {
  int a;
  float b;
};

void foo(out int x, out int y, out int z, out S w, out int v) {
  x = 1;
  y = x << 1;
  z = x << 2;
  w.a = x << 3;
  v = x << 4;
}

// CHECK: %A = OpVariable %_ptr_Uniform_type_RWStructuredBuffer_int Uniform
RWStructuredBuffer<int> A;

// CHECK: %B = OpVariable %_ptr_Private_int Private
static int B;

// CHECK: %C = OpVariable %_ptr_Workgroup_int Workgroup
groupshared int C;

// CHECK: %D = OpVariable %_ptr_Workgroup_S Workgroup
groupshared S D;

[numthreads(1,1,1)]
void main() {
// CHECK: %E = OpVariable %_ptr_Function_int Function
  int E;

// CHECK:    {{%\d+}} = OpFunctionCall %void %foo %param_var_x %param_var_y %param_var_z %param_var_w %param_var_v
  foo(A[0], B, C, D, E);
  A[0] = A[0] | B | C | D.a | E;
}
