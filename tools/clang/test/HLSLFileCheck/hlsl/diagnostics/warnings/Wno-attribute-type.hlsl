// RUN: %dxc -T vs_6_0 -Wno-attribute-type %s | FileCheck %s
// Make sure the specified warning gets turned off

// Compile with vs profile instead of cs on purpose in order to produce an error,
// otherwise, the warnings will not be captured in the output for FileCheck.

// attribute %0 must have a uint literal argument
// CHECK-NOT: uint literal argument
[numthreads(1.0f, 0, 0)]
void main() {
  return;
}

// CHECK: error: attribute numthreads only
