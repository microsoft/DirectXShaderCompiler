// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s

union s0 {
  uint abc;
};

union s1 {
  s0 a;
  uint b;
};

// CHECK: @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 1)
// CHECK: ret void
s1 main() : OUT {
  s1 s;
  s.a.abc = 1;
  return s;
}
