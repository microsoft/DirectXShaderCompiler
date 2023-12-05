// RUN: not %dxc -T ps_6_0 -E main -spirv -Gis %s 2>&1 | FileCheck %s

void main() {}

// CHECK: -Gis is not supported with -spirv
