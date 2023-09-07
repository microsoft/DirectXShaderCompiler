// RUN: %dxc -T cs_6_0 -E mymain %s | FileCheck %s

// CHECK: error: invalid shader stage attribute combination
// CHECK: note: conflicting attribute is here

[shader("compute")]
[shader("node")]
[numthreads(1, 0, 0)]
void mymain() {
  return;
}
