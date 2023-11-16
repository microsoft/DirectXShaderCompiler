// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK: OpEntryPoint Fragment %main "main" [[fragStencilVar:%[0-9]+]]
// CHECK: OpDecorate [[fragStencilVar]] BuiltIn FragStencilRefEXT

// CHECK: [[fragStencilVar]] = OpVariable %_ptr_Output_int Output

[[vk::ext_extension("SPV_EXT_shader_stencil_export")]]
[[vk::ext_builtin_output(/* FragStencilRefEXT */ 5014)]]
void gl_FragStencilRefARB(int);

void main() {
  // CHECK: OpStore [[fragStencilVar]] %int_10
  gl_FragStencilRefARB(10);
}
