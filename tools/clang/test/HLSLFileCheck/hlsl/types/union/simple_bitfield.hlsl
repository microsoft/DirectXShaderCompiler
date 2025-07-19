// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s

union s0 {
  int x: 8;
  int y: 8;
  int z: 16;
};

int main() : OUT {
  s0 s;
  s.y = 1;
  // CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 1)
  return s.z;
}
