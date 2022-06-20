// RUN: %dxc -T ps_6_0 -E main -spirv -Qstrip_reflect

void main() {}

// CHECK: -Qstrip_reflect is not supported with -spirv
