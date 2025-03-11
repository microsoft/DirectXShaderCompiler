// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s

union s1 {
  bool a;
  uint b;
  float c;
};

// CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 1)
// CHECK: ret void
s1 main() : OUT {
  s1 s;
  s.a = true;
  return s;
}
