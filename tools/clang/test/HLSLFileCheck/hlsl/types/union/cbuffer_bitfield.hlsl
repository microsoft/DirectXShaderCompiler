// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s

// CHECK: %Foo = type { %union.U }
// CHECK: %union.U = type { i32 }
union U {
  int x: 8;
  int y: 8;
  int z: 16;
};

cbuffer Foo {
  U input;
};

int main() : OUT {
  // CHECK: call void @dx.op.storeOutput.i32
  return input.z;
}
