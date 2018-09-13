// Run: %dxc -T ps_6_0 -E main -Oconfig=--test-unknown-flag

void main() {}

// CHECK: failed to optimize SPIR-V: Unknown flag '--test-unknown-flag'