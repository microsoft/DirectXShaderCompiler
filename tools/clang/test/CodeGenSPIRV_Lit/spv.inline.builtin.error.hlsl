// RUN: not %dxc -T ps_6_0 -E main -fcgl  %s -spirv 2>&1 | FileCheck %s

[[vk::ext_builtin_input(/* NumWorkgroups */ 24)]]
static uint3 gl_NumWorkGroups;

void assign(inout uint3 val) {
  val.x = 10;
}

void main() {
// CHECK: error: cannot assign to input variable
  gl_NumWorkGroups = 3;

// CHECK: error: cannot use builtin input as inout argument
  assign(gl_NumWorkGroups);
}
