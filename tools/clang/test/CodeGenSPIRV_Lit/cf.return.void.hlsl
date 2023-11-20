// RUN: %dxc -T ps_6_0 -E main

void A() {
}

void main() {
  // CHECK: OpReturn
  return A();
}
