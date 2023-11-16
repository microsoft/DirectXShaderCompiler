// RUN: %dxc -T cs_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK: OpEntryPoint GLCompute %main "main" %gl_NumWorkGroups
// CHECK: OpDecorate %gl_NumWorkGroups BuiltIn NumWorkgroups

// CHECK: %gl_NumWorkGroups = OpVariable %_ptr_Input_v3uint Input

[[vk::ext_builtin_input(/* NumWorkgroups */ 24)]]
uint3 gl_NumWorkGroups();

[numthreads(32,1,1)]
void main() {
  // CHECK: {{%[0-9]+}} = OpLoad %v3uint %gl_NumWorkGroups
  uint3 numWorkgroups = gl_NumWorkGroups();
}
