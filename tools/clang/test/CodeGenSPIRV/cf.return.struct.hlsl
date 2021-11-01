// RUN: %dxc -T ps_6_0 -E main

struct S {
  int x;
  float y;
};

S foo();

void main() {

// CHECK: [[foo:%\d+]] = OpFunctionCall %S %foo
// CHECK: OpStore %result [[foo]]
  S result = foo();
}

S foo() {
// CHECK: %s = OpVariable %_ptr_Function_S Function
// CHECK: OpStore %s {{%\d+}}
  S s = {1, 2.0};

// CHECK: [[s:%\d+]] = OpLoad %S %s
// CHECK: OpReturnValue [[s]]
  return s;
}
