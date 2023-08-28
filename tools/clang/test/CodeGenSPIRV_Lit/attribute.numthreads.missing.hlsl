// RUN: not %dxc -T cs_6_0 -E main -fcgl  %s -spirv  2>&1 | FileCheck %s

// CHECK: 4:6: error: thread group size [numthreads(x,y,z)] is missing from the entry-point function
void main() {}
