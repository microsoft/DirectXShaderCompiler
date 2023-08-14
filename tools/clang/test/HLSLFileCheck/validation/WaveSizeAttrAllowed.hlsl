// RUN: %dxc -E main -T lib_6_7 %s | FileCheck %s

// This test used to fail because it didn't take into account
// the possibility of the compute shader being a library target.
// Now, this test is an example of what should be allowed
// and no errors are expected.

// CHECK: define void @main()

[Shader("compute")]
[WaveSize(8)]
[numthreads(2,2,2)]
void main() {
  return;
}