// RUN: %dxc -T cs_6_0 -E mymain %s | FileCheck %s

// CHECK: error: invalid shader stage attribute combination
// CHECK: note: implicit attribute from target profile is: compute

[shader("pixel")]
[numthreads(1, 0, 0)]
void mymain() {
  return;
}
