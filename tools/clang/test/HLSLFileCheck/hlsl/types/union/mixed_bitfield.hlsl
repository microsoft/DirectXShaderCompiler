// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s

union s0 {
  int x: 8;
  int y: 8;
  float z;
};

s0 main() : OUT {
  s0 s;
  s.z = 1.0;
  // We don't define the results of this operation in the spec yet, but want to make sure that it doesn't crash the compiler when we see it.
  // CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32
  return s;
}
