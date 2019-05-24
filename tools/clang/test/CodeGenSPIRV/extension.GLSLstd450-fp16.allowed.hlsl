// Run: %dxc -T ps_6_2 -E main -enable-16bit-types

// The GLSL extended instruction set may only use 16-bit floats if AMD_gpu_shader_half_float extension is used.

// CHECK: OpCapability Float16
// CHECK: OpExtension "SPV_AMD_gpu_shader_half_float"
// CHECK: OpExtInstImport "GLSL.std.450"
void main() {
  float16_t4 a;
  float16_t4 result = atan(a);
}
