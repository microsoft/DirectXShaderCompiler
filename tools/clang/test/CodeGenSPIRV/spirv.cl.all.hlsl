// Run: %dxc -E main -T ps_6_1 -fspv-target-env=vulkan1.1

// This test ensures that command line options used to generate this module
// are added to the SPIR-V using OpModuleProcessed.

// Note: -spirv, -fcgl, and -Vd are added by the test infrastructure automatically.
// CHECK: OpModuleProcessed "dxc-cl-option: -E main -T ps_6_1 -spirv -fcgl -Vd -fspv-target-env=vulkan1.1"

void main() {}