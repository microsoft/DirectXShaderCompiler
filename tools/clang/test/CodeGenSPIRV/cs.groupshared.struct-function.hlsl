// Run: %dxc -T cs_6_0 -E main -Vd

struct foo {
  int bar[10][10];
  int baz() {
    return bar[0][0];
  }
};

// CHECK: %a = OpVariable %_ptr_Workgroup_foo Workgroup
groupshared foo a;

void main() {
// CHECK: [[callBaz:%\d+]] = OpFunctionCall %int %foo_baz %a
  int b = a.baz();
}
