// RUN: %dxc -T ps_6_0 -E main -spirv -Gec

void main() {}

// CHECK: -Gec is not supported with -spirv
