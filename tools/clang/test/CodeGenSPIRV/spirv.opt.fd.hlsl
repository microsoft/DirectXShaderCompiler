// RUN: %dxc -T ps_6_0 -E main -spirv -Fd file.ext

void main() {}

// CHECK: -Fd is not supported with -spirv
