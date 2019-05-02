// Run: %dxc -T ps_6_2 -E main -enable-16bit-types -fspv-extension=KHR

// The command line option instructs the compiler to only use core Vulkan features and KHR extensions,
// and prevents the compiler from using vendor-specific extensions.

// CHECK: 10:23: error: SPIR-V extension 'SPV_AMD_gpu_shader_half_float' required for 16-bit float but not permitted to use

void main() {
  float16_t4 a;
  float16_t4 result = atan(a);
}
