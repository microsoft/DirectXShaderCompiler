// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 0)

bool3 main() : OUT {
  bool3 b = false;
  return b;
}
