// RUN: %dxc -T cs_6_0 -E mymain %s | FileCheck %s

// CHECK-DAG: error: Invalid shader stage attribute combination
// CHECK-DAG: note: See conflicting shader attribute

[shader("pixel")]
[numthreads(1, 0, 0)]
void mymain() {
  return;
}
