// Run: %dxc -T cs_6_0 -E main

struct S {
  int a;
  float b;

  void foo(S x, inout S y, out S z, S w) {
    a = 1;
    x.a = a << 1;
    y.a = x.a << 1;
    z.a = x.a << 2;
    w.a = x.a << 3;
  }
};

// CHECK: %A = OpVariable %_ptr_Uniform_type_RWStructuredBuffer_S Uniform
RWStructuredBuffer<S> A;

// CHECK: %B = OpVariable %_ptr_Private_S Private
static S B;

// CHECK: %C = OpVariable %_ptr_Workgroup_S Workgroup
groupshared S C;

// CHECK: %D = OpVariable %_ptr_Workgroup_S Workgroup
groupshared S D;

[numthreads(1,1,1)]
void main() {
// CHECK: %E = OpVariable %_ptr_Function_S Function
  S E;

// CHECK: %param_var_x = OpVariable %_ptr_Function_S Function
// CHECK: %param_var_w = OpVariable %_ptr_Function_S Function

// CHECK:        [[A:%\d+]] = OpAccessChain %_ptr_Uniform_S_0 %A %int_0 %uint_0
// CHECK-NEXT: [[A_0:%\d+]] = OpLoad %S_0 [[A]]
// CHECK-NEXT:   [[a:%\d+]] = OpCompositeExtract %int [[A_0]] 0
// CHECK-NEXT:   [[b:%\d+]] = OpCompositeExtract %float [[A_0]] 1
// CHECK-NEXT: [[A_0:%\d+]] = OpCompositeConstruct %S [[a]] [[b]]
// CHECK-NEXT:                OpStore %param_var_x [[A_0]]
// CHECK-NEXT:   [[E:%\d+]] = OpLoad %S %E
// CHECK-NEXT:                OpStore %param_var_w [[E]]
// CHECK-NEXT:     {{%\d+}} = OpFunctionCall %void %S_foo %D %param_var_x %B %C %param_var_w
  D.foo(A[0], B, C, E);

  A[0].a = A[0].a | B.a | C.a | D.a;
}
