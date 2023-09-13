// RUN: %dxc -T ps_6_0 -E main -spirv -Gis

void main() {}

// CHECK: -Gis is not supported with -spirv
