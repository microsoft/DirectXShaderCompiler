// RUN: not %dxc -T ps_6_0 -E main -fcgl  %s -spirv 2>&1 | FileCheck %s

[[vk::ext_builtin_input(/* NumWorkgroups */ 24)]]
static uint3 gl_NumWorkGroups;

void main() {
// CHECK: error: cannot assign to input variable
  gl_NumWorkGroups = 3;
}
