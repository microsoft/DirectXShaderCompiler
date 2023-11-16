// RUN: not %dxc -T ps_6_0 -E main -fcgl  %s -spirv 2>&1 | FileCheck %s

[[vk::ext_builtin_input(/* NumWorkgroups */ 24)]]
uint3 gl_NumWorkGroups(int);

[[vk::ext_extension("SPV_EXT_shader_stencil_export")]]
[[vk::ext_builtin_output(/* FragStencilRefEXT */ 5014)]]
void gl_FragStencilRefARB(int, float);

void main() {
// CHECK: error: function with vk::ext_builtin_input cannot take arguments
  uint3 numWorkgroups = gl_NumWorkGroups(10);
// CHECK: error: function with vk::ext_builtin_output must take exactly one attribute
  gl_FragStencilRefARB(10, 3.5);
}
