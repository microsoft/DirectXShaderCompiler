// RUN: %dxc -E truncate -HV 202x -T vs_6_2 %s | FileCheck %s -check-prefix=CHECK-TRUNCATE

union s0 {
  int x: 1;
  int y: 8;
  uint64_t z;
};

int truncate() : OUT {
  s0 s;
  s.y = 1000;
  // CHECK-TRUNCATE: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 232)
  return s.z;
}
