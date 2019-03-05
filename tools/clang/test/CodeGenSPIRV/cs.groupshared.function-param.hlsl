// Run: %dxc -T cs_6_0 -E main -Vd

// CHECK: %foo = OpVariable %_ptr_Workgroup__arr__arr_int_uint_10_uint_10 Workgroup
groupshared int foo[10][10];

int bar(int arg[10][10]) {
  return arg[0][0];
}

void main() {
  // CHECK: [[callBar:%\d+]] = OpFunctionCall %int %bar %foo
  int a = bar(foo);
}
