// RUN: %dxc -T ps_6_0 -E main -spirv -Fre file.ext

void main() {}

// CHECK: -Fre is not supported with -spirv
