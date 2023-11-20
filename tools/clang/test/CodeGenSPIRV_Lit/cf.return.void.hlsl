// RUN: %dxc -T ps_6_0 -E main -fcgl  %s -spirv | FileCheck %s

void A() {
}

void main() {
  // CHECK: OpReturn
  return A();
}
